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
#include "HAPTLV+Internal.h"
#include "HAPSession.h"

#include "App.h"
#include "App_Secure_Camera.h"
#include "DB.h"
#include "POSCameraController.h"
#include "POSDataStream.h"
#include <stdio.h>
#include "ingenicVideoPipeline.h"


// TODO: several structures in here end with a capital S, this was to deconflict them with the same name structures in App_Camera.c
// for some reason linking failed with multiple definitions
// same for isValidS


bool isValidS(void* unsused HAP_UNUSED) {
    return true;
}

extern AccessoryConfiguration accessoryConfiguration;

HAP_RESULT_USE_CHECK
HAPError HandleEventSnapActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.operatingMode.eventSnapshots;
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, *value);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleEventSnapActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, value);
    if (accessoryConfiguration.state.operatingMode.eventSnapshots != value) {
        accessoryConfiguration.state.operatingMode.eventSnapshots = value;

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandlePeriodicSnapActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.operatingMode.eventSnapshots;
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, *value);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandlePeriodicSnapActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, value);
    if (accessoryConfiguration.state.operatingMode.eventSnapshots != value) {
        accessoryConfiguration.state.operatingMode.eventSnapshots = value;

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }
    return kHAPError_None;
}


HAP_RESULT_USE_CHECK
HAPError HandleHomeKitCamActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.motion.active;
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, *value);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleHomeKitCamActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, value);
    if (accessoryConfiguration.state.motion.active != value) {
        accessoryConfiguration.state.motion.active = value;

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }
    return kHAPError_None;
}



/* ---------------------------------------------------------------------------------------------*/



const HAPUInt32TLVFormat fragmentLengthFormat = { .type = kHAPTLVFormatType_UInt32,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 8000 },
                                                          .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember fragmentLengthMember = { .valueOffset = HAP_OFFSETOF(mediaContainerParamsStruct, fragmentLength),
                                                           .isSetOffset = 0,
                                                           .tlvType = 1,
                                                           .debugDescription = "Media Container Fragment Length",
                                                           .format = &fragmentLengthFormat,
                                                           .isOptional = false, 
                                                           .isFlat = false };


HAP_STRUCT_TLV_SUPPORT(void, MediaContainerParamsFormat);
const MediaContainerParamsFormat mediaContainerParamsFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &fragmentLengthMember,
                                                     NULL },
    .callbacks = { .isValid = isValidS }
};

const HAPStructTLVMember mediaContainerParamsMember = { .valueOffset =
                                                            HAP_OFFSETOF(mediaContainerConfigStruct, mediaContainerParams),
                                                    .isSetOffset = 0,
                                                    .tlvType = 2,
                                                    .debugDescription = "Media Container Params",
                                                    .format = &mediaContainerParamsFormat,
                                                    .isOptional = false,
                                                    .isFlat = false };

const HAPUInt8TLVFormat mediaContainerTypeFormat = { .type = kHAPTLVFormatType_UInt8,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 0 },
                                                          .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember mediaContainerTypeMember = { .valueOffset = HAP_OFFSETOF(mediaContainerConfigStruct, containerType),
                                                           .isSetOffset = 0,
                                                           .tlvType = 1,
                                                           .debugDescription = "Media Container Type",
                                                           .format = &mediaContainerTypeFormat,
                                                           .isOptional = false, 
                                                           .isFlat = false };


HAP_STRUCT_TLV_SUPPORT(void, MediaContainerFormat);
const MediaContainerFormat mediaContainerFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &mediaContainerTypeMember,
                                                     &mediaContainerParamsMember,
                                                     NULL },
    .callbacks = { .isValid = isValidS }
};

const HAPStructTLVMember mediaContainerMember = { .valueOffset =
                                                            HAP_OFFSETOF(selectedGeneralConfigStruct, mediaContainerConfig),
                                                    .isSetOffset = 0,
                                                    .tlvType = 3,
                                                    .debugDescription = "Media Container Configs",
                                                    .format = &mediaContainerFormat,
                                                    .isOptional = false,
                                                    .isFlat = false };

const HAPUInt64TLVFormat triggerFormat = { .type = kHAPTLVFormatType_UInt64,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 3 },
                                                          .callbacks = { .getDescription = NULL } };


const HAPStructTLVMember triggerMember = { .valueOffset = HAP_OFFSETOF(selectedGeneralConfigStruct, trigger),
                                                           .isSetOffset = 0,
                                                           .tlvType = 2,
                                                           .debugDescription = "Video Trigger Bitfield",
                                                           .format = &triggerFormat,
                                                           .isOptional = false, 
                                                           .isFlat = false };

const HAPUInt32TLVFormat prebufferLenFormat = { .type = kHAPTLVFormatType_UInt32,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 8000 },
                                                          .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember prebufferLenMember = { .valueOffset = HAP_OFFSETOF(selectedGeneralConfigStruct, prebufferlen),
                                                           .isSetOffset = 0,
                                                           .tlvType = 1,
                                                           .debugDescription = "Video Prebuffer Length",
                                                           .format = &prebufferLenFormat,
                                                           .isOptional = false, 
                                                           .isFlat = false };

