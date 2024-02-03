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


#ifndef POSRTPCONTROLLER_H
#define POSRTPCONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAP.h"
#include "POSSRTPCrypto.h"

typedef uint64_t HAPTime;

HAP_ENUM_BEGIN(uint8_t, RTPType){
    RTPType_Simple,
    RTPType_H264
} HAP_ENUM_END(uint8_t, RTPType);

typedef struct
{
    uint32_t streamType;
    uint32_t clockFreq;
    uint32_t nsToTimestampConvLSW;
    uint32_t nsToTimestampConvMSW;
    uint32_t outstreamSSRC;
    uint32_t instreamSSRC;
    uint32_t maximumMTU;
    uint32_t outStreamHeaderPlusTagSize;
    uint32_t rtcpInterval;
    char localCNAME[32];
    uint32_t localCNAMElength;
    RTPType encodeType;
    uint32_t padding1;
    void *payloadBytes; 
    uint32_t numPayloadBytes; 
    uint32_t timeStampToSend; 
    uint32_t nextPayloadStart; 
    uint32_t randomOutputSequenceNrBase; 
    uint32_t outTimeStampBase; 
    HAPTime streamStartTime;  
    uint32_t outSequenceNr;
    uint32_t totalOutPacketBytesWritten;
    uint32_t outSeqNrOfLastSenderReport;
    uint32_t outSeqNrOfTwoPreviousSenderReport;
    uint32_t outputRTCPcounter;
    uint8_t cvoID;
    uint8_t cvoInformation;
    uint8_t lastcvoInformationSent;
    uint16_t KeyFrameRequestedBitOne;
    //HAPTime unused1;
    HAPTime lastSentKeyFrame;
    HAPTime lastRTCPReportHAPTime;
    uint8_t *lastRecEncPacketPtr;
    uint32_t lastRecPacketPayloadSize;
    uint32_t lastRecTimeStamp;
    uint32_t lastRecSqNrAndROC;
    uint32_t firstRecSqNr;
    uint32_t instreamExtendedHighestSeqNrRec;
    uint32_t instreamBaseTimeStamp;
    uint64_t instreamTimeBase;
    uint32_t instreamSequenceNr;
    uint32_t lastRecvReportRTPIndex;
    uint32_t instreamExpectedPackets;
    uint32_t LastRecvNtpTime65536thsOfSecond;
    uint32_t LastRecSenderReportNTPLSW;
    uint32_t LastRecSenderReportNTPMSW;
    uint32_t recRTCPTimestamp;
    uint32_t jitter_transit;
    uint32_t lastRecIndex;
    uint32_t SRTCPReplayBitmap;
    uint32_t interarrivalJitterCalc; 
    uint32_t roundTripTimeCalculation;
    uint64_t lastRecvNTPtime;
    uint32_t lastRecFIRframeRequestSequenceNumber;
    uint32_t lastRecTSTRCmdWord;
    uint32_t bitRate;
    uint32_t requestNewBitrateAtCheck;
    uint32_t lastReceivedTMMBRPayload;
    uint32_t lastRecTSTRSeqNr;
    uint8_t SPSNALU[128];
    uint32_t SPSNALUNumBytes;
    uint8_t PPSNALU[128];
    uint32_t PPSNALUNumBytes;
    srtp_ctx context_input_srtp;
    srtp_ctx context_output_srtp;
    srtp_ctx context_input_rtcp;
    srtp_ctx context_output_rtcp;
} POSRTPStreamRef;



typedef struct {
    uint8_t type;
    uint32_t ssrc;
    uint16_t maxBitRate;
    float RTCPInterval;
    uint16_t maximumMTU;
} POSRTPParameters;


HAP_ENUM_BEGIN(uint8_t, SRTPCryptoType){
    CRYPTOTYPE_AES_CM_128_HMAC_SHA1_80 = 1,
    CRYPTOTYPE_AES_256_CM_HMAC_SHA1_80 = 2
} HAP_ENUM_END(uint8_t, SRTPCryptoType);

typedef struct {
    SRTPCryptoType cryptoType;
    union {
        struct {
            uint8_t key[16];
            uint8_t salt[14];
        } AES_CM_128_HMAC_SHA1_80;

        struct {
            uint8_t key[32];
            uint8_t salt[14];
        } AES_256_CM_HMAC_SHA1_80; //sic
    } Key_Union;
}POSSRTPParameters;

HAPError POSRTPStreamEnd(POSRTPStreamRef *stream);

HAPError POSRTPStreamStart(POSRTPStreamRef *stream, POSRTPParameters *rtpParameters,
                           RTPType encodeType, uint32_t clockFrequency, uint32_t localSSRC,
                           HAPTime startTime, char *cnameString,
                           POSSRTPParameters *srtpInParameters,
                           POSSRTPParameters *srtpOutParameters);

typedef uint64_t NTPEpochTime;

NTPEpochTime HAPTimeToNTPTime(HAPTime);

uint64_t Upper64ofMul64(uint64_t a, uint64_t b);

void POSRTPStreamPushPayload(POSRTPStreamRef *stream, void *bytes, size_t numBytes, size_t *numPayloadBytes,
                             HAPTimeNS sampleTime, HAPTime actualTime);

void POSRTPStreamPollPacket
               (POSRTPStreamRef *stream, void *bytes, size_t maxBytes, size_t *numPacketBytes);

uint32_t POSMakeRTPPacket(POSRTPStreamRef *stream,uint8_t *payloadBytes,size_t numDataBytes,uint8_t *packetBytes,

                  size_t maxBytes,size_t *numPacketBytes,uint16_t FUHeader,uint8_t cvoID,
                  uint8_t MarkerBit);

void POSRTPStreamPushPacket(POSRTPStreamRef *stream, void *bytes, size_t numPacketBytes, size_t *numPayloadBytes,
                            HAPTime actualTime);


void POSRTPStreamPollPayload
               (POSRTPStreamRef *stream,void *bytes,size_t maxBytes,size_t *numBytes, HAPTimeNS *sampleTime);

void POSRTPStreamCheckFeedback
               (POSRTPStreamRef *stream,HAPTime actualTime,void *bytes,size_t maxBytes,
               size_t *numBytes,uint32_t *bitRate,bool *newKeyFrame,uint32_t *dropoutTime);




#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
