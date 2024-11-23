/* 
 * This file is part of the positron distribution (https://github.com/radredgreen/positron).
 * Copyright (c) 2024 RadRedGreen.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "HAP.h"
#include "HAPBase.h"

#include "App_Camera.h"
#include "App.h"
#include "DB.h"
#include <arpa/inet.h>
#include "util_base64.h"
#include <HAP.h>
#include <HAP+Internal.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/prctl.h>

#include <signal.h> // pthread kill

#include "POSCameraController.h"
#include "POSRTPController.h"
#include "POSSRTPCrypto.h"

#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_framesource.h>
#include <imp/imp_encoder.h>
#include <imp/imp_ivs.h>
#include <imp/imp_ivs_move.h>
#include <imp/imp_isp.h>
#include <imp/imp_audio.h>

#include <imp/imp_osd.h>

#include "aacenc_lib.h"
#include "aacdecoder_lib.h"

#include "POSRingBuffer.h"

//#define MUTE_ALL_SOUND

extern AccessoryConfiguration accessoryConfiguration;

static const HAPLogObject logObject = {.subsystem = kHAP_LogSubsystem, .category = "POSCameraController"};

static HAPTime ActualTime()
{
  struct timespec spec;
  clock_gettime(CLOCK_REALTIME, &spec);
  uint32_t sec = (uint32_t)spec.tv_sec;
  uint32_t ns = (uint32_t)spec.tv_nsec;
  return (HAPTime)sec * 1000000000 + ns;
}

int callback_param = 0;
extern HAPAccessory accessory;
uint64_t lastMotionNotification;
int previousDetected = 0;

static void HandleIVSMotionCallback(
    void *_Nullable context,
    size_t contextSize)
{
  // run in the hap loop
  HAPAssert(context);
  HAPAssert(contextSize == sizeof(int));

  int motion_param = *((int *)context);

  switch (motion_param)
  {
  case 0:
  {
    accessoryConfiguration.state.motion.detected = false;
  }
  break;
  case 1:
  {
    accessoryConfiguration.state.motion.detected = true;
  }
  break;
  default:
  {
  }
  break;
  }
  // rate limit to 1 second
  if ((accessoryConfiguration.state.motion.detected != previousDetected) && (ActualTime() - (1 << 30) > lastMotionNotification))
  {
    previousDetected = accessoryConfiguration.state.motion.detected;
    lastMotionNotification = ActualTime();
    HAPAccessoryServerRaiseEvent(
        accessoryConfiguration.server,
        &motionDetectedCharacteristic,
        &motionDetectService,
        &accessory);
  }
}

// todo, move this function to a file dedicated to motion detect
void *sample_ivs_move_get_result_process(void *arg)
{
  int i = 0, ret = 0;
  int chn_num = (int)arg;
  IMP_IVS_MoveOutput *result = NULL;

  prctl(PR_SET_NAME, "ivs_get_result");

  for (i = 0; true; i++)
  {
    ret = IMP_IVS_PollingResult(chn_num, IMP_IVS_DEFAULT_TIMEOUTMS);
    if (ret < 0)
    {
      HAPLogError(&logObject, "IMP_IVS_PollingResult(%d, %d) failed", chn_num, IMP_IVS_DEFAULT_TIMEOUTMS);
      return (void *)-1;
    }
    ret = IMP_IVS_GetResult(chn_num, (void **)&result);
    if (ret < 0)
    {
      HAPLogError(&logObject, "IMP_IVS_GetResult(%d) failed", chn_num);
      return (void *)-1;
    }

    // HAPLogDebug(&logObject,"frame[%d], result->retRoi(%d,%d,%d,%d)", i, result->retRoi[0], result->retRoi[1], result->retRoi[2], result->retRoi[3]);
    if (result->retRoi[0])
      HAPLogInfo(&logObject, "Motion Detected: frame[%d], result->retRoi(%d)", i, result->retRoi[0]);
    else
    {
      //  HAPLogDebug(&logObject,"frame[%d], result->retRoi(%d)", i, result->retRoi[0]);
    }

    HAPError err;
    callback_param = result->retRoi[0];
    err = HAPPlatformRunLoopScheduleCallback(
        HandleIVSMotionCallback, &callback_param, sizeof callback_param);
    if (err)
    {
      HAPLogError(&kHAPLog_Default, "SignalHandler failed");
      // HAPFatalError();
    }

    ret = IMP_IVS_ReleaseResult(chn_num, (void *)result);
    if (ret < 0)
    {
      HAPLogError(&logObject, "IMP_IVS_ReleaseResult(%d) failed", chn_num);
      return (void *)-1;
    }
#if 0
		if (i % 20 == 0) {
			ret = sample_ivs_set_sense(chn_num, i % 5);
			if (ret < 0) {
				HAPLogError(&logObject, "sample_ivs_set_sense(%d, %d) failed", chn_num, i % 5);
				return (void *)-1;
			}
		}
#endif
  }

  return (void *)0;
}

static int frmrate_sp[3] = {0};
static int statime_sp[3] = {0};
static int bitrate_sp[3] = {0};

typedef uint64_t HAPEpochTime;

// todo, move this function to a file dedicated to the video stream
static void *srtp_video_feedback(void *context)
{
  AccessoryContext *myContext = context;

  int sock = myContext->session.videoFeedbackThread.socket;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(sock, &fds);

  prctl(PR_SET_NAME, "pos_srtp_feedb");

  uint8_t packet[4096];
  size_t numPacketBytes = 0;
  HAPEpochTime time;
  int ret;
  int chnNum = myContext->session.videoThread.chn_num;

  // WORKAROUND ( SELECT CAUSES RUNAWAY HAP )
  // this is causing high cpu utilization in the HAP run loop... not sure why
  //  while( ! myContext->session.videoFeedbackThread.threadStop ){
  //    int ret;
  //    //select can modify timeout to be the time not slept
  //    struct timeval timeout;
  //    timeout.tv_sec = 5;
  //    timeout.tv_usec = 0; // 1 s
  //    ret = select(sock+1, &fds, NULL, NULL, &timeout);
  //    if (ret == -1 && errno == EINTR){
  //      continue;
  //    }
  //    if (ret < 0){
  //      HAPLogError(&logObject, "Select failed in srtp_video_feedback thread");
  //      return NULL;
  //    }
  //    if (ret  > 0){

  // work around with a nonblocking socket
  //  ret = setnonblock(sock);
  //  if (ret < 0) {
  //    HAPLogError(&logObject, "srtp_video_feedback setnonblock(%d) failed", chnNum);
  //  }

  while (!myContext->session.videoFeedbackThread.threadStop)
  {

//    if (FD_ISSET(sock, &fds))
//   {

      size_t numReceivedBytes;
      do
      {
        numReceivedBytes = recv(sock, packet, sizeof(packet), 0);
      } while ((numReceivedBytes == -1) && (errno = EINTR));
      time = ActualTime();

      if (numReceivedBytes > 0)
      {
        // HAPLogDebug(&logObject, "srtp_video_feedback thread got a packet.  Len: %d", numReceivedBytes);
        // hexDump("srtcpPacket",&packet, numReceivedBytes, 16);
        POSRTPStreamPushPacket(
            &myContext->session.rtpVideoStream,
            packet,
            numReceivedBytes,
            &numPacketBytes,
            time);
        if (numPacketBytes)
        {
          uint8_t newPacket[4096];
          size_t numPayloadBytes = 0;
          HAPTimeNS sampleTime;
          POSRTPStreamPollPayload(
              &myContext->session.rtpVideoStream,
              newPacket,
              sizeof(newPacket),
              &numPayloadBytes,
              &sampleTime);
          // PushSpeakerData , packet1, numPayloadBytes, sampleTime
        }
      }
    //}
    //    }

    time = ActualTime();
    uint32_t bitRate = 0;
    bool newKeyFrame = 0;
    uint32_t dropoutTime;

    POSRTPStreamCheckFeedback(
        &myContext->session.rtpVideoStream,
        time,
        packet,
        sizeof(packet),
        &numPacketBytes,
        &bitRate,
        &newKeyFrame,
        &dropoutTime);

    // Send RTCP feedback, if any
    if (numPacketBytes)
    {
      ret = send(sock, packet, numPacketBytes, 0);
      ;
      if (ret != numPacketBytes)
      {
        HAPLogError(&logObject, "Tried to send %d bytes, but send only sent %d", numPacketBytes, ret);
        // printf("send error (%d): %s\n", errno, strerror(errno));
      }
    }
    if (bitRate)
    {
      HAPLogInfo(&logObject, "RTCP requested new bitrate: %d", bitRate);
      ret = IMP_Encoder_SetChnBitRate(chnNum, bitRate, bitRate << 2);
      if (ret < 0)
      {
        HAPLogError(&logObject, "IMP_Encoder_SetChnBitRate(%d, %d) failed", chnNum, bitRate);
      }
    }
    if (newKeyFrame)
    {
      HAPLogInfo(&logObject, "RTCP requested new keyframe");
      ret = IMP_Encoder_RequestIDR(myContext->session.videoThread.chn_num);
      if (ret < 0)
      {
        HAPLogError(&logObject, "IMP_Encoder_RequestIDR(%d) failed", chnNum);
      }
    }
    // this will never be called since we're blocking on read instead of using select
    if (dropoutTime > 300)
    {
      HAPLogError(&logObject, "Stream timeout, closing video stream");
      // not sure what to do here.  call posStopStream? - can't that'll wait in this
      // do this for now

      // Stop the videoThread
      myContext->session.videoThread.threadStop = 1;
      // Stop this thread
      myContext->session.videoFeedbackThread.threadStop = 1;
      //
      accessoryConfiguration.state.streaming = kHAPCharacteristicValue_StreamingStatus_Available;
      myContext->session.status = kHAPCharacteristicValue_StreamingStatus_Available;
    }
  }
  HAPLogError(&logObject, "Exiting video rtcp feedback thread.");
  return NULL;
}

// todo, move this function to a file dedicated to the video stream
static void *get_srtp_video_stream(void *context)
{
#if 1
  //  HAPLogError(&logObject, "In capture thread.");
  int val, i, chnNum, ret, sock;

  AccessoryContext *myContext = context;

  sock = myContext->session.videoThread.socket;

  chnNum = myContext->session.videoThread.chn_num;

  prctl(PR_SET_NAME, "pos_srtp_vid");

  ret = IMP_Encoder_StartRecvPic(chnNum);
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_Encoder_StartRecvPic(%d) failed", chnNum);
    return ((void *)-1);
  }

  while (!myContext->session.videoThread.threadStop)
  {
    // HAPLogError(&logObject, "In capture loop.");

    ret = IMP_Encoder_PollingStream(chnNum, 1000);
    if (ret < 0)
    {
      HAPLogError(&logObject, "IMP_Encoder_PollingStream(%d) timeout", chnNum);
      continue;
    }

    IMPEncoderStream ImpEncStream;
    /* Get H264 or H265 Stream */
    ret = IMP_Encoder_GetStream(chnNum, &ImpEncStream, 1);
    if (ret < 0)
    {
      HAPLogError(&logObject, "IMP_Encoder_GetStream(%d) failed", chnNum);
      return ((void *)-1);
    }

    int len = 0;
    int ret, i, nr_pack = ImpEncStream.packCount;

    //  HAPLogDebug(&logObject, "----------packCount=%d, ImpEncStream->seq=%u start----------", ImpEncStream.packCount, ImpEncStream.seq);
    for (i = 0; i < nr_pack; i++)
    {
      len += ImpEncStream.pack[i].length; // bit rate logging

      // HAPLogDebug(&logObject, "[%d]:%10u,%10lld,%10u,%10u,%10u", i, ImpEncStream.pack[i].length, ImpEncStream.pack[i].timestamp, ImpEncStream.pack[i].frameEnd, *((uint32_t *)(&ImpEncStream.pack[i].nalType)), ImpEncStream.pack[i].sliceType);

      IMPEncoderPack *pack = &ImpEncStream.pack[i];

      HAPAssert(ImpEncStream.streamSize - pack->offset >= pack->length); // shouldn't happen

      // hexDump("imp pack", (void *)(ImpEncStream.virAddr + pack->offset), 128, 16);
      if (pack->length && !myContext->session.videoThread.threadPause)
      {
        size_t numPayloadBytes = 0;
        // TODO:  Should the sequence number be derived from the encoder instead of the ActualTime?
        POSRTPStreamPushPayload(
            &myContext->session.rtpVideoStream,
            (void *)(ImpEncStream.virAddr + pack->offset + 4), // 4 removes the nal start prefix
            pack->length - 4,
            &numPayloadBytes,
            pack->timestamp * 1000, // us to ns conversion
            ActualTime());

        if (numPayloadBytes > 0)
        {
          for (;;)
          {
            uint8_t packet_data[4096];
            size_t packet_len = 0;
            POSRTPStreamPollPacket(
                &myContext->session.rtpVideoStream,
                packet_data,
                sizeof(packet_data),
                &packet_len);
            if (packet_len == 0)
              break;
            // printf("sending %d bytes\n", packet_len);
            // printf("sending %d video bytes, index %d, fd %d\n", packet_len,*(uint16_t *)(&packet_data[2]), sock);
            ret = send(sock, packet_data, packet_len, 0);
            if (ret != packet_len)
            {
              HAPLogError(&logObject,"Tried to send %d bytes, but send only sent %d", packet_len, ret);
              // printf("send error (%d): %s\n", errno, strerror(errno));
            }
          }
        }
      }
    }

    // WORKAROUND ( SELECT CAUSES RUNAWAY HAP )
    if (ImpEncStream.seq % 128 == 0)
    { //...periodically
      uint64_t NTPTime = HAPTimeToNTPTime(ActualTime());
      int dropoutTime = (uint32_t)(NTPTime >> 32) - (uint32_t)(myContext->session.rtpVideoStream.lastRecvNTPtime);
      HAPLogInfo(&logObject, "Last RTCP frame was %d sec ago", dropoutTime);
      if (dropoutTime > 30)
      {
        HAPLogError(&logObject, "Haven't receieved an RTCP frame in 30 seconds, ending stream");
        myContext->session.videoThread.threadStop = 1;
        myContext->session.videoFeedbackThread.threadStop = 1;
        myContext->session.audioThread.threadStop = 1;
        myContext->session.audioFeedbackThread.threadStop = 1;

        //printf("accessoryConfiguration.state.streaming address in get_srtp_video_stream: %p\n", &accessoryConfiguration.state.streaming);
        accessoryConfiguration.state.streaming = kHAPCharacteristicValue_StreamingStatus_Available;
        myContext->session.status = kHAPCharacteristicValue_StreamingStatus_Available;
      }
    }

    // HAPLogDebug(&logObject, "----------packCount=%d, ImpEncStream->seq=%u end----------", ImpEncStream.packCount, ImpEncStream.seq);

    IMPEncoderChnStat stat;
    IMP_Encoder_Query(0, &stat);
    if (ret < 0)
    {
      HAPLogError(&logObject, "IMP_Encoder_Query(%d) failed", chnNum);
    }
    if (stat.leftStreamFrames > 2)
    {
      HAPLogInfo(&logObject, "Falling behind, leftStreamFrames: %d", stat.leftStreamFrames);
    }

    bitrate_sp[chnNum] += len;
    frmrate_sp[chnNum]++;

    int64_t now = IMP_System_GetTimeStamp() / 1000;
    if (((int)(now - statime_sp[chnNum]) / 1000) >= 2)
    { // report bit rate every 2 seconds
      double fps = (double)frmrate_sp[chnNum] / ((double)(now - statime_sp[chnNum]) / 1000);
      double kbr = (double)bitrate_sp[chnNum] * 8 / (double)(now - statime_sp[chnNum]);

      HAPLogInfo(&logObject, "streamNum[%d]:FPS: %d,Bitrate: %d(kbps)", chnNum, (int)floor(fps), (int)floor(kbr));

      frmrate_sp[chnNum] = 0;
      bitrate_sp[chnNum] = 0;
      statime_sp[chnNum] = now;
    }

    IMP_Encoder_ReleaseStream(chnNum, &ImpEncStream);
  }
  HAPLogError(&logObject, "IMP_Encoder_StopRecvPic");

  ret = IMP_Encoder_StopRecvPic(chnNum);
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_Encoder_StopRecvPic(%d) failed", chnNum);
    return ((void *)-1);
  }

  HAPLogError(&logObject, "Exiting capture thread.");
  return ((void *)0);