HAP_STRUCT_TLV_SUPPORT(void, SelectedGeneralConfigFormat);
const SelectedGeneralConfigFormat selectedGeneralConfigFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members =
            (const HAPStructTLVMember* const[]) { 
                                                  &prebufferLenMember,
                                                  &triggerMember,
                                                  &mediaContainerMember,
                                                  NULL },
    .callbacks = { .isValid = isValidS }
};

const HAPStructTLVMember selectedGeneralConfigMember = { .valueOffset =
                                                                      HAP_OFFSETOF(selectedCameraRecordingConfigStruct, selectedGeneralConfig),
                                                              .isSetOffset = 0,
                                                              .tlvType = 1,
                                                              .debugDescription = "Selected General Configuration Type",
                                                              .format = &selectedGeneralConfigFormat,
                                                              .isOptional = false,
                                                              .isFlat = false };


const HAPUInt16TLVFormat videoAttrImageWidthFormat = { .type = kHAPTLVFormatType_UInt16,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 0xFFFF },
                                                          .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember videoAttrImageWidthMember = { .valueOffset = HAP_OFFSETOF(videoConfigAttributesStruct, width),
                                                           .isSetOffset = 0,
                                                           .tlvType = 1,
                                                           .debugDescription = "Video Codec Param Image Width",
                                                           .format = &videoAttrImageWidthFormat,
                                                           .isOptional = false, 
                                                           .isFlat = false };


const HAPUInt16TLVFormat videoAttrImageHeightFormat = { .type = kHAPTLVFormatType_UInt16,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 0xFFFF },
                                                          .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember videoAttrImageHeightMember = { .valueOffset = HAP_OFFSETOF(videoConfigAttributesStruct, height),
                                                           .isSetOffset = 0,
                                                           .tlvType = 2,
                                                           .debugDescription = "Video Codec Param Image height",
                                                           .format = &videoAttrImageHeightFormat,
                                                           .isOptional = false, 
                                                           .isFlat = false };


const HAPUInt8TLVFormat videoAttrFrameRateFormat = { .type = kHAPTLVFormatType_UInt8,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 0xFF },
                                                          .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember videoAttrFrameRateMember = { .valueOffset = HAP_OFFSETOF(videoConfigAttributesStruct, framerate),
                                                           .isSetOffset = 0,
                                                           .tlvType = 3,
                                                           .debugDescription = "Video Codec Param Frame Rate",
                                                           .format = &videoAttrFrameRateFormat,
                                                           .isOptional = false, 
                                                           .isFlat = false };



HAP_STRUCT_TLV_SUPPORT(void, VideoAttributesFormatS);
const VideoAttributesFormatS videoAttributesFormatS = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &videoAttrImageWidthMember,
                                                     &videoAttrImageHeightMember,
                                                     &videoAttrFrameRateMember,
                                                     NULL },
    .callbacks = { .isValid = isValidS }
};

const HAPStructTLVMember videoAttributesMemberS = { .valueOffset =
                                                            HAP_OFFSETOF(selectedVideoConfigStruct, videoConfigAttributes),
                                                    .isSetOffset = 0,
                                                    .tlvType = 3,
                                                    .debugDescription = "Video Codec Attributes",
                                                    .format = &videoAttributesFormatS,
                                                    .isOptional = false,
                                                    .isFlat = false };


const HAPUInt8TLVFormat videoCodecParamsProfileIDFormatS = { .type = kHAPTLVFormatType_UInt8,
                                                             .constraints = { .minimumValue = 0, .maximumValue = 2 },
                                                             .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember videoCodecParamsProfileIDMemberS = { .valueOffset =
                                                                      HAP_OFFSETOF(videoRecCodecParamsStruct, profileID),
                                                              .isSetOffset = 0,
                                                              .tlvType = 1,
                                                              .debugDescription =
                                                                      "Video Codec Config Params Prifile ID",
                                                              .format = &videoCodecParamsProfileIDFormatS,
                                                              .isOptional = false,
                                                              .isFlat = false };


const HAPUInt8TLVFormat videoCodecParamsLevelFormatSS = { .type = kHAPTLVFormatType_UInt8,
                                                             .constraints = { .minimumValue = 0, .maximumValue = 2 },
                                                             .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember videoCodecParamsLevelMemberS = { .valueOffset =
                                                                      HAP_OFFSETOF(videoRecCodecParamsStruct, level),
                                                              .isSetOffset = 0,
                                                              .tlvType = 2,
                                                              .debugDescription =
                                                                      "Video Codec Config Params Level",
                                                              .format = &videoCodecParamsLevelFormatSS,
                                                              .isOptional = false,
                                                              .isFlat = false };


const HAPUInt32TLVFormat videoCodecParamsBitrateFormat = { .type = kHAPTLVFormatType_UInt32,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 0xFFFFFFFF },
                                                          .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember videoCodecParamsBitrateMember = { .valueOffset = HAP_OFFSETOF(videoRecCodecParamsStruct, bitrate),
                                                           .isSetOffset = 0,
                                                           .tlvType = 3,
                                                           .debugDescription = "Video Codec Param Bitrate",
                                                           .format = &videoCodecParamsBitrateFormat,
                                                           .isOptional = false, 
                                                           .isFlat = false };


const HAPUInt32TLVFormat videoCodecParamsIFrameIntervalFormat = { .type = kHAPTLVFormatType_UInt32,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 8000 },
                                                          .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember videoCodecParamsIFrameIntervalMember = { .valueOffset = HAP_OFFSETOF(videoRecCodecParamsStruct, iframeinterval),
                                                           .isSetOffset = 0,
                                                           .tlvType = 4,
                                                           .debugDescription = "Video Codec Param I Frame Interval",
                                                           .format = &videoCodecParamsIFrameIntervalFormat,
                                                           .isOptional = false, 
                                                           .isFlat = false };

HAP_STRUCT_TLV_SUPPORT(void, VideoCodecConfigFormat);
const VideoCodecConfigFormat videoCodecParamsFormatS = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &videoCodecParamsProfileIDMemberS,
                                                     &videoCodecParamsLevelMemberS,
                                                     &videoCodecParamsBitrateMember,
                                                     &videoCodecParamsIFrameIntervalMember,
                                                     NULL },
    .callbacks = { .isValid = isValidS }
};

