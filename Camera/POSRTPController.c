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


// I went on a months long quest, fought with a dragon and brought back this treasure.

#include "POSRTPController.h"
#include <string.h>
#include "HAP.h"
#include <HAP+Internal.h>
#include <stdio.h>

static const HAPLogObject logObject = { .subsystem = kHAP_LogSubsystem, .category = "POSRTPController" };

uint64_t Upper64ofMul64(uint64_t a, uint64_t b)

{
  uint64_t c;
  uint64_t d;

  c = (b >> 0x20) * (a & 0xffffffff);
  d = (a >> 0x20) * (b & 0xffffffff);
  return (c >> 0x20) + (d >> 0x20) + (a >> 0x20) * (b >> 0x20) +
         ((d & 0xffffffff) +
              ((a & 0xffffffff) * (b & 0xffffffff) >> 0x20) + (c & 0xffffffff) >>
          0x20);
}

NTPEpochTime HAPTimeToNTPTime(HAPTime hapTime)
{

  // b = 2208988800000000000 is the number of ns between the NTPEpoch (1/1/1900)
  // and the Unix/Hap epoch 1/1/1970

  // HAP LSB is 1 ns
  // NTP LSB is 1/2^32 of a second

  // ntpTime = ( (hapTime + b) * m ) /2^64;

  uint64_t temp;

  temp = hapTime + 2208988800000000000;
  temp = Upper64ofMul64(temp, 5441186219426131130);
  return temp + (hapTime + 2208988800000000000) * 4;
};

HAPError POSRTPStreamEnd(POSRTPStreamRef *stream){
  memset(stream, 0, sizeof(POSRTPStreamRef));
    return kHAPError_None;
}

HAPError POSRTPStreamStart(POSRTPStreamRef *stream, POSRTPParameters *rtpParameters,
                           RTPType encodeType, uint32_t clockFrequency, uint32_t localSSRC,
                           HAPTime startTime, char *cnameString,
                           POSSRTPParameters *srtpInParameters,
                           POSSRTPParameters *srtpOutParameters){

  uint32_t tagSize;
  uint8_t *key;
  uint32_t keySize;
  uint8_t *salt;

  HAPRawBufferZero(stream, sizeof(POSRTPStreamRef));
  HAPPlatformRandomNumberFill(&stream->outTimeStampBase, 4);
  HAPPlatformRandomNumberFill(&stream->randomOutputSequenceNrBase, 2);

  stream->lastSentKeyFrame = startTime;
  stream->streamStartTime = startTime;
  stream->lastRTCPReportHAPTime = startTime;
  stream->lastRecvNTPtime = HAPTimeToNTPTime(startTime) >> 32;
  stream->rtcpInterval = rtpParameters->RTCPInterval * 1000;
  
  stream->encodeType = encodeType;
  stream->streamType = rtpParameters->type;
  stream->clockFreq = clockFrequency;
  uint64_t temp = Upper64ofMul64((uint64_t)clockFrequency << 32, 5441186219426131130);
  stream->nsToTimestampConvLSW = (uint32_t)(temp);
  stream->nsToTimestampConvMSW = clockFrequency * 4 + (int)(temp >> 32);
  stream->outstreamSSRC = localSSRC;
  stream->instreamSSRC = rtpParameters->ssrc;
  stream->bitRate = rtpParameters->maxBitRate * 1000;
  stream->requestNewBitrateAtCheck = rtpParameters->maxBitRate * 1000;
  uint8_t idx = 0;
  for (; idx < 32 && cnameString[idx] != 0; idx++)
    stream->localCNAME[idx] = cnameString[idx];
  stream->localCNAME[idx] = cnameString[idx];
  stream->localCNAME[idx] = 0;
  stream->localCNAMElength = idx;
  stream->lastRecFIRframeRequestSequenceNumber = 0xffffffff;
  stream->lastRecTSTRCmdWord = 0xffffffff;
  stream->lastReceivedTMMBRPayload = 0xffffffff;
  stream->lastRecTSTRSeqNr = 0xffffffff;

  if (srtpInParameters->cryptoType == CRYPTOTYPE_AES_CM_128_HMAC_SHA1_80)
  {
    key = (uint8_t *)&(srtpInParameters->Key_Union.AES_CM_128_HMAC_SHA1_80.key[0]);
    tagSize = 10;
    salt = (uint8_t *)&(srtpInParameters->Key_Union.AES_CM_128_HMAC_SHA1_80.salt[0]);
    keySize = 16;
  }
  else
  {
    if (srtpInParameters->cryptoType == CRYPTOTYPE_AES_256_CM_HMAC_SHA1_80)
    {
      key = (uint8_t *)&(srtpInParameters->Key_Union.AES_256_CM_HMAC_SHA1_80.key[0]);
      tagSize = 10;
      salt = (uint8_t *)&(srtpInParameters->Key_Union.AES_256_CM_HMAC_SHA1_80.salt[0]);
      keySize = 32;
    }
    else
    {
      // refusing
      HAPLogError(&logObject,"POSRTPStreamStart with no input crypto!");
      HAPFatalError();
      tagSize = 0;
      keySize = 0;
      salt = 0;
      key = 0;
    }
  }
  srtp_setupContext(&stream->context_input_srtp, &stream->context_input_rtcp, key, keySize,
                    salt, tagSize, rtpParameters->ssrc);

  if (srtpOutParameters->cryptoType == CRYPTOTYPE_AES_CM_128_HMAC_SHA1_80)
  {
    key = (uint8_t *)&(srtpOutParameters->Key_Union.AES_CM_128_HMAC_SHA1_80.key[0]);
    tagSize = 10;
    salt = (uint8_t *)&(srtpOutParameters->Key_Union.AES_CM_128_HMAC_SHA1_80.salt[0]);
    keySize = 16;
  }
  else
  {
    if (srtpOutParameters->cryptoType == CRYPTOTYPE_AES_256_CM_HMAC_SHA1_80)
    {
      key = &(srtpOutParameters->Key_Union.AES_256_CM_HMAC_SHA1_80.key[0]);
      tagSize = 10;
      salt = &(srtpOutParameters->Key_Union.AES_256_CM_HMAC_SHA1_80.salt[0]);
      keySize = 32;
    }
    else
    {
      // refusing
      HAPLogError(&logObject,"POSRTPStreamStart with no output crypto!");
      HAPFatalError();
      tagSize = 0;
      keySize = 0;
      salt = 0;
      key = 0;
    }
  }

  srtp_setupContext(&stream->context_output_srtp, &stream->context_output_rtcp, key,
                    keySize, salt, tagSize, localSSRC);

  if (rtpParameters->maximumMTU == 0)
    stream->maximumMTU = 0x10000;
  else
    stream->maximumMTU = rtpParameters->maximumMTU;

  stream->outStreamHeaderPlusTagSize = (stream->context_output_srtp).tag_size + 0xc;
  return kHAPError_None;
}

void POSRTPStreamPushPayload(POSRTPStreamRef *stream, void *bytes, size_t numBytes, size_t *numPayloadBytes,
                             HAPTimeNS sampleTime, HAPTime actualTime)