#endif
}

ring_buffer_t ring_buffer;
void * ring_buffer_index_storage[128];
uint8_t ring_buffer_storage[1024*128];
pthread_mutex_t spkQueueMutex;
pthread_cond_t spkQueueCond;


// todo, move this function to a file dedicated to the audio stream
static void *srtp_audio_feedback(void *context)
{
  AccessoryContext *myContext = context;
  int sock = myContext->session.audioFeedbackThread.socket;
//  fd_set fds;
//  FD_ZERO(&fds);
//  FD_SET(sock, &fds);

  prctl(PR_SET_NAME, "pos_aud_feedb");

  uint8_t packet[4096];
  size_t numPacketBytes = 0;
  HAPEpochTime time;
  int ret;



  // setup aac decoding
  AACENC_ERROR aacErr = AACENC_OK;
  HANDLE_AACDECODER hAacDec;
  hAacDec = aacDecoder_Open(TT_MP4_RAW, 1);
  if (hAacDec == NULL)
  {
    HAPLogError(&logObject, "aacDecoder_Open error");
  }
  
  // trial and error 
  // stream info: channel = 1	sample_rate = 16000	frame_size = 480	aot = 39	bitrate = 0
  //uint8_t eld_conf[] = {0xf8, 0xf0, 0x30, 0x00};
  uint8_t eld_conf[] = {0xf8, 0xf0, 0x21, 0x2c, 0x00, 0xbc, 0x00};
  uint8_t *conf[] = {eld_conf};
  int conf_len = sizeof(eld_conf);

  aacErr = aacDecoder_ConfigRaw(hAacDec, conf, &conf_len);
  if (aacErr != AAC_DEC_OK)
  {
    HAPLogError(&logObject, "aacDecoder_Open error");
  }

  char ancBuffer[1024];
  aacErr = aacDecoder_AncDataInit(hAacDec, ancBuffer, sizeof(ancBuffer));
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacDecoder_AncDataInit err");
  }


	CStreamInfo *stream_info;
  stream_info = aacDecoder_GetStreamInfo(hAacDec);
  if (stream_info == NULL) {
    HAPLogError(&logObject, "aacDecoder_GetStreamInfo failed!");
	  //return NULL;
  }
  HAPLogDebug(&logObject, "> stream info: channel = %d\tsample_rate = %d\tframe_size = %d\taot = %d\tbitrate = %d",   \
          stream_info->channelConfig, stream_info->aacSampleRate,
          stream_info->aacSamplesPerFrame, stream_info->aot, stream_info->bitRate);



  // WORKAROUND ( SELECT CAUSES RUNAWAY HAP )
  // this is causing high cpu utilization in the HAP run loop... not sure why
  //  while( ! myContext->session.audioFeedbackThread.threadStop ){
  //    int ret;
  //    //select can modify timeout to be the time not slept
  //    struct timeval timeout;
  //    timeout.tv_sec = 5;
  //    timeout.tv_usec = 0; // 1 s
  //    ret = select(sock+1, &fds, NULL, NULL, &timeout);
  //    if (ret == -1 && errno == EINTR){
  //      continue;
  //    }
  //    if (ret < 0){
  //      HAPLogError(&logObject, "Select failed in srtp_audio_feedback thread");
  //      return NULL;
  //    }
  //    if (ret  > 0){

  // work around with a nonblocking socket
  //  ret = setnonblock(sock);
  //  if (ret < 0) {
  //    HAPLogError(&logObject, "srtp_audio_feedback setnonblock(%d) failed", chnNum);
  //  }

  while (!myContext->session.audioFeedbackThread.threadStop)
  {

    //if (FD_ISSET(sock, &fds))
    //{

      size_t numReceivedBytes;
      do
      {
        numReceivedBytes = recv(sock, packet, sizeof(packet), 0);
      } while ((numReceivedBytes == -1) && (errno = EINTR));
      time = ActualTime();

      if (numReceivedBytes > 0)
      {
        HAPLogDebug(&logObject, "srtp_audio_feedback thread got a packet.  Len: %d", numReceivedBytes);
        //hexDump("srtcpPacket", &packet, numReceivedBytes, 16);
        numPacketBytes = 0;
        POSRTPStreamPushPacket(
            &myContext->session.rtpAudioStream,
            packet,
            numReceivedBytes,
            &numPacketBytes,
            time);
        if (numPacketBytes)
        {
          uint8_t newPacket[4096];
          size_t numPayloadBytes = 0;
          HAPTimeNS sampleTime;
          POSRTPStreamPollPayload(
              &myContext->session.rtpAudioStream,
              newPacket,
              sizeof(newPacket),
              &numPayloadBytes,
              &sampleTime);
          //hexDump("decrypted srtpPacket", &newPacket, numPayloadBytes, 16);
          // PushSpeakerData , newPacket, numPayloadBytes, sampleTime

          if(numPayloadBytes){
            uint8_t *pNewPacket4 = &(newPacket[4]);

            //hexDump("rfc3640 header", &newPacket, 16,16);

            size_t decoderNumPayloadBytes = numPayloadBytes - 4;

            uint16_t headerSize = __bswap_16 ( * (uint16_t * )(&(newPacket[2])) ) >> 3; // not sure why this needs swapped

            //printf("Header Size: %d, Packet Size: %d\n", headerSize, decoderNumPayloadBytes);

            size_t bytesNotUsed = decoderNumPayloadBytes;
            aacErr = aacDecoder_Fill(hAacDec, &pNewPacket4, &decoderNumPayloadBytes, &bytesNotUsed);
            if (aacErr != AACENC_OK)
            {
              HAPLogError(&logObject, "aacDecoder_Fill err");
            }
            if (bytesNotUsed)
              HAPLogError(&logObject, "the decoder didn't use bytes from the controller: %d", bytesNotUsed);

            INT_PCM timeData[1024];
            aacErr = aacDecoder_DecodeFrame(hAacDec, &timeData, 1024, 0);
            if (aacErr != AACENC_OK)
            {
              HAPLogError(&logObject, "aacDecoder_DecodeFrame err: %d", aacErr);
              //hexDump("BAD aacFrame",  pNewPacket4, decoderNumPayloadBytes, 16 );
            }
            else{
              //hexDump("GOOD aacFrame",  pNewPacket4, decoderNumPayloadBytes, 16 );
            }
            // trying to catch some of the errors where the decoder bails on claimed ancillary data
            #if 1 //is this doing anything?

            uint8_t * ancPtr = NULL;
            int ancSize = 0;
            aacErr = aacDecoder_AncDataGet(hAacDec, 0, &ancPtr, &ancSize);
            if (aacErr != AACENC_OK)
            {
              HAPLogError(&logObject, "aacDecoder_DecodeFrame err: %d", aacErr);
            }

            if(ancSize != 0){
              HAPLogInfo(&logObject, "aacDecoder_AncDataGet got some data", aacErr);
              //hexDump("ancData", ancPtr, ancSize, 16);
            }

            #endif

            //send data to the ring buffer...

            //Lock the queue mutex to make sure that adding data to the queue happens correctly
            pthread_mutex_lock(&spkQueueMutex);

            //Push new data to the queue
            memcpy(&ring_buffer_storage[ring_buffer.head_index*1024], &timeData, 1024);
            ptr_ring_buffer_queue(&ring_buffer, &ring_buffer_storage[ring_buffer.head_index*1024]);

            //Signal the condition variable that new data is available in the queue
            pthread_cond_signal(&spkQueueCond);

            //Done, unlock the mutex
            pthread_mutex_unlock(&spkQueueMutex);
          }
        }
      }
   //}
    //    }

    time = ActualTime();
    uint32_t bitRate = 0;
    bool newKeyFrame = 0;
    uint32_t dropoutTime;

    //printf("Calling POSRTPStreamCheckFeedback in the audio feedback thread\n");
    numPacketBytes = 0;
    POSRTPStreamCheckFeedback(
        &myContext->session.rtpAudioStream,
        time,
        packet,
        sizeof(packet),
        &numPacketBytes,
        &bitRate,
        &newKeyFrame,
        &dropoutTime);
    // Send RTCP feedback, if any
    if (numPacketBytes)
    {
      //printf("Sending audio RTCP feedback %d bytes\n", numPacketBytes);
      //hexDump("audio feedback srtp packet", packet, numPacketBytes, 16);
      ret = send(sock, packet, numPacketBytes, 0);
      ;
      if (ret != numPacketBytes)
      {
        HAPLogError(&logObject, "Tried to send %d bytes, but send only sent %d", numPacketBytes, ret);
        // printf("send error (%d): %s\n", errno, strerror(errno));
      }
    }
  }

  // dealocate aac decoder and transport layer structures
  aacDecoder_Close(hAacDec);

  HAPLogError(&logObject, "Exiting audio rtcp feedback thread.");
  //return NULL;
}