const HAPStructTLVMember videoCodecParamsMemberS = { .valueOffset =
                                                            HAP_OFFSETOF(selectedVideoConfigStruct, videoCodecParams),
                                                    .isSetOffset = 0,
                                                    .tlvType = 2,
                                                    .debugDescription = "Video Codec Parameters",
                                                    .format = &videoCodecParamsFormatS,
                                                    .isOptional = false,
                                                    .isFlat = false };


const HAPUInt8TLVFormat videoCodecFormat = { .type = kHAPTLVFormatType_UInt8,
                                                             .constraints = { .minimumValue = 0, .maximumValue = 2 },
                                                             .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember videoCodecMember = { .valueOffset =
                                                                      HAP_OFFSETOF(selectedVideoConfigStruct, videoCodec),
                                                              .isSetOffset = 0,
                                                              .tlvType = 1,
                                                              .debugDescription =
                                                                      "Video Codec",
                                                              .format = &videoCodecFormat,
                                                              .isOptional = false,
                                                              .isFlat = false };



HAP_STRUCT_TLV_SUPPORT(void, SelectedVideoCodecConfigFormat);
const SelectedVideoCodecConfigFormat selectedVideoCodecConfigFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members =
            (const HAPStructTLVMember* const[]) { 
                                                  &videoCodecMember,
                                                  &videoCodecParamsMemberS,
                                                  &videoAttributesMemberS,
                                                  NULL },
    .callbacks = { .isValid = isValidS }
};


const HAPStructTLVMember selectedVideoConfigMember = { .valueOffset =
                                                                      HAP_OFFSETOF(selectedCameraRecordingConfigStruct, selectedVideoConfig),
                                                              .isSetOffset = 0,
                                                              .tlvType = 2,
                                                              .debugDescription = "Selected Video Recording Configuration Type",
                                                              .format = &selectedVideoCodecConfigFormat,
                                                              .isOptional = false,
                                                              .isFlat = false };


const HAPUInt8TLVFormat audioCodecParamsAudioChannelsFormatS = { .type = kHAPTLVFormatType_UInt8,
                                                                .constraints = { .minimumValue = 0, .maximumValue = 2 },
                                                                .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember audioCodecParamsAudioChannelsMemberS = {
    .valueOffset = HAP_OFFSETOF(audioRecCodecParamsStruct, channels),
    .isSetOffset = 0,
    .tlvType = 1,
    .debugDescription = "Audio Codec Config Params Audio Channels",
    .format = &audioCodecParamsAudioChannelsFormatS,
    .isOptional = false,
    .isFlat = false
};


const HAPUInt8TLVFormat audioCodecParamsBitRateFormatS = { .type = kHAPTLVFormatType_UInt8,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 1 },
                                                          .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember audioCodecParamsBitRateMemberS = { .valueOffset = HAP_OFFSETOF(audioRecCodecParamsStruct,  bitrateMode),
                                                           .isSetOffset = 0,
                                                           .tlvType = 2,
                                                           .debugDescription = "Audio Codec Config Params Bit Rate",
                                                           .format = &audioCodecParamsBitRateFormatS,
                                                           .isOptional = false,
                                                           .isFlat = false };


const HAPUInt8TLVFormat audioCodecParamsSampleRateFormatS = { .type = kHAPTLVFormatType_UInt8,
                                                             .constraints = { .minimumValue = 0, .maximumValue = 5 },
                                                             .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember audioCodecParamsSampleRateMemberS = { .valueOffset =
                                                                      HAP_OFFSETOF(audioRecCodecParamsStruct, sampleRate),
                                                              .isSetOffset = 0,
                                                              .tlvType = 3,
                                                              .debugDescription =
                                                                      "Audio Codec Config Params Sample Rate",
                                                              .format = &audioCodecParamsSampleRateFormatS,
                                                              .isOptional = false,
                                                              .isFlat = false };


const HAPUInt32TLVFormat audioCodecParamsMaxBitrateFormat = { .type = kHAPTLVFormatType_UInt32,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 0xFFFFFFFF },
                                                          .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember audioCodecParamsMaxBitrateMember = { .valueOffset = HAP_OFFSETOF(audioRecCodecParamsStruct, maxAudioBitrate),
                                                           .isSetOffset = 0,
                                                           .tlvType = 4,
                                                           .debugDescription = "Audio Codec Param Max Bitrate",
                                                           .format = &audioCodecParamsMaxBitrateFormat,
                                                           .isOptional = false, 
                                                           .isFlat = false };


HAP_STRUCT_TLV_SUPPORT(void, AudioCodecConfigFormat);
const AudioCodecConfigFormat audioCodecParamsFormatS = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &audioCodecParamsAudioChannelsMemberS,
                                                     &audioCodecParamsBitRateMemberS,
                                                     &audioCodecParamsSampleRateMemberS,
                                                     &audioCodecParamsMaxBitrateMember,
                                                     NULL },
    .callbacks = { .isValid = isValidS }
};

