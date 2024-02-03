/*
 * App_Camera.h
 *
 *  Created on: Dec 29, 2020
 *      Author: joe
 */

#ifndef APP_CAMERA_H
#define APP_CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAP.h"
// #include "HAPBase.h"
#include "HAPTLV+Internal.h"

#include <arpa/inet.h>

#include "POSRTPController.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

/**
 * Handle read request to the 'Streaming Status' characteristic of the IP camera service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleStreamingStatusRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

/**
 * Handle read request to the 'Supported Audio' characteristic of the IP camera service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleSupportedAudioRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

/**
 * Handle read request to the 'Supported Video' characteristic of the IP camera service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleSupportedVideoRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

/**
 * Handle read request to the 'Supported Video' characteristic of the IP camera service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleSupportedRTPConfigRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

/**
 * Handle read request to the 'Selected RTP' characteristic of the IP camera service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleSelectedRTPConfigRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

/**
 * Handle write request to the 'Selected RTP' characteristic of the IP camera service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleSelectedRTPConfigWrite(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicWriteRequest* request,
        HAPTLVReaderRef* requestReader,
        void* _Nullable context);

/**
 * Handle read request to the 'Setup Endpoints' characteristic of the IP camera service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleSetupEndpointsRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

/**
 * Handle write request to the 'Setup Endpoints' characteristic of the IP camera service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleSetupEndpointsWrite(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicWriteRequest* request,
        HAPTLVReaderRef* requestReader,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleMicMuteRead(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicReadRequest* request,
        bool* value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleMicMuteWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleMotionDetectedRead(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicReadRequest* request,
        bool* value,
        void* _Nullable context);
/*
 * -------------------------------------------------------
 */

/**
 * @file streaming.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2021-01-20
 *
 * @copyright Copyright (c) 2021
 *
 */

// #ifndef STREAMING_H
// #define STREAMING_H

// #ifdef __cplusplus
// extern "C" {
// #endif

// #include "HAP.h"
// #include "HAPTLV+Internal.h"
// #include <arpa/inet.h>

// #if __has_feature(nullability)
// #pragma clang assume_nonnull begin
// #endif

#define KEYLENGTH  32
#define SALTLENGTH 14
#define UUIDLENGTH 16

typedef struct {
    uint8_t audioChannels;
    uint8_t bitRate;
    uint8_t sampleRate;
    uint8_t rtpTime;
} audioCodecParamsStruct;

typedef struct {
    uint16_t audioCodecType;
    audioCodecParamsStruct audioCodecParams;
} audioCodecConfigStruct;

typedef struct {
    audioCodecConfigStruct audioCodecConfig;
    uint8_t comfortNoiseSupport;
} supportedAudioConfigStruct;

typedef struct {
    uint8_t profileID;
    uint8_t level;
    uint8_t packetizationMode;
    uint8_t CVOEnabled;
} videoCodecParamsStruct;

typedef struct {
    uint16_t imageWidth;
    uint16_t imageHeight;
    uint8_t frameRate;
} videoAttributesStruct;

typedef struct {
    uint8_t videoCodecType;
    videoCodecParamsStruct videoCodecParams;
    videoAttributesStruct videoAttributes;

} videoCodecConfigStruct;

typedef struct {
    videoCodecConfigStruct videoCodecConfig;
} supportedVideoConfigStruct;

HAP_ENUM_BEGIN(uint8_t, HAPCharacteristicValue_RTPCommand) { /** Inactive. */
                                                             kHAPCharacteristicValue_RTPCommand_End = 0,

                                                             /** Active. */
                                                             kHAPCharacteristicValue_RTPCommand_Start,

                                                             /** Active. */
                                                             kHAPCharacteristicValue_RTPCommand_Suspend,

                                                             /** Active. */
                                                             kHAPCharacteristicValue_RTPCommand_Resume,

                                                             /** Active. */
                                                             kHAPCharacteristicValue_RTPCommand_Reconfigure
} HAP_ENUM_END(uint8_t, HAPCharacteristicValue_RTPCommand);

