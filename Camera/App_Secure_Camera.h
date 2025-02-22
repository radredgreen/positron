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

#ifndef APPLICATIONS_CAMERA_APP_SECURE_CAMERA_H_
#define APPLICATIONS_CAMERA_APP_SECURE_CAMERA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "HAP.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

typedef struct {
    int threadPause;
    int threadStop;
    int chn_num;
    pthread_t thread;
} recordingSession;

void checkFormats();
/*
selectedCameraRecordingConfig
    .selectedGeneralConfig
        .prebufferlen
        .trigger
        .mediaContainerConfig
            .containerType
            .mediaContainerParams
                .fragmentLength
    .selectedVideoConfig
        .videoConfig
            .videoCodec
            .videoCodecParams
                .profileID
                .level
                .bitrate
                .iframeinterval
            .videoAttributes
                .width
                .height
                .framerate
    .selectedAudioConfig
        .audioCodec
        .audioCodecParams
            .channels
            .bitrateMode
            .sampleRate
            .maxAudioBitrate 
*/

typedef struct{
    uint32_t fragmentLength;
} mediaContainerParamsStruct;

typedef struct{
    uint8_t containerType;
    mediaContainerParamsStruct mediaContainerParams;
} mediaContainerConfigStruct;

typedef struct{
    uint32_t prebufferlen;
    uint64_t trigger;
    mediaContainerConfigStruct mediaContainerConfig;
} selectedGeneralConfigStruct;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t framerate;
} videoConfigAttributesStruct;

typedef struct {
    uint8_t profileID;
    uint8_t level;
    uint32_t bitrate;
    uint32_t iframeinterval;
} videoRecCodecParamsStruct;

/*
typedef struct {
    uint8_t videoCodec;
    videoRecCodecParamsStruct videoCodecParams;
    videoConfigAttributesStruct videoConfigAttributes;
} videoConfigStruct;

typedef struct {
    videoConfigStruct videoConfig;
} selectedVideoConfigStruct;
*/
typedef struct {
    uint8_t videoCodec;
    videoRecCodecParamsStruct videoCodecParams;
    videoConfigAttributesStruct videoConfigAttributes;
} selectedVideoConfigStruct;

typedef struct {
    uint8_t channels;
    uint8_t bitrateMode;
    uint8_t sampleRate;
    uint32_t maxAudioBitrate;
} audioRecCodecParamsStruct;



/*
typedef struct {
    uint8_t audioCodec;
    audioRecCodecParamsStruct audioCodecParams;
} audioConfigStruct;

typedef struct {
    audioConfigStruct audioConfig;
} selectedAudioConfigStruct;
*/
typedef struct {
    uint8_t audioCodec;
    audioRecCodecParamsStruct audioCodecParams;
} selectedAudioConfigStruct;

typedef struct {
    selectedGeneralConfigStruct selectedGeneralConfig;
    selectedVideoConfigStruct selectedVideoConfig;
    selectedAudioConfigStruct selectedAudioConfig;
    bool configured;
} selectedCameraRecordingConfigStruct;


typedef struct {
    uint32_t port;
} transportParamsStruct;

typedef struct {
    uint8_t status;
    transportParamsStruct transportParams;
    uint8_t accessorySalt [32];
} setupDataStreamTransportReadStruct;

typedef struct {
    uint8_t command;
    uint8_t type;
    uint8_t controllerSalt [32];
} setupDataStreamTransportWriteStruct;

HAP_RESULT_USE_CHECK
HAPError HandleEventSnapActiveRead(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicReadRequest* request,
        bool* value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleEventSnapActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context);
HAP_RESULT_USE_CHECK
HAPError HandlePeriodicSnapActiveRead(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicReadRequest* request,
        bool* value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandlePeriodicSnapActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleHomeKitCamActiveRead(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicReadRequest* request,
        bool* value,
        void* _Nullable context);
HAP_RESULT_USE_CHECK
HAPError HandleHomeKitCamActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context HAP_UNUSED);


HAP_RESULT_USE_CHECK
HAPError HandleSelectedCamRecConfRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSelectedCamRecConfWrite(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicWriteRequest* request,
        HAPTLVReaderRef* requestReader,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleCamRecMgmtActiveRead(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicReadRequest* request,
        HAPCharacteristicValue_Active* value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleCamRecMgmtActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicWriteRequest* request,
        HAPCharacteristicValue_Active value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleCamRecMgmtRecAudioActiveRead(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicReadRequest* request,
        HAPCharacteristicValue_Active* value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleCamRecMgmtRecAudioActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicWriteRequest* request,
        HAPCharacteristicValue_Active value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSetupDSTransportRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSetupDSTransportWrite(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicWriteRequest* request,
        HAPTLVReaderRef* requestReader,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSupportedAudioConfRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSupportedCamRecConfRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSupportedDSTransportConfRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleDSVersionRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPStringCharacteristicReadRequest* request,
        char* value,
        size_t maxValueBytes,
        void* _Nullable context HAP_UNUSED);

HAP_RESULT_USE_CHECK
HAPError HandleSupportedVidRecConfRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleRTPActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPUInt8CharacteristicReadRequest* request HAP_UNUSED,
        HAPCharacteristicValue_Active* value,
        void* _Nullable context HAP_UNUSED);

HAP_RESULT_USE_CHECK
HAPError HandleRTPActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicWriteRequest* request,
        HAPCharacteristicValue_Active value,
        void* _Nullable context HAP_UNUSED);
HAP_RESULT_USE_CHECK
HAPError HandleMotionActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED);

HAP_RESULT_USE_CHECK
HAPError HandleMotionActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context HAP_UNUSED);
HAP_RESULT_USE_CHECK
HAPError HandleNightVisionRead(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicReadRequest* request,
        bool* value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleNightVisionWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context);
HAPError HandleOperatingModeIndicatorRead(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicReadRequest* request,
        bool* value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleOperatingModeIndicatorWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_CAMERA_APP_SECURE_CAMERA_H_ */