const HAPStructTLVMember audioCodecParamsMemberS = { .valueOffset =
                                                            HAP_OFFSETOF(selectedAudioConfigStruct, audioCodecParams),
                                                    .isSetOffset = 0,
                                                    .tlvType = 2,
                                                    .debugDescription = "Audio Codec Parameters",
                                                    .format = &audioCodecParamsFormatS,
                                                    .isOptional = false,
                                                    .isFlat = false };


const HAPUInt8TLVFormat audioCodecFormat = { .type = kHAPTLVFormatType_UInt8,
                                                                .constraints = { .minimumValue = 0, .maximumValue = 1 },
                                                                .callbacks = { .getDescription = NULL } };


const HAPStructTLVMember audioCodecMember = {
    .valueOffset = HAP_OFFSETOF(selectedAudioConfigStruct, audioCodec),
    .isSetOffset = 0,
    .tlvType = 1,
    .debugDescription = "Audio Codec Config Params Audio Channels",
    .format = &audioCodecFormat,
    .isOptional = false,
    .isFlat = false
};

HAP_STRUCT_TLV_SUPPORT(void, SelectedAudioCodecConfigFormat);
const SelectedAudioCodecConfigFormat selectedAudioCodecConfigFormat = {
        .type = kHAPTLVFormatType_Struct,
    .members =
            (const HAPStructTLVMember* const[]) { 
                                                  &audioCodecMember,
                                                  &audioCodecParamsMemberS,
                                                  NULL },
    .callbacks = { .isValid = isValidS }
};


const HAPStructTLVMember selectedAudioConfigMember = { .valueOffset =
                                                                      HAP_OFFSETOF(selectedCameraRecordingConfigStruct, selectedAudioConfig),
                                                              .isSetOffset = 0,
                                                              .tlvType = 3,
                                                              .debugDescription = "Selected Audio Recording Configuration Type",
                                                              .format = &selectedAudioCodecConfigFormat,
                                                              .isOptional = false,
                                                              .isFlat = false };


HAP_STRUCT_TLV_SUPPORT(void, SelectedCameraRecordingConfigFormatWrite);
const SelectedCameraRecordingConfigFormatWrite selectedCameraRecordingConfigFormatWrite = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &selectedGeneralConfigMember,
                                                     &selectedVideoConfigMember,
                                                     &selectedAudioConfigMember,
                                                     NULL },
    .callbacks = { .isValid = isValidS }
};

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
            .videoConfigParams
                .profileID
                .level
                .bitrate
                .iframeinterval
            .videoAttributes
                .width
                .height
                .framerate
    .selectedAudioConfig
        .audioConfig
            .audioCodec
            .codecParams
                .channels
                .bitratemode
                .sampleRate
                .maxAudioBitrate 
*/



selectedCameraRecordingConfigStruct selectedCameraRecordingConfig =
{
    .selectedGeneralConfig =
    {
        .prebufferlen = 4000,
        .trigger = 1,
        .mediaContainerConfig = 
        {
            .containerType = 0,
            .mediaContainerParams =
            {
                .fragmentLength = 4000
            }
        }
    },
    .selectedVideoConfig = 
    {
        .videoCodecParams = {
            .profileID = 1,
            .level = 1,
            .bitrate = 2000,
            .iframeinterval = 4000
        },
        .videoConfigAttributes = {
            .width = 1920,
            .height = 1080,
            .framerate = 24
        }
    },
    .selectedAudioConfig = {
        .audioCodec = 0, // change this to 1 = aac-eld
        .audioCodecParams =
        {
            .channels = 1,  // 1 channel
            .bitrateMode = 0,        // Variable
            .sampleRate = 1,      // 16kHz 8 not supported on MBP
            .maxAudioBitrate = 32
        }
    },
    .configured = 0
};


//  011d  selected general
//      0104 prebuffer
//          a00f0000 4000
//      0208 event trigger
//          0100000000000000 motion
//      030b media container configs
//          0101 container type
//              00 fragmented mp4
//          0206 container parameters
//              0104 fragment length
//                  a00f0000 4000
//  0224 selected video config
//      0101 codec
//          00 h264
//      0212 video codec params
//          0101 profile 
//              01 main
//          0201 level
//              01 3.2
//          0304 bitrate
//              d0070000 2000
//          0404 iframe interval
//              a00f0000 4000
//      030b video attributes
//          0102 width
//              8007 1920
//          0202 height
//              3804 1080
//          0301 framerate
//              18 24
//  0314 selected audio
//          0101 codec
//              01 aac-eld
//          020f audio codec params
//              0101 channels
//                  01 1
//              0201 bitrate modes
//                  00 variable
//              0301 sample rates
//                  01 16khz
//              0404 max bitrate
//                  20000000 32