{

  uint8_t NALType;
  uint32_t temp;

  if (stream == 0)
    return;
  if (bytes == 0)
    return;
  if (numPayloadBytes == 0)
    return;
  if (numBytes == 0)
    return;

  *numPayloadBytes = numBytes;
  stream->payloadBytes = bytes;
  stream->numPayloadBytes = numBytes;
  temp = Upper64ofMul64((uint64_t)sampleTime, (uint64_t)stream->nsToTimestampConvLSW + ((uint64_t)stream->nsToTimestampConvMSW << 32));
  stream->timeStampToSend = stream->outTimeStampBase + temp;
  stream->nextPayloadStart = 0;
  if (stream->encodeType != RTPType_H264)
    return;
  NALType = *(uint8_t *)bytes & 0x1f;
  *numPayloadBytes = numBytes;

  if (NALType == 5)
  {
    /* Coded slice of an IDR picture */
    stream->KeyFrameRequestedBitOne = stream->encodeType;
    stream->lastSentKeyFrame = actualTime;
    stream->KeyFrameRequestedBitOne = 0;
    if (stream->SPSNALUNumBytes == 0)
    {
      // wait for sequence parameter set
      return;
    }
    if (stream->PPSNALUNumBytes == 0x0)
    {
      // wait for picture parameter set
      return;
    }
    // trigger a new SPS and PPS send
    stream->nextPayloadStart = 0xffffffff;
    return;
  }
  if (NALType == 7)
  {
    /* Sequence parameter set */
    if (0x7f < numBytes)
    {
      return;
    }
    HAPRawBufferCopyBytes(stream->SPSNALU, bytes, numBytes);
    stream->SPSNALUNumBytes = numBytes;
  }
  else
  {
    if (NALType != 8)
    {
      return;
    }
    if (0x7f < numBytes)
    {
      return;
    }
    HAPRawBufferCopyBytes(stream->PPSNALU, bytes, numBytes);
    stream->PPSNALUNumBytes = numBytes;
  }
  stream->payloadBytes = (void *)0x0;

  *numPayloadBytes = 0;
  return;
}

void POSRTPStreamPollPacket(POSRTPStreamRef *stream, void *bytes, size_t maxBytes, size_t *numPacketBytes)

{
  uint16_t FUHeader;
  uint32_t maxBytesLessAddlHeadersTag;
  uint32_t rtpPacketSize;
  uint8_t NALType;
  uint8_t NALNRI;
  uint8_t *PPSNALStart;
  uint8_t *payloadBytes;
  uint32_t maxBytesLessHeaderTag;
  uint8_t cvoID;
  bool MarkerBit;
  bool TestBit;

  if (stream == 0)
    return;
  if (bytes == 0)
    return;
  if (numPacketBytes == 0)
    return;

  payloadBytes = stream->payloadBytes;
  if (payloadBytes == (uint8_t *)0x0)
  {
    *numPacketBytes = 0;
    return;
  }
  if (stream->maximumMTU <= maxBytes)
  {
    maxBytes = stream->maximumMTU;
  }
  if (stream->encodeType == RTPType_H264)
  {

    rtpPacketSize = stream->numPayloadBytes;
    NALType = (uint8_t)*payloadBytes & 0x1f;
    NALNRI = *payloadBytes & 0x60;
    maxBytesLessHeaderTag = maxBytes - stream->outStreamHeaderPlusTagSize;
    if (stream->nextPayloadStart == 0xffffffff)
    {

      // Send SPS and PPS (also triggered by an IDR)
      ((uint8_t *)bytes)[0xc] = (NALNRI) + 0x18;
      ((uint8_t *)bytes)[0xd] = (uint8_t)(stream->SPSNALUNumBytes >> 8);
      ((uint8_t *)bytes)[0xe] = (uint8_t)stream->SPSNALUNumBytes;
      HAPRawBufferCopyBytes((uint8_t *)bytes + 0xf, stream->SPSNALU, stream->SPSNALUNumBytes);
      PPSNALStart = (uint8_t *)bytes + stream->SPSNALUNumBytes;
      PPSNALStart[0xf] = (uint8_t)(stream->PPSNALUNumBytes >> 8);
      PPSNALStart[0x10] = (uint8_t)stream->PPSNALUNumBytes;
      HAPRawBufferCopyBytes(PPSNALStart + 0x11, stream->PPSNALU, stream->PPSNALUNumBytes);
      payloadBytes = (uint8_t *)((uint8_t *)bytes + 0xc);
      stream->nextPayloadStart = 0;
      MarkerBit = false;
      cvoID = 0;
      FUHeader = 0;
      rtpPacketSize = stream->SPSNALUNumBytes + stream->PPSNALUNumBytes + 5;
    }
    else
    {
      cvoID = stream->cvoID;
      maxBytesLessAddlHeadersTag = maxBytesLessHeaderTag;
      if (cvoID != 0)
      {
        if ((NALType == 5) || // 5 is an IDR, <7 are all picture types
            ((NALType < 7 && (stream->cvoInformation != stream->lastcvoInformationSent))))
        {
          maxBytesLessAddlHeadersTag = maxBytesLessHeaderTag - 8;
        }
        else
        {
          cvoID = 0;
          maxBytesLessAddlHeadersTag = maxBytesLessHeaderTag;
        }
      }
      // stream->numPayloadBytes is the number of bytes pushpayload put on the stream and is not updated as pollpacket sends packets
      if (maxBytesLessAddlHeadersTag - 2 < stream->numPayloadBytes)
      {
        // Fragementation Unit Packet
        // https://datatracker.ietf.org/doc/html/rfc6184#page-29
        // shrink the packet size by the amount sent in previous FU packets (if any)
        uint32_t payloadSizeLeftToSend = stream->numPayloadBytes - stream->nextPayloadStart;
        maxBytesLessAddlHeadersTag = maxBytesLessAddlHeadersTag - 2;
        FUHeader = ((NALNRI + 0x1c) * 0x100 | NALType);
        if (maxBytesLessAddlHeadersTag < payloadSizeLeftToSend)
        {
          // The remaining number of bytes won't fit in this packet
          if (stream->nextPayloadStart == 0)
          {
            // start of a fragmented series
            // consume this payload non-fu header to be replace with our fu header
            // HAPLogDebug(&logObject,("Start of a FU\n");
            stream->nextPayloadStart = 1;
            FUHeader = FUHeader | 0x80;
          }
          // start or middle of a fragment series
          MarkerBit = false; // sending a partial nal, not the end of the fu series
          // HAPLogDebug(&logObject,("Start/Middle of a FU\n");

          rtpPacketSize = maxBytesLessAddlHeadersTag;

          cvoID = 0;
          payloadBytes = payloadBytes + stream->nextPayloadStart;
          stream->nextPayloadStart = rtpPacketSize + stream->nextPayloadStart;
        }
        else
        {
          // end of a fragmented NALU
          // don't need to limit rtpPacketSize since all the bytes fit
          // HAPLogDebug(&logObject,("End of a FU\n");
          rtpPacketSize = payloadSizeLeftToSend;
          FUHeader = FUHeader | 0x40;
          stream->payloadBytes = 0;
          MarkerBit = NALType < 7;
          payloadBytes = payloadBytes + stream->nextPayloadStart;
          stream->nextPayloadStart = rtpPacketSize + stream->nextPayloadStart;
        }
      }
      else
      {
        // A NALU without FU
        MarkerBit = NALType < 7;
        stream->payloadBytes = 0;
        FUHeader = 0;
      }
    }
  }
  else
  { // Simple encoding
    if (stream->encodeType != RTPType_Simple)
      return;
    rtpPacketSize = stream->numPayloadBytes;
    stream->payloadBytes = 0;
    MarkerBit = false;
    cvoID = 0;
    FUHeader = 0;
  }

  POSMakeRTPPacket(stream, payloadBytes, rtpPacketSize, bytes, maxBytes, numPacketBytes, FUHeader,
                   cvoID, MarkerBit);
  return;
}