pthread_t speaker_pthread = NULL;
static void *speaker_thread(void *context)
{
  AccessoryContext *myContext = context;

  prctl(PR_SET_NAME, "pos_spk");

  int ret;
  
  // setup ingenic audio output
  int devID = 0;
  IMPAudioIOAttr attr;
  attr.samplerate = AUDIO_SAMPLE_RATE_16000;
  attr.bitwidth = AUDIO_BIT_WIDTH_16;
  attr.soundmode = AUDIO_SOUND_MODE_MONO;
  attr.frmNum = 30;
  attr.numPerFrm = 480;
  attr.chnCnt = 1;
  ret = IMP_AO_SetPubAttr(devID, &attr);
  if (ret != 0)
  {
    HAPLogError(&logObject, "set ao %d attr err: %d", devID, ret);
    //return NULL;
  }

  memset(&attr, 0x0, sizeof(attr));
  ret = IMP_AO_GetPubAttr(devID, &attr);
  if (ret != 0)
  {
    HAPLogError(&logObject, "get ao %d attr err: %d", devID, ret);
    //return NULL;
  }

  HAPLogDebug(&logObject, "Audio Out GetPubAttr samplerate:%d", attr.samplerate);
  HAPLogDebug(&logObject, "Audio Out GetPubAttr   bitwidth:%d", attr.bitwidth);
  HAPLogDebug(&logObject, "Audio Out GetPubAttr  soundmode:%d", attr.soundmode);
  HAPLogDebug(&logObject, "Audio Out GetPubAttr     frmNum:%d", attr.frmNum);
  HAPLogDebug(&logObject, "Audio Out GetPubAttr  numPerFrm:%d", attr.numPerFrm);
  HAPLogDebug(&logObject, "Audio Out GetPubAttr     chnCnt:%d", attr.chnCnt);

  /* Step 2: enable AO device. */
  ret = IMP_AO_Enable(devID);
  if (ret != 0)
  {
    HAPLogError(&logObject, "enable ao %d err", devID);
    //return NULL;
  }

  /* Step 3: enable AO channel. */
  int chnID = 0;
  ret = IMP_AO_EnableChn(devID, chnID);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio play enable channel failed");
    //return NULL;
  }

  /* Step 4: Set audio channel volume. */
  int chnVol = 80;
  ret = IMP_AO_SetVol(devID, chnID, chnVol);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio Play set volume failed");
    //return NULL;
  }

  ret = IMP_AO_GetVol(devID, chnID, &chnVol);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio Play get volume failed");
    //return NULL;
  }
  HAPLogDebug(&logObject, "Audio Out GetVol    vol:%d", chnVol);

  int aogain = 28;
  ret = IMP_AO_SetGain(devID, chnID, aogain);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio Record Set Gain failed");
    //return NULL;
  }

  ret = IMP_AO_GetGain(devID, chnID, &aogain);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio Record Get Gain failed");
    //return NULL;
  }
  HAPLogDebug(&logObject, "Audio Out GetGain    gain : %d", aogain);
  

  while (!myContext->session.audioFeedbackThread.threadStop)
  {
    //get data from the ring buffer...

    //Start by locking the queue mutex
    pthread_mutex_lock(&spkQueueMutex);

    //As long as the queue is empty,
    while(ptr_ring_buffer_is_empty(&ring_buffer)) {
        // - wait for the condition variable to be signalled
        //Note: This call unlocks the mutex when called and
        //relocks it before returning!
        pthread_cond_wait(&spkQueueCond, &spkQueueMutex);
    }

    //As we returned from the call, there must be new data in the queue - get it,

    uint8_t timeData[960];
    uint8_t * ptr_new_data;
    ptr_ring_buffer_dequeue(&ring_buffer, &ptr_new_data);
    memcpy(&timeData, ptr_new_data, 960); // this will be a pointer in ring_buffer_storage
    
    //Now unlock the mutex
    pthread_mutex_unlock(&spkQueueMutex);

    /* Step 5: send frame data. */
    IMPAudioFrame frm;
    frm.virAddr = (uint32_t *)timeData;
    frm.len = 480*2;  //bytes?
    ret = IMP_AO_SendFrame(devID, chnID, &frm, BLOCK);
    if (ret != 0)
    {
      HAPLogError(&logObject, "send Frame Data error");
      // return NULL;
    }

  }
  ret = IMP_AO_FlushChnBuf(devID, chnID);
  if (ret != 0)
  {
    HAPLogError(&logObject, "IMP_AO_FlushChnBuf error");
    //return NULL;
  }
  /* Step 6: disable the audio channel. */
  ret = IMP_AO_DisableChn(devID, chnID);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio channel disable error");
    //return NULL;
  }

  /* Step 7: disable the audio devices. */
  ret = IMP_AO_Disable(devID);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio device disable error");
    //return NULL;
  }

}