HAP_RESULT_USE_CHECK
HAPError HandleSelectedCamRecConfRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(responseWriter);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);


    if (selectedCameraRecordingConfig.configured)
        return HAPTLVWriterEncode(responseWriter, &selectedCameraRecordingConfigFormatWrite, &selectedCameraRecordingConfig);
    else
        return kHAPError_None;

}

HAP_RESULT_USE_CHECK
HAPError HandleSelectedCamRecConfWrite(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicWriteRequest* request HAP_UNUSED,
        HAPTLVReaderRef* requestReader,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(requestReader);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    selectedCameraRecordingConfig.configured = true;
    HAPError err;
    err = HAPTLVReaderDecode(requestReader, 
        &selectedCameraRecordingConfigFormatWrite, 
        &selectedCameraRecordingConfig);

    
    // configure the recording stream here
    //
    // the recording stream is already running because we tap off the recording video pipeline for motion detect and screenshots
    // the only task here is to reconfigure the already running stream


    if (err = kHAPError_None)
        selectedCameraRecordingConfig.configured = true;

    return err;
}


HAP_RESULT_USE_CHECK
HAPError HandleCamRecMgmtActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPUInt8CharacteristicReadRequest* request HAP_UNUSED,
        HAPCharacteristicValue_Active* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.operatingMode.recordingActive;
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, *value);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleCamRecMgmtActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicWriteRequest* request,
        HAPCharacteristicValue_Active value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, value);
    if (accessoryConfiguration.state.operatingMode.recordingActive != value) {
        accessoryConfiguration.state.operatingMode.recordingActive = value;

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }
    return kHAPError_None;
}
HAP_RESULT_USE_CHECK
HAPError HandleCamRecMgmtRecAudioActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPUInt8CharacteristicReadRequest* request HAP_UNUSED,
        HAPCharacteristicValue_Active* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.operatingMode.recordingAudioActive;
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, *value);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleCamRecMgmtRecAudioActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicWriteRequest* request,
        HAPCharacteristicValue_Active value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, value);
    if (accessoryConfiguration.state.operatingMode.recordingAudioActive != value) {
        accessoryConfiguration.state.operatingMode.recordingAudioActive = value;

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }
    return kHAPError_None;
}


setupDataStreamTransportReadStruct setupDataStreamTransportRead =
{
    .status = 0,
    .transportParams = {
        .port = 1010 // to be populated later
    }
    //.accessorySalt =
};


const HAPUInt16TLVFormat setupDSPortFormat = { .type = kHAPTLVFormatType_UInt16,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 0xFFFF },
                                                          .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember setupDSPortMember = { .valueOffset = HAP_OFFSETOF(transportParamsStruct, port),
                                                           .isSetOffset = 0,
                                                           .tlvType = 1,
                                                           .debugDescription = "Data Stream Transport Port",
                                                           .format = &setupDSPortFormat,
                                                           .isOptional = false, 
                                                           .isFlat = false };


HAP_STRUCT_TLV_SUPPORT(void, SetupDSParamsFormat);
const SetupDSParamsFormat setupDSParamsFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &setupDSPortMember,
                                                     NULL },
    .callbacks = { .isValid = isValidS }
};

const HAPStructTLVMember setupDSParamsMember = { .valueOffset =
                                                                      HAP_OFFSETOF(setupDataStreamTransportReadStruct, transportParams),
                                                              .isSetOffset = 0,
                                                              .tlvType = 2,
                                                              .debugDescription =
                                                                      "Setup Data Stream Transport Params Read",
                                                              .format = &setupDSParamsFormat,
                                                              .isOptional = false,
                                                              .isFlat = false };

const HAPUInt8TLVFormat setupDSStatusFormat = { .type = kHAPTLVFormatType_UInt8,
                                                             .constraints = { .minimumValue = 0, .maximumValue = 2 },
                                                             .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember setupDSStatusMember = { .valueOffset =
                                                                      HAP_OFFSETOF(setupDataStreamTransportReadStruct, status),
                                                              .isSetOffset = 0,
                                                              .tlvType = 1,
                                                              .debugDescription =
                                                                      "Setup Data Stream Status Read",
                                                              .format = &setupDSStatusFormat,
                                                              .isOptional = false,
                                                              .isFlat = false };

HAPError decodeUInt8DSACCSaltValue(uint8_t* value, void* bytes, size_t numBytes) {
    HAPRawBufferCopyBytes((uint8_t*) value, (uint8_t*) bytes, DSACCSALTLENGTH);
    return kHAPError_None;
};
HAPError encodeUInt8DSACCSaltValue(uint8_t* value, void* bytes, size_t maxBytes HAP_UNUSED, size_t* numBytes) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    HAPRawBufferCopyBytes((uint8_t*) bytes, (uint8_t*) value, DSACCSALTLENGTH);
    *numBytes = DSACCSALTLENGTH;
    return kHAPError_None;
}
HAPError getAccSaltValueDescription(uint8_t* value HAP_UNUSED, char* bytes, size_t maxBytes HAP_UNUSED) {
    char str64[128];
    size_t encLen64;
//    util_base64_encode(value, DSACCSALTLENGTH, str64, 128, &encLen64);
//    str64[encLen64]='\0';
    for(uint8_t i = 0 ; i < DSACCSALTLENGTH ; i++){
        snprintf(bytes+3*i, maxBytes - 3*i, " %02x", *(value+i));
    }
    //printf("\n\nSalt: %s\n\n", str64);
    //    *numBytes = encLen64+1;
//    HAPRawBufferCopyBytes(bytes, str64, encLen64+1);

    return kHAPError_None;
};