uint32_t POSMakeRTPPacket(POSRTPStreamRef *stream, uint8_t *payloadBytes, size_t numDataBytes, uint8_t *packetBytes,
                          size_t maxBytes, size_t *numPacketBytes, uint16_t FUHeader, uint8_t cvoID,
                          uint8_t MarkerBit)

{
  uint32_t tempBytesWritten;
  uint32_t packetStartOffset;
  uint8_t *packetStartByte;
  uint32_t numFUHeaderBytes;
  uint32_t index;

  if (numPacketBytes == 0)
    return (stream->totalOutPacketBytesWritten);
  *numPacketBytes = 0;
  if (stream == 0)
    return (stream->totalOutPacketBytesWritten);
  if (payloadBytes == 0)
    return (stream->totalOutPacketBytesWritten);
  if (packetBytes == 0)
    return (stream->totalOutPacketBytesWritten);
  if (22 >= maxBytes)
    return (stream->totalOutPacketBytesWritten);

  // RTP Payload Format for H.264 Video
  // https://www.ietf.org/rfc/rfc3984.txt

  index = stream->randomOutputSequenceNrBase + stream->outSequenceNr;
  // RFC 1889 ver 2
  // https://datatracker.ietf.org/doc/html/rfc1889
  ((uint8_t *)packetBytes)[0] = (uint8_t)0x80;
  ((uint8_t *)packetBytes)[1] = (uint8_t)stream->streamType + MarkerBit * -0x80;
  ((uint8_t *)packetBytes)[2] = (uint8_t)(index >> 8);
  ((uint8_t *)packetBytes)[3] = (uint8_t)index;
  ((uint8_t *)packetBytes)[4] = (uint8_t)(stream->timeStampToSend >> 24);
  ((uint8_t *)packetBytes)[5] = (uint8_t)(stream->timeStampToSend >> 16);
  ((uint8_t *)packetBytes)[6] = (uint8_t)(stream->timeStampToSend >> 8);
  ((uint8_t *)packetBytes)[7] = (uint8_t)stream->timeStampToSend;
  ((uint8_t *)packetBytes)[8] = (uint8_t)(stream->outstreamSSRC >> 24);
  ((uint8_t *)packetBytes)[9] = (uint8_t)(stream->outstreamSSRC >> 16);
  ((uint8_t *)packetBytes)[10] = (uint8_t)(stream->outstreamSSRC >> 8);
  ((uint8_t *)packetBytes)[11] = (uint8_t)stream->outstreamSSRC;

  packetStartOffset = 12;
  if (cvoID != 0)
  {
    // send cvoID
    // Not that useful: https://datatracker.ietf.org/doc/html/rfc8834#name-coordination-of-video-orien
    // General header extensions described here https://datatracker.ietf.org/doc/html/rfc8285
    // page 84: https://www.etsi.org/deliver/etsi_ts/126100_126199/126114/16.07.00_60/ts_126114v160700p.pdf
    *(uint8_t *)packetBytes = 0x90;
    // https://www.rfc-editor.org/rfc/rfc5285#section-4.2
    // "Bede"
    ((uint8_t *)packetBytes)[12] = 0xbe;
    ((uint8_t *)packetBytes)[13] = 0xde;
    // Length (number of headers)
    ((uint8_t *)packetBytes)[14] = 0;
    ((uint8_t *)packetBytes)[15] = 1;
    // ID and len
    ((uint8_t *)packetBytes)[16] = (char)((cvoID << 0x1c) >> 0x18);
    // Data
    ((uint8_t *)packetBytes)[17] = stream->cvoInformation;
    // Pad
    ((uint8_t *)packetBytes)[18] = 0;
    ((uint8_t *)packetBytes)[19] = 0;
    packetStartOffset = 20;
    stream->lastcvoInformationSent = stream->cvoInformation;
  }

  packetStartByte = packetBytes + packetStartOffset;

  //*packetStartByte = 128;
  if (FUHeader == 0)
  {
    numFUHeaderBytes = 0;
  }
  else
  {
    *(uint8_t *)(packetStartByte) = (uint8_t)(FUHeader >> 8);
    *(uint8_t *)(packetStartByte + 1) = (uint8_t)FUHeader;
    packetStartOffset = packetStartOffset + 2;
    numFUHeaderBytes = 2;
  }

  //  this should be impossible with the correct calculation in Poll Packet
  HAPAssert(maxBytes >= packetStartOffset + numDataBytes + (stream->context_output_srtp).tag_size);

  if ((stream->context_output_srtp).key_size == 0)
  {
    HAPRawBufferCopyBytes(packetStartByte + numFUHeaderBytes, payloadBytes, numDataBytes);
  }
  else
  {
    // https://datatracker.ietf.org/doc/html/rfc3711
    // this will encrypt the already written header bytes and then add the payloadBytes to bytes
    // this is a one copy srtp implementation and the one copy happens here with the enc output

    srtp_encrypt(&stream->context_output_srtp, packetStartByte, payloadBytes, numFUHeaderBytes,
                 numDataBytes, index);

    srtp_authenticate(&stream->context_output_srtp,                      // context
                      packetStartByte + numFUHeaderBytes + numDataBytes, // tag output
                      packetBytes,                                       // bytes
                      packetStartOffset + numDataBytes,                  // numbytes
                      index >> 16);                                      // index
    packetStartOffset = packetStartOffset + (stream->context_output_srtp).tag_size;
  }
  stream->outSequenceNr = stream->outSequenceNr + 1;
  tempBytesWritten = stream->totalOutPacketBytesWritten;
  stream->totalOutPacketBytesWritten = numDataBytes + tempBytesWritten + numFUHeaderBytes;
  *numPacketBytes = numDataBytes + packetStartOffset;
  return (uint32_t)tempBytesWritten;
}

uint32_t localEndian(uint32_t input)
{
  uint32_t byteSwapTemp = (input >> 0x10) << 0x18 | (input >> 0x18) << 0x10;
  return ((byteSwapTemp >> 0x10) + (byteSwapTemp | (input & 0xff) << 8 | input >> 8 & 0xff) * 0x10000);
}


uint32_t TimeToTimestamp(POSRTPStreamRef *stream, uint64_t actualTime)
{
  uint64_t TimeStampDelta;
  uint64_t timeSinceStreamStart;

  timeSinceStreamStart = actualTime - stream->streamStartTime;
  TimeStampDelta = Upper64ofMul64((uint64_t)timeSinceStreamStart, ((uint64_t)stream->nsToTimestampConvLSW) +
                                                                          (((uint64_t)(stream->nsToTimestampConvMSW)) << 32));

  return stream->outTimeStampBase + TimeStampDelta;
}

void POSRTPStreamPushPacket(POSRTPStreamRef *stream, void *bytes, size_t numPacketBytes, size_t *numPayloadBytes,
                            HAPTime actualTime)

