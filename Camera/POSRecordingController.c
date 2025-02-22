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

#include <signal.h> // pthread kill
#include <arpa/inet.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <bits/time.h>
#include <sys/prctl.h>

#include "HAP.h"
#include "HAPBase.h"
#include <HAP+Internal.h>

#include "util_base64.h"

#include "App_Camera.h"
#include "App.h"
#include "DB.h"

#include "POSRecordingController.h"
#include "POSDataStreamParser.h"
#include "POSMP4Muxer.h"
#include "POSRingBufferVideoIn.h"


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

#include "hexdump.h"

posRecordingBufferStruct posRecordingBuffer;

static const HAPLogObject logObject = {.subsystem = kHAP_LogSubsystem, .category = "POSRecordingController"};

static HAPTime ActualTime()
{
  struct timespec spec;
  clock_gettime(CLOCK_REALTIME, &spec);
  uint32_t sec = (uint32_t)spec.tv_sec;
  uint32_t ns = (uint32_t)spec.tv_nsec;
  return (HAPTime)sec * 1000000000 + ns;
}

static int frmrate_sp[3] = { 0 };
static int statime_sp[3] = { 0 };
static int bitrate_sp[3] = { 0 };

//temporary
//char * filename;
//int * fd;


#define RING_BUFFER_SIZE_VIDEO 256 //  can store indexes for 10.6 seconds at 24 fps
ring_buffer_vi_element_t vringstorage[RING_BUFFER_SIZE_VIDEO]; 
ring_buffer_vi_t vring;


void * rmem_buffer;
void * rmem_buffer_a_head;
void * rmem_buffer_v_head;

#define RMEM_BUFFER_SIZE 1024*1024*8

#define RMEM_BUFFER_V_SIZE 1024*1024*7 // at 2000kbps (hk max), this is 28 seconds, should be more than enough
#define RMEM_BUFFER_V_OFFS 0
#define RMEM_BUFFER_A_SIZE 1024*1024*1
#define RMEM_BUFFER_A_OFFS 1024*1024*7
int64_t media_init_us;

//extern void * video_vbm_malloc(int a, int b);
//extern int allocMem(void * rmem_buffer, uint32_t RMEM_BUFFER_SIZE, const char *name);

// NOTE:  The attempts to store the frame in the encoder (don't release) or allocate RMEM didn't work.  
// Store the memory here (on the stack) until the encoder can be rewritten (in the distant future)
uint8_t rmem_raw_buffer[RMEM_BUFFER_SIZE];