#define POS_AUDIO_OUTPUT_SAMPLE_RATE 16000
#define AAC_ENC_OUTPUT_MAX_SIZE 8192

// todo, move this function to a file dedicated to the audio stream
static void *get_srtp_audio_stream(void *context)
{  
  //  HAPLogError(&logObject, "In audio capture thread.");
  int val, i, chnNum, ret, sock;

  AccessoryContext *myContext = context;

  sock = myContext->session.audioThread.socket;

  prctl(PR_SET_NAME, "pos_srtp_aud");

  // setup the ingenic audio stream;

  // setup the aac codec
  HAPAssert(myContext->session.audioParameters.codecConfig.audioCodecType == 2);

  // not using comfort noise
  // HAPAssert( myContext->session.audioParameters.comfortNoise == 0 );
  // myContext->session.audioParameters.rtpParameters.comfortNoisePayload;

  HANDLE_AACENCODER aacEncHandle = NULL;
  AACENC_ERROR aacErr = AACENC_OK;
  aacErr = aacEncOpen(&aacEncHandle, 0, 1);
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncOpen err");
  }
  // Audio Object Type 39 = AAC-ELD
  aacErr = aacEncoder_SetParam(aacEncHandle, AACENC_AOT, AOT_ER_AAC_ELD);
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncoder_SetParam AACENC_AOT err");
  }
  aacErr = aacEncoder_SetParam(aacEncHandle, AACENC_SAMPLERATE, POS_AUDIO_OUTPUT_SAMPLE_RATE);
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncoder_SetParam AACENC_SAMPLERATE err");
  }
  // only one center channel
  aacErr = aacEncoder_SetParam(aacEncHandle, AACENC_CHANNELMODE, MODE_1);
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncoder_SetParam AACENC_CHANNELMODE err");
  }
  // center channel is the first channel
  aacErr = aacEncoder_SetParam(aacEncHandle, AACENC_CHANNELORDER, 1);
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncoder_SetParam AACENC_CHANNELORDER err");
  }
  aacErr = aacEncoder_SetParam(aacEncHandle, AACENC_BITRATEMODE, 4); // variable high bit rate per RFC3640
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncoder_SetParam AACENC_BITRATEMODE err");
  }

  // pretty sure this is being ignored according to the aac docs because of variable bitrate above
  int audioEncoderBitrate = myContext->session.audioParameters.codecConfig.audioCodecParams.bitRate << 10;
  aacErr = aacEncoder_SetParam(aacEncHandle, AACENC_BITRATE, audioEncoderBitrate);
  //  aacErr = aacEncoder_SetParam(aacEncHandle, AACENC_BITRATE, 128000);
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncoder_SetParam AACENC_BITRATE err");
  }
  aacErr = aacEncoder_SetParam(aacEncHandle, AACENC_GRANULE_LENGTH, 480); 
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncoder_SetParam AACENC_GRANULE_LENGTH err");
  }
  aacErr = aacEncoder_SetParam(aacEncHandle, AACENC_TRANSMUX, TT_MP4_RAW); // TT_MP4_LOAS);//TT_MP4_LATM_MCP1);
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncoder_SetParam AACENC_TRANSMUX err");
  }
  // signaling mode, no idea.  guess.
  aacErr = aacEncoder_SetParam(aacEncHandle, AACENC_SIGNALING_MODE, 2); //Explicit hierarchical signaling
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncoder_SetParam AACENC_SIGNALING_MODE err");
  }
  // aafterburner mode.  doc says this takes more cpu and ram for better audio.
  aacErr = aacEncoder_SetParam(aacEncHandle, AACENC_AFTERBURNER, 0);
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncoder_SetParam AACENC_AFTERBURNER err");
  }
  // finalize settings

  aacErr = aacEncEncode(aacEncHandle, NULL, NULL, NULL, NULL);
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncEncode finalize settings err");
  }