{
  uint64_t HAPNTPTimeOutput;
  uint32_t NTPactualTimeMSW;
  uint32_t NTPactualTimeLSW;
  int srtpVerifyResult;
  char *pLogString;

  *numPayloadBytes = 0;

  // Push RTP Packet from network to POS RTP stream
  if (stream == 0)
    return;
  uint32_t tagSize = (stream->context_input_rtcp).tag_size;
  if (numPacketBytes < tagSize)
  {
    HAPLogError(&logObject,"POSRTPStreamPushPacket Exiting: tagSize (%d) > numPacketBytes (%d)", tagSize, numPacketBytes);
    //exit(0);
    return;
  }

  uint32_t minAcceptablePacketSize = 8;
  if ((stream->context_input_rtcp).key_size != 0)
  {
    minAcceptablePacketSize = (stream->context_input_srtp).tag_size + 12;
  }

  // heuristic to process as RTP
  if (((numPacketBytes < minAcceptablePacketSize) || ((*(uint8_t *)bytes & 0x20) != 0)) ||
      ((((uint8_t *)bytes)[1] & 0xfe) != 200))
  {
    // processess as RTP Packet
    //HAPLogDebug(&logObject,"processess as RTP Packet");

    int seqIncrement;
    uint32_t packetTimestamp;
    uint32_t localTimeStamp;
    int32_t jitter_d;

    // If RFC 1889 version is not 2, drop
    if (*(uint8_t *)bytes >> 6 != 2)
    {
      HAPLogError(&logObject,"Exiting: wrong 1889 version");
      return;
    }
    // If extension or contributing identifiers are not blank, drop
    if ((*(uint8_t *)bytes & 0x1f) != 0)
    {
      HAPLogError(&logObject,"Exiting: ext or contrib ident");
      return;
    }
    // If not stream type, drop
    if ((uint8_t)((uint8_t *)bytes)[1] & 0x7f != stream->streamType)
    {
      HAPLogError(&logObject,"Exiting: wrong stream type");
      return;
    }


    // Check SSRC for match, else drop
    uint32_t packetSSRC = *(uint32_t *)((uint8_t *)bytes + 8);
    if (localEndian(packetSSRC) != stream->instreamSSRC)
    {
      HAPLogError(&logObject,"Exiting: packetSSRC != stream->instreamSSRC");
      //exit(0);
      return;
    }
    //else printf("packetSSRC == stream->instreamSSRC\n");

    if (stream->encodeType != RTPType_Simple)
    {
      HAPLogError(&logObject,"refusing RTPType_H264");
      return;
    }
    packetTimestamp = *(uint32_t *)((uint8_t *)bytes + 4);
    localTimeStamp = localEndian(packetTimestamp);

    // Sequence checks
    int32_t recSequenceNr = (uint32_t)(((uint8_t *)bytes)[2] * 0x100) + (uint32_t)(((uint8_t *)bytes)[3]);
    //printf("recSequenceNr = %d\n", recSequenceNr);
    if (stream->instreamSequenceNr == 0)
    {
      // Adopt incoming sequence nr
      stream->firstRecSqNr = recSequenceNr;
      stream->lastRecSqNrAndROC = recSequenceNr;
      seqIncrement = 0;
      stream->instreamBaseTimeStamp = localTimeStamp;
      stream->instreamTimeBase = 0;
    }
    else
    {
      *(short *)&seqIncrement = (short)recSequenceNr - (short)stream->lastRecSqNrAndROC;
      seqIncrement = (int)(short)seqIncrement;
      // Recv the same packet twice, drop
      if (seqIncrement < 1)
      {
        *numPayloadBytes = 0;
        return;
      }
      stream->lastRecSqNrAndROC = stream->lastRecSqNrAndROC + seqIncrement;
    }
    if ((stream->context_input_srtp).key_size != 0)
    {
      numPacketBytes = numPacketBytes - tagSize;

      srtpVerifyResult =
          srtp_verifyAuthentication(&(stream->context_input_srtp), bytes + numPacketBytes, bytes,
                                    numPacketBytes, stream->lastRecSqNrAndROC >> 16);
      if (srtpVerifyResult == 0)
      {
        *numPayloadBytes = 0;
        HAPLogError(&logObject,"SRTP input authentication failed");
        // exit(1);
        return;
      }
      //else HAPLogDebug(&logObject,"SRTP input authentication success");
    }

    if (seqIncrement > 5)
    {
      // missed too many, reset the stream
      stream->instreamBaseTimeStamp = localTimeStamp;
      stream->instreamTimeBase = 0;
    }
    stream->lastRecPacketPayloadSize = numPacketBytes - 12;
    stream->lastRecEncPacketPtr = (uint8_t *)bytes + 12;
    stream->lastRecTimeStamp = localTimeStamp;
    if (stream->instreamExtendedHighestSeqNrRec < *(uint32_t *)&stream->lastRecSqNrAndROC)
    {
      stream->instreamExtendedHighestSeqNrRec = *(uint32_t *)&stream->lastRecSqNrAndROC;
    }
    stream->instreamSequenceNr = stream->instreamSequenceNr + 1;
    uint32_t timestampFromTime = TimeToTimestamp(stream, actualTime);
    // https://datatracker.ietf.org/doc/html/rfc3550#page-94

    jitter_d = (timestampFromTime - localTimeStamp) - stream->jitter_transit;
    stream->jitter_transit = timestampFromTime - localTimeStamp; // transit
    if (jitter_d < 0)
      jitter_d = -jitter_d;
    stream->interarrivalJitterCalc =
        (stream->interarrivalJitterCalc - ((stream->interarrivalJitterCalc) + 8 >> 4)) + jitter_d;

    HAPNTPTimeOutput = HAPTimeToNTPTime(actualTime);

    NTPactualTimeMSW = (uint32_t)((uint64_t)HAPNTPTimeOutput >> 0x20);
    NTPactualTimeLSW = HAPNTPTimeOutput & 0xffffffff;
    stream->lastRecvNTPtime = NTPactualTimeMSW;

    *numPayloadBytes = numPacketBytes - 0xc;
    return;
  }
  else
  {
    // process as RTCP Packet
    int tagOffset;

    //HAPLogDebug(&logObject,"Process as an RTCP Packet");
    if (localEndian(*(uint32_t *)((uint8_t *)bytes + 4)) != stream->instreamSSRC)
    {
      HAPLogError(&logObject,"SSRC doesn't match.  Packet %08x, instreamssrc %08x .", localEndian(*(uint32_t *)((uint8_t *)bytes + 4)), stream->instreamSSRC);
      return;
    }
    //"(stream->context_input_rtcp).key_size: %d\n", (stream->context_input_rtcp).key_size);
    if ((stream->context_input_rtcp).key_size != 0)
    {

      tagOffset = numPacketBytes - tagSize;
      // is this an error?  -4 removes verification of the SRTCP Index, which doesn't seem right
      numPacketBytes = tagOffset - 4;
      uint32_t packetEndPtr = *(uint32_t *)((uint8_t *)bytes + numPacketBytes);
      uint32_t recIndex = localEndian(packetEndPtr);
      srtpVerifyResult = srtp_verifyAuthentication(&stream->context_input_rtcp, (uint8_t *)bytes + tagOffset, bytes,
                                                   numPacketBytes, recIndex);
      if (srtpVerifyResult == 0)
      {
        HAPLogError(&logObject,"SRTCP input authentication failed");
        return;
      }
      HAPLogDebug(&logObject,"SRTCP input authentication success");

      // replay protection as defined in https://datatracker.ietf.org/doc/html/rfc2401#page-1-58
      uint32_t index = recIndex & 0x7fffffff; // most significant bit indicates whether the payload is encrypted
      uint32_t recIndexDelta = index - *(int *)&stream->lastRecIndex;
      uint32_t newBitmap = 0;
      if ((int)recIndexDelta < 1)
      {
        if ((int)recIndexDelta < -31)
        {
          HAPLogInfo(&logObject,"(int)recIndexDelta < -31");
          return;
        }
        newBitmap = 1 << (-recIndexDelta & 0x1f); // packet location in the bitmap
        if ((newBitmap & *(uint32_t *)&stream->SRTCPReplayBitmap) != 0)
        {
          HAPLogError(&logObject,"Replayed SRTCP input packet");
          return;
        }
        stream->SRTCPReplayBitmap = newBitmap | stream->SRTCPReplayBitmap;
      }
      else
      {
        if ((int)recIndexDelta < 32)
        {
          newBitmap = *(int *)&stream->SRTCPReplayBitmap << (recIndexDelta & 0x1f) | 1; // move the bitmap
        }
        else
        {
          newBitmap = 1; // jumped far ahead, create a fresh bitmap
        }
        *(uint32_t *)&stream->SRTCPReplayBitmap = newBitmap;
        *(uint32_t *)&stream->lastRecIndex = index;
      }
      if (recIndex >> 0x1f != 0)
      {
        srtp_decrypt(&stream->context_input_rtcp, (uint8_t *)bytes + 8, (uint8_t *)bytes + 8,
                     tagOffset - 12, index);
      }
      else
        HAPLogInfo(&logObject,"(recIndex >> 0x1f == 0)");
    }

    // HAPLogBufferDebugInternal(&kHAPRTPController_PacketLog,bytes,numPacketBytes,"(%p) <",stream);
    uint8_t *rtcpPacketPointer = (uint8_t *)bytes;
    while (rtcpPacketPointer + 4 <= (uint8_t *)bytes + numPacketBytes)
    {
      // Walk through RTP packets in received buffer and
      // check RTP header Version for consistency
      if (*rtcpPacketPointer >> 6 != 2)
      {
        HAPLogInfo(&logObject,"*rtcpPacketPointer >> 6 != 2): returning");
        return;
      }
      rtcpPacketPointer =
          rtcpPacketPointer +
          (uint8_t)rtcpPacketPointer[2] * 0x400 + (uint8_t)rtcpPacketPointer[3] * 4 + 4;
    }
    if ((uint8_t *)bytes + numPacketBytes != rtcpPacketPointer)
    {
      //HAPLogInfo(&logObject,"(uint8_t *)bytes + numPacketBytes = %p\n", (uint8_t *)bytes + numPacketBytes);
      //HAPLogInfo(&logObject,"rtcpPacketPointer = %p\n", rtcpPacketPointer);
      //HAPLogInfo(&logObject,"((uint8_t *)bytes + numPacketBytes != rtcpPacketPointer), returning");
      return;
    }

    HAPNTPTimeOutput = HAPTimeToNTPTime(actualTime);
    NTPactualTimeMSW = (uint32_t)((uint64_t)HAPNTPTimeOutput >> 0x20);
    NTPactualTimeLSW = HAPNTPTimeOutput & 0xffffffff;
    stream->lastRecvNTPtime = NTPactualTimeMSW;

    uint32_t LastRecvNtpTime65536thsOfSecond = NTPactualTimeLSW >> 0x10 | NTPactualTimeMSW << 0x10;
    while (bytes < (void *) rtcpPacketPointer)
    {
      // Walk through RTCP packets
      // https://datatracker.ietf.org/doc/html/rfc3550#section-6.4.1
      uint32_t *RTCPReportBlockStart;
      uint8_t rtcpPacketType = ((uint8_t *)bytes)[1];
      uint32_t rtcpPacketSize = (uint32_t)((uint8_t *)bytes)[2] * 0x400 + (uint32_t)((uint8_t *)bytes)[3] * 4 + 4;
      uint32_t rtcpReportCount = (uint32_t) * (uint8_t *)bytes & 0x1f;
      if (rtcpPacketType < 200)
      {
        // Ignore Packet Types < 200
        HAPLogInfo(&logObject,"Ignore Packet Types < 200");
        bytes = (uint8_t *)bytes + rtcpPacketSize;
        continue;
      }
      if (rtcpPacketType < 202)
      {
        // Accept
        // Sender Report (type 200)
        // Receiver Report (type 201)
        if ((rtcpPacketSize > 7) && localEndian(*(uint32_t *)((uint8_t *)bytes + 4)) == stream->instreamSSRC)
        {
          if (rtcpPacketType == 200)
          {
            // Sender Report
            if (rtcpPacketSize == rtcpReportCount * 0x18 + 0x1c)
            {
              uint32_t recRTPTimestamp = *(uint32_t *)(bytes + 16);
              stream->LastRecvNtpTime65536thsOfSecond = LastRecvNtpTime65536thsOfSecond;
              stream->LastRecSenderReportNTPMSW = localEndian(*(uint32_t *)(bytes + 8));
              stream->LastRecSenderReportNTPLSW = localEndian(*(uint32_t *)(bytes + 12));
              RTCPReportBlockStart = (uint32_t *)((uint8_t *)bytes + 28);
              stream->recRTCPTimestamp = localEndian(recRTPTimestamp);
            }
            else
            {
              // reject: next rtcp report
              HAPLogInfo(&logObject,"reject: next rtcp report ");
              bytes = (uint8_t *)bytes + rtcpPacketSize;
              continue;
            }
          }
          else
          {
            if (rtcpPacketSize == rtcpReportCount * 0x18 + 8)
            {
              // Receiver Report
              RTCPReportBlockStart = (uint32_t *)((uint8_t *)bytes + 8);
            }
            else
            {
              // reject: next rtcp report
              bytes = (uint8_t *)bytes + rtcpPacketSize;
              continue;
            }
          }
          while (rtcpReportCount != 0)
          {
            if (localEndian(*RTCPReportBlockStart) == stream->outstreamSSRC)
            {
              uint32_t localLastSenderReport = localEndian(*(uint32_t *)((uint8_t *)RTCPReportBlockStart + 16));           // last Sender Report
              uint32_t localDelaySinceLastSenderReport = localEndian(*(uint32_t *)((uint8_t *)RTCPReportBlockStart + 20)); // delay since last sender report
              if ((localLastSenderReport != 0) && (localDelaySinceLastSenderReport != 0))
              {
                // bottom of page 40: https://datatracker.ietf.org/doc/html/rfc3550#page-40
                uint32_t rtt_calc = (LastRecvNtpTime65536thsOfSecond - localLastSenderReport) - localDelaySinceLastSenderReport;
                // This smoothed Tr algorithm might be better https://datatracker.ietf.org/doc/html/rfc8083#page-7
                uint32_t rtt_decay = stream->roundTripTimeCalculation -
                                     (stream->roundTripTimeCalculation >> 4);
                if (rtt_calc <= rtt_decay)
                {
                  rtt_calc = rtt_decay;
                }
                stream->roundTripTimeCalculation = rtt_calc;
              }
            }
            RTCPReportBlockStart = RTCPReportBlockStart + 6;
            rtcpReportCount = rtcpReportCount - 1 & 0xff;
          }
        }
        // done: next rtcp report
        bytes = (uint8_t *)bytes + rtcpPacketSize;
        continue;
      }
      // RTCP Packet Number >206
      if ((((uint8_t)rtcpPacketType > 206) || (rtcpPacketSize < 12)) || (localEndian(*(uint32_t *)((uint8_t *)bytes + 4)) != stream->instreamSSRC))
      {
        // next rtcp report
        bytes = (uint8_t *)bytes + rtcpPacketSize;
        continue;
      }
      // packet type 206 (PSFB) processing https://datatracker.ietf.org/doc/html/rfc5104
      uint32_t psfb_fmt = rtcpReportCount;
      if (rtcpPacketType != 205)
      {
        if (psfb_fmt == 1)
        {
          // Packet Loss Indication https://datatracker.ietf.org/doc/html/rfc4585#section-6.3.1
          if ((rtcpPacketSize == 12) &&
              (localEndian(*(uint32_t *)((uint8_t *)bytes + 8)) == stream->outstreamSSRC))
          {
            HAPLogInfo(&logObject,"processing a PLI packet");
            if (*(int *)&stream->KeyFrameRequestedBitOne == 0)
            {
              int64_t rtt = stream->roundTripTimeCalculation;
              pLogString = "no new key frame";
              // if (2 * rtt (uints: middle32) + lastsentkeyframe (uints: ns since hap epoch))<= actualTime (uints: ns since hap epoch)
              if (((rtt << 0x11 & 0xffffffff) * 1000000000 >> 0x20) +
                      ((rtt << 0x11) >> 0x20) * 1000000000 +
                      (int64_t)(stream->lastSentKeyFrame) <=
                  actualTime)
              {
                stream->KeyFrameRequestedBitOne = 2;
                pLogString = "request new key frame";
              }
              uint64_t lastSentKeyFrameNTPTime = HAPTimeToNTPTime(stream->lastSentKeyFrame);
              // Packet Loss Indication
              HAPLogInfo(&logObject,"PLI: %s, time=%lu.%03lu, key-time=%lu.%03lu, rtt=%lu.%03lu",
                     pLogString, NTPactualTimeMSW & 0xffff, ((uint64_t)(NTPactualTimeLSW) * 1000) >> 0x20,
                     (lastSentKeyFrameNTPTime >> 0x20) & 0xffff, ((lastSentKeyFrameNTPTime & 0xffffffff) * 1000 >> 0x20),
                     rtt >> 0x10, (rtt & 0xffff) * 1000 >> 0x10);
            }
            else
            {
              HAPLogInfo(&logObject,"PLI: key frame already requested");
            }
            bytes = (uint8_t *)bytes + rtcpPacketSize;
            continue;
          }
        }
        else
        {
          if (psfb_fmt == 4)
          {
            // https://datatracker.ietf.org/doc/html/rfc5104#section-4.2.2
            if (((rtcpPacketSize == 20) && (localEndian(*(uint32_t *)((uint8_t *)bytes + 12)) == stream->outstreamSSRC)) &&
                ((uint8_t)((uint8_t *)bytes)[0x10] != stream->lastRecFIRframeRequestSequenceNumber))
            {
              stream->lastRecFIRframeRequestSequenceNumber = (uint8_t)((uint8_t *)bytes)[0x10];
              if (*(int *)&stream->KeyFrameRequestedBitOne == 0)
              {
                *(uint32_t *)&stream->KeyFrameRequestedBitOne = 2;
                HAPLogInfo(&logObject, "FIR: request new key frame");
              }
              else
              {
                HAPLogInfo(&logObject, "FIR: key frame already requested");
              }
              bytes = (uint8_t *)bytes + rtcpPacketSize;
              continue;
            }
          }
          else
          {
            if (((psfb_fmt == 5) && (rtcpPacketSize == 20)) && (localEndian(*(uint32_t *)((uint8_t *)bytes + 12)) == stream->outstreamSSRC))
            {
              // https://datatracker.ietf.org/doc/html/rfc5104#section-4.3.2
              uint8_t TSTRcmdWordLoc = ((uint8_t *)bytes)[0x10];
              uint16_t TSTRcmdWord = (uint16_t)TSTRcmdWordLoc;
              if (TSTRcmdWord != stream->lastRecTSTRCmdWord)
              {
                stream->lastRecTSTRCmdWord = TSTRcmdWord;
                if ((stream->lastRecTSTRSeqNr == 0xffffffff) ||
                    (0 < (char)(TSTRcmdWordLoc - (char)(stream->lastRecTSTRSeqNr >> 0x18))))
                {
                  stream->lastRecTSTRSeqNr = TSTRcmdWord * 0x1000000 + 0x1f;
                }
                else

                  // Temporal Spatial Tradeoff Request (0-31)
                  // HAPLogInfoInternal(&logObject,"TSTR received: index=%d",
                  //                   (uint)((uint8_t *)bytes)[0x13] & 0x1f);
                  HAPLogInfo(&logObject,"TSTR received: index=%d",
                         (uint8_t)((uint8_t *)bytes)[0x13] & 0x1f);
                bytes = (uint8_t *)bytes + rtcpPacketSize;
                continue;
              }
            }
          }
        }
        // next rtcp report
        bytes = (uint8_t *)bytes + rtcpPacketSize;
        continue;
      }
      // 205 RTPFB Processing https://datatracker.ietf.org/doc/html/rfc4585#section-6.1
      if (((psfb_fmt != 3) || (rtcpPacketSize != 20)) || (localEndian(*(uint32_t *)((uint8_t *)bytes + 12)) != stream->outstreamSSRC))
      {

        HAPLogInfo(&logObject,"psfb_fmt %d, rtcpPacketSize %d, ssrc %d", psfb_fmt, rtcpPacketSize, (localEndian(*(uint32_t *)((uint8_t *)bytes + 12))));
        // next rtcp report
        bytes = (uint8_t *)bytes + rtcpPacketSize;
        continue;
      }
      //TMMBR https://datatracker.ietf.org/doc/html/rfc5104#section-4.2.1
      uint32_t recvCmdWord = localEndian(*(uint32_t *)((uint8_t *)bytes + 0x10));
      uint32_t maxPacketSize = stream->maximumMTU - stream->outStreamHeaderPlusTagSize;
      uint32_t MxTBRmantessa = recvCmdWord * 0x40 >> 0xf;
      //TODO: this could be better coded, but is accurate
      // (recvCmdWord >> 0x1a & 1f) is the value of the MxTBR exponent
                //   MxTBR = mantissa * 2^exp 
      uint32_t MxTBRoverhead = recvCmdWord & 0x1ff;
      uint32_t MxTBRexponent = recvCmdWord >> 0x1a & 0x1f;
      uint32_t idx = 0;
      if (0xffffffffU >> (MxTBRexponent) < MxTBRmantessa) {
      label:
        // MxTBRoverhead is the TMMBR measured overhead value
        if (maxPacketSize == 0) {
          return; //trap(7);
        }
        recvCmdWord = (MxTBRoverhead * 25000000) / maxPacketSize + 25000000;
        while (0x1ffff < recvCmdWord) {
          recvCmdWord = recvCmdWord >> 1;
          idx = idx + 1;
        }
        MxTBRmantessa = 25000000;
        recvCmdWord = idx << 0x1a | recvCmdWord << 9 | MxTBRoverhead;
      }
      else {
        idx = MxTBRmantessa << (MxTBRexponent);
        if (maxPacketSize == 0) {
          return; //trap(7);
        }
        MxTBRmantessa = idx - (MxTBRoverhead * idx) / maxPacketSize;
        if (25000000 < MxTBRmantessa) goto label;
      }
      stream->lastReceivedTMMBRPayload = recvCmdWord;
      stream->requestNewBitrateAtCheck = MxTBRmantessa;
      if (MxTBRmantessa == stream->bitRate){
        // next rtcp report
        bytes = (uint8_t *)bytes + rtcpPacketSize;
        continue;
      }
      HAPLogInfo(&logObject,"new bitrate requested: %d -> %d", stream->bitRate, stream->requestNewBitrateAtCheck);
      bytes = (uint8_t *)bytes + rtcpPacketSize;
    }
  }
  return;
}