HAP_VALUE_TLV_SUPPORT(uint8_t, SetupDSAccessorySaltFormat);
const SetupDSAccessorySaltFormat setupDSAccessorySaltFormat = { .type = kHAPTLVFormatType_Value,
                                                            .callbacks = { .decode = decodeUInt8DSACCSaltValue,
                                                                           .encode = encodeUInt8DSACCSaltValue,
                                                                           .getDescription = getAccSaltValueDescription} };

const HAPStructTLVMember setupDSAccessorySaltMember = { .valueOffset = HAP_OFFSETOF(setupDataStreamTransportReadStruct, accessorySalt),
                                                     .isSetOffset = 0,
                                                     .tlvType = 3,
                                                     .debugDescription =  "Data Stream Accessory Salt" ,
                                                     .format = &setupDSAccessorySaltFormat,
                                                     .isOptional = false,
                                                     .isFlat = false };


HAP_STRUCT_TLV_SUPPORT(void, SetupDSTransportFormatRead);
const SetupDSTransportFormatRead setupDSTransportFormatRead = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &setupDSStatusMember,
                                                     &setupDSParamsMember,
                                                     &setupDSAccessorySaltMember,
                                                     NULL },
    .callbacks = { .isValid = isValidS }
};

HAP_RESULT_USE_CHECK
HAPError HandleSetupDSTransportRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {

    HAPError err;
    HAPPrecondition(responseWriter);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    err =  HAPTLVWriterEncode(responseWriter, &setupDSTransportFormatRead, &setupDataStreamTransportRead);

    //change the salt after every read
    HAPPlatformRandomNumberFill(&setupDataStreamTransportRead.accessorySalt,32);
    return err;
}



//setupDataStreamTransportWriteStruct setupDataStreamTransportWrite =
//{
    // .command = 0,
    // .type = 0,
    // .controllerSalt =
//};


const HAPUInt8TLVFormat setupDSCommandFormat = { .type = kHAPTLVFormatType_UInt8,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 0xFF },
                                                          .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember setupDSCommandMember = { .valueOffset = HAP_OFFSETOF(setupDataStreamTransportWriteStruct, command),
                                                           .isSetOffset = 0,
                                                           .tlvType = 1,
                                                           .debugDescription = "Data Stream Session Command",
                                                           .format = &setupDSCommandFormat,
                                                           .isOptional = false, 
                                                           .isFlat = false };


const HAPUInt8TLVFormat setupDSTransportTypeFormat = { .type = kHAPTLVFormatType_UInt8,
                                                             .constraints = { .minimumValue = 0, .maximumValue = 5 },
                                                             .callbacks = { .getDescription = NULL } };

const HAPStructTLVMember setupDSTransportTypeMember = { .valueOffset =
                                                                      HAP_OFFSETOF(setupDataStreamTransportWriteStruct, type),
                                                              .isSetOffset = 0,
                                                              .tlvType = 2,
                                                              .debugDescription =
                                                                      "Setup Data Stream Transport Type",
                                                              .format = &setupDSTransportTypeFormat,
                                                              .isOptional = false,
                                                              .isFlat = false };

HAP_RESULT_USE_CHECK
HAPError decodeUInt8DSCONSaltValue(uint8_t* value, void* bytes, size_t numBytes) {
    HAPRawBufferCopyBytes((uint8_t*) value, (uint8_t*) bytes, DSCONSALTLENGTH);
    return kHAPError_None;
};
HAPError encodeUInt8DSCONSaltValue(uint8_t* value, void* bytes, size_t maxBytes HAP_UNUSED, size_t* numBytes) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    HAPRawBufferCopyBytes((uint8_t*) bytes, (uint8_t*) value, DSACCSALTLENGTH);
    *numBytes = SALTLENGTH;
    return kHAPError_None;
}
HAPError getConSaltValueDescription(uint8_t* value HAP_UNUSED, char* bytes, size_t maxBytes HAP_UNUSED) {
/*    char str64[128];
    size_t encLen64;
    util_base64_encode(value, DSCONSALTLENGTH, str64, 128, &encLen64);
    str64[encLen64]='\0';
    //printf("\n\nSalt: %s\n\n", str64);
    //    *numBytes = encLen64+1;
    HAPRawBufferCopyBytes(bytes, str64, encLen64+1);
*/
    for(uint8_t i = 0 ; i < DSCONSALTLENGTH ; i++){
        snprintf(bytes+3*i, maxBytes - 3*i, " %02x", *(value+i));
    }

    return kHAPError_None;
};