//#define AI_BASIC_TEST_RECORD_FILE "ai_record.pcm"

  AACENC_InfoStruct encInfo;
  aacErr = aacEncInfo(aacEncHandle, &encInfo);
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncInfo err");
  }
  //hexDump("encInfo.confBuf", &encInfo.confBuf, 64, 16);

  int devID = 1;
  int chnID = 0;

	//FILE *record_file = fopen(AI_BASIC_TEST_RECORD_FILE, "wb");
	//if(record_file == NULL) {
	//	IMP_LOG_ERR("ASFDASDF", "fopen %s failed\n", AI_BASIC_TEST_RECORD_FILE);
	//	return NULL;
	//}

  /* Step 1: set public attribute of AI device. */

  IMPAudioIOAttr attr;

  switch (myContext->session.audioParameters.codecConfig.audioCodecParams.sampleRate)
  {
  case 0:
  {
    attr.samplerate = AUDIO_SAMPLE_RATE_8000;
  }
  break;
  case 1:
  {
    attr.samplerate = AUDIO_SAMPLE_RATE_16000;
  }
  break;
  case 2:
  {
    attr.samplerate = AUDIO_SAMPLE_RATE_24000;
  }
  break;
  default:
  {
    attr.samplerate = AUDIO_SAMPLE_RATE_16000;
  }
  break;
  }
  attr.bitwidth = AUDIO_BIT_WIDTH_16;

  HAPAssert(myContext->session.audioParameters.codecConfig.audioCodecParams.audioChannels == 1);
  attr.soundmode = AUDIO_SOUND_MODE_MONO;

  attr.frmNum = 40; /**<The number of buffered frames, value range: [2, MAX_AUDIO_FRAME_NUM] */

  // HomeKit Bug: When a phone gets a connection though an apple tv it's been requesting 60ms even though 
  // supported-audio-configuration only advertises 30 ms and the apple tv and the phone individually request 30ms

  // 30 ms
  if (myContext->session.audioParameters.codecConfig.audioCodecParams.rtpTime != 30)
  {
    HAPLogError(&logObject, "requested rtpTime: %d", myContext->session.audioParameters.codecConfig.audioCodecParams.rtpTime);
    // return NULL;
  }
  // What to do now?
  // Audio seems to work, so just ignore the error!
  // HAPAssert(myContext->session.audioParameters.codecConfig.audioCodecParams.rtpTime == 30);

  // 16000 samples/sec * 0.030 seconds = 480 samples per frame
  attr.numPerFrm = 480; /**<Number of sampling points per frame */

  attr.chnCnt = 1; /**< Number of supported channels */
  ret = IMP_AI_SetPubAttr(devID, &attr);
  if (ret != 0)
  {
    HAPLogError(&logObject, "set ai %d attr err: %d", devID, ret);
    // return NULL;
  }

  memset(&attr, 0x0, sizeof(attr));
  ret = IMP_AI_GetPubAttr(devID, &attr);
  if (ret != 0)
  {
    HAPLogError(&logObject, "get ai %d attr err: %d", devID, ret);
    // return NULL;
  }

  /* Step 2: enable AI device. */
  ret = IMP_AI_Enable(devID);
  if (ret != 0)
  {
    HAPLogError(&logObject, "enable ai %d err", devID);
    return NULL;
  }

  /* Step 3: set audio channel attribute of AI device. */
  IMPAudioIChnParam chnParam;
  chnParam.usrFrmDepth = 40; // not sure
  ret = IMP_AI_SetChnParam(devID, chnID, &chnParam);
  if (ret != 0)
  {
    HAPLogError(&logObject, "set ai %d channel %d attr err: %d", devID, chnID, ret);
    return NULL;
  }

  memset(&chnParam, 0x0, sizeof(chnParam));
  ret = IMP_AI_GetChnParam(devID, chnID, &chnParam);
  if (ret != 0)
  {
    HAPLogError(&logObject, "get ai %d channel %d attr err: %d", devID, chnID, ret);
    return NULL;
  }

  HAPLogDebug(&logObject, "Audio In GetChnParam usrFrmDepth : %d", chnParam.usrFrmDepth);

  /* Step 4: enable AI channel. */
  ret = IMP_AI_EnableChn(devID, chnID);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio Record enable channel failed");
    return NULL;
  }

  /* Step 5: Set audio channel volume. */
  int chnVol = 60;
  ret = IMP_AI_SetVol(devID, chnID, chnVol);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio Record set volume failed");
    return NULL;
  }

  ret = IMP_AI_GetVol(devID, chnID, &chnVol);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio Record get volume failed");
    return NULL;
  }
  HAPLogDebug(&logObject, "Audio In GetVol    vol : %d", chnVol);

  int aigain = 28;
  ret = IMP_AI_SetGain(devID, chnID, aigain);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio Record Set Gain failed");
    return NULL;
  }

  ret = IMP_AI_GetGain(devID, chnID, &aigain);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio Record Get Gain failed");
    return NULL;
  }
  HAPLogDebug(&logObject, "Audio In GetGain    gain : %d", aigain);

  HAPLogDebug(&logObject, "Audio In GetPubAttr samplerate : %d", attr.samplerate);
  HAPLogDebug(&logObject, "Audio In GetPubAttr   bitwidth : %d", attr.bitwidth);
  HAPLogDebug(&logObject, "Audio In GetPubAttr  soundmode : %d", attr.soundmode);
  HAPLogDebug(&logObject, "Audio In GetPubAttr     frmNum : %d", attr.frmNum);
  HAPLogDebug(&logObject, "Audio In GetPubAttr  numPerFrm : %d", attr.numPerFrm);
  HAPLogDebug(&logObject, "Audio In GetPubAttr     chnCnt : %d", attr.chnCnt);

  while (!myContext->session.audioThread.threadStop)
  {
    // HAPLogError(&logObject, "In capture audio loop.");

    ret = IMP_AI_PollingFrame(devID, chnID, 1000);
    if (ret != 0)
    {
      HAPLogError(&logObject, "Audio Polling Frame Data error");
    }
    IMPAudioFrame frm;

    ret = IMP_AI_GetFrame(devID, chnID, &frm, BLOCK);
    if (ret != 0)
    {
      HAPLogError(&logObject, "Audio Get Frame Data error");
      return NULL;
    }

    /* Step 7: Save the recording data to a file. */
		//fwrite(frm.virAddr, 1, frm.len, record_file);

    // printf("Audio Bitwidth: %d\n", frm.bitwidth);
    // printf("Audio Length: %d\n", frm.len);
    // printf("Audio Seq: %d\n", frm.seq);
    // printf("Audio soundmode: %d\n", frm.soundmode);
    // printf("Audio timeStamp: %lld\n", frm.timeStamp);
    // printf("Audio virAddr: %p\n", frm.virAddr);

    if (frm.len && !myContext->session.audioThread.threadPause)
    {
      int iidentify = IN_AUDIO_DATA;
      int oidentify = OUT_BITSTREAM_DATA;

      AACENC_BufDesc ibuf = {0};
      ibuf.numBufs = 1;
      ibuf.bufs = (void **)&frm.virAddr;
      ibuf.bufferIdentifiers = &iidentify;
      ibuf.bufSizes = frm.len;
      uint32_t ibufElSizes = 2; // 16bits
      ibuf.bufElSizes = &ibufElSizes;

      AACENC_InArgs iargs = {0};
      iargs.numInSamples = frm.len >> 1;            // 2 bytes per sample
      uint8_t aacData[AAC_ENC_OUTPUT_MAX_SIZE + 4]; // 4 for the au header
      uint8_t *pAacData = (&aacData[4]);

      AACENC_BufDesc obuf = {0};
      obuf.numBufs = 1;
      obuf.bufs = (void **)&pAacData;

      obuf.bufferIdentifiers = &oidentify;
      uint32_t outBufSize = AAC_ENC_OUTPUT_MAX_SIZE;
      obuf.bufSizes = &outBufSize;
      uint32_t obufElSizes = 1; // not sure about this
      obuf.bufElSizes = &obufElSizes;

      AACENC_OutArgs oargs = {0};
      // compress the frame
      aacErr = aacEncEncode(aacEncHandle, &ibuf, &obuf, &iargs, &oargs);
      if (aacErr != AACENC_OK)
      {
        HAPLogError(&logObject, "aacEncEncode err: %d", aacErr);
      }
      // printf("made it back from aacEncEncode!! Bytes out: %d, %d, %d, %d\n", oargs.numOutBytes, oargs.numInSamples, oargs.numAncBytes, oargs.bitResState);
      if (oargs.numOutBytes > 0)
      {
        // build the au header.  RFC3640 s3.2.1 and s3.3.6
        *(uint8_t *)(&aacData[0]) = 0;  // 16 bits au headers length
        *(uint8_t *)(&aacData[1]) = 16; // 16 bits au headers length
        *(uint16_t *)(&aacData[2]) = __bswap_16(oargs.numOutBytes << 3);

        //        hexDump("audio packet pushed",&aacData, oargs.numOutBytes+4, 16);

        size_t numPayloadBytes = 0;
        // TODO:  Should the sequence number be derived from the encoder instead of the ActualTime?
        POSRTPStreamPushPayload(
            &myContext->session.rtpAudioStream,
            (void *)(&aacData),
            oargs.numOutBytes + 4, // + 4 for header
            &numPayloadBytes,
            frm.timeStamp * 1000, // us to ns conversion (checked)
            ActualTime());

        if (numPayloadBytes > 0)
        {
          for (;;)
          {
            uint8_t packet_data[4096];
            size_t packet_len = 0;
            POSRTPStreamPollPacket(
                &myContext->session.rtpAudioStream,
                packet_data,
                sizeof(packet_data),
                &packet_len);
            if (packet_len == 0)
              break;
            // printf("sending %d audio bytes, index %d, fd %d\n", packet_len,*(uint16_t *)(&packet_data[2]), sock);
            //hexDump("send audio packet", packet_data, packet_len,16);
            ret = send(sock, packet_data, packet_len, 0);
            if (ret != packet_len)
            {
              HAPLogError(&logObject, "Tried to send %d audio bytes, but send only sent %d", packet_len, ret);
              // printf("send error (%d): %s\n", errno, strerror(errno));
            }
          }
        }
      }
    }

    /* Step 8: release the audio record frame. */
    ret = IMP_AI_ReleaseFrame(devID, chnID, &frm);
    if (ret != 0)
    {
      HAPLogError(&logObject, "Audio release frame data error");
      return NULL;
    }
  }

  aacErr = aacEncClose(&aacEncHandle);
  if (aacErr != AACENC_OK)
  {
    HAPLogError(&logObject, "aacEncClose err");
  }

  /* Step 9: disable the audio channel. */
  ret = IMP_AI_DisableChn(devID, chnID);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio channel disable error");
    // return NULL;
  }

  /* Step 10: disable the audio devices. */
  ret = IMP_AI_Disable(devID);
  if (ret != 0)
  {
    HAPLogError(&logObject, "Audio device disable error");
    // return NULL;
  }
}