void POSRTPStreamPollPayload(POSRTPStreamRef *stream, void *bytes, size_t maxBytes, size_t *numBytes, HAPTimeNS *sampleTime)

{
  if (numBytes == 0)
    return;
  *(uint32_t *)numBytes = 0;
  if (sampleTime == 0)
    return;
  if (stream->lastRecEncPacketPtr == 0)
    return;

  uint32_t lastRecPacketPayloadSize = stream->lastRecPacketPayloadSize;
  if (maxBytes < lastRecPacketPayloadSize)
  {
    return;
  }
  uint32_t keySize = (stream->context_input_srtp).key_size;
  if (keySize != 0)
  {
    srtp_decrypt(&stream->context_input_srtp, // context
                 bytes,                       // output
                 stream->lastRecEncPacketPtr, // input
                 lastRecPacketPayloadSize,    // size
                 stream->lastRecSqNrAndROC);  //>>16);  //index
  }
  *(uint32_t *)numBytes = lastRecPacketPayloadSize;
  stream->lastRecEncPacketPtr = 0;
  stream->lastRecPacketPayloadSize = 0;

  // samplesSinceBase needs to be uint32_t to perform modulo 32 subtraction
  uint32_t samplesSinceBase = stream->lastRecTimeStamp - stream->instreamBaseTimeStamp;

  uint64_t samplePeriodNS;
  uint64_t sampleNS;
  if (stream->clockFreq == 24000)
  {
    sampleNS = (samplesSinceBase * 0xa2c2aaaaab) >> 0x18;
  }
  else
  {
    switch (stream->clockFreq)
    {
    case 16000:
      samplePeriodNS = 0xf424;
      break;
    case 8000:
      samplePeriodNS = 0x1e848;
      break;
    default:
      return;
    }
    sampleNS = ((uint64_t)samplesSinceBase) * samplePeriodNS;
  }

  *sampleTime = sampleNS + stream->instreamTimeBase;
  if (samplesSinceBase >= stream->clockFreq)
  {
    // once per second reset the time base to avoid timestamp overflow
    stream->instreamBaseTimeStamp = stream->instreamBaseTimeStamp + stream->clockFreq;
    stream->instreamTimeBase = stream->instreamTimeBase + 1000000000;
  }

  return;
}