typedef struct {
    uint8_t payloadType;      // type of video codec
    uint32_t ssrc;            // ssrc for video stream
    uint16_t maximumBitrate;  // in kbps and averaged over 1 sec
    uint32_t minRTCPinterval; // Minimum RTCP interval in seconds formatted as a 4 byte little endian ieee754 floating
                              // point value
} rtpParameters;

typedef struct {
    rtpParameters vRtpParameters;
    uint16_t maxMTU; // MTU accessory must use to transmit video RTP packets  Only populated for non-default?? value.
} videoRtpParameters;

typedef struct {
    rtpParameters rtpParameters;
    uint8_t comfortNoisePayload; // Only required when Comfort Noise is chosen in selected audio parameters TLV
} audioRtpParameters;

typedef struct {
    // Selected video codec type?
    videoCodecConfigStruct codecConfig;  //codec parameters?
    // Selected video attributes?
    videoRtpParameters vRtpParameters;
} selectedVideoParameters;

typedef struct {
    audioCodecConfigStruct codecConfig;
    audioRtpParameters rtpParameters;
    uint8_t comfortNoise; // 1 = Comfort Noise selected
} selectedAudioParameters;

typedef struct {
    HAPIPAddressVersion ipAddrVersion; // Tried to use HAPIPAddressVersion but it says v4 = 1 but docs say 0.
    char ipAddress[INET6_ADDRSTRLEN];
    HAPNetworkPort videoPort;
    HAPNetworkPort audioPort;
} controllerAddressStruct;

typedef struct {
    uint8_t srtpMasterKey[KEYLENGTH]; // 16 for 128, 32 for 256 crypto suite
    uint8_t srtpMasterSalt[SALTLENGTH];
    HAPCharacteristicValue_SupportedRTPConfiguration srtpCryptoSuite;
} srtpParameters;

typedef struct {
    int socket;
    int threadPause;
    int threadStop;
    int chn_num;
    pthread_t thread;
} POSStreamingThread;

typedef struct {
    uint8_t sessionId[UUIDLENGTH];
    uint32_t setupWriteStatus;
    HAPCharacteristicValue_StreamingStatus status;
    controllerAddressStruct controllerAddress;
    srtpParameters videoParams;
    srtpParameters audioParams;
    controllerAddressStruct accessoryAddress;
    uint32_t ssrcVideo;
    uint32_t ssrcAudio;
    selectedVideoParameters videoParameters;
    selectedAudioParameters audioParameters;
    POSRTPStreamRef rtpVideoStream;
    POSRTPStreamRef rtpAudioStream;
    POSStreamingThread videoThread; // video out
    POSStreamingThread videoFeedbackThread; // video rtcp in
    POSStreamingThread audioThread; // audio out
    POSStreamingThread audioFeedbackThread; // audio rtcp in, audio in
} streamingSession;

typedef struct {
    uint8_t sessionId[UUIDLENGTH];
    HAPCharacteristicValue_RTPCommand command;
} sessionControl;

typedef struct {
    sessionControl control;
    selectedVideoParameters videoParameters;
    selectedAudioParameters audioParameters;
} selectedRTPStruct;

// HAPError handleSessionRead(HAPTLVWriterRef* responseWriter, streamingSession* session);

// HAPError handleSessionWrite(HAPTLVReaderRef* responseReader, streamingSession* session);

HAPError handleSelectedWrite(HAPTLVReaderRef* responseReader, selectedRTPStruct* selectedRTP);

void checkFormats();

HAPError handleAudioRead(HAPTLVWriterRef* responseWriter);
HAPError handleVideoRead(HAPTLVWriterRef* responseWriter);

// #if __has_feature(nullability)
// #pragma clang assume_nonnull end
// #endif

// #ifdef __cplusplus
// }
// #endif

// #endif /* STREAMING_H */

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif /* APP_CAMERA_H */