static void *get_hksv_video_record(void *context)
{
  //  HAPLogError(&logObject, "In recording thread.");
  int val, i, chnNum, ret, sock;
  AccessoryContext *myContext = context;

  prctl(PR_SET_NAME, "pos_hksv_vid");

  chnNum = myContext->recording.chn_num;

  //init the ring buffer
  ptr_ring_buffer_vi_init(&vring, (ring_buffer_vi_element_t *) &vringstorage, RING_BUFFER_SIZE_VIDEO);

  // malloc memory for the recording buffer
  // deep call into the IMP library to allocate memory in the reserved 64MB for the video codec.
  //if( rmem_buffer == NULL ) {
  //  allocMem(&rmem_buffer, RMEM_BUFFER_SIZE, "posRecord");
  //  rmem_buffer = video_vbm_malloc(RMEM_BUFFER_SIZE, 0);
  //  printf("rmem_buffer: %p\n", rmem_buffer);
  //}
  rmem_buffer = &rmem_raw_buffer;
  HAPAssert(rmem_buffer);
  rmem_buffer_v_head = rmem_buffer + RMEM_BUFFER_V_OFFS;
  rmem_buffer_a_head = rmem_buffer + RMEM_BUFFER_A_OFFS;
  // This is a bit hacky, but the (very long term) goal is to replace lib imp and have the codec write into a ring buffer in rmem.
  // Unfortunately, this means we're going to have 2 copies when streaming, one copy out of the codec to this buffer and
  // another copy from this buffer (along with encryption) to the network output buffer.  The (very long term) plan reduces this to 1 copy.  

  POSMp4VideoTrack vtrack;
  POSMp4AudioTrack atrack;

  vtrack.baseMediaDecodeTime = 0;
  vtrack.sequenceNumber = 1;
  vtrack.ring = &vring;
  vtrack.ring_trun_index = vtrack.ring->tail_index;
  vtrack.ring_mdat_index = vtrack.ring->tail_index;

  atrack.mute = 1; // todo: mute for now to get video working, add audio later
  atrack.baseMediaDecodeTime = 0;
  // atrack.ring = &aring;
  // atrack.ring_trun_index = 0;//atrack.ring->tail_index;
  // atrack.ring_mdat_index = 0;//atrack.ring->tail_index;

  size_t mdatLen = 0;

  ret = IMP_Encoder_StartRecvPic(chnNum);
  if (ret < 0) {
    HAPLogError(&logObject, "IMP_Encoder_StartRecvPic(%d) failed", chnNum);
    return ((void *)-1);
  }

  while (!myContext->recording.threadStop){
    ret = IMP_Encoder_PollingStream(chnNum, 1000);
    if (ret < 0) {
      HAPLogError(&logObject, "IMP_Encoder_PollingStream(%d) timeout", chnNum);
      continue;
    }

    IMPEncoderStream stream;
    /* Get H264 or H265 Stream */
    ret = IMP_Encoder_GetStream(chnNum, &stream, 1);
		if (ret < 0) {
      HAPLogError(&logObject, "IMP_Encoder_GetStream(%d) failed", chnNum);
      return ((void *)-1);
    }

    //calculate the len
		int i, len = 0;
    for (i = 0; i < stream.packCount; i++) {
      HAPAssert(stream.streamSize - stream.pack[i].offset >= stream.pack[i].length); // shouldn't happen
      //printf("pack.len: %d, pack.timestamp: %lld \n", stream.pack[i].length, stream.pack[i].timestamp );
      int NALType = *(uint8_t *)(stream.virAddr+stream.pack[i].offset +4) & 0x1f;
      int numBytes = stream.pack[i].length - 4;
      if (NALType == 7){
        /* Sequence parameter set */
        //printf("Got a SPS.\n");
        if (0x7f < numBytes) continue;
        HAPRawBufferCopyBytes(&vtrack.SPSNALU, (void *)(stream.virAddr+stream.pack[i].offset +4), numBytes);
        vtrack.SPSNALUNumBytes = numBytes;
        continue; // don't put this in the ring buffer or count it in len
      }
      if (NALType == 8){
        /* Picture parameter set */
        //printf("Got a PPS.\n");
        if (0x7f < numBytes) continue;
        HAPRawBufferCopyBytes(&vtrack.PPSNALU, (void *)(stream.virAddr+stream.pack[i].offset +4), numBytes);
        vtrack.PPSNALUNumBytes = numBytes;
        continue; // don't put this in the ring buffer or count it in len
      }

      len += stream.pack[i].length - 4;  //4 - remove the nal header bytes
    }

    //do some bitrate reporting
    bitrate_sp[chnNum] += len;
    frmrate_sp[chnNum]++;
    int64_t now = IMP_System_GetTimeStamp() / 1000;
    if(((int)(now - statime_sp[chnNum]) / 1000) >= 10){ // report bit rate every 10 seconds
      double fps = (double)frmrate_sp[chnNum] / ((double)(now - statime_sp[chnNum]) / 1000);
      double kbr = (double)bitrate_sp[chnNum] * 8 / (double)(now - statime_sp[chnNum]);
      HAPLogDebug(&logObject,"streamNum[%d]:FPS: %d,Bitrate: %d(kbps)",chnNum, (int)floor(fps),(int)floor(kbr) );
      frmrate_sp[chnNum] = 0;
      bitrate_sp[chnNum] = 0;
      statime_sp[chnNum] = now;
    }

    // wrap the buffer if needed
    if(rmem_buffer_v_head + len > rmem_buffer + RMEM_BUFFER_V_OFFS + RMEM_BUFFER_V_SIZE){ // would overflow
      rmem_buffer_v_head = rmem_buffer + RMEM_BUFFER_V_OFFS;
    }

    //printf("len: %d\n", len);
    int headm1 = (vring.head_index - 1) & vring.buffer_mask;

    // check that we don't over flow the RMEM buffer
    if (!ptr_ring_buffer_vi_is_empty(&vring)){
      if(vring.buffer[headm1].loc >= vring.buffer[vring.tail_index].loc)
        HAPAssert( (rmem_buffer_v_head >= vring.buffer[headm1].loc + vring.buffer[headm1].len) || 
          (rmem_buffer_v_head + len < vring.buffer[vring.tail_index].loc) );
      else
        HAPAssert( (rmem_buffer_v_head >= vring.buffer[headm1].loc + vring.buffer[headm1].len) && 
          (rmem_buffer_v_head + len < vring.buffer[vring.tail_index].loc) );
    }

    // build the element to insert in the ring buffer
    ring_buffer_vi_element_t newElement;
    newElement.loc = rmem_buffer_v_head;
    newElement.len = len;
    newElement.timestamp = stream.pack[0].timestamp;
        
    // copy the packets from the stream to RMEM
    for (i = 0; i < stream.packCount; i++) {
      int NALType = *(uint8_t *)(stream.virAddr+stream.pack[i].offset +4) & 0x1f;
      int numBytes = stream.pack[i].length - 4;
      if (NALType == 7){
        /* Sequence parameter set */
        continue; // don't put this in the ring buffer or count it in len
      }
      if (NALType == 8){
        /* Picture parameter set */
        continue; // don't put this in the ring buffer or count it in len
      }
      memcpy(rmem_buffer_v_head, (void *)(stream.virAddr+stream.pack[i].offset +4), stream.pack[i].length -4); // 4 removes the annex b nal start bytes
      rmem_buffer_v_head += stream.pack[i].length -4;
    }

    // release the driver encoder stream for the next frame
    IMP_Encoder_ReleaseStream(chnNum, &stream);

    // don't enqueue stream frames with only SPS and PPS nalus
    if( len == 0 ) continue;

    // dequeue the end element of the ring buffer
    if(ptr_ring_buffer_vi_is_full(&vring)){ 
      if(posRecordingBuffer.isDatastreamCtxValid && vring.tail_index == vtrack.ring_mdat_index){
        //we're about to free the element that the element being sent...
        HAPLogError(&logObject, "Recording ring buffer overrun.  Freeing data that is unsent.");
        posRecordingBuffer.isOverflowed = true;
      }
      
      ring_buffer_vi_element_t freeEle;
      ptr_ring_buffer_vi_dequeue(&vring, &freeEle);
      // no need to free any memory because the RMEM buffer is circuilar and will just be over written
      
    }

    // enqueue the new elementn in the ring buffer
    ptr_ring_buffer_vi_queue(&vring, &newElement);

    // now that we have this frame, update the duration of the last frame
    if(ptr_ring_buffer_vi_num_items((&vring)) >= 2){
      vring.buffer[((vring.head_index-1) & RING_BUFFER_MASK((&vring))) ].dur = 
        vring.buffer[((vring.head_index-1) & RING_BUFFER_MASK((&vring))) ].timestamp / 1000 - 
        vring.buffer[((vring.head_index-2) & RING_BUFFER_MASK((&vring))) ].timestamp / 1000;
        //printf("Updating duration: %d\n", vring.buffer[((vring.head_index-1) & RING_BUFFER_MASK((&vring))) ].dur);
    }


    // check to see if the ring buffer is empty
    if(!ptr_ring_buffer_vi_is_empty(&vring))
    // check to see if a data stream has connected to the recording buffer
    if(posRecordingBuffer.isDatastreamCtxValid == true)
    // todo: race condition between these two lines? what if the contex is invalidated here?
    // if so, try to get it's mutex lock
    if( pthread_mutex_trylock(&posRecordingBuffer.datastreamCtx->mutex ) == 0){
      //got the lock, else try the lock on the next frame
      posDataStreamStruct * datastream = posRecordingBuffer.datastreamCtx;

      uint32_t iframeCountPOS = 0 ;
      // count the fragments in the ring buffer
      for( size_t tempIndex = vring.tail_index; tempIndex != ((vring.head_index -1) & RING_BUFFER_MASK((&vring)));
          tempIndex = ((tempIndex + 1) & RING_BUFFER_MASK((&vring)))  ){

        //printf("counting I-frame:%d\n", ((*(uint8_t *)(vring.buffer[tempIndex].loc)) & 0x1f));

        if( ((*(uint8_t *)(vring.buffer[tempIndex].loc)) & 0x1f) == 5 && 
             (vring.buffer[tempIndex].timestamp > (vring.buffer[vring.head_index].timestamp - 6000000)) ){
          iframeCountPOS = iframeCountPOS + 1 ;
          //printf("tempIndex: %d\n", tempIndex);
          //hexDump("(*(uint8_t *)(vring.buffer[tempIndex].loc)", ((vring.buffer[tempIndex].loc)), 16 ,16);
        }
      }
      //printf("I-Frames in recording buffer: %d\n", iframeCountPOS);

      // special cases to avoid too many nested parens
      if( posRecordingBuffer.isLastDataChunk == true && 
          posRecordingBuffer.closeRequested && 
          posRecordingBuffer.newChunkNeeded == true){ 
        // the last chunk finished sending and was the last of a fragment (end of stream was sent) and a close was requested by the controller
        // wait for an ack to close the datastream
        posRecordingBuffer.isInitializationSent = false;
        start10sTimer(datastream);
        //printf("going to WAIT_FOR_ACK\n");
        posRecordingBuffer.datastreamCtx->datastreamState = WAIT_FOR_ACK;
        posRecordingBuffer.datastreamCtx->interests.hasSpaceAvailable = false;
        posRecordingBuffer.datastreamCtx->interests.hasBytesAvailable = true;
        UpdateInterests(posRecordingBuffer.datastreamCtx);
      } // go to releasing the mutex
      else if (posRecordingBuffer.isOverflowed){
        // todo: should send a close with a reason here
        // just stop sending for now
        HAPLogError(&logObject, "Recording ring buffer overrun. Not sending. Waiting for the controller to time out.");

      } // go to releasing the mutex
      else if (iframeCountPOS < 2){
        // not a whole fragment in the recodring buffer
        // release the mutex and wait for a new frame
      } // go to releasing the mutex
      else if (posRecordingBuffer.newChunkNeeded == true){
        HAPIPByteBufferClear(&(posRecordingBuffer.chunkBuffer));
        posRecordingBuffer.chunkBuffer.limit = posRecordingBuffer.chunkBuffer.position;
        char * dataTypeStr;
        HAPLogDebug(&logObject, "In the recording loop and data is needed");

        HAPIPByteBuffer * b;
        b = &posRecordingBuffer.chunkBuffer;
        size_t lenWritten = 0;

        uint8_t scratch_buff[4096];  //todo: is there a way to get rid of this?
        size_t mooSize = 0;  // size of the moov or moof box

        // build the scratch buffer if needed which will go after the datastream header, and before any mdat data
        if ( posRecordingBuffer.isInitializationSent == false ){ 
          HAPLogDebug(&logObject, "New connection");
          // new connection
          posRecordingBuffer.dataSequenceNumber = 1;
          posRecordingBuffer.dataChunkSequenceNumber = 0; // will be increamented to 1 below
          // send the mediainitializaation
          dataTypeStr = "mediaInitialization";
          posRecordingBuffer.isLastDataChunk = true;
          posRecordingBuffer.isInitializationSent = true;
          posRecordingBuffer.endOfStreamSent = false;
          
          // we're hardcoded at 4 sec I frame interval, and 4 sec preroll buffer so start from the end
          // find the oldest I frame < 6 seconds old in the ring buffer to try put the first motion in the second frame
          while ( (*(uint8_t *)(vring.buffer[vring.tail_index].loc) & 0x1f) != 5  || 
           (vring.buffer[vring.tail_index].timestamp < (vring.buffer[vring.head_index].timestamp - 6000000))){ //us
            // dequeue the tail
            ring_buffer_vi_element_t freeEle;
            //printf("dequeing: %d, tail: %d, head: %d\n", (*(uint8_t *)(vring.buffer[vring.tail_index].loc) & 0x1f), vring.tail_index, vring.head_index);
            ptr_ring_buffer_vi_dequeue(&vring, &freeEle);
          }
          //hexDump("(*(uint8_t *)(vring.buffer[vring.tail_index].loc)", ((vring.buffer[vring.tail_index].loc)), 16 ,16);
          HAPAssert((*(uint8_t *)(vring.buffer[vring.tail_index].loc) & 0x1f) == 5); 
          HAPAssert((vring.buffer[vring.tail_index].timestamp >= (vring.buffer[vring.head_index].timestamp - 6000000)));

          vtrack.ring_trun_index = vring.tail_index;
          vtrack.ring_mdat_index = vring.tail_index;

          // todo: add audio
          //atrack.ring_trun_index = aring.tail_index;
          //atrack.ring_mdat_index = aring.tail_index;

          // make the moov media initialization fragment in the scratch buffer
          mooSize = POSWriteMoov((uint8_t *)(&scratch_buff), sizeof(scratch_buff), &vtrack, &atrack);
          posRecordingBuffer.dataTotalSize = mooSize;
          posRecordingBuffer.sentDataSize = 0;

        } else {
          // not a new connection
          HAPLogDebug(&logObject, "Resume connection");
          if(posRecordingBuffer.isLastDataChunk == true) { 
            HAPLogDebug(&logObject, "New Fragment");
            // start a new fragment: last sent chunk was the last of a fragment
            // ready to send a new fragment
            // check to see if a close was requested

            // the tail should have been left on an I frame
            //printf("tail_index: %d\n", vring.tail_index);
            //hexDump("(*(uint8_t *)(vring.buffer[vring.tail_index].loc)", ((vring.buffer[vring.tail_index].loc)), 16 ,16);
            HAPAssert((*(uint8_t *)(vring.buffer[vring.tail_index].loc) & 0x1f) == 5); 

            // make a new fragment
            dataTypeStr = "mediaFragment";
            // can't send more than 262144 = 0x40000
            // if sending less than 0x40000, set isLastDataChunk to true
            posRecordingBuffer.dataSequenceNumber ++;
            posRecordingBuffer.dataChunkSequenceNumber = 0; // one indexed, but this will be incremented before the first send
            dataTypeStr = "mediaFragment";
            posRecordingBuffer.isLastDataChunk = false;

            // make the moof media fragment in the scratch buffer
            size_t totalsize;
            mooSize = POSWriteMoof((uint8_t *)(&scratch_buff), sizeof(scratch_buff), &vtrack, &atrack, &totalsize, &mdatLen);
            posRecordingBuffer.dataTotalSize = totalsize;
            posRecordingBuffer.sentDataSize = 0;
          }
        }
      
        // if a close was requested and this is the last chunk, set endOfStream
        if( posRecordingBuffer.closeRequested && 
          ( posRecordingBuffer.dataTotalSize - posRecordingBuffer.sentDataSize < DATASTREAM_MAX_CHUNK_SIZE) ){
          posRecordingBuffer.endOfStreamSent = true;
          HAPLogDebug(&logObject, "Setting endOfStream\n");

        }
        if(posRecordingBuffer.dataTotalSize - posRecordingBuffer.sentDataSize < DATASTREAM_MAX_CHUNK_SIZE) 
          posRecordingBuffer.isLastDataChunk = true;
          
        posRecordingBuffer.dataChunkSequenceNumber ++;

        HAPLogInfo(&logObject, "makeDataSendEventStart sequence - %llu, chunk - %llu \n", posRecordingBuffer.dataSequenceNumber, posRecordingBuffer.dataChunkSequenceNumber);
        // make the header with the stream parser
        makeDataSendEventStart(
          &b->data[b->position+4],
          b->capacity-4,
          &lenWritten,
          posRecordingBuffer.streamId,
          dataTypeStr,
          posRecordingBuffer.dataSequenceNumber,
          posRecordingBuffer.isLastDataChunk,
          posRecordingBuffer.dataChunkSequenceNumber,
          posRecordingBuffer.dataTotalSize,
          posRecordingBuffer.endOfStreamSent);
        b->limit = 4+lenWritten;
        //HAPLogInfo(&logObject, "done makeDataSendEventStart: %d\n", b->limit);

        //record the pointer location for the datasend data tag location and reserve space
        uint8_t * dataSendDataTag_ptr = &b->data[b->limit];
        HAPAssert(b->capacity > b->limit + 5);
        b->limit += 5;

        int dataSendDataBegin = b->limit;

        // copy in the scratch buffer
        if (mooSize > 0){
          //printf("moo, mooSize: %d\n", mooSize);
          HAPAssert(b->capacity > b->limit + mooSize);
          memcpy(&b->data[b->limit], scratch_buff, mooSize);
          b->limit += mooSize;
          posRecordingBuffer.sentDataSize += mooSize;
        }
        if(posRecordingBuffer.dataSequenceNumber != 1){
          // (continue to) make the mdat box in the ip buffer, checking for chunk max size of 0x40000

          HAPAssert ( b-> capacity - b->limit > DATASTREAM_MAX_CHUNK_SIZE );

          bool mdatDone = false;
          int mdatChunkBytes;
          mdatChunkBytes = POSWriteMdat( (char *) &b->data[b->limit], 
                        DATASTREAM_MAX_CHUNK_SIZE - mooSize, //b->capacity-b->limit, 
                        &vtrack, &atrack, 
                        posRecordingBuffer.dataTotalSize, 
                        mdatLen, &mdatDone);
          //printf("mdatChunkBytes: %d\n", mdatChunkBytes);
          HAPAssert ( mdatChunkBytes < DATASTREAM_MAX_CHUNK_SIZE );
          HAPAssert ( mdatChunkBytes + b->limit < b -> capacity );

          posRecordingBuffer.sentDataSize += mdatChunkBytes;
          //printf("%d == (%llu == %llu)\n", mdatDone, posRecordingBuffer.dataTotalSize, posRecordingBuffer.sentDataSize);
          HAPAssert ( mdatDone == (posRecordingBuffer.dataTotalSize == posRecordingBuffer.sentDataSize) );

          if(mdatDone){
            //printf("mdatDone!\n");
            do{
              // dequeue the tail
              ring_buffer_vi_element_t freeEle;
              //printf("mdatDone dequeing: %d\n", (*(uint8_t *)(vring.buffer[vring.tail_index].loc) & 0x1f));
              ptr_ring_buffer_vi_dequeue(&vring, &freeEle);
            }while ( (*(uint8_t *)(vring.buffer[vring.tail_index].loc) & 0x1f) != 5 );
          }

          b->limit += mdatChunkBytes;
        }

        uint32_t dataFieldLength = (b->limit - dataSendDataBegin);
        // write the datasend data tag and size to the buffer always 32 bit
        if((b->limit - dataSendDataBegin) < 0x10000){
          HAPLogError(&logObject, "Writing the DataStream data chunk length (%d) as a LENGTH32LE number although it would fit in a smaller type", dataFieldLength);
          HAPLogError(&logObject, "Open a bug report if this causes failures");
        }
        *dataSendDataTag_ptr = DATA_LENGTH32LE;
        dataSendDataTag_ptr ++;
        HAPWriteLittleInt32(dataSendDataTag_ptr, dataFieldLength);
        //printf("done writeDataLength: %d\n", b->limit - dataSendDataBegin);


        // record the payload length
        b->data[b->position] = 0x01; // datastream type
        b->data[b->position+1] = (uint8_t)(((b->limit-4) >> 16) & 0xff); // len MSB
        b->data[b->position+2] = (uint8_t)(((b->limit-4) >> 8) & 0xff); // len
        b->data[b->position+3] = (uint8_t)(((b->limit-4)) & 0xff); // len LSB
        
/*        
        filename = "/tmp/recording.mp4";
        //printf("entering fopen (%s)\n", filename);
        // open the file
        fd = fopen(filename,"ab");
        //printf("done fopen\n");

        // copy the file data to the end of the buffer
        int fr = fwrite(&(b->data[dataSendDataBegin]), 1, b->limit - dataSendDataBegin, fd);
                //assume no errors while dugging....
        fclose(fd);
        printf("done fwrite: %d, %d\n", b->limit, fr);
      
        char filename2[128];
        sprintf(&filename2, "/tmp/chunk/chunk%llu_%llu.ds",posRecordingBuffer.dataSequenceNumber, posRecordingBuffer.dataChunkSequenceNumber);
        //printf("entering fopen (%s)\n", filename);
        // open the file
        fd = fopen(filename2,"wb");
        //printf("done fopen\n");

        // copy the file data to the end of the buffer
        fr = fwrite(&(b->data[b->position]), 1, b->limit - b-> position, fd);
                //assume no errors while dugging....
        fclose(fd);
        printf("done fwrite: %d, %d\n", b->limit, fr);

        sprintf(&filename2, "/tmp/chunk/chunk%llu_%llu.mp4",posRecordingBuffer.dataSequenceNumber, posRecordingBuffer.dataChunkSequenceNumber);
        //printf("entering fopen (%s)\n", filename);
        // open the file
        fd = fopen(filename2,"wb");
        //printf("done fopen\n");

        // copy the file data to the end of the buffer
        fr = fwrite(&(b->data[dataSendDataBegin]), 1, b->limit - dataSendDataBegin, fd);
                //assume no errors while dugging....
        fclose(fd);
        printf("done fwrite: %d, %d\n", b->limit, fr);

        sprintf(&filename2, "/tmp/fragment/chunk%llu.mp4",posRecordingBuffer.dataSequenceNumber);
        //printf("entering fopen (%s)\n", filename);
        // open the file
        fd = fopen(filename2,"ab");
        //printf("done fopen\n");

        // copy the file data to the end of the buffer
        fr = fwrite(&(b->data[dataSendDataBegin]), 1, b->limit - dataSendDataBegin, fd);
                //assume no errors while dugging....
        fclose(fd);
        printf("done fwrite: %d, %d\n", b->limit, fr);

*/

        //HAPLogBufferDebug(&kHAPLog_Default, &b->data[b->position], b->limit - b->position, "Chunk Buffer");
        //hexDump("Chunk Buffer", b->data, 256, 16);

        HAPAssert(b->limit <= b->capacity-16); // check room for the tag

        uint64_t nonce_buffer;

        HAPLogDebug(&logObject, "Tx Nonce: %lld", datastream->tx_nonce);

        HAPWriteLittleUInt64(&nonce_buffer, datastream->tx_nonce);
        datastream->tx_nonce ++;

        // encrypt the plainText
        HAP_chacha20_poly1305_encrypt_aad(
                &b->data[b->limit], // tag location
                &b->data[4], // encryptedBytes
                &b->data[4], // plaintextBytes
                b->limit-4, // numPlaintextBytes
                b->data, //aadBytes
                4, // numAADBytes
                (uint8_t *)&nonce_buffer,
                sizeof(nonce_buffer),
                datastream->AccessoryToControllerKey);
        b->limit += 16; //tag size

        HAPAssert(b->data);
        HAPAssert(b->position <= b->limit);
        HAPAssert(b->limit <= b->capacity);

        //hexDump("Enc Chunk Buffer", b->data, 256, 16);

        // update interests to tell the hap thread to send
        datastream -> datastreamState = SEND_CHUNK;
        datastream -> interests.hasSpaceAvailable = true;
        UpdateInterests(datastream);

        posRecordingBuffer.newChunkNeeded = false;
      }
      pthread_mutex_unlock(&posRecordingBuffer.datastreamCtx->mutex );
    }
    else 
    {
      HAPLogInfo(&logObject, "Didn't get the recording buffer lock");
    }
  }

  ret = IMP_Encoder_StopRecvPic(chnNum);
  if (ret < 0) {
    HAPLogError(&logObject, "IMP_Encoder_StopRecvPic(%d) failed", chnNum);
    return ((void *)-1);
  }

  HAPLogError(&logObject, "Exiting capture thread.");
  return ((void *)0);
}