void POSRTPStreamCheckFeedback(POSRTPStreamRef *stream, HAPTime actualTime, void *bytes, size_t maxBytes,
                               size_t *numBytes, uint32_t *bitRate, bool *newKeyFrame, uint32_t *dropoutTime)
{
  if (numBytes == 0)
  {
    HAPLogError(&logObject,"numBytes == 0, bailing.");
    return;
  }
  else *numBytes = 0;

  if (actualTime == 0)
  {
    HAPLogError(&logObject,"actualTime == 0, bailing.");
    return;
  }
  if (bitRate != 0)
  {
    if (stream->requestNewBitrateAtCheck == stream->bitRate)
    {
      *bitRate = 0;
    }
    else
    {
      *bitRate = stream->requestNewBitrateAtCheck;
      stream->bitRate = stream->requestNewBitrateAtCheck;
    }
  }
  if (newKeyFrame != 0)
  {
    if (*(int *)&stream->KeyFrameRequestedBitOne == 2)
    {
      *newKeyFrame = 1;
      stream->KeyFrameRequestedBitOne = 1;
    }
    else
    {
      *newKeyFrame = 0;
    }
  }
  if (maxBytes < 128){
    HAPLogError(&logObject,"not enough space, bailing.");
    return; // enough space to fit any size packet and avoid checking throughout
  }
  uint64_t NTPTime = HAPTimeToNTPTime(actualTime);

  if (dropoutTime != 0)
  {
    // printf("NTPTime %lu, lastRecvNTPtime %lu\n",NTPTime>>32,(stream->lastRecvNTPtime));
    *dropoutTime = (uint32_t)(NTPTime >> 32) - (uint32_t)(stream->lastRecvNTPtime);
  }

  if (stream->lastReceivedTMMBRPayload == 0xffffffff)
  {
    if (actualTime < stream->lastRTCPReportHAPTime + (uint64_t)stream->rtcpInterval * 1000000)
    { // ms to ns conv
      HAPLogDebug(&logObject,"not time to send a report yet, rtcpInterval: %u\n", (uint64_t)stream->rtcpInterval * 1000000);
      return;
    }
  }

  stream->lastRTCPReportHAPTime = actualTime;

  uint32_t deltaSinceLastRecvReport = stream->instreamSequenceNr - stream->lastRecvReportRTPIndex;
  stream->lastRecvReportRTPIndex = stream->instreamSequenceNr;

  // https://datatracker.ietf.org/doc/html/rfc3550#appendix-A.3
  uint32_t lastInstreamExpectedPackets = stream->instreamExpectedPackets;
  uint32_t localOutSeqNr = stream->outSequenceNr;
  uint32_t instreamExpectedPackets = stream->instreamExtendedHighestSeqNrRec - *(int *)&stream->firstRecSqNr + 1;
  stream->instreamExpectedPackets = instreamExpectedPackets;

  uint32_t localOutSeqNrOfTwoPreviousSenderReport = stream->outSeqNrOfTwoPreviousSenderReport; // yes
  stream->outSeqNrOfTwoPreviousSenderReport = stream->outSeqNrOfLastSenderReport;
  stream->outSeqNrOfLastSenderReport = localOutSeqNr;

  //HAPLogDebug(&logObject,"Starting RTCP Packet");
  uint8_t rtcpLengthWords = (deltaSinceLastRecvReport != 0) * 6 + (localOutSeqNr != localOutSeqNrOfTwoPreviousSenderReport) * 5 + 1;
  uint8_t payloadType = 200;
  if (localOutSeqNr == localOutSeqNrOfTwoPreviousSenderReport)
  {
    //haven't sent a message in 2 reports, we're now sending a receiver report
    payloadType = 201;
  }
  printf("payloadType: %d\n", payloadType);

  uint8_t *ptrNextReport = (uint8_t *)bytes;

  *(uint8_t *)(ptrNextReport + 0) = (deltaSinceLastRecvReport != 0) + 0x80;
  *(uint8_t *)(ptrNextReport + 1) = payloadType;
  *(uint8_t *)(ptrNextReport + 2) = 0;
  *(uint8_t *)(ptrNextReport + 3) = rtcpLengthWords;
  *(uint32_t *)(ptrNextReport + 4) = localEndian(stream->outstreamSSRC);
  ptrNextReport = ptrNextReport + 8;

  if (localOutSeqNr != localOutSeqNrOfTwoPreviousSenderReport)
  {
    // Sender Report  https://datatracker.ietf.org/doc/html/rfc3550#page-36
    //HAPLogDebug(&logObject,"Starting Sender Report");
    *(uint32_t *)(ptrNextReport + 0) = localEndian((uint32_t)(NTPTime >> 32));
    *(uint32_t *)(ptrNextReport + 4) = localEndian((uint32_t)NTPTime);
    *(uint32_t *)(ptrNextReport + 8) = localEndian(TimeToTimestamp(stream, actualTime));
    *(uint32_t *)(ptrNextReport + 12) = localEndian(localOutSeqNr);
    *(uint32_t *)(ptrNextReport + 16) = localEndian(stream->totalOutPacketBytesWritten);

    ptrNextReport = ptrNextReport + 20;
  }

  if (deltaSinceLastRecvReport != 0)
  {

    // Receiver Report  https://datatracker.ietf.org/doc/html/rfc3550
    //HAPLogDebug(&logObject,"Starting Receiver Report");
    // calculations defined in https://datatracker.ietf.org/doc/html/rfc3550#appendix-A.3
    int32_t cumRecvPacketsLost = (int32_t)(instreamExpectedPackets - stream->instreamSequenceNr);
    if ((int)cumRecvPacketsLost < -0x800000)
    {
      cumRecvPacketsLost = 0xff800000;
    }
    if (cumRecvPacketsLost > 0x7fffff)
      cumRecvPacketsLost = 0x7fffff;

    uint64_t delaySinceLastRecSenderReport;
    uint32_t lastSenderReport;
    if ((stream->LastRecSenderReportNTPLSW | stream->LastRecSenderReportNTPMSW) == 0)
    {
      delaySinceLastRecSenderReport = 0;
      lastSenderReport = 0;
    }
    else
    {
      lastSenderReport = stream->LastRecSenderReportNTPMSW << 0x10 |
                         stream->LastRecSenderReportNTPLSW >> 0x10;
      delaySinceLastRecSenderReport = NTPTime >> 0x10 - stream->LastRecvNtpTime65536thsOfSecond;
    }

    uint32_t expectedInterval = instreamExpectedPackets - lastInstreamExpectedPackets;
    uint8_t fractionLost = 0;
    if (expectedInterval != 0)
      fractionLost = (uint8_t)(((expectedInterval - deltaSinceLastRecvReport) << 8) / expectedInterval);

    // build receiver report
    *(uint32_t *)(ptrNextReport + 0) = localEndian(stream->instreamSSRC);
    *(uint8_t *)(ptrNextReport + 4) = (uint8_t)fractionLost;
    *(uint8_t *)(ptrNextReport + 5) = (uint8_t)(cumRecvPacketsLost >> 16);
    *(uint8_t *)(ptrNextReport + 6) = (uint8_t)(cumRecvPacketsLost >> 8);
    *(uint8_t *)(ptrNextReport + 7) = (uint8_t)cumRecvPacketsLost;

    *(uint32_t *)(ptrNextReport + 8) = localEndian(stream->instreamExtendedHighestSeqNrRec);

    *(uint8_t *)(ptrNextReport + 12) = (uint8_t)(stream->interarrivalJitterCalc >> 0x1c);
    *(uint8_t *)(ptrNextReport + 13) = (uint8_t)(stream->interarrivalJitterCalc >> 0x14);
    *(uint8_t *)(ptrNextReport + 14) = (uint8_t)(stream->interarrivalJitterCalc >> 0xc);
    *(uint8_t *)(ptrNextReport + 15) = (uint8_t)(stream->interarrivalJitterCalc >> 4);

    *(uint32_t *)(ptrNextReport + 16) = localEndian(lastSenderReport);
    *(uint32_t *)(ptrNextReport + 20) = localEndian(delaySinceLastRecSenderReport);

    ptrNextReport = ptrNextReport + 24;
  }
  if ((localOutSeqNr != localOutSeqNrOfTwoPreviousSenderReport) || (deltaSinceLastRecvReport != 0))
  {
    //HAPLogDebug(&logObject,"Starting SDES Report");

    uint32_t cnameReportLength = stream->localCNAMElength + 10 >> 2;
    *(uint8_t *)(ptrNextReport + 0) = 0x81; // V2, one source count
    *(uint8_t *)(ptrNextReport + 1) = 202;  // SDES
    *(uint8_t *)(ptrNextReport + 2) = (uint8_t)((cnameReportLength << 0x10) >> 0x18);
    *(uint8_t *)(ptrNextReport + 3) = (uint8_t)cnameReportLength;
    *(uint32_t *)(ptrNextReport + 4) = localEndian(stream->outstreamSSRC);
    *(uint8_t *)(ptrNextReport + 8) = 1; // CNAME
    *(uint8_t *)(ptrNextReport + 9) = (uint8_t)stream->localCNAMElength;
    HAPRawBufferCopyBytes(ptrNextReport + 10, stream->localCNAME, (uint8_t)stream->localCNAMElength);
    ptrNextReport = ptrNextReport + 10 + (uint8_t)stream->localCNAMElength;
    do
    {
      uint8_t *stringTerm = ptrNextReport;
      ptrNextReport = stringTerm + 1;
      *stringTerm = 0;
    } while (((uint32_t)ptrNextReport & 3) != 0); // relies on word being 4 byte aligned

    if (stream->lastReceivedTMMBRPayload != 0xffffffff)
    {
      //HAPLogDebug(&logObject,"Starting TMMBN Report");

      // send TMMBN message https://datatracker.ietf.org/doc/html/rfc5104
      *(uint8_t *)(ptrNextReport + 0) = 0x84; // TMMBN
      *(uint8_t *)(ptrNextReport + 1) = 205;  // RTPFB
      *(uint8_t *)(ptrNextReport + 2) = 0;    // length
      *(uint8_t *)(ptrNextReport + 3) = 4;    // length
      *(uint32_t *)(ptrNextReport + 4) = localEndian(stream->outstreamSSRC);
      *(uint32_t *)(ptrNextReport + 8) = 0;
      *(uint32_t *)(ptrNextReport + 12) = localEndian(stream->instreamSSRC);
      *(uint32_t *)(ptrNextReport + 16) = localEndian(stream->lastReceivedTMMBRPayload);
      ptrNextReport = ptrNextReport + 20;
      stream->lastReceivedTMMBRPayload = 0xffffffff;
    }
    if (stream->lastRecTSTRSeqNr != 0xffffffff)
    {
      HAPLogDebug(&logObject,"Starting TSTR Report");
      // send TSTR message https://datatracker.ietf.org/doc/html/rfc5104
      *(uint8_t *)(ptrNextReport + 0) = 0x86; // xTSTR
      *(uint8_t *)(ptrNextReport + 1) = 206;  // PSFB
      *(uint8_t *)(ptrNextReport + 2) = 0;    // length
      *(uint8_t *)(ptrNextReport + 3) = 4;    // length
      *(uint32_t *)(ptrNextReport + 4) = localEndian(stream->outstreamSSRC);
      *(uint32_t *)(ptrNextReport + 8) = 0;
      *(uint32_t *)(ptrNextReport + 12) = localEndian(stream->instreamSSRC);
      *(uint32_t *)(ptrNextReport + 16) = localEndian(stream->lastRecTSTRSeqNr);
      ptrNextReport = ptrNextReport + 20;
      stream->lastRecTSTRSeqNr = 0xffffffff;
    }

    //HAPLogDebug(&logObject,"outputRTCPcounter: %d\n", stream->outputRTCPcounter);

    if ((stream->context_output_rtcp).key_size != 0)
    {

      srtp_encrypt(&stream->context_output_rtcp,            // context
                           ((uint8_t *)(bytes)) + 8,         // output
                           ((uint8_t *)(bytes)) + 8,         // input
                           0,                                // header bytes
                           ptrNextReport - (uint8_t *)bytes, // num data bytes
                           stream->outputRTCPcounter);       // index

      *(uint32_t *)(ptrNextReport) = localEndian(stream->outputRTCPcounter | 0x80000000);

      //HAPLogDebug(&logObject,"Starting Authenticate");
      srtp_authenticate(&stream->context_output_rtcp,            // context
                        ptrNextReport + 4,                       // tag
                        (uint8_t *)bytes,                        // bytes
                        ptrNextReport - (uint8_t *)bytes,        // num bytes
                        stream->outputRTCPcounter | 0x80000000); // index
      //HAPLogDebug(&logObject,"Authenticate Done");
      ptrNextReport = ptrNextReport + 4 + (stream->context_output_rtcp).tag_size;

      stream->outputRTCPcounter = stream->outputRTCPcounter + 1 & 0x7fffffff;
    }
  }
  *numBytes = ptrNextReport - (uint8_t *)bytes;
//  HAPLogError(&logObject,"POSRTPStreamCheckFeedback exiting with numBytes: %d\n", *numBytes);
  
  return;
}