HAP_VALUE_TLV_SUPPORT(uint8_t, SetupDSControllerSaltFormat);
const SetupDSControllerSaltFormat setupDSControllerSaltFormat = { .type = kHAPTLVFormatType_Value,
                                                            .callbacks = { .decode = decodeUInt8DSCONSaltValue,
                                                                           .encode = encodeUInt8DSCONSaltValue,
                                                                           .getDescription = getConSaltValueDescription} };

const HAPStructTLVMember setupDSControllerSaltMember = { .valueOffset = HAP_OFFSETOF(setupDataStreamTransportWriteStruct, controllerSalt),
                                                     .isSetOffset = 0,
                                                     .tlvType = 3,
                                                     .debugDescription =  "Data Stream Controller Salt" ,
                                                     .format = &setupDSControllerSaltFormat,
                                                     .isOptional = false,
                                                     .isFlat = false };


HAP_STRUCT_TLV_SUPPORT(void, SetupDSTransportFormatWrite);
const SetupDSTransportFormatWrite setupDSTransportFormatWrite = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &setupDSCommandMember,
                                                     &setupDSTransportTypeMember,
                                                     &setupDSControllerSaltMember,
                                                     NULL },
    .callbacks = { .isValid = isValidS }
};


HAP_RESULT_USE_CHECK
HAPError HandleSetupDSTransportWrite(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicWriteRequest* request HAP_UNUSED,
        HAPTLVReaderRef* requestReader,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(requestReader);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    setupDataStreamTransportWriteStruct setupDataStreamTransportWrite;

    HAPError err;
    err = HAPTLVReaderDecode(requestReader, &setupDSTransportFormatWrite, &setupDataStreamTransportWrite);

    // start the data stream listening thread
    uint16_t port;
    POSDataStreamSetup(setupDataStreamTransportWrite, setupDataStreamTransportRead, &port, 
        (uint8_t *)&((HAPSession *)request->session)->hap.cv_KEY);
        //&((HAPSession *)request->session)->hap.controllerToAccessory.controlChannel.key.bytes);
        //(&((HAPSession *)request->session)->state.pairVerify.cv_KEY ));
        //&((HAPSession *)request->session)->hap.controllerToAccessory.controlChannel.key.bytes);
        //((HAPSession *)request->session)->hap.controllerToAccessory.controlChannel.key.bytes);
        //&(((HAPSession *)request->session)->state.pairVerify.SessionKey));
        //pairVerify.cv_KEY
        

    setupDataStreamTransportRead.transportParams.port = port;
    return err;
}

HAP_RESULT_USE_CHECK
HAPError HandleSupportedAudioConfRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(responseWriter);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    HAPError err;
    const HAPTLV supportedAudioConfig = // codec configuration
    {   .type = 0x01,
        .value = { 
            .bytes =
                        (uint8_t[]) { 
                0x01, 0x01,  // codec
                0x00,  // AAC-LC  // CHANGE TO 0x01 == AAC-ELD!
                0x02, 0x09, // codec parameters
                0x01, 0x01, // channels
                0x01, 
                0x02, 0x01, // bitmode
                0x00, // variable
                0x03, 0x01, // sample
                0x01 // 16khz
                // Type 4, max audio bit rate is only in selected audio
            },
            .numBytes = 14
        }
    };
    err = HAPTLVWriterAppend(responseWriter, &supportedAudioConfig); // codec configuration
    
    return err;

}

HAP_RESULT_USE_CHECK
HAPError HandleSupportedCamRecConfRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(responseWriter);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    HAPError err;

    const HAPTLV supportedCameraConfig1 = // prebuffer length 
    {   .type = 0x01,
        .value = { .bytes =
                        (uint8_t[]) { 0x40, 0x1f, 0x00, 0x00 }, // 8000 ms
                    .numBytes = 4
        }
    };
    const HAPTLV supportedCameraConfig2 = // event trigger options
    {   .type = 0x02,
        .value = { .bytes =
                        (uint8_t[]) { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // motion
                    .numBytes = 8
        }
    };
    const HAPTLV supportedCameraConfig3 = // media container options
    {   .type = 0x03,
        .value = { .bytes =
                        (uint8_t[]) { 
                            0x01, 0x01,  // media container type
                            0x00, // fragmented mp4
                            0x02, 0x06, // media container parameters
                            0x01, 0x04, // fragment length
                            0xa0, 0x0f, 0x00, 0x00 // 4000 ms
                         },
                    .numBytes = 11
        }
    };

    err = HAPTLVWriterAppend(responseWriter, &supportedCameraConfig1); // prebuffer length 
    if (err != kHAPError_None) return err;
    err = HAPTLVWriterAppend(responseWriter, &supportedCameraConfig2); // event trigger options 
    if (err != kHAPError_None) return err;
    err = HAPTLVWriterAppend(responseWriter, &supportedCameraConfig3); // media container options
    
    return err;
}

HAP_RESULT_USE_CHECK
HAPError HandleSupportedDSTransportConfRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(responseWriter);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    HAPError err;

    const HAPTLV supportedDataStreamConfig =
    {   .type = 0x01,
        .value = { 
            .bytes =
                        (uint8_t[]) { 
                0x01, 0x01,  // Transport type
                0x00,  // HomeKit Data Stream
            },
            .numBytes = 3
        }
    };
    err = HAPTLVWriterAppend(responseWriter, &supportedDataStreamConfig);
    
    return err;

}