extern setupDataStreamTransportReadStruct setupDataStreamTransportRead;

void RecordingContextInitialize(AccessoryContext *context)
{
  HAPLogDebug(&kHAPLog_Default, "Initializing recording context.");

  AccessoryContext *myContext = context;
  HAPPlatformRandomNumberFill(&setupDataStreamTransportRead.accessorySalt,32);

  
  myContext->recording.thread = (pthread_t) NULL;
  posRecordingBuffer.chunkBuffer.data = (uint8_t *)&(posRecordingBuffer.chunkBufferData);
  posRecordingBuffer.chunkBuffer.capacity = kHAPDataStream_ChunkBufferSize; 
  HAPIPByteBufferClear(&(posRecordingBuffer.chunkBuffer));
  posRecordingBuffer.chunkBuffer.limit = posRecordingBuffer.chunkBuffer.position;
  posRecordingBuffer.datastreamCtx = (posDataStreamStruct *) NULL;
  posRecordingBuffer.isDatastreamCtxValid = false;
  posRecordingBuffer.newChunkNeeded = false;
  posRecordingBuffer.isInitializationSent = false;

  // pass to video thread
  myContext->recording.threadPause = 0;
  myContext->recording.threadStop = 0;
  myContext->recording.chn_num = 1;

	HAPLogInfo(&logObject, "Starting hksv capture thread ");
  int ret;
	ret = pthread_create(&(myContext->recording.thread), NULL, get_hksv_video_record, (void *)context );
	if (ret < 0) {
		HAPLogError(&logObject, "Create ChnNum get_hksv_video_record failed");
	}

}