void StreamContextInitialize(AccessoryContext *context)
{
  HAPLogDebug(&kHAPLog_Default, "Initializing streaming context.");
  AccessoryContext *myContext = context;
  myContext->session.videoThread.thread = NULL;
  myContext->session.videoFeedbackThread.thread = NULL;
  myContext->session.status = kHAPCharacteristicValue_StreamingStatus_Available;
  accessoryConfiguration.state.streaming = kHAPCharacteristicValue_StreamingStatus_Available;
}

void StreamContextDeintialize(AccessoryContext *context)
{
  HAPLogDebug(&kHAPLog_Default, "Deinitalizing streaming context.");
  // todo, stop the ingenic video pipeline
}

#define kHAPRTPProfileBaseline 0
#define kHAPRTPProfileMain 1
#define kHAPRTPProfileHigh 2

void *posStartStream(AccessoryContext *context HAP_UNUSED)
{
  int ret;
  HAPLogInfo(&logObject, "posStartStream");

  AccessoryContext *myContext = context;

  // what to do with sessionID?
  // nothing, it was alraedy checked.
  myContext->session.sessionId;

  myContext->session.status = kHAPCharacteristicValue_StreamingStatus_InUse;

  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.codecConfig.videoCodecParams.CVOEnabled: %d",
              myContext->session.videoParameters.codecConfig.videoCodecParams.CVOEnabled);
  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.codecConfig.videoCodecParams.packetizationMode: %d",
              myContext->session.videoParameters.codecConfig.videoCodecParams.packetizationMode);
  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.codecConfig.videoCodecParams.level: %d",
              myContext->session.videoParameters.codecConfig.videoCodecParams.level);
  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.codecConfig.videoCodecType: %d",
              myContext->session.videoParameters.codecConfig.videoCodecType);
  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.codecConfig.videoCodecParams.profileID: %d",
              myContext->session.videoParameters.codecConfig.videoCodecParams.profileID);
  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.codecConfig.videoAttributes.imageWidth: %d",
              myContext->session.videoParameters.codecConfig.videoAttributes.imageWidth);
  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.codecConfig.videoAttributes.imageHeight: %d",
              myContext->session.videoParameters.codecConfig.videoAttributes.imageHeight);
  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.codecConfig.videoAttributes.frameRate: %d",
              myContext->session.videoParameters.codecConfig.videoAttributes.frameRate);
  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.vRtpParameters.vRtpParameters.maximumBitrate: %d",
              myContext->session.videoParameters.vRtpParameters.vRtpParameters.maximumBitrate);

  // todo, move setting up the encoder to the video thread
  // setup encoder
  
  // not supporting cvo
  myContext->session.videoParameters.codecConfig.videoCodecParams.CVOEnabled;

  // 0 - non interleaved, not using
  myContext->session.videoParameters.codecConfig.videoCodecParams.packetizationMode;

  // 3.1, 3.2, 4 - not sure what to do with this... not passing to encoder
  myContext->session.videoParameters.codecConfig.videoCodecParams.level;

  // 0 - h264, the onlyone supported in the docs, TODO experiment with 1 which might be H265 HEVC
  myContext->session.videoParameters.codecConfig.videoCodecType;

  IMPEncoderChnAttr enc0_channel0_attr;
  // TODO: experiment with different encoder rc modes
  static const IMPEncoderRcMode S_RC_METHOD = IMP_ENC_RC_MODE_CBR; // IMP_ENC_RC_MODE_CAPPED_VBR; //IMP_ENC_RC_MODE_FIXQP; //IMP_ENC_RC_MODE_CAPPED_QUALITY;

  // avc baseline, main and high are all supported by t31 as well as hevc main (not used)
  IMPEncoderProfile encoderProfile = IMP_ENC_PROFILE_AVC_BASELINE;
  if (myContext->session.videoParameters.codecConfig.videoCodecParams.profileID == kHAPRTPProfileHigh)
  {
    encoderProfile = IMP_ENC_PROFILE_AVC_HIGH;
  }
  else
  {
    if (myContext->session.videoParameters.codecConfig.videoCodecParams.profileID == kHAPRTPProfileMain)
      encoderProfile = IMP_ENC_PROFILE_AVC_MAIN;
  }

  ret = IMP_Encoder_UnRegisterChn(0 /* encChn */);
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_Encoder_RegisterChn(%d, %d) error: %d", 0, 0, ret);
    return NULL;
  }

  ret = IMP_Encoder_DestroyChn(/*encChn*/ 0);
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_Encoder_CreateChn(%d) error !", 0);
    return NULL;
  }
  
  // vRtpParameters.maximumBitrate is in kbps, but imp wants bps
  ret = IMP_Encoder_SetDefaultParam(&enc0_channel0_attr,
                                    encoderProfile,
                                    S_RC_METHOD,
                                    myContext->session.videoParameters.codecConfig.videoAttributes.imageWidth,
                                    myContext->session.videoParameters.codecConfig.videoAttributes.imageHeight,
                                    myContext->session.videoParameters.codecConfig.videoAttributes.frameRate,
                                    1,
                                    myContext->session.videoParameters.codecConfig.videoAttributes.frameRate * 2 / 1, // GOP length (2 secc)
                                    2,                                                                                // uMaxSameSenceCnt ??
                                    (S_RC_METHOD == IMP_ENC_RC_MODE_FIXQP) ? 35 : -1,                                 // iInitialQP
                                    myContext->session.videoParameters.vRtpParameters.vRtpParameters.maximumBitrate); // uTargetBitRate (seems to be kbps)
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_Encoder_SetDefaultParam(%d) error !", 0);
    return NULL;
  }
  ret = IMP_Encoder_CreateChn(/*encChn*/ 0, &enc0_channel0_attr);
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_Encoder_CreateChn(%d) error !", 0);
    return NULL;
  }

  ret = IMP_Encoder_RegisterChn(0 /* encGroup */, 0 /* encChn */);
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_Encoder_RegisterChn(%d, %d) error: %d", 0, 0, ret);
    return NULL;
  }
  
  // setup rtpcontroller

  HAPLogDebug(&kHAPLog_Default, "myContext->session.ssrcVideo: %d",
              myContext->session.ssrcVideo);
  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.vRtpParameters.maxMTU: %d",
              myContext->session.videoParameters.vRtpParameters.maxMTU);
  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.vRtpParameters.vRtpParameters.maximumBitrate: %d",
              myContext->session.videoParameters.vRtpParameters.vRtpParameters.maximumBitrate);
  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.vRtpParameters.vRtpParameters.minRTCPinterval: %d",
              myContext->session.videoParameters.vRtpParameters.vRtpParameters.minRTCPinterval);
  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.vRtpParameters.vRtpParameters.payloadType: %d",
              myContext->session.videoParameters.vRtpParameters.vRtpParameters.payloadType);
  HAPLogDebug(&kHAPLog_Default, "myContext->session.videoParameters.vRtpParameters.vRtpParameters.ssrc: %d",
              myContext->session.videoParameters.vRtpParameters.vRtpParameters.ssrc);
  
  // Could almost just pass .vRtpParameters into StreamStart
  POSRTPParameters videoRtpParameters;
  videoRtpParameters.maxBitRate = myContext->session.videoParameters.vRtpParameters.vRtpParameters.maximumBitrate << 10;
  videoRtpParameters.maximumMTU = myContext->session.videoParameters.vRtpParameters.maxMTU;
  videoRtpParameters.RTCPInterval = myContext->session.videoParameters.vRtpParameters.vRtpParameters.minRTCPinterval;
  videoRtpParameters.ssrc = myContext->session.videoParameters.vRtpParameters.vRtpParameters.ssrc;
  videoRtpParameters.type = myContext->session.videoParameters.vRtpParameters.vRtpParameters.payloadType;

  POSSRTPParameters videoOutSrtpParameters;
  memset(&videoOutSrtpParameters, 0, sizeof(videoOutSrtpParameters));

  if (myContext->session.videoParams.srtpCryptoSuite == 0)
  {                                        // kHAP  AES_CM_128_HMAC_SHA1_80){
    videoOutSrtpParameters.cryptoType = 1; // CRYPTOTYPE_AES_CM_128_HMAC_SHA1_80;
    memcpy(&videoOutSrtpParameters.Key_Union.AES_CM_128_HMAC_SHA1_80.key, myContext->session.videoParams.srtpMasterKey, 16);
    memcpy(&videoOutSrtpParameters.Key_Union.AES_CM_128_HMAC_SHA1_80.salt, myContext->session.videoParams.srtpMasterSalt, 14);
  }
  else
  {
    videoOutSrtpParameters.cryptoType = CRYPTOTYPE_AES_256_CM_HMAC_SHA1_80;
    memcpy(&videoOutSrtpParameters.Key_Union.AES_256_CM_HMAC_SHA1_80.key, myContext->session.videoParams.srtpMasterKey, 32);
    memcpy(&videoOutSrtpParameters.Key_Union.AES_256_CM_HMAC_SHA1_80.salt, myContext->session.videoParams.srtpMasterSalt, 14);
  }

  POSSRTPParameters videoInSrtpParameters;
  memset(&videoInSrtpParameters, 0, sizeof(videoInSrtpParameters));

  if (myContext->session.videoParams.srtpCryptoSuite == 0)
  { // kHAP   _AES_CM_128_HMAC_SHA1_80){
    videoInSrtpParameters.cryptoType = 1;
    memcpy(&videoInSrtpParameters.Key_Union.AES_CM_128_HMAC_SHA1_80.key, myContext->session.videoParams.srtpMasterKey, 16);
    memcpy(&videoInSrtpParameters.Key_Union.AES_CM_128_HMAC_SHA1_80.salt, myContext->session.videoParams.srtpMasterSalt, 14);
  }
  else
  {
    videoInSrtpParameters.cryptoType = CRYPTOTYPE_AES_256_CM_HMAC_SHA1_80;
    memcpy(&videoInSrtpParameters.Key_Union.AES_256_CM_HMAC_SHA1_80.key, myContext->session.videoParams.srtpMasterKey, 32);
    memcpy(&videoInSrtpParameters.Key_Union.AES_256_CM_HMAC_SHA1_80.salt, myContext->session.videoParams.srtpMasterSalt, 14);
  }

  char cnameString[32];
  memset(cnameString, 0, sizeof(cnameString));
  int ipAddressLen = strnlen(myContext->session.accessoryAddress.ipAddress, INET6_ADDRSTRLEN);
  int addrStrTailStart = ipAddressLen - 16;
  if (addrStrTailStart < 0)
    addrStrTailStart = 0;
  strcpy(cnameString, "Wyrecam");
  strncpy(&(cnameString[7]), &(myContext->session.accessoryAddress.ipAddress[addrStrTailStart]), 16);
  HAPLogDebug(&kHAPLog_Default, "computed cnameString: %s", cnameString);

  POSRTPStreamStart(&myContext->session.rtpVideoStream,
                    &videoRtpParameters,
                    RTPType_H264,
                    90000, // rfc6184 90khz clock
                    myContext->session.ssrcVideo,
                    ActualTime(),
                    &cnameString,
                    &videoInSrtpParameters,
                    &videoOutSrtpParameters);

  IMP_Encoder_FlushStream(0);
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_Encoder_FlushStream(%d) error: %d", 0, ret);
    return NULL;
  }

  ret = IMP_FrameSource_EnableChn(1);
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_FrameSource_EnableChn(%d) error: %d", 1, ret);
    return NULL;
  }

  // pass to video thread
  myContext->session.videoThread.threadPause = 0;
  myContext->session.videoThread.threadStop = 0;
  myContext->session.videoThread.chn_num = 0;

  HAPLogInfo(&logObject, "Starting srtp video capture thread");
  //(((myContext->session.videoStream.socket) << 16) | (/*chn_num*/ 0)); // this is a little silly, should pass a stucture of some kind
  ret = pthread_create(&myContext->session.videoThread.thread, NULL, get_srtp_video_stream, (void *)context);
  if (ret < 0)
  {
    HAPLogError(&logObject, "Create ChnNum%d get_srtp_video_stream failed", (/*chn_num*/ 0));
  }

  myContext->session.videoFeedbackThread.threadPause = 0;
  myContext->session.videoFeedbackThread.threadStop = 0;


  //todo: move these pthread handles to global variables...
  HAPLogInfo(&logObject, "Starting srtp video feedback thread");
  ret = pthread_create(&myContext->session.videoFeedbackThread.thread, NULL, srtp_video_feedback, (void *)context);
  if (ret < 0)
  {
    HAPLogError(&logObject, "Create ChnNum%d get_srtp_video_stream failed", (/*chn_num*/ 0));
  }

  POSSRTPParameters audioOutSrtpParameters;
  memset(&audioOutSrtpParameters, 0, sizeof(audioOutSrtpParameters));

  if (myContext->session.audioParams.srtpCryptoSuite == 0)
  {                                        // kHAP  AES_CM_128_HMAC_SHA1_80){
    audioOutSrtpParameters.cryptoType = 1; // CRYPTOTYPE_AES_CM_128_HMAC_SHA1_80;
    memcpy(&audioOutSrtpParameters.Key_Union.AES_CM_128_HMAC_SHA1_80.key, myContext->session.audioParams.srtpMasterKey, 16);
    memcpy(&audioOutSrtpParameters.Key_Union.AES_CM_128_HMAC_SHA1_80.salt, myContext->session.audioParams.srtpMasterSalt, 14);
  }
  else
  {
    audioOutSrtpParameters.cryptoType = CRYPTOTYPE_AES_256_CM_HMAC_SHA1_80;
    memcpy(&audioOutSrtpParameters.Key_Union.AES_256_CM_HMAC_SHA1_80.key, myContext->session.audioParams.srtpMasterKey, 32);
    memcpy(&audioOutSrtpParameters.Key_Union.AES_256_CM_HMAC_SHA1_80.salt, myContext->session.audioParams.srtpMasterSalt, 14);
  }

  POSSRTPParameters audioInSrtpParameters;
  memset(&audioInSrtpParameters, 0, sizeof(audioInSrtpParameters));

  if (myContext->session.audioParams.srtpCryptoSuite == 0)
  { // kHAP   _AES_CM_128_HMAC_SHA1_80){
    audioInSrtpParameters.cryptoType = 1;
    memcpy(&audioInSrtpParameters.Key_Union.AES_CM_128_HMAC_SHA1_80.key, myContext->session.audioParams.srtpMasterKey, 16);
    memcpy(&audioInSrtpParameters.Key_Union.AES_CM_128_HMAC_SHA1_80.salt, myContext->session.audioParams.srtpMasterSalt, 14);
  }
  else
  {
    audioInSrtpParameters.cryptoType = CRYPTOTYPE_AES_256_CM_HMAC_SHA1_80;
    memcpy(&audioInSrtpParameters.Key_Union.AES_256_CM_HMAC_SHA1_80.key, myContext->session.audioParams.srtpMasterKey, 32);
    memcpy(&audioInSrtpParameters.Key_Union.AES_256_CM_HMAC_SHA1_80.salt, myContext->session.audioParams.srtpMasterSalt, 14);
  }

  POSRTPParameters audioRtpParameters;
  audioRtpParameters.maxBitRate = myContext->session.audioParameters.rtpParameters.rtpParameters.maximumBitrate << 10;
  audioRtpParameters.maximumMTU = 0; // myContext->session.videoParameters.vRtpParameters.maxMTU; // uhhh, where is the audio mtu supposed to come from?
  audioRtpParameters.RTCPInterval = *(float *)&(myContext->session.audioParameters.rtpParameters.rtpParameters.minRTCPinterval);
  audioRtpParameters.ssrc = myContext->session.audioParameters.rtpParameters.rtpParameters.ssrc;
  audioRtpParameters.type = myContext->session.audioParameters.rtpParameters.rtpParameters.payloadType;

  POSRTPStreamStart(&myContext->session.rtpAudioStream,
                    &audioRtpParameters,
                    RTPType_Simple,
                    POS_AUDIO_OUTPUT_SAMPLE_RATE,
                    myContext->session.ssrcAudio,
                    ActualTime(),
                    &cnameString,
                    &audioInSrtpParameters,
                    &audioOutSrtpParameters);

  // pass to audio thread
  myContext->session.audioThread.threadPause = 0;
  myContext->session.audioThread.threadStop = 0;
  myContext->session.audioThread.chn_num = 0;

