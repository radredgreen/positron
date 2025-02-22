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


#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <string.h>

#include "HAP.h"
#include "HAPBase.h"

#include "App_Camera.h"
#include "App.h"
#include "DB.h"
#include "util_base64.h"
#include "POSCameraController.h"


//Temporary debugging
#include <stdio.h>

// Socket, address and port lookups


/* ---------------------------------------------------------------------------------------------*/


bool isValid(void* unsused HAP_UNUSED) {
    return true;
}



//value out
//bytes in
HAP_RESULT_USE_CHECK
HAPError decodeIpAddress(uint8_t* value, void* bytes, size_t numBytes) {
char dbgout[INET6_ADDRSTRLEN+1];
//strncpy(dbgout, bytes, INET6_ADDRSTRLEN);
//dbgout[numBytes] = 0;
//printf("decodeIpAddress, %s\n", dbgout);
    if (numBytes < INET6_ADDRSTRLEN)
        HAPRawBufferCopyBytes((uint8_t*) value, (uint8_t*) bytes, numBytes);
    else
        HAPRawBufferCopyBytes((uint8_t*) value, (uint8_t*) bytes, INET6_ADDRSTRLEN);

    return kHAPError_None;
};


HAPError encodeIpAddress(uint8_t*  value, void* bytes, size_t maxBytes, size_t* numBytes) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    HAPRawBufferCopyBytes((uint8_t*) bytes, (uint8_t*) value, INET6_ADDRSTRLEN);
    *numBytes = INET6_ADDRSTRLEN;
    //printf("encodeIpAddress, %s\n", bytes);
    return kHAPError_None;
}


HAPError encodeUInt8UUIDValue(uint8_t* value, void* bytes, size_t maxBytes HAP_UNUSED, size_t* numBytes) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    HAPRawBufferCopyBytes((uint8_t*) bytes, (uint8_t*) value, UUIDLENGTH);
    *numBytes = UUIDLENGTH;
    return kHAPError_None;
};

HAP_RESULT_USE_CHECK
HAPError decodeUInt8UUIDValue(uint8_t* value, void* bytes, size_t numBytes) {
    HAPRawBufferCopyBytes((uint8_t*) value, (uint8_t*) bytes, UUIDLENGTH);
    return kHAPError_None;
};

HAPError encodeUInt8KeyValue(uint8_t* value, void* bytes, size_t maxBytes HAP_UNUSED, size_t* numBytes) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    HAPRawBufferCopyBytes((uint8_t*) bytes, (uint8_t*) value, KEYLENGTH);
    *numBytes = KEYLENGTH;

//    char keyStr64[128];
//    size_t encLen64;
//    util_base64_encode(value, KEYLENGTH, keyStr64, 128, &encLen64);
//    keyStr64[encLen64]='\0';
//    printf("\n\nKey: %s\n\n", keyStr64);
//    *numBytes = encLen64+1;
//    HAPRawBufferCopyBytes(bytes, keyStr64, encLen64);
    return kHAPError_None;
};

HAP_RESULT_USE_CHECK
HAPError decodeUInt8KeyValue(uint8_t* value, void* bytes, size_t numBytes) {
    HAPRawBufferCopyBytes((uint8_t*) value, (uint8_t*) bytes, KEYLENGTH);
    return kHAPError_None;
};


HAPError encodeUInt8SaltValue(uint8_t* value, void* bytes, size_t maxBytes HAP_UNUSED, size_t* numBytes) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    HAPRawBufferCopyBytes((uint8_t*) bytes, (uint8_t*) value, SALTLENGTH);
    *numBytes = SALTLENGTH;
    return kHAPError_None;
};
HAP_RESULT_USE_CHECK
HAPError decodeUInt8SaltValue(uint8_t* value, void* bytes, size_t numBytes) {
    HAPRawBufferCopyBytes((uint8_t*) value, (uint8_t*) bytes, SALTLENGTH);
    return kHAPError_None;
};


HAPError getIpAddressDescription(uint8_t * value, char* bytes, size_t maxBytes) {
    char str[INET6_ADDRSTRLEN];
    HAPRawBufferCopyBytes(bytes, value, INET6_ADDRSTRLEN);
    //printf("getIpAddressDescription, %s\n", bytes);
    return kHAPError_None;
};

HAPError getUInt8ValueDescription(uint8_t* value HAP_UNUSED, char* bytes, size_t maxBytes HAP_UNUSED) {
    char description[] = "UInt8 Value Description";
    HAPRawBufferCopyBytes(bytes, description, sizeof(description));

    return kHAPError_None;
};

HAPError getKeyValueDescription(uint8_t* value HAP_UNUSED, char* bytes, size_t maxBytes HAP_UNUSED) {
    char str64[128];
    size_t encLen64;
    util_base64_encode(value, KEYLENGTH, str64, 128, &encLen64);
    str64[encLen64]='\0';
    //printf("\n\nKey: %s\n\n", str64);
    //    *numBytes = encLen64+1;
    HAPRawBufferCopyBytes(bytes, str64, encLen64+1);

    return kHAPError_None;
};
HAPError getSaltValueDescription(uint8_t* value HAP_UNUSED, char* bytes, size_t maxBytes HAP_UNUSED) {
    char str64[128];
    size_t encLen64;
    util_base64_encode(value, SALTLENGTH, str64, 128, &encLen64);
    str64[encLen64]='\0';
    //printf("\n\nSalt: %s\n\n", str64);
    //    *numBytes = encLen64+1;
    HAPRawBufferCopyBytes(bytes, str64, encLen64+1);

    return kHAPError_None;
};


HAPError getUInt8UUIDValueDescription(uint8_t* value, char* bytes, size_t maxBytes) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    // char description[] = "UInt8 Value Description";
    // HAPRawBufferCopyBytes(bytes, description, sizeof(description));
    return HAPUUIDGetDescription((HAPUUID*) value, bytes, maxBytes);
};

/* ---------------------------------------------------------------------------------------------*/

supportedAudioConfigStruct supportedAudioConfigValue =
{
    .audioCodecConfig =
    {
        .audioCodecType = 2,     // AAC-ELD
        .audioCodecParams =
        {
            .audioChannels = 1,  // 1 channel
            .bitRate = 0,        // Variable
            .sampleRate = 1,      // 16kHz 8 not supported on MBP
            .rtpTime = 30        // 30 ms per packet
        }
    },
    .comfortNoiseSupport = false
};

HAP_STRUCT_TLV_SUPPORT(void, SupportedAudioConfigFormat)
HAP_STRUCT_TLV_SUPPORT(void, AudioCodecConfigFormat)
HAP_STRUCT_TLV_SUPPORT(void, AudioCodecParamsFormat)