void RecordingContextDeintialize(AccessoryContext *context)
{
  HAPLogDebug(&kHAPLog_Default, "Deinitalizing recording context.");
  // todo, stop the ingenic video pipeline
}

void posStartRecord(AccessoryContext *context)
{
  int ret;
  AccessoryContext *myContext = context;
  
  // the recording thread is initalized in ingenicVideoPipeline and is always running
  HAPLogInfo(&logObject, "posStartRecord");
  

}

void posStopRecord(AccessoryContext *context HAP_UNUSED)
{
  AccessoryContext *myContext = context;
  // there is no way to stop the recording thread.  The recording thread video stream is used for snapshots, motion, day/night, etc

}

void posReconfigureRecord(AccessoryContext *context HAP_UNUSED)
{
  int ret;
  AccessoryContext *myContext = context;

  HAPLogInfo(&logObject, "posReconfigureRecord");
  
  // not supporting reconfigure yet

// Rewrite this code to read the the selected recording configuration and write the correct video encode pipeline elements
/*

  ret = IMP_FrameSource_DisableChn(1);
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_FrameSource_DisableChn(%d) error: %d", 1, ret);
    return;
  }

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

  IMPEncoderChnAttr enc0_channel0_attr;

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
                                    myContext->session.videoParameters.vRtpParameters.maximumBitrate); // uTargetBitRate (seems to be kbps)
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_Encoder_SetDefaultParam(%d) error !", 0);
    return NULL;
  }

  IMP_Encoder_FlushStream(0);
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_Encoder_FlushStream(%d) error: %d", 0, ret);
    return NULL;
  }

  ret = IMP_Encoder_RequestIDR(myContext->session.videoThread.chn_num);
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_Encoder_RequestIDR(%d) failed", myContext->session.videoThread.chn_num);
  }

  ret = IMP_FrameSource_EnableChn(1);
  if (ret < 0)
  {
    HAPLogError(&logObject, "IMP_FrameSource_EnableChn(%d) error: %d", 1, ret);
    return NULL;
  }

*/

}