#ifndef MUTE_ALL_SOUND
  HAPLogInfo(&logObject, "Starting srtp audio capture thread");
  //(((myContext->session.audioStream.socket) << 16) | (/*chn_num*/ 0)); // this is a little silly, should pass a stucture of some kind
  ret = pthread_create(&myContext->session.audioThread.thread, NULL, get_srtp_audio_stream, (void *)context);
  if (ret < 0)
  {
    HAPLogError(&logObject, "Create ChnNum%d get_srtp_audio_stream failed", (/*chn_num*/ 0));
  }

  myContext->session.audioFeedbackThread.threadPause = 0;
  myContext->session.audioFeedbackThread.threadStop = 0;

  //Init the ring buffer that the audio feedback thread and the speaker thread will use to communicate
  //Initialize the mutex and the condition variable
  pthread_mutex_init(&spkQueueMutex, NULL);
  pthread_cond_init(&spkQueueCond, NULL);

  //Initialize the speaker ring buffer
  ptr_ring_buffer_init(&ring_buffer,&ring_buffer_index_storage, 128);
  HAPLogInfo(&logObject, "Starting srtp audio feedback thread");
  ret = pthread_create(&myContext->session.audioFeedbackThread.thread, NULL, srtp_audio_feedback, (void *)context);
  if (ret < 0)
  {
    HAPLogError(&logObject, "Create ChnNum%d get_srtp_audio_stream failed", (/*chn_num*/ 0));
  }
  HAPLogInfo(&logObject, "Starting speaker thread");
  if (speaker_pthread == NULL){
    ret = pthread_create(&speaker_pthread, NULL, speaker_thread, (void *)context);
  } 
  if (ret < 0)
  {
    HAPLogError(&logObject, "Create ChnNum%d get_srtp_audio_stream failed", (/*chn_num*/ 0));
  }