const HAPUInt8TLVFormat audioCodecParamsAudioChannelsFormat = { .type = kHAPTLVFormatType_UInt8,
                                                                .constraints = { .minimumValue = 1, .maximumValue = 1 },
                                                                .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember audioCodecParamsAudioChannelsMember = {
    .valueOffset = HAP_OFFSETOF(audioCodecParamsStruct, audioChannels),
    .isSetOffset = 0,
    .tlvType = 1,
    .debugDescription = "Audio Codec Config Params Audio Channels",
    .format = &audioCodecParamsAudioChannelsFormat,
    .isOptional = false,
    .isFlat = false
};

const HAPUInt8TLVFormat audioCodecParamsBitRateFormat = { .type = kHAPTLVFormatType_UInt8,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 2 },
                                                          .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember audioCodecParamsBitRateMember = { .valueOffset = HAP_OFFSETOF(audioCodecParamsStruct, bitRate),
                                                           .isSetOffset = 0,
                                                           .tlvType = 2,
                                                           .debugDescription = "Audio Codec Config Params Bit-Rate",
                                                           .format = &audioCodecParamsBitRateFormat,
                                                           .isOptional = false,
                                                           .isFlat = false };

const HAPUInt8TLVFormat audioCodecParamsSampleRateFormat = { .type = kHAPTLVFormatType_UInt8,
                                                             .constraints = { .minimumValue = 0, .maximumValue = 2 },
                                                             .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember audioCodecParamsSampleRateMember = { .valueOffset =
                                                                      HAP_OFFSETOF(audioCodecParamsStruct, sampleRate),
                                                              .isSetOffset = 0,
                                                              .tlvType = 3,
                                                              .debugDescription =
                                                                      "Audio Codec Config Params Sample Rate",
                                                              .format = &audioCodecParamsSampleRateFormat,
                                                              .isOptional = false,
                                                              .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt8TLVFormat audioCodecParamsRTPTimeFormat = { .type = kHAPTLVFormatType_UInt8,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 0xFF },
                                                          .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember audioCodecParamsRTPTimeMember = { .valueOffset = HAP_OFFSETOF(audioCodecParamsStruct, rtpTime),
                                                           .isSetOffset = 0,
                                                           .tlvType = 4,
                                                           .debugDescription = "Audio Codec Param RTP Time",
                                                           .format = &audioCodecParamsRTPTimeFormat,
                                                           .isOptional = false, // TODO - make this optional for write
                                                           .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt16TLVFormat audioCodecTypeFormat = { .type = kHAPTLVFormatType_UInt16,
                                                  .constraints = { .minimumValue = 0, .maximumValue = 6 },
                                                  .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember audioCodecTypeMember = { .valueOffset = HAP_OFFSETOF(audioCodecConfigStruct, audioCodecType),
                                                  .isSetOffset = 0,
                                                  .tlvType = 1,
                                                  .debugDescription = "Audio Codec Type",
                                                  .format = &audioCodecTypeFormat,
                                                  .isOptional = false,
                                                  .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const AudioCodecConfigFormat audioCodecParamsFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &audioCodecParamsAudioChannelsMember,
                                                     &audioCodecParamsBitRateMember,
                                                     &audioCodecParamsSampleRateMember,
                                                     &audioCodecParamsRTPTimeMember,//HERE
                                                     NULL },
    .callbacks = { .isValid = isValid }
};
const HAPStructTLVMember audioCodecParamsMember = { .valueOffset =
                                                            HAP_OFFSETOF(audioCodecConfigStruct, audioCodecParams),
                                                    .isSetOffset = 0,
                                                    .tlvType = 2,
                                                    .debugDescription = "Audio Codec Parameters",
                                                    .format = &audioCodecParamsFormat,
                                                    .isOptional = false,
                                                    .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const AudioCodecConfigFormat audioCodecConfigFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &audioCodecTypeMember, &audioCodecParamsMember, NULL },
    .callbacks = { .isValid = isValid }
};
const HAPStructTLVMember audioCodecConfigMember = { .valueOffset =
                                                            HAP_OFFSETOF(supportedAudioConfigStruct, audioCodecConfig),
                                                    .isSetOffset = 0,
                                                    .tlvType = 1,
                                                    .debugDescription = "Audio Codec Config",
                                                    .format = &audioCodecConfigFormat,
                                                    .isOptional = false,
                                                    .isFlat = false };

const HAPUInt8TLVFormat comfortNoiseFormat = { .type = kHAPTLVFormatType_UInt8,
                                               .constraints = { .minimumValue = 0, .maximumValue = 255 },
                                               .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember comfortNoiseMember = { .valueOffset =
                                                        HAP_OFFSETOF(supportedAudioConfigStruct, comfortNoiseSupport),
                                                .isSetOffset = 0,
                                                .tlvType = 2,
                                                .debugDescription = "Comfort Noise Support",
                                                .format = &comfortNoiseFormat,
                                                .isOptional = false,
                                                .isFlat = false };

const SupportedAudioConfigFormat supportedAudioConfigFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &audioCodecConfigMember, &comfortNoiseMember, NULL },
    .callbacks = { .isValid = isValid }
};

supportedVideoConfigStruct supportedVideoConfigValue = {
    .videoCodecConfig = { .videoCodecType = 0,
                          .videoCodecParams = { .profileID = 1,
                                                .level = 2,
                                                .packetizationMode = 0,
                                                .CVOEnabled = 0 }, // TODO - Make enums for profileID, and level
                          .videoAttributes = { .imageWidth = 1920, .imageHeight = 1080, .frameRate = 25 } }
};

HAP_STRUCT_TLV_SUPPORT(void, SupportedVideoConfigFormat)
HAP_STRUCT_TLV_SUPPORT(void, VideoCodecConfigFormat)
HAP_STRUCT_TLV_SUPPORT(void, VideoCodecParamsFormat)
HAP_STRUCT_TLV_SUPPORT(void, VideoAttributesFormat)

const HAPUInt8TLVFormat videoCodecParamsProfileIDFormat = { .type = kHAPTLVFormatType_UInt8,
                                                            .constraints = { .minimumValue = 0, .maximumValue = 2 },
                                                            .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember videoCodecParamsProfileIDMember = { .valueOffset =
                                                                     HAP_OFFSETOF(videoCodecParamsStruct, profileID),
                                                             .isSetOffset = 0,
                                                             .tlvType = 1,
                                                             .debugDescription = "Video Codec Config Params Profile ID",
                                                             .format = &videoCodecParamsProfileIDFormat,
                                                             .isOptional = false,
                                                             .isFlat = false };

const HAPUInt8TLVFormat videoCodecParamsLevelFormat = { .type = kHAPTLVFormatType_UInt8,
                                                        .constraints = { .minimumValue = 0, .maximumValue = 2 },
                                                        .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember videoCodecParamsLevelMember = { .valueOffset = HAP_OFFSETOF(videoCodecParamsStruct, level),
                                                         .isSetOffset = 0,
                                                         .tlvType = 2,
                                                         .debugDescription = "Video Codec Config Params Level",
                                                         .format = &videoCodecParamsLevelFormat,
                                                         .isOptional = false,
                                                         .isFlat = false };

const HAPUInt8TLVFormat videoCodecParamsPacketizationModeFormat = { .type = kHAPTLVFormatType_UInt8,
                                                                    .constraints = { .minimumValue = 0,
                                                                                     .maximumValue = 2 },
                                                                    .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember videoCodecParamsPacketizationModeMember = {
    .valueOffset = HAP_OFFSETOF(videoCodecParamsStruct, packetizationMode),
    .isSetOffset = 0,
    .tlvType = 3,
    .debugDescription = "Video Codec Config Packetization Mode",
    .format = &videoCodecParamsPacketizationModeFormat,
    .isOptional = false,
    .isFlat = false
};

const HAPUInt8TLVFormat videoCodecParamsCVOEnabledFormat = { .type = kHAPTLVFormatType_UInt8,
                                                             .constraints = { .minimumValue = 0, .maximumValue = 2 },
                                                             .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember videoCodecParamsCVOEnabledMember = { .valueOffset =
                                                                      HAP_OFFSETOF(videoCodecParamsStruct, CVOEnabled),
                                                              .isSetOffset = 0,
                                                              .tlvType = 4,
                                                              .debugDescription = "CVO Enabled",
                                                              .format = &videoCodecParamsCVOEnabledFormat,
                                                              .isOptional = true, //todo, testing
                                                              .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt8TLVFormat videoCodecTypeFormat = { .type = kHAPTLVFormatType_UInt8,
                                                 .constraints = { .minimumValue = 0, .maximumValue = 1 },
                                                 .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember videoCodecTypeMember = { .valueOffset = HAP_OFFSETOF(videoCodecConfigStruct, videoCodecType),
                                                  .isSetOffset = HAP_OFFSETOF(videoCodecConfigStruct, videoCodecTypeIsSet),
                                                  .tlvType = 1,
                                                  .debugDescription = "Video Codec Type",
                                                  .format = &videoCodecTypeFormat,
                                                  .isOptional = true,
                                                  .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const VideoCodecConfigFormat videoCodecParamsFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &videoCodecParamsProfileIDMember,
                                                     &videoCodecParamsLevelMember,
                                                     &videoCodecParamsPacketizationModeMember,
                                                     //&videoCodecParamsCVOEnabledMember,
                                                     NULL },
    .callbacks = { .isValid = isValid }
};
const HAPStructTLVMember videoCodecParamsMember = { .valueOffset =
                                                            HAP_OFFSETOF(videoCodecConfigStruct, videoCodecParams),
                                                    .isSetOffset = HAP_OFFSETOF(videoCodecConfigStruct, videoCodecParamsIsSet),
                                                    .tlvType = 2,
                                                    .debugDescription = "Video Codec Parameters",
                                                    .format = &videoCodecParamsFormat,
                                                    .isOptional = true,
                                                    .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt16TLVFormat videoAttributesImageWidthFormat = { .type = kHAPTLVFormatType_UInt16,
                                                             .constraints = { .minimumValue = 0, .maximumValue = 4096 },
                                                             .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember videoAttributesImageWidthMember = { .valueOffset =
                                                                     HAP_OFFSETOF(videoAttributesStruct, imageWidth),
                                                             .isSetOffset = 0,
                                                             .tlvType = 1,
                                                             .debugDescription = "Video Attributes Image Width",
                                                             .format = &videoAttributesImageWidthFormat,
                                                             .isOptional = false,
                                                             .isFlat = false };

const HAPUInt16TLVFormat videoAttributesImageHeightFormat = { .type = kHAPTLVFormatType_UInt16,
                                                              .constraints = { .minimumValue = 0,
                                                                               .maximumValue = 4096 },
                                                              .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember videoAttributesImageHeightMember = { .valueOffset =
                                                                      HAP_OFFSETOF(videoAttributesStruct, imageHeight),
                                                              .isSetOffset = 0,
                                                              .tlvType = 2,
                                                              .debugDescription = "Video Attributes Image Height",
                                                              .format = &videoAttributesImageHeightFormat,
                                                              .isOptional = false,
                                                              .isFlat = false };

const HAPUInt8TLVFormat videoAttributesFrameRateFormat = { .type = kHAPTLVFormatType_UInt8,
                                                           .constraints = { .minimumValue = 0, .maximumValue = 255 },
                                                           .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember videoAttributesFrameRateMember = { .valueOffset =
                                                                    HAP_OFFSETOF(videoAttributesStruct, frameRate),
                                                            .isSetOffset = 0,
                                                            .tlvType = 3,
                                                            .debugDescription = "Video Attributes Frame Rate",
                                                            .format = &videoAttributesFrameRateFormat,
                                                            .isOptional = false,
                                                            .isFlat = false };

const VideoAttributesFormat videoAttributesFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &videoAttributesImageWidthMember,
                                                     &videoAttributesImageHeightMember,
                                                     &videoAttributesFrameRateMember,
                                                     NULL },
    .callbacks = { .isValid = isValid }
};
const HAPStructTLVMember videoAttributesMember = { .valueOffset = HAP_OFFSETOF(videoCodecConfigStruct, videoAttributes),
                                                   .isSetOffset = 0,
                                                   .tlvType = 3,
                                                   .debugDescription = "Video Attributes",
                                                   .format = &videoAttributesFormat,
                                                   .isOptional = false,
                                                   .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const VideoCodecConfigFormat videoCodecConfigFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &videoCodecTypeMember,
                                                     &videoCodecParamsMember,
                                                     &videoAttributesMember,
                                                     NULL },
    .callbacks = { .isValid = isValid }
};
const HAPStructTLVMember videoCodecConfigMember = { .valueOffset =
                                                            HAP_OFFSETOF(supportedVideoConfigStruct, videoCodecConfig),
                                                    .isSetOffset = 0,
                                                    .tlvType = 1,
                                                    .debugDescription = "Video Codec Config",
                                                    .format = &videoCodecConfigFormat,
                                                    .isOptional = false,
                                                    .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const SupportedVideoConfigFormat supportedVideoConfigFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &videoCodecConfigMember, NULL },
    .callbacks = { .isValid = isValid }
};

HAP_STRUCT_TLV_SUPPORT(void, AddressTypeFormat);
HAP_STRUCT_TLV_SUPPORT(void, SrtpParametersFormat);
HAP_STRUCT_TLV_SUPPORT(void, StreamingSessionFormat);
HAP_STRUCT_TLV_SUPPORT(void, SelectedRTPFormat);
HAP_STRUCT_TLV_SUPPORT(void, RTPParametersFormat);
HAP_STRUCT_TLV_SUPPORT(void, VideoRTPParametersTypeFormat);
HAP_STRUCT_TLV_SUPPORT(void, AudioRTPParametersTypeFormat);
HAP_STRUCT_TLV_SUPPORT(void, SelectedVideoParametersFormat);
HAP_STRUCT_TLV_SUPPORT(void, SelectedAudioParametersFormat);
HAP_STRUCT_TLV_SUPPORT(void, SessionControlTypeFormat);
HAP_VALUE_TLV_SUPPORT(uint8_t, IpAddressTypeFormat);
HAP_VALUE_TLV_SUPPORT(uint8_t, SessionIdTypeFormat);
HAP_VALUE_TLV_SUPPORT(uint8_t, SrtpMasterKeyTypeFormat);
HAP_VALUE_TLV_SUPPORT(uint8_t, SrtpMasterSaltTypeFormat);

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt8TLVFormat ipAddrVersionTypeFormat = { .type = kHAPTLVFormatType_UInt8,
                                                    .constraints = { .minimumValue = 0, .maximumValue = 1 },
                                                    .callbacks = { .getBitDescription = NULL,
                                                                   .getDescription = NULL } };

const HAPStructTLVMember ipAddrVersionTypeMember = { .valueOffset =
                                                             HAP_OFFSETOF(controllerAddressStruct, ipAddrVersion),
                                                     .isSetOffset = 0,
                                                     .tlvType = 1,
                                                     .debugDescription = "Controller IP Address Type",
                                                     .format = &ipAddrVersionTypeFormat,
                                                     .isOptional = false,
                                                     .isFlat = false };

const IpAddressTypeFormat ipAddressTypeFormat = {
    .type = kHAPTLVFormatType_Value,
    .callbacks = { .decode = decodeIpAddress, .encode = encodeIpAddress, .getDescription = getIpAddressDescription }
};

const HAPStructTLVMember ipAddressTypeMember = { .valueOffset = HAP_OFFSETOF(controllerAddressStruct, ipAddress),
                                                 .isSetOffset = 0,
                                                 .tlvType = 2,
                                                 .debugDescription = "IP Address Type",
                                                 .format = &ipAddressTypeFormat,
                                                 .isOptional = false,
                                                 .isFlat = false };

const HAPUInt16TLVFormat rtpPortTypeFormat = { .type = kHAPTLVFormatType_UInt16,
                                               .constraints = { .minimumValue = 1024, .maximumValue = 65535 },
                                               .callbacks = { .getBitDescription = NULL, .getDescription = NULL } };

const HAPStructTLVMember videoPortTypeMember = { .valueOffset = HAP_OFFSETOF(controllerAddressStruct, videoPort),
                                                 .isSetOffset = 0,
                                                 .tlvType = 3,
                                                 .debugDescription = "Video RTP Port Type",
                                                 .format = &rtpPortTypeFormat,
                                                 .isOptional = false,
                                                 .isFlat = false };

const HAPStructTLVMember audioPortTypeMember = { .valueOffset = HAP_OFFSETOF(controllerAddressStruct, audioPort),
                                                 .isSetOffset = 0,
                                                 .tlvType = 4,
                                                 .debugDescription = "Audio RTP Port Type",
                                                 .format = &rtpPortTypeFormat,
                                                 .isOptional = false,
                                                 .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const AddressTypeFormat addressTypeFormat = { .type = kHAPTLVFormatType_Struct,
                                              .members = (const HAPStructTLVMember* const[]) { &ipAddrVersionTypeMember,
                                                                                               &ipAddressTypeMember,
                                                                                               &videoPortTypeMember,
                                                                                               &audioPortTypeMember,
                                                                                               NULL },
                                              .callbacks = { .isValid = isValid } };

const HAPStructTLVMember controllerAddressTypeMember = { .valueOffset =
                                                                 HAP_OFFSETOF(streamingSession, controllerAddress),
                                                         .isSetOffset = 0,
                                                         .tlvType = 3,
                                                         .debugDescription = "Controller Address Type",
                                                         .format = &addressTypeFormat,
                                                         .isOptional = false,
                                                         .isFlat = false };

const HAPStructTLVMember accessoryAddressTypeMember = { .valueOffset = HAP_OFFSETOF(streamingSession, accessoryAddress),
                                                        .isSetOffset = 0,
                                                        .tlvType = 3,
                                                        .debugDescription = "Accessory Address Type",
                                                        .format = &addressTypeFormat,
                                                        .isOptional = false,
                                                        .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const SessionIdTypeFormat sessionIdTypeFormat = { .type = kHAPTLVFormatType_Value,
                                                  .callbacks = { .decode = decodeUInt8UUIDValue,
                                                                 .encode = encodeUInt8UUIDValue,
                                                                 .getDescription = getUInt8UUIDValueDescription } };

const HAPStructTLVMember sessionIdTypeMember = { .valueOffset = HAP_OFFSETOF(streamingSession, sessionId),
                                                 .isSetOffset = 0,
                                                 .tlvType = 1,
                                                 .debugDescription = "Session ID Type",
                                                 .format = &sessionIdTypeFormat,
                                                 .isOptional = false,
                                                 .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt8TLVFormat streamingStatusTypeFormat = { .type = kHAPTLVFormatType_UInt8,
                                                      .constraints = { .minimumValue = 0, .maximumValue = 2 },
                                                      .callbacks = { .getDescription = NULL,
                                                                     .getBitDescription = NULL } };

const HAPStructTLVMember streamingStatusTypeMember = { .valueOffset = HAP_OFFSETOF(streamingSession, status),
                                                       .isSetOffset = 0,
                                                       .tlvType = 2,
                                                       .debugDescription = "Streaming Status Type",
                                                       .format = &streamingStatusTypeFormat,
                                                       .isOptional = false,
                                                       .isFlat = false };

const HAPUInt32TLVFormat setupWriteStatusTypeFormat = { .type = kHAPTLVFormatType_UInt32,
                                                        .constraints = { .minimumValue = 0, .maximumValue = 255 },
                                                        .callbacks = { .getDescription = NULL,
                                                                       .getBitDescription = NULL } };

const HAPStructTLVMember setupWriteStatusTypeMember = { .valueOffset = HAP_OFFSETOF(streamingSession, setupWriteStatus),
                                                        .isSetOffset = 0,
                                                        .tlvType = 2,
                                                        .debugDescription = "Setup Write Status Type",
                                                        .format = &setupWriteStatusTypeFormat,
                                                        .isOptional = false,
                                                        .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt8TLVFormat srtpCryptoSuiteTypeFormat = { .type = kHAPTLVFormatType_UInt8,
                                                      .constraints = { .minimumValue = 0, .maximumValue = 255 },
                                                      .callbacks = { .getBitDescription = NULL,
                                                                     .getDescription = NULL } };

const SrtpMasterKeyTypeFormat srtpMasterKeyTypeFormat = { .type = kHAPTLVFormatType_Value,
                                                          .callbacks = { .decode = decodeUInt8KeyValue,
                                                                         .encode = encodeUInt8KeyValue,
                                                                         .getDescription = getKeyValueDescription } };

const SrtpMasterSaltTypeFormat srtpMasterSaltTypeFormat = { .type = kHAPTLVFormatType_Value,
                                                            .callbacks = { .decode = decodeUInt8SaltValue,
                                                                           .encode = encodeUInt8SaltValue,
                                                                           .getDescription =
                                                                                   getSaltValueDescription } };

const HAPStructTLVMember srtpCryptoSuiteTypeMember = { .valueOffset = HAP_OFFSETOF(srtpParameters, srtpCryptoSuite),
                                                       .isSetOffset = 0,
                                                       .tlvType = 1,
                                                       .debugDescription = "SRTP Crypto Suite Type",
                                                       .format = &srtpCryptoSuiteTypeFormat,
                                                       .isOptional = false,
                                                       .isFlat = false };

const HAPStructTLVMember srtpMasterKeyTypeMember = { .valueOffset = HAP_OFFSETOF(srtpParameters, srtpMasterKey),
                                                     .isSetOffset = 0,
                                                     .tlvType = 2,
                                                     .debugDescription = "Master Key Type",
                                                     .format = &srtpMasterKeyTypeFormat,
                                                     .isOptional = false,
                                                     .isFlat = false };

const HAPStructTLVMember srtpMasterSaltTypeMember = { .valueOffset = HAP_OFFSETOF(srtpParameters, srtpMasterSalt),
                                                      .isSetOffset = 0,
                                                      .tlvType = 3,
                                                      .debugDescription = "Master Salt Type",
                                                      .format = &srtpMasterSaltTypeFormat,
                                                      .isOptional = false,
                                                      .isFlat = false };

const SrtpParametersFormat srtpParametersFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &srtpMasterKeyTypeMember,
                                                     &srtpMasterSaltTypeMember,
                                                     &srtpCryptoSuiteTypeMember,
                                                     NULL },
    .callbacks = { .isValid = isValid }
};

/* ---------------------------------------------------------------------------------------------*/

const HAPStructTLVMember videoParamsTypeMember = { .valueOffset = HAP_OFFSETOF(streamingSession, videoParams),
                                                   .isSetOffset = 0,
                                                   .tlvType = 4,
                                                   .debugDescription = "Video Params Type",
                                                   .format = &srtpParametersFormat,
                                                   .isOptional = false,
                                                   .isFlat = false };

const HAPStructTLVMember audioParamsTypeMember = { .valueOffset = HAP_OFFSETOF(streamingSession, audioParams),
                                                   .isSetOffset = 0,
                                                   .tlvType = 5,
                                                   .debugDescription = "Audio Params Type",
                                                   .format = &srtpParametersFormat,
                                                   .isOptional = false,
                                                   .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt32TLVFormat ssrcTypeFormat = { .type = kHAPTLVFormatType_UInt32,
                                            .constraints = { .minimumValue = 0, .maximumValue = 0xFFFFFFFF },
                                            .callbacks = { .getBitDescription = NULL, .getDescription = NULL } };

const HAPStructTLVMember ssrcVideoTypeMember = { .valueOffset = HAP_OFFSETOF(streamingSession, ssrcVideo),
                                                 .isSetOffset = 0,
                                                 .tlvType = 6,
                                                 .debugDescription = "SSRC Video Type",
                                                 .format = &ssrcTypeFormat,
                                                 .isOptional = false,
                                                 .isFlat = false };

const HAPStructTLVMember ssrcAudioTypeMember = { .valueOffset = HAP_OFFSETOF(streamingSession, ssrcAudio),
                                                 .isSetOffset = 0,
                                                 .tlvType = 7,
                                                 .debugDescription = "SSRC Audio Type",
                                                 .format = &ssrcTypeFormat,
                                                 .isOptional = false,
                                                 .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const StreamingSessionFormat streamingSessionFormatWrite = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &sessionIdTypeMember,
                                                     &controllerAddressTypeMember,
                                                     &videoParamsTypeMember,
                                                     &audioParamsTypeMember,
                                                     NULL },
    .callbacks = { .isValid = isValid }
};

const StreamingSessionFormat streamingSessionFormatRead = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &sessionIdTypeMember,
                                                     &setupWriteStatusTypeMember,
                                                     &accessoryAddressTypeMember,
                                                     &videoParamsTypeMember,
                                                     &audioParamsTypeMember,
                                                     &ssrcVideoTypeMember,
                                                     &ssrcAudioTypeMember,
                                                     NULL },
    .callbacks = { .isValid = isValid }
};

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt8TLVFormat payloadTypeFormat = { .type = kHAPTLVFormatType_UInt8,
                                              .constraints = { .minimumValue = 0, .maximumValue = 0xFF },
                                              .callbacks = { .getBitDescription = NULL, .getDescription = NULL } };

const HAPStructTLVMember payloadTypeMember = { .valueOffset = HAP_OFFSETOF(rtpParameters, payloadType),
                                               .isSetOffset = HAP_OFFSETOF(rtpParameters, payloadTypeIsSet),
                                               .tlvType = 1,
                                               .debugDescription = "Payload Type",
                                               .format = &payloadTypeFormat,
                                               .isOptional = true,
                                               .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPStructTLVMember selectedSsrcTypeMember = { .valueOffset = HAP_OFFSETOF(rtpParameters, ssrc),
                                                    .isSetOffset = HAP_OFFSETOF(rtpParameters, ssrcIsSet),
                                                    .tlvType = 2,
                                                    .debugDescription = "Selected SSRC Type",
                                                    .format = &ssrcTypeFormat,
                                                    .isOptional = true,
                                                    .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt16TLVFormat maximumBitrateTypeFormat = { .type = kHAPTLVFormatType_UInt16,
                                                      .constraints = { .minimumValue = 0, .maximumValue = 0xFFFF },
                                                      .callbacks = { .getBitDescription = NULL,
                                                                     .getDescription = NULL } };

const HAPStructTLVMember maximumBitrateTypeMember = { .valueOffset = HAP_OFFSETOF(rtpParameters, maximumBitrate),
                                                      .isSetOffset = HAP_OFFSETOF(rtpParameters, maximumBitrateIsSet),
                                                      .tlvType = 3,
                                                      .debugDescription = "Maximum Bit Rate Type",
                                                      .format = &maximumBitrateTypeFormat,
                                                      .isOptional = true,
                                                      .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt32TLVFormat minRTCPIntervalTypeFormat = { .type = kHAPTLVFormatType_UInt32,
                                                       .constraints = { .minimumValue = 0, .maximumValue = 0xFFFFFFFF },
                                                       .callbacks = { .getBitDescription = NULL,
                                                                      .getDescription = NULL } };

const HAPStructTLVMember minRTCPIntervalTypeMember = { .valueOffset = HAP_OFFSETOF(rtpParameters, minRTCPinterval),
                                                       .isSetOffset = HAP_OFFSETOF(rtpParameters, minRTCPintervalIsSet),
                                                       .tlvType = 4,
                                                       .debugDescription = "Min RTCP Interval Type",
                                                       .format = &minRTCPIntervalTypeFormat,
                                                       .isOptional = true,
                                                       .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt16TLVFormat maximumMTUTypeFormat = { .type = kHAPTLVFormatType_UInt16,
                                                  .constraints = { .minimumValue = 0, .maximumValue = 0xFFFF },
                                                  .callbacks = { .getBitDescription = NULL, .getDescription = NULL } };

const HAPStructTLVMember maximumMTUTypeMember = { .valueOffset = HAP_OFFSETOF(rtpParameters, maxMTU),
                                                  .isSetOffset = HAP_OFFSETOF(rtpParameters, maxMTUIsSet),
                                                  .tlvType = 5,
                                                  .debugDescription = "Maximum MTU Type",
                                                  .format = &maximumMTUTypeFormat,
                                                  .isOptional = true,
                                                  .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const VideoRTPParametersTypeFormat videoRTPParametersTypeFormat = { .type = kHAPTLVFormatType_Struct,
                                                                    .members =
                                                                            (const HAPStructTLVMember* const[]) {
                                                                                    &payloadTypeMember,
                                                                                    &selectedSsrcTypeMember,
                                                                                    &maximumBitrateTypeMember,
                                                                                    &minRTCPIntervalTypeMember,
                                                                                    &maximumMTUTypeMember,
                                                                                    NULL },
                                                                    .callbacks = { .isValid = isValid } };

const HAPStructTLVMember videoRTPParametersTypeMember = { .valueOffset =
                                                                  HAP_OFFSETOF(selectedVideoParameters, vRtpParameters),
                                                          .isSetOffset = 0,
                                                          .tlvType = 4,
                                                          .debugDescription = "Selected Video Parameters",
                                                          .format = &videoRTPParametersTypeFormat,
                                                          .isOptional = false,
                                                          .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt8TLVFormat comfortNoisePayloadTypeFormat = { .type = kHAPTLVFormatType_UInt8,
                                                          .constraints = { .minimumValue = 0, .maximumValue = 0xFF },
                                                          .callbacks = { .getBitDescription = NULL,
                                                                         .getDescription = NULL } };

const HAPStructTLVMember comfortNoisePayloadTypeMember = {
    .valueOffset = HAP_OFFSETOF(audioRtpParameters, comfortNoisePayload),
    .isSetOffset = 0,
    .tlvType = 6,
    .debugDescription = "Comfort Noise Payload Type",
    .format = &comfortNoisePayloadTypeFormat,
    .isOptional = false,
    .isFlat = false
};

/* ---------------------------------------------------------------------------------------------*/

const AudioRTPParametersTypeFormat audioRTPParametersTypeFormat = { .type = kHAPTLVFormatType_Struct,
                                                                    .members =
                                                                            (const HAPStructTLVMember* const[]) {
                                                                                    // &audioRtpParametersMember,
                                                                                    &payloadTypeMember,
                                                                                    &selectedSsrcTypeMember,
                                                                                    &maximumBitrateTypeMember,
                                                                                    &minRTCPIntervalTypeMember,
                                                                                    &comfortNoisePayloadTypeMember,
                                                                                    NULL },
                                                                    .callbacks = { .isValid = isValid } };

const HAPStructTLVMember audioRTPParametersTypeMember = { .valueOffset =
                                                                  HAP_OFFSETOF(selectedAudioParameters, rtpParameters),
                                                          .isSetOffset = 0,
                                                          .tlvType = 3,
                                                          .debugDescription = "Selected Audio RTP Parameters Type",
                                                          .format = &audioRTPParametersTypeFormat,
                                                          .isOptional = false,
                                                          .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const SelectedVideoParametersFormat selectedVideoParametersFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members =
            (const HAPStructTLVMember* const[]) { // &streamingVideoCodecConfigMember,
                                                  &videoCodecTypeMember,
                                                  &videoCodecParamsMember,
                                                  &videoAttributesMember,
                                                  &videoRTPParametersTypeMember,
                                                  NULL },
    .callbacks = { .isValid = isValid }
};

const HAPStructTLVMember SelectedVideoRtpParametersMember = { .valueOffset =
                                                                      HAP_OFFSETOF(selectedRTPStruct, videoParameters),
                                                              .isSetOffset = 
                                                                      HAP_OFFSETOF(selectedRTPStruct, videoParametersIsSet),
                                                              .tlvType = 2,
                                                              .debugDescription = "Selected Video RTP Parameters Type",
                                                              .format = &selectedVideoParametersFormat,
                                                              .isOptional = true,
                                                              .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt8TLVFormat comfortNoiseSelectedTypeFormat = { .type = kHAPTLVFormatType_UInt8,
                                                           .constraints = { .minimumValue = 0, .maximumValue = 1 },
                                                           .callbacks = { .getBitDescription = NULL,
                                                                          .getDescription = NULL } };

const HAPStructTLVMember comfortNoiseSelectedTypeMember = { .valueOffset =
                                                                    HAP_OFFSETOF(selectedAudioParameters, comfortNoise),
                                                            .isSetOffset = 0,
                                                            .tlvType = 4,
                                                            .debugDescription = "Comfort Noise Type",
                                                            .format = &comfortNoiseSelectedTypeFormat,
                                                            .isOptional = false,
                                                            .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const SelectedAudioParametersFormat selectedAudioParametersFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members =
            (const HAPStructTLVMember* const[]) { // &streamingAudioCodecConfigMember,
                                                  &audioCodecTypeMember,
                                                  &audioCodecParamsMember,
                                                  &audioRTPParametersTypeMember,
                                                  &comfortNoiseSelectedTypeMember,
                                                  NULL },
    .callbacks = { .isValid = isValid }
};

const HAPStructTLVMember SelectedAudioRtpParametersMember = { .valueOffset =
                                                                      HAP_OFFSETOF(selectedRTPStruct, audioParameters),
                                                              .isSetOffset = HAP_OFFSETOF(selectedRTPStruct, audioParametersIsSet),
                                                              .tlvType = 3,
                                                              .debugDescription = "Selected Audio RTP Parameters Type",
                                                              .format = &selectedAudioParametersFormat,
                                                              .isOptional = true,
                                                              .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt8TLVFormat controlSessionCommandTypeFormat = { .type = kHAPTLVFormatType_UInt8,
                                                            .constraints = { .minimumValue = 0, .maximumValue = 4 },
                                                            .callbacks = { .getBitDescription = NULL,
                                                                           .getDescription = NULL } };

const HAPStructTLVMember controlSessionCommandTypeMember = { .valueOffset = HAP_OFFSETOF(sessionControl, command),
                                                             .isSetOffset = 0,
                                                             .tlvType = 2,
                                                             .debugDescription = "Command Type",
                                                             .format = &controlSessionCommandTypeFormat,
                                                             .isOptional = false,
                                                             .isFlat = false };

const HAPStructTLVMember controlSessionIdTypeMember = { .valueOffset = HAP_OFFSETOF(sessionControl, sessionId),
                                                        .isSetOffset = 0,
                                                        .tlvType = 1,
                                                        .debugDescription = "Control Session ID Type",
                                                        .format = &sessionIdTypeFormat,
                                                        .isOptional = false,
                                                        .isFlat = false };

const SessionControlTypeFormat sessionControlTypeFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members =
            (const HAPStructTLVMember* const[]) { &controlSessionCommandTypeMember, &controlSessionIdTypeMember, NULL },
    .callbacks = { .isValid = isValid }
};

const HAPStructTLVMember sessionControlTypeMember = { .valueOffset = HAP_OFFSETOF(selectedRTPStruct, control),
                                                      .isSetOffset = 0,
                                                      .tlvType = 1,
                                                      .debugDescription = "Session Control Type",
                                                      .format = &sessionControlTypeFormat,
                                                      .isOptional = false,
                                                      .isFlat = false };
/* ---------------------------------------------------------------------------------------------*/

const SelectedRTPFormat selectedRTPFormatWrite = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &sessionControlTypeMember,
                                                     &SelectedVideoRtpParametersMember,
                                                     &SelectedAudioRtpParametersMember,
                                                     NULL },
    .callbacks = { .isValid = isValid }
};


HAP_RESULT_USE_CHECK
HAPError VerifyCodecConfigTLV(void* actualBytes, size_t numActualBytes) {
    HAPError err;
    HAPTLVReaderRef configReader;
    HAPTLVReaderCreateWithOptions(
            &configReader,
            &(const HAPTLVReaderOptions) {
                    .bytes = actualBytes, .maxBytes = numActualBytes, .numBytes = numActualBytes });
    HAPTLV tlv;
    bool found;
    err = HAPTLVReaderGetNext(&configReader, &found, &tlv);
    HAPAssert(!err);
    if (tlv.type == 1) {
        uint8_t codecBuffer[tlv.value.numBytes];
        HAPRawBufferCopyBytes(codecBuffer, &tlv.value.bytes, tlv.value.numBytes);
        HAPTLVReaderRef codecReader;
        HAPTLVReaderCreateWithOptions(
                &codecReader,
                &(const HAPTLVReaderOptions) {
                        .bytes = codecBuffer, .maxBytes = tlv.value.numBytes, .numBytes = tlv.value.numBytes });
        HAPTLV typeTLV, paramsTLV, attributesTLV;
        typeTLV.type = 1;
        paramsTLV.type = 2;
        attributesTLV.type = 3;
        err = HAPTLVReaderGetAll(&codecReader, (HAPTLV* const[]) { &typeTLV, &paramsTLV, &attributesTLV, NULL });
        HAPAssert(!err);
        HAPLogDebug(&kHAPLog_Default, "tlv type: %d\ntlv size: %lu\n", typeTLV.type, typeTLV.value.numBytes);
        HAPLogDebug(&kHAPLog_Default, "tlv type: %d\ntlv size: %lu\n", paramsTLV.type, paramsTLV.value.numBytes);
        HAPLogDebug(
                &kHAPLog_Default, "tlv type: %d\ntlv size: %lu\n", attributesTLV.type, attributesTLV.value.numBytes);

        uint8_t attributesBuffer[attributesTLV.value.numBytes];
        HAPRawBufferCopyBytes(attributesBuffer, &attributesTLV.value.bytes, attributesTLV.value.numBytes);
        HAPTLVReaderRef attributesReader;
        HAPTLVReaderCreateWithOptions(
                &attributesReader,
                &(const HAPTLVReaderOptions) { .bytes = attributesBuffer,
                                               .maxBytes = attributesTLV.value.numBytes,
                                               .numBytes = attributesTLV.value.numBytes });
        HAPTLV widthTLV, heightTLV, framerateTLV;
        widthTLV.type = 1;
        heightTLV.type = 2;
        framerateTLV.type = 3;
        err = HAPTLVReaderGetAll(&attributesReader, (HAPTLV* const[]) { &widthTLV, &heightTLV, &framerateTLV, NULL });
        HAPAssert(!err);
        //((const uint8_t*) tlvs->stateTLV->value.bytes)[0]
        HAPLogDebug(
                &kHAPLog_Default,
                "width: %u\nheight: %u\n:framerate: %d\n",
                ((const uint16_t*) widthTLV.value.bytes)[0],
                ((const uint16_t*) heightTLV.value.bytes)[0],
                ((const uint8_t*) framerateTLV.value.bytes)[0]);
    }
    return err;
};

extern AccessoryConfiguration accessoryConfiguration;

HAP_RESULT_USE_CHECK
HAPError HandleStreamingStatusRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {

    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    HAPLogInfo(&kHAPLog_Default, "streaming state: %d", accessoryConfiguration.state.streaming);
    HAPError err;

    err = HAPTLVWriterAppend(
            responseWriter,
            &(const HAPTLV) { .type = 1, // Streaming Status
                              .value = { .bytes = &accessoryConfiguration.state.streaming, .numBytes = 1 } });
    HAPAssert(!err);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleSupportedAudioRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter HAP_UNUSED,
        void* _Nullable context HAP_UNUSED) {

    HAPPrecondition(responseWriter);

    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    return HAPTLVWriterEncode(responseWriter, &supportedAudioConfigFormat, &supportedAudioConfigValue);
}

HAP_RESULT_USE_CHECK
HAPError HandleSupportedVideoRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(responseWriter);

    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    HAPError err;

    const HAPTLV videoConfig = { .type = 0x01,
                                 .value = { .bytes =
                                                    (uint8_t[]) {
                                                            0x01,
                                                            0x01,
                                                            0x00, // CodecType
                                                            0x02,
                                                            0x18, // CodecParams
                                                            0x01,
                                                            0x01,
                                                            0x00, // ProfileId - Constrained
                                                            0x01,
                                                            0x01,
                                                            0x01, // ProfileId - Main
                                                            0x01,
                                                            0x01,
                                                            0x02, // ProfileId - High
                                                            0x02,
                                                            0x01,
                                                            0x00, // Level 3.1
                                                            0x02,
                                                            0x01,
                                                            0x01, // Level 3.2
                                                            0x02,
                                                            0x01,
                                                            0x02, // Level 4
                                                            0x03,
                                                            0x01,
                                                            0x00, // PacketMode
                                                            0x04,
                                                            0x01,
                                                            0x00, // CVO Enabled false
                                                            0x03, 0x0B,             // Attributes
                                                            0x01, 0x02, 0x80, 0x07, // Width 1920 - bytes flipped
                                                            0x02, 0x02, 0x38, 0x04, // Height 1080 - bytes flipped
                                                            0x03, 0x01, 0x18,       // Framerate
                                                            0xFF, 0x00,              // Seperator
                                                            0x03, 0x0B,             // Attributes
                                                            0x01, 0x02, 0x00, 0x05, // Width 1280 - bytes flipped
                                                            0x02, 0x02, 0xD0, 0x02, // Height 720 - bytes flipped
                                                            0x03, 0x01, 0x18,       // Framerate
                                                            0xFF, 0x00,              // Seperator
                                                            0x03, 0x0B,             // Attributes
                                                            0x01, 0x02, 0x00, 0x04, // Width 1024 - bytes flipped
                                                            0x02, 0x02, 0x40, 0x02, // Height 576 - bytes flipped
                                                            0x03, 0x01, 0x18,       // Framerate
                                                            0xFF, 0x00,              // Seperator
                                                            0x03, 0x0B,             // Attributes
                                                            0x01, 0x02, 0x20, 0x03, // Width 800 - bytes flipped
                                                            0x02, 0x02, 0xc2, 0x01, // Height 450 - bytes flipped
                                                            0x03, 0x01, 0x18,       // Framerate
                                                            0xFF, 0x00, // Seperator
                                                            0x03, 0x0B, // Attributes
                                                            0x01, 0x02, 0x80, 0x02, // Width 640 - bytes flipped
                                                            0x02, 0x02, 0x68, 0x01, // Height 360 - byte flipped
                                                            0x03, 0x01, 0x18, // Framerate
                                                            0xFF, 0x00, // Seperator
                                                            0x03, 0x0B, // Attributes
                                                            //for apple watch
                                                            0x01, 0x02, 0x40, 0x01, // Width 320 - bytes flipped
                                                            0x02, 0x02, 0xF0, 0x00, // Height 240 - bytes flipped
                                                            0x03, 0x01, 0x0F // Framerate
                                                    },
                                            .numBytes = 0x48 + 15 * 3 } }; // 12 * 3 + 6 * 4 + 6 * 2 + 15 *3
    // err = VerifyCodecConfigTLV((void*) &videoConfig, 72);
    // HAPAssert(!err);
    err = HAPTLVWriterAppend(responseWriter, &videoConfig);
    return err;
}

HAP_RESULT_USE_CHECK
HAPError HandleSupportedRTPConfigRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(responseWriter);

    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    HAPError err;

    uint8_t supportedSRTPcryptoSuite = 1; // 0 - AES_CM_128_HMAC_SHA1_80. 1 - aes256

    err = HAPTLVWriterAppend(
            responseWriter,
            &(const HAPTLV) { .type = 2, // SRTP Crypto Suite
                              .value = { .bytes = &supportedSRTPcryptoSuite, .numBytes = 1 } });
    HAPAssert(!err);

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleSelectedRTPConfigRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter HAP_UNUSED,
        void* _Nullable context HAP_UNUSED) {

    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleSelectedRTPConfigWrite(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicWriteRequest* request HAP_UNUSED,
        HAPTLVReaderRef* requestReader,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(requestReader);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    AccessoryContext* myContext = context;
    selectedRTPStruct selectedRtp;
    HAPError err;
    selectedRtp.videoParameters = myContext->session.videoParameters;
    selectedRtp.audioParameters = myContext->session.audioParameters;
    
    err = HAPTLVReaderDecode(requestReader, &selectedRTPFormatWrite, &selectedRtp);

    if (HAPRawBufferAreEqual(&(myContext->session.sessionId), &(selectedRtp.control.sessionId), UUIDLENGTH)) {
        myContext->session.videoParameters = selectedRtp.videoParameters;
        myContext->session.audioParameters = selectedRtp.audioParameters;
        if (selectedRtp.control.command == kHAPCharacteristicValue_RTPCommand_Start) {
            if(accessoryConfiguration.state.rtp.active){
                // Added with support for HKSV
                HAPLogDebug(&kHAPLog_Default, "Starting stream");
                accessoryConfiguration.state.streaming = kHAPCharacteristicValue_StreamingStatus_InUse;
                posStartStream(context);
            } else {
                HAPLogError(&kHAPLog_Default, "Attempted to start a stream while streaming is not enabled");
            }
        } else if (selectedRtp.control.command == kHAPCharacteristicValue_RTPCommand_End) {
            HAPLogDebug(&kHAPLog_Default, "Ending Stream");
            accessoryConfiguration.state.streaming = kHAPCharacteristicValue_StreamingStatus_Available;
            posStopStream(context);
        } else if (selectedRtp.control.command == kHAPCharacteristicValue_RTPCommand_Reconfigure) {
            HAPLogDebug(&kHAPLog_Default, "Reconfiguring Stream");
            posReconfigureStream(context);
        } else if (selectedRtp.control.command == kHAPCharacteristicValue_RTPCommand_Suspend) {
            HAPLogDebug(&kHAPLog_Default, "Suspending Stream");
            posSuspendStream(context);
        } else if (selectedRtp.control.command == kHAPCharacteristicValue_RTPCommand_Resume) {
            HAPLogDebug(&kHAPLog_Default, "Resuming Stream");
            posResumeStream(context);
        } else {
            HAPLogDebug(&kHAPLog_Default, "Unknown stream configuration control command: %d", selectedRtp.control.command);
        }
    }
    else {
        HAPLogError(&kHAPLog_Default, "Session ID doesn't match the session ID in setup endpoints ");
    }


    return kHAPError_None;
}


int OpenSocket(
    char *localAddr, // out, needs to be INET6_ADDRSTRLEN long
    uint16_t *localPort, // out 
    char *remoteAddr, // in
    uint16_t remotePort){ // in

    // Open the socket on read endpoint to get the outbound IP address and port
    // adapted from 'man getaddrinfo'
    int              sfd, s;
    size_t           len;
    ssize_t          nread;
    struct addrinfo  hints;
    struct addrinfo  *result, *rp;

    /* Obtain address(es) matching host video address/port. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    char portStr[32];
    snprintf(portStr, 32, "%d", remotePort);
    s = getaddrinfo(remoteAddr, (char *)&portStr, &hints, &result);
    if (s != 0) {
        HAPLogError(&kHAPLog_Default, "getaddrinfo failed: %s\n", gai_strerror(s));
        return -1;
    }

    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully connect(2).
        If socket(2) (or connect(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                    rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

        close(sfd);
    }
    freeaddrinfo(result);           /* No longer needed */
    if (rp == NULL) {               /* No address succeeded */
        HAPLogError(&kHAPLog_Default, "Could not connect to controller port at: %s:%d", 
                    remoteAddr, 
                    remotePort);
        return -1;
    }

    HAPLogDebug(&kHAPLog_Default, "Connection success to controller port at: %s:%d", 
                    remoteAddr, 
                    remotePort);
    HAPLogDebug(&kHAPLog_Default, "Connection success to controller. FD: %d", sfd);

    struct sockaddr_in localAddress;
    socklen_t addressLength = sizeof(localAddress);
    if (getsockname(sfd, (struct sockaddr*)&localAddress,&addressLength) != 0){
        HAPLogError(&kHAPLog_Default, "getsockname failed.");
        return -1;
    }

    *localPort = ntohs(localAddress.sin_port);
    strncpy(localAddr, inet_ntoa( localAddress.sin_addr), INET6_ADDRSTRLEN);
    HAPLogDebug(&kHAPLog_Default, "Local connection at: %s:%d", 
                localAddr, 
                *localPort);
    return sfd;
}

HAP_RESULT_USE_CHECK
HAPError HandleSetupEndpointsRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter HAP_UNUSED,
        void* _Nullable context) {
    HAPLogDebug(&kHAPLog_Default, "%s", __func__);
    HAPError err;

    AccessoryContext* myContext = context;

    // close the socket in prepairation for new connection
    if(myContext->session.videoThread.socket >  0){
        HAPLogInfo(&kHAPLog_Default, "Closing a videoStream socket");
        close(myContext->session.videoThread.socket);
        myContext->session.videoThread.socket = -1;
        myContext->session.videoFeedbackThread.socket = -1;
    }
    if(myContext->session.audioThread.socket >  0){
        HAPLogInfo(&kHAPLog_Default, "Closing a audioStream socket");
        close(myContext->session.audioThread.socket);
        myContext->session.audioThread.socket = -1;
        myContext->session.audioFeedbackThread.socket = -1;
    }

    int sfd = OpenSocket(
        (char *)&myContext->session.accessoryAddress.ipAddress,
        &myContext->session.accessoryAddress.videoPort,
        (char *)&myContext->session.controllerAddress.ipAddress,
        myContext->session.controllerAddress.videoPort);

    if(sfd > 0){
        myContext->session.videoThread.socket = sfd;
        myContext->session.videoFeedbackThread.socket = sfd;
    } else {
        myContext->session.videoThread.socket = -1;
        myContext->session.videoFeedbackThread.socket = -1;
    }

    HAPLogInfo(&kHAPLog_Default, "Local socket of video connection: %s, %d", 
           myContext->session.accessoryAddress.ipAddress, 
           myContext->session.accessoryAddress.videoPort);

    int sfd_audio = OpenSocket(
        (char *) &myContext->session.accessoryAddress.ipAddress,
        &myContext->session.accessoryAddress.audioPort,
        (char *) &myContext->session.controllerAddress.ipAddress,
        myContext->session.controllerAddress.audioPort);

    if(sfd_audio > 0){
        myContext->session.audioThread.socket = sfd_audio;
        myContext->session.audioFeedbackThread.socket = sfd_audio;
    } else {
        myContext->session.audioThread.socket = -1;
        myContext->session.audioFeedbackThread.socket = -1;
    }

    HAPLogInfo(&kHAPLog_Default, "Local socket of audio connection: %s, %d", 
           myContext->session.accessoryAddress.ipAddress, 
           myContext->session.accessoryAddress.audioPort);
           
    //myContext->session.controllerAddress.ipAddrVersion = 0;
    //myContext->session.accessoryAddress.ipAddrVersion = 0;

    /*&sessionIdTypeMember,
    &setupWriteStatusTypeMember,
    &accessoryAddressTypeMember,
    &videoParamsTypeMember,
    &audioParamsTypeMember,
    &ssrcVideoTypeMember,
    &ssrcAudioTypeMember,*/

    myContext->session.accessoryAddress.ipAddrVersion = myContext->session.controllerAddress.ipAddrVersion;

    /*
    printf("myContext->session.sessionId: %s\n", myContext->session.sessionId);
    printf("myContext->session.setupWriteStatus: %s\n", myContext->session.setupWriteStatus);
    printf("myContext->session.accessoryAddress.audioPort: %d\n", myContext->session.accessoryAddress.audioPort);
    printf("myContext->session.accessoryAddress.ipAddress: %s\n", myContext->session.accessoryAddress.ipAddress);
    printf("myContext->session.accessoryAddress.videoPort, %d\n",myContext->session.accessoryAddress.videoPort);
    printf("myContext->session.accessoryAddress.ipAddrVersion, %d\n", myContext->session.accessoryAddress.ipAddrVersion);
    printf("myContext->session.videoParams.srtpCryptoSuite: %d\n,", myContext->session.videoParams.srtpCryptoSuite);;
    printf("myContext->session.videoParams.srtpMasterKey, %16x\n", myContext->session.videoParams.srtpMasterKey);
    printf("myContext->session.videoParams.srtpMasterSalt: %16x\n", myContext->session.videoParams.srtpMasterSalt);
    printf("myContext->session.audioParams.srtpCryptoSuite: %d\n,", myContext->session.audioParams.srtpCryptoSuite);;
    printf("myContext->session.audioParams.srtpMasterKey, %16x\n", myContext->session.audioParams.srtpMasterKey);
    printf("myContext->session.audioParams.srtpMasterSalt: %16x\n", myContext->session.audioParams.srtpMasterSalt);
    printf("myContext->session.ssrcVideo: %d", myContext->session.ssrcVideo);
    printf("myContext->session.ssrcAudio: %d", myContext->session.ssrcAudio);
    */

    err = HAPTLVWriterEncode(responseWriter, &streamingSessionFormatRead, &(myContext->session));

    return err;
}

HAP_RESULT_USE_CHECK
HAPError HandleSetupEndpointsWrite(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicWriteRequest* request HAP_UNUSED,
        HAPTLVReaderRef* requestReader,
        void* _Nullable context) {
    // HAPPrecondition(requestReader);

    HAPLogDebug(&kHAPLog_Default, "%s", __func__);
    HAPError err;
    AccessoryContext* myContext = context;
    streamingSession* newSession = &(myContext->session);

    // close the socket in prepairation for new connection
    if(myContext->session.videoThread.socket >  0){
        close(myContext->session.videoThread.socket);
        myContext->session.videoThread.socket = -1;
        myContext->session.videoFeedbackThread.socket = -1;
        HAPLogError(&kHAPLog_Default, "Unexpectedly closing an open videoStream socket!");
    }
    if(myContext->session.audioThread.socket >  0){
        close(myContext->session.audioThread.socket);
        myContext->session.audioThread.socket = -1;
        myContext->session.audioFeedbackThread.socket = -1;
        HAPLogError(&kHAPLog_Default, "Unexpectedly closing an open audioStream socket!");
    }
    err = HAPTLVReaderDecode(requestReader, &streamingSessionFormatWrite, newSession);


    return err;
}

HAP_RESULT_USE_CHECK
HAPError HandleMicMuteRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.microphone.muted;
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, *value ? "true" : "false");

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleMicMuteWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, value ? "true" : "false");
    if (accessoryConfiguration.state.microphone.muted != value) {
        accessoryConfiguration.state.microphone.muted = value;

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleMotionDetectedRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.motion.detected;
    //printf("%s: %s\n", __func__, *value ? "true" : "false");
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, *value ? "true" : "false");
    return kHAPError_None;
}