HAP_RESULT_USE_CHECK
HAPError HandleDSVersionRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPStringCharacteristicReadRequest* request,
        char* value,
        size_t maxValueBytes,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    const char* stringToCopy = "1.0";
    size_t numBytes = HAPStringGetNumBytes(stringToCopy);
    HAPAssert(numBytes <= 64);
    if (numBytes >= maxValueBytes) {
        HAPLogCharacteristic(
                &kHAPLog_Default,
                request->characteristic,
                request->service,
                request->accessory,
                "Not enough space (needed: %zu, available: %zu).",
                numBytes + 1,
                maxValueBytes);
        return kHAPError_OutOfResources;
    }
    HAPRawBufferCopyBytes(value, stringToCopy, numBytes);
    value[numBytes] = '\0';
    return kHAPError_None;
}



HAP_RESULT_USE_CHECK
HAPError HandleSupportedVidRecConfRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(responseWriter);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    HAPError err;

    const HAPTLV supportedSecureVideoConfig = // codec configuration
    {   .type = 0x01,
        .value = { .bytes =
                        (uint8_t[]) { 

                        0x01, 0x01, //codec
                            0x00,   //h264
                        0x02, 0x0b, // video codec parameters
                            0x01, 0x01, //profile ID
                                0x01,   // main
                            0x02, 0x01, // level
                                0x00,   // 3.1
                            0x00, 0x00, 
                            0x02, 0x01, // level
                                0x02,   // 4
                            0x03, 0x0b, 
                                0x01, 0x02, 
                                    0x80, 0x07, //1920
                                0x02, 0x02, 
                                    0x38, 0x04, //1080
                                0x03, 0x01, 
                                    0x18,       //24
                            0x00, 0x00,     
                            0x03, 0x0b, 
                                0x01, 0x02, 
                                    0x00, 0x05, //1280
                                0x02, 0x02, 
                                    0xd0, 0x02, //768
                                0x03, 0x01, 
                                    0x18,       //24
                            0x00, 0x00, 
                            0x03, 0x0b, 
                                0x01, 0x02, 
                                    0x80, 0x07, //1920
                                0x02, 0x02, 
                                    0x38, 0x04, //1080
                                0x03, 0x01, 
                                    0x0f,       //15
                            0x00, 0x00,   
                            0x03, 0x0b, 
                                0x01, 0x02, 
                                    0x00, 0x05, //1280
                                0x02, 0x02, 
                                    0xd0, 0x02, //768
                                0x03, 0x01, 
                                    0x0f,       //15
                        },
                    .numBytes = 3+13+15*3+13  
        }
    };

    err = HAPTLVWriterAppend(responseWriter, &supportedSecureVideoConfig); // codec configuration
    
    return err;
}

HAP_RESULT_USE_CHECK
HAPError HandleRTPActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPUInt8CharacteristicReadRequest* request HAP_UNUSED,
        HAPCharacteristicValue_Active* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.rtp.active;
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, *value);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleRTPActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicWriteRequest* request,
        HAPCharacteristicValue_Active value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, value);
    if (accessoryConfiguration.state.rtp.active != value) {
        accessoryConfiguration.state.rtp.active = value;

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }
    return kHAPError_None;
}
HAP_RESULT_USE_CHECK
HAPError HandleMotionActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.motion.active;
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, *value);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleMotionActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, value);
    if (accessoryConfiguration.state.motion.active != value) {
        accessoryConfiguration.state.motion.active = value;

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleNightVisionWrite(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicWriteRequest* request HAP_UNUSED,
        bool value HAP_UNUSED,
        void* _Nullable context HAP_UNUSED) {
        HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, value);

    if (accessoryConfiguration.state.operatingMode.nightVision != value) {
        accessoryConfiguration.state.operatingMode.nightVision = value;

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleNightVisionRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.operatingMode.nightVision;
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, *value);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleOperatingModeIndicatorWrite(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicWriteRequest* request HAP_UNUSED,
        bool value HAP_UNUSED,
        void* _Nullable context HAP_UNUSED) {
        HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, value);

    if (accessoryConfiguration.state.operatingMode.indicator != value) {
        accessoryConfiguration.state.operatingMode.indicator = value;

        set_red_led(accessoryConfiguration.state.operatingMode.indicator);

        // not sure if this is a good idea, every 2 toggles of the operating mode inidicator flips the view stream
        if(value){
            accessoryConfiguration.state.operatingMode.flip = !accessoryConfiguration.state.operatingMode.flip;
        }
        set_flip(accessoryConfiguration.state.operatingMode.flip);
        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleOperatingModeIndicatorRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.operatingMode.indicator;
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, *value);
    return kHAPError_None;
}

void checkFormats() {

    const HAPTLVFormat* formats[] = {
        &setupDSTransportFormatWrite, &setupDSTransportFormatRead, NULL
    };
    for (size_t i = 0; i < 2; i++) {
        bool valid;
        valid = HAPTLVFormatIsValid(formats[i]);
        HAPLogDebug(&kHAPLog_Default, "%zu: %d", i, valid);
    }
}