#endif
}

void *posStopStream(AccessoryContext *context HAP_UNUSED)
{
  int ret;
  AccessoryContext *myContext = context;
  myContext->session.videoThread.threadStop = 1;
  myContext->session.audioThread.threadStop = 1;
  myContext->session.status = kHAPCharacteristicValue_StreamingStatus_Available;
  accessoryConfiguration.state.streaming = kHAPCharacteristicValue_StreamingStatus_Available;

  HAPLogInfo(&logObject, "Joining srtp capture thread ");

  if (myContext->session.videoThread.thread != NULL)
  {
    ret = pthread_join(myContext->session.videoThread.thread, NULL);
    if (ret < 0)
    {
      HAPLogError(&logObject, "Join ChnNum%d get_srtp_video_stream failed", (/*chn_num*/ 0));
    }
  }

  HAPLogInfo(&logObject, "Joining srtp audio capture thread ");

  if (myContext->session.audioThread.thread != NULL)
  {
    ret = pthread_join(myContext->session.audioThread.thread, NULL);
    if (ret < 0)
    {
      HAPLogError(&logObject, "Join ChnNum%d get_srtp_audio_stream failed", (/*chn_num*/ 0));
    }
  }

  ret = IMP_FrameSource_DisableChn(1);
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_FrameSource_DisableChn(%d) error: %d", 1, ret);
    return;
  }
  myContext->session.videoFeedbackThread.threadStop = 1;
  myContext->session.audioFeedbackThread.threadStop = 1;

  // WORKAROUND ( SELECT CAUSES RUNAWAY HAP )
  // HAPLogInfo(&logObject, "Joining srtp feedback thread ");
  // if ( myContext->session.videoFeedbackThread.thread != NULL){
  //  ret = pthread_join(myContext->session.videoFeedbackThread.thread, NULL);
  //  if (ret < 0) {
  //    HAPLogError(&logObject, "Join ChnNum%d srtp feedback thread failed", (/*chn_num*/ 0 ));
  //  }
  //}

  // work around since select isn't working in the feeback thread
  HAPLogInfo(&logObject, "Killing video srtp feedback thread ");
  if (myContext->session.videoFeedbackThread.thread != NULL)
  {
    ret = pthread_kill(myContext->session.videoFeedbackThread.thread, 0);
    if (ret < 0)
    {
      HAPLogError(&logObject, "Killing ChnNum%d srtp video feedback thread failed", (/*chn_num*/ 0));
    }
  }
#ifndef MUTE_ALL_SOUND
  // work around since select isn't working in the feeback thread
  HAPLogInfo(&logObject, "Killing audio srtp feedback thread ");
  if (myContext->session.audioFeedbackThread.thread != NULL)
  {
    ret = pthread_kill(myContext->session.audioFeedbackThread.thread, 0);
    if (ret < 0)
    {
      HAPLogError(&logObject, "Killing ChnNum%d srtp audio feedback thread failed", (/*chn_num*/ 0));
    }
  }

  HAPLogInfo(&logObject, "Killing speaker thread ");
  if (speaker_pthread != NULL)
  {
    ret = pthread_kill(speaker_pthread, 0);
    if (ret < 0)
    {
      HAPLogError(&logObject, "Killing speaker thread failed");
    }
  }

  // not closing the socket here so that a new stream start command can reuse the socket
  // write setup endpoint will close the socket

  // not ending the stream here so reconfigure can stop -> start
  // POSRTPStreamEnd(&myContext->session.rtpVideoStream);
  #endif
}

void *posReconfigureStream(AccessoryContext *context HAP_UNUSED)
{
  // WORKAROUND ( IGNORING RECONFIGURATION )
  //  	HAPLogInfo(&logObject, "posReconfigureStream");
  HAPLogError(&logObject, "Ignoring reconfigure command - Don't know how to parse partial TLVs yet.");
  // Example (base64): ARUCAQQBEHOaCxibNkonjz4Gk2lOW7YCGQMLAQKAAgICaAEDAR4ECgMChAAEBAAAAAA=
  // (hex): 01150201040110739a0b189b364a278f3e0693694e5bb60219030b010280020202680103011e040a03028400040400000000
  // Not doing this for now.  need to get partial TLV updates to work for this
  //    posStopStream(context);
  //    posStartStream(context);
}

void *posSuspendStream(AccessoryContext *context HAP_UNUSED)
{
  HAPLogInfo(&logObject, "posSuspendStream");
  AccessoryContext *myContext = context;
  myContext->session.videoThread.threadPause = 1;
  myContext->session.audioThread.threadPause = 1;
}

void *posResumeStream(AccessoryContext *context HAP_UNUSED)
{
  HAPLogInfo(&logObject, "posResumeStream");
  AccessoryContext *myContext = context;
  myContext->session.videoThread.threadPause = 0;
  myContext->session.audioThread.threadPause = 0;
}
