/*
 * App_Camera.c
 *
 *  Created on: Dec 29, 2020
 *      Author: joe
 */

#include "HAP.h"
#include "HAPBase.h"

#include "App_Camera.h"
#include "App.h"
#include "DB.h"
// #include "streaming.h"
#include "Ffmpeg.h"
#include "util_base64.h"

#include <string.h>

/* ---------------------------------------------------------------------------------------------*/
// TODO - Refactor these helper functions out of here.

bool isValid(void* unsused HAP_UNUSED) {
    return true;
}

/**
 * The callback used to decode a value.
 *
 * @param[out] value                Decoded value.
 * @param      bytes                Encoded value buffer. May be modified.
 * @param      numBytes             Length of encoded value buffer.
 *
 * @return kHAPError_None           If successful.
 * @return kHAPError_InvalidData    If invalid data was encountered while parsing.
 */
HAP_RESULT_USE_CHECK
HAPError decodeIpAddress(in_addr_t* value, void* bytes, size_t numBytes) {
    in_addr_t* ia = value;
    int s;
    char ipAddr[numBytes + 1]; // Not sure why unless char needs \0
    HAPRawBufferZero(ipAddr, numBytes + 1);
    HAPRawBufferCopyBytes(&ipAddr, bytes, numBytes);
    s = inet_pton(AF_INET, ipAddr, ia);
    if (s <= 0) {
        HAPLogError(&kHAPLog_Default, "%s\n", "Invalid address");
        return kHAPError_InvalidData;
    };
    return kHAPError_None;
};

HAP_RESULT_USE_CHECK
HAPError decodeUInt8value(uint8_t* value, void* bytes, size_t numBytes) {
    HAPRawBufferCopyBytes((uint8_t*) value, (uint8_t*) bytes, numBytes);
    return kHAPError_None;
};

/**
 * The callback used to encode a value.
 *
 * @param      value                Value to encode.
 * @param[out] bytes                Encoded value buffer.
 * @param      maxBytes             Capacity of encoded value buffer.
 * @param[out] numBytes             Length of encoded value buffer.
 *
 * @return kHAPError_None           If successful.
 * @return kHAPError_Unknown        If an unknown error occurred while serializing.
 * @return kHAPError_InvalidState   If serialization is not possible in the current state.
 * @return kHAPError_OutOfResources If out of resources while serializing.
 * @return kHAPError_Busy           If serialization is temporarily not possible.
 */
HAPError encodeIpAddress(in_addr_t* value, void* bytes, size_t maxBytes, size_t* numBytes) {
    if (maxBytes < INET_ADDRSTRLEN) {
        return kHAPError_OutOfResources;
    };
    in_addr_t* ia = value;
    inet_ntop(AF_INET, ia, bytes, INET_ADDRSTRLEN);
    if (bytes == NULL) {
        return kHAPError_Unknown;
    }
    *numBytes = strlen(bytes);
    return kHAPError_None;
};

HAPError encodeUInt8UUIDValue(uint8_t* value, void* bytes, size_t maxBytes HAP_UNUSED, size_t* numBytes) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    HAPRawBufferCopyBytes((uint8_t*) bytes, (uint8_t*) value, UUIDLENGTH);
    *numBytes = UUIDLENGTH;
    return kHAPError_None;
};


/*
HAPError encodeUInt8KeyValue(uint8_t* value, void* bytes, size_t maxBytes HAP_UNUSED, size_t* numBytes) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    util_base64_encode(value, KEYLENGTH, bytes, maxBytes, numBytes);
    //((char *) bytes)[(int *)numBytes] = '\0';
    //printf("\nmaxBytes: %lu, numBytes: %lu, ssrc64length, %lu, value(first byte), %x\n", maxBytes, *numBytes, ssrc64length, (unsigned int ) value);
    printf("\n\n\nKey: %s\n\n\n", bytes);
    return kHAPError_None;
};

HAPError encodeUInt8SaltValue(uint8_t* value, void* bytes, size_t maxBytes HAP_UNUSED, size_t* numBytes) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    util_base64_encode(value, SALTLENGTH, bytes, maxBytes, numBytes);
//    ((char *) bytes)[(int *)numBytes] = '\0';
    printf("\n\n\nSalt: %s\n\n\n", bytes);
    return kHAPError_None;
};
*/

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

HAPError encodeUInt8SaltValue(uint8_t* value, void* bytes, size_t maxBytes HAP_UNUSED, size_t* numBytes) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    HAPRawBufferCopyBytes((uint8_t*) bytes, (uint8_t*) value, SALTLENGTH);
    *numBytes = SALTLENGTH;
    return kHAPError_None;
};


/**
 * The callback used to get the description of a value.
 *
 * @param      value                Valid value.
 * @param[out] bytes                Buffer to fill with the value's description. Will be NULL-terminated.
 * @param      maxBytes             Capacity of buffer.
 *
 * @return kHAPError_None           If successful.
 * @return kHAPError_OutOfResources If the supplied buffer is not large enough.
 */

HAPError getIpAddressDescription(in_addr_t* value, char* bytes, size_t maxBytes) {
    if (maxBytes < INET_ADDRSTRLEN) {
        return kHAPError_OutOfResources;
    };
    in_addr_t* ia = value;
    inet_ntop(AF_INET, ia, bytes, INET_ADDRSTRLEN);
    return kHAPError_None;
};

HAPError getUInt8ValueDescription(uint8_t* value HAP_UNUSED, char* bytes, size_t maxBytes HAP_UNUSED) {
    char description[] = "UInt8 Value Description";
    HAPRawBufferCopyBytes(bytes, description, sizeof(description));
    //snprintf(bytes,maxBytes,"%s",
    //HAPRawBufferCopyBytes(bytes, value, sizeof(value));

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
            .sampleRate = 1      // 16kHz 8 not supported on MBP
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
                          .videoAttributes = { .imageWidth = 1920, .imageHeight = 1080, .frameRate = 30 } }
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
                                                              .isOptional = false,
                                                              .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt8TLVFormat videoCodecTypeFormat = { .type = kHAPTLVFormatType_UInt8,
                                                 .constraints = { .minimumValue = 0, .maximumValue = 1 },
                                                 .callbacks = { .getDescription = NULL } };
const HAPStructTLVMember videoCodecTypeMember = { .valueOffset = HAP_OFFSETOF(videoCodecConfigStruct, videoCodecType),
                                                  .isSetOffset = 0,
                                                  .tlvType = 1,
                                                  .debugDescription = "Video Codec Type",
                                                  .format = &videoCodecTypeFormat,
                                                  .isOptional = false,
                                                  .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const VideoCodecConfigFormat videoCodecParamsFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &videoCodecParamsProfileIDMember,
                                                     &videoCodecParamsLevelMember,
                                                     &videoCodecParamsPacketizationModeMember,
                                                     &videoCodecParamsCVOEnabledMember,
                                                     NULL },
    .callbacks = { .isValid = isValid }
};
const HAPStructTLVMember videoCodecParamsMember = { .valueOffset =
                                                            HAP_OFFSETOF(videoCodecConfigStruct, videoCodecParams),
                                                    .isSetOffset = 0,
                                                    .tlvType = 2,
                                                    .debugDescription = "Video Codec Parameters",
                                                    .format = &videoCodecParamsFormat,
                                                    .isOptional = false,
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
HAP_VALUE_TLV_SUPPORT(in_addr_t, IpAddressTypeFormat);
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
                                               .constraints = { .minimumValue = 49152, .maximumValue = 65535 },
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
                                                  .callbacks = { .decode = decodeUInt8value,
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
                                                          .callbacks = { .decode = decodeUInt8value,
                                                                         .encode = encodeUInt8KeyValue,
                                                                         .getDescription = getKeyValueDescription } };

const SrtpMasterSaltTypeFormat srtpMasterSaltTypeFormat = { .type = kHAPTLVFormatType_Value,
                                                            .callbacks = { .decode = decodeUInt8value,
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
                                               .isSetOffset = 0,
                                               .tlvType = 1,
                                               .debugDescription = "Payload Type",
                                               .format = &payloadTypeFormat,
                                               .isOptional = false,
                                               .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPStructTLVMember selectedSsrcTypeMember = { .valueOffset = HAP_OFFSETOF(rtpParameters, ssrc),
                                                    .isSetOffset = 0,
                                                    .tlvType = 2,
                                                    .debugDescription = "Selected SSRC Type",
                                                    .format = &ssrcTypeFormat,
                                                    .isOptional = false,
                                                    .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt16TLVFormat maximumBitrateTypeFormat = { .type = kHAPTLVFormatType_UInt16,
                                                      .constraints = { .minimumValue = 0, .maximumValue = 0xFFFF },
                                                      .callbacks = { .getBitDescription = NULL,
                                                                     .getDescription = NULL } };

const HAPStructTLVMember maximumBitrateTypeMember = { .valueOffset = HAP_OFFSETOF(rtpParameters, maximumBitrate),
                                                      .isSetOffset = 0,
                                                      .tlvType = 3,
                                                      .debugDescription = "Maximum Bit Rate Type",
                                                      .format = &maximumBitrateTypeFormat,
                                                      .isOptional = false,
                                                      .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt32TLVFormat minRTCPIntervalTypeFormat = { .type = kHAPTLVFormatType_UInt32,
                                                       .constraints = { .minimumValue = 0, .maximumValue = 0xFFFFFFFF },
                                                       .callbacks = { .getBitDescription = NULL,
                                                                      .getDescription = NULL } };

const HAPStructTLVMember minRTCPIntervalTypeMember = { .valueOffset = HAP_OFFSETOF(rtpParameters, minRTCPinterval),
                                                       .isSetOffset = 0,
                                                       .tlvType = 4,
                                                       .debugDescription = "Min RTCP Interval Type",
                                                       .format = &minRTCPIntervalTypeFormat,
                                                       .isOptional = false,
                                                       .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------------------------*/

const HAPUInt16TLVFormat maximumMTUTypeFormat = { .type = kHAPTLVFormatType_UInt16,
                                                  .constraints = { .minimumValue = 0, .maximumValue = 0xFFFF },
                                                  .callbacks = { .getBitDescription = NULL, .getDescription = NULL } };

const HAPStructTLVMember maximumMTUTypeMember = { .valueOffset = HAP_OFFSETOF(videoRtpParameters, maxMTU),
                                                  .isSetOffset = 0,
                                                  .tlvType = 5,
                                                  .debugDescription = "Maximum MTU Type",
                                                  .format = &maximumMTUTypeFormat,
                                                  .isOptional = false,
                                                  .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

const VideoRTPParametersTypeFormat videoRTPParametersTypeFormat = { .type = kHAPTLVFormatType_Struct,
                                                                    .members =
                                                                            (const HAPStructTLVMember* const[]) {
                                                                                    // &videoRtpParametersMember,
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

/* const HAPStructTLVMember streamingVideoCodecConfigMember = { .valueOffset = HAP_OFFSETOF(selectedVideoParameters,
   codecConfig), .isSetOffset = 0, .tlvType = 1, .debugDescription = "Video Codec Config", .format =
   &videoCodecConfigFormat, .isOptional = false, .isFlat = true }; */

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
// TODO - Fix this and selectedRTPStruct to align.
const HAPStructTLVMember SelectedVideoRtpParametersMember = { .valueOffset =
                                                                      HAP_OFFSETOF(selectedRTPStruct, videoParameters),
                                                              .isSetOffset = 0,
                                                              .tlvType = 2,
                                                              .debugDescription = "Selected Video RTP Parameters Type",
                                                              .format = &selectedVideoParametersFormat,
                                                              .isOptional = false,
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

/* const HAPStructTLVMember streamingAudioCodecConfigMember = { .valueOffset = HAP_OFFSETOF(selectedAudioParameters,
   codecConfig), .isSetOffset = 0, .tlvType = 1, .debugDescription = "Audio Codec Config", .format =
   &audioCodecConfigFormat, .isOptional = false, .isFlat = true }; */

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
                                                              .isSetOffset = 0,
                                                              .tlvType = 3,
                                                              .debugDescription = "Selected Audio RTP Parameters Type",
                                                              .format = &selectedAudioParametersFormat,
                                                              .isOptional = false,
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

// TODO - Use this instead of actually putting it in HandlSupportedVideoRead.
const HAPTLV videoConfig = { .type = 0x01,
                             .value = { .bytes =
                                                (uint8_t[]) {
                                                        0x01, 0x01, 0x00,       // CodecType
                                                        0x02, 0x18,             // CodecParams
                                                        0x01, 0x01, 0x00,       // ProfileId - Constrained
                                                        0x01, 0x01, 0x01,       // ProfileId - Main
                                                        0x01, 0x01, 0x02,       // ProfileId - High
                                                        0x02, 0x01, 0x00,       // Level 3.1
                                                        0x02, 0x01, 0x01,       // Level 3.2
                                                        0x02, 0x01, 0x02,       // Level 4
                                                        0x03, 0x01, 0x00,       // PacketMode
                                                        0x04, 0x01, 0x00,       // CVO Enabled false
                                                        0x03, 0x0B,             // Attributes
                                                        0x01, 0x02, 0x80, 0x07, // Width 1920 - bytes flipped
                                                        0x02, 0x02, 0x38, 0x04, // Height 1080 - bytes flipped
                                                        0x03, 0x01, 0x1E,       // Framerate
                                                        0x03, 0x0B,             // Attributes
                                                        0x01, 0x02, 0x00, 0x05, // Width 1280 - bytes flipped
                                                        0x02, 0x02, 0xD0, 0x02, // Height 720 - bytes flipped
                                                        0x03, 0x01, 0x1E,       // Framerate
                                                        0x03, 0x0B,             // Attributes
                                                        0x01, 0x02, 0x40, 0x01, // Width 320 - bytes flipped
                                                        0x02, 0x02, 0xF0, 0x00, // Height 240 - bytes flipped
                                                        0x03, 0x01, 0x0F        // Framerate
                                                },
                                        .numBytes = 0x44 } };

/* ---------------------------------------------------------------------------------------------*/

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
        HAPRawBufferCopyBytes(codecBuffer, &tlv.value.bytes[0], tlv.value.numBytes);
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
        HAPRawBufferCopyBytes(attributesBuffer, &attributesTLV.value.bytes[0], attributesTLV.value.numBytes);
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

    handleAudioRead(responseWriter);
    return kHAPError_None;
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

    // const HAPTLV videoCodecTypeTLV = { .type = 1, .value = { .bytes = (uint8_t[]) { 0 }, .numBytes = 1 } };

    // const HAPTLV videoCodecParamsTLV = {
    //     .type = 2,
    //     .value = { .bytes = (const HAPTLV* const[]) { &(const HAPTLV) { .type = 1,
    //                                                                     .value = { .bytes = (uint8_t[]) { 1 },
    //                                                                                .numBytes = 1 } }, // profileId
    //                                                   &(const HAPTLV) { .type = 2,
    //                                                                     .value = { .bytes = (uint8_t[]) { 2 },
    //                                                                                .numBytes = 1 } }, // level
    //                                                   &(const HAPTLV) { .type = 3,
    //                                                                     .value = { .bytes = (uint8_t[]) { 0 },
    //                                                                                .numBytes = 1 } }, // packetMode
    //                                                   NULL },
    //                .numBytes = 3 + (3 * 2) }
    // };

    // const HAPTLV videoCVOEnabledTLV = { .type = 4, .value = { .bytes = (uint8_t[]) { 0 }, .numBytes = 1 } };

    // const HAPTLV videoAttributes1080TLV = {
    //     .type = 3,
    //     .value = { .bytes = (const HAPTLV* const[]) { &(const HAPTLV) { .type = 1,
    //                                                                     .value = { .bytes = (uint16_t[]) { 1920 },
    //                                                                                .numBytes = 2 } }, // Width
    //                                                   &(const HAPTLV) { .type = 2,
    //                                                                     .value = { .bytes = (uint16_t[]) { 1080 },
    //                                                                                .numBytes = 2 } }, // Height
    //                                                   &(const HAPTLV) { .type = 3,
    //                                                                     .value = { .bytes = (uint8_t[]) { 30 },
    //                                                                                .numBytes = 1 } }, // Framerate
    //                                                   NULL },
    //                .numBytes = 5 + (3 * 2) }
    // };

    // const HAPTLV videoAttributes720TLV = {
    //     .type = 3,
    //     .value = { .bytes = (const HAPTLV* const[]) { &(const HAPTLV) { .type = 1,
    //                                                                     .value = { .bytes = (uint16_t[]) { 1280 },
    //                                                                                .numBytes = 2 } }, // Width
    //                                                   &(const HAPTLV) { .type = 2,
    //                                                                     .value = { .bytes = (uint16_t[]) { 720 },
    //                                                                                .numBytes = 2 } }, // Height
    //                                                   &(const HAPTLV) { .type = 3,
    //                                                                     .value = { .bytes = (uint8_t[]) { 30 },
    //                                                                                .numBytes = 1 } }, // Framerate
    //                                                   NULL },
    //                .numBytes = 5 + (3 * 2) }
    // };

    // const HAPTLV videoAttributes320TLV = {
    //     .type = 3,
    //     .value = { .bytes = (const HAPTLV* const[]) { &(const HAPTLV) { .type = 1,
    //                                                                     .value = { .bytes = (uint16_t[]) { 320 },
    //                                                                                .numBytes = 2 } }, // Width
    //                                                   &(const HAPTLV) { .type = 2,
    //                                                                     .value = { .bytes = (uint16_t[]) { 240 },
    //                                                                                .numBytes = 2 } }, // Height
    //                                                   &(const HAPTLV) { .type = 3,
    //                                                                     .value = { .bytes = (uint8_t[]) { 15 },
    //                                                                                .numBytes = 1 } }, // Framerate
    //                                                   NULL },
    //                .numBytes = 5 + (3 * 2) }
    // };

    // const HAPTLV videoAttributes640TLV = {
    //     .type = 3,
    //     .value = { .bytes = (const HAPTLV* const[]) { &(const HAPTLV) { .type = 1,
    //                                                                     .value = { .bytes = (uint16_t[]) { 640 },
    //                                                                                .numBytes = 2 } }, // Width
    //                                                   &(const HAPTLV) { .type = 2,
    //                                                                     .value = { .bytes = (uint16_t[]) { 480 },
    //                                                                                .numBytes = 2 } }, // Height
    //                                                   &(const HAPTLV) { .type = 3,
    //                                                                     .value = { .bytes = (uint8_t[]) { 30 },
    //                                                                                .numBytes = 1 } }, // Framerate
    //                                                   NULL },
    //                .numBytes = 5 + (3 * 2) }
    // };

    // const HAPTLV videoCodecConfigTLV = {
    //     .type = 1,
    //     .value = { .bytes = (const HAPTLV* const[]) { &videoCodecTypeTLV,
    //                                                   &videoCodecParamsTLV,
    //                                                   &videoAttributes1080TLV,
    //                                                   &(const HAPTLV) { .type = kHAPPairingTLVType_Separator, .value
    //                                                   = { .bytes = NULL, .numBytes = 0 } }, &videoAttributes720TLV,
    //                                                   &(const HAPTLV) { .type = kHAPPairingTLVType_Separator, .value
    //                                                   = { .bytes = NULL, .numBytes = 0 } }, &videoAttributes320TLV,
    //                                                   &(const HAPTLV) { .type = kHAPPairingTLVType_Separator, .value
    //                                                   = { .bytes = NULL, .numBytes = 0 } }, &videoAttributes640TLV,
    //                                                   &videoCVOEnabledTLV,
    //                                                   NULL },
    //                .numBytes = videoCodecTypeTLV.value.numBytes + videoCodecParamsTLV.value.numBytes +
    //                            videoAttributes1080TLV.value.numBytes + videoAttributes720TLV.value.numBytes +
    //                            +videoAttributes320TLV.value.numBytes + videoAttributes640TLV.value.numBytes +
    //                            videoCVOEnabledTLV.value.numBytes + (5 * 2) +6 }
    // };
    // err = HAPTLVWriterAppend(responseWriter, &videoCodecConfigTLV);
    // HAPAssert(!err);
    // return kHAPError_None;
    // TODO - What am I even doing in the rest of this function?
    // err = HAPTLVWriterEncode(responseWriter, &supportedVideoConfigFormat, &supportedVideoConfigValue);
    // HAPAssert(!err);
    // return kHAPError_None;

    // err = handleVideoRead(responseWriter);
    // return err;
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
                                                            0x03,
                                                            0x0B, // Attributes
                                                            0x01,
                                                            0x02,
                                                            0x80,
                                                            0x07, // Width 1920 - bytes flipped
                                                            0x02,
                                                            0x02,
                                                            0x38,
                                                            0x04, // Height 1080 - bytes flipped
                                                            0x03,
                                                            0x01,
                                                            0x1E, // Framerate
                                                            // 0xFF, 0x00,              // Seperator
                                                            // 0x03, 0x0B,             // Attributes
                                                            // 0x01, 0x02, 0x00, 0x05, // Width 1280 - bytes flipped
                                                            // 0x02, 0x02, 0xD0, 0x02, // Height 720 - bytes flipped
                                                            // 0x03, 0x01, 0x1E,       // Framerate
                                                            0xFF,
                                                            0x00, // Seperator
                                                            0x03,
                                                            0x0B, // Attributes
                                                            0x01,
                                                            0x02,
                                                            0x80,
                                                            0x02, // Width 640 - bytes flipped
                                                            0x02,
                                                            0x02,
                                                            0xE0,
                                                            0x01, // Height 480 - byte flipped
                                                            0x03,
                                                            0x01,
                                                            0x1E, // Framerate
                                                            0xFF,
                                                            0x00, // Seperator
                                                            0x03,
                                                            0x0B, // Attributes
                                                            0x01,
                                                            0x02,
                                                            0x40,
                                                            0x01, // Width 320 - bytes flipped
                                                            0x02,
                                                            0x02,
                                                            0xF0,
                                                            0x00, // Height 240 - bytes flipped
                                                            0x03,
                                                            0x01,
                                                            0x0F // Framerate
                                                    },
                                            .numBytes = 0x48 } }; // 12 * 3 + 6 * 4 + 6 * 2
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

    uint8_t supportedSRTPcryptoSuite = 0; // 0 - AES_CM_128_HMAC_SHA1_80

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

    err = HAPTLVReaderDecode(requestReader, &selectedRTPFormatWrite, &selectedRtp);

    if (HAPRawBufferAreEqual(myContext->session.sessionId, selectedRtp.control.sessionId, UUIDLENGTH)) {
        myContext->session.videoParameters = selectedRtp.videoParameters;
        myContext->session.audioParameters = selectedRtp.audioParameters;
        if (selectedRtp.control.command == kHAPCharacteristicValue_RTPCommand_Start) {
            HAPLogDebug(&kHAPLog_Default, "Starting stream.");
            pthread_create(&myContext->streamingThread, NULL, startOutStream, context);
            accessoryConfiguration.state.streaming = kHAPCharacteristicValue_StreamingStatus_InUse;
            // startStream(context);
        } else if (selectedRtp.control.command == kHAPCharacteristicValue_RTPCommand_End) {
            HAPLogDebug(&kHAPLog_Default, "Ending Stream.");
            accessoryConfiguration.state.streaming = kHAPCharacteristicValue_StreamingStatus_Available;
            if (myContext->streamingThread) {
                int s;
                s = pthread_cancel(myContext->streamingThread);
                if (s) {
                    HAPLogDebug(&kHAPLog_Default, "Thread didn't cancel.");
                }
                myContext->streamingThread = 0;
            }
        } else if (selectedRtp.control.command == kHAPCharacteristicValue_RTPCommand_Reconfigure) {
            HAPLogDebug(&kHAPLog_Default, "Reconfiguring Stream.");
            /* code */
        } else {
            HAPLogDebug(&kHAPLog_Default, "control command: %d", selectedRtp.control.command);
        }
    }

    // HAPError err;

    // const HAPTLVReader* myReader = (const HAPTLVReader*)requestReader;
    // HAPLogDebug(&kHAPLog_Default, "numBytes: %lu\n", myReader->numBytes);
    // for (size_t i = 0; i < myReader->numBytes; i++)
    // {
    //     HAPLogDebug(&kHAPLog_Default, "0x%02X", ((uint8_t*)myReader->bytes)[i]);
    // }

    /*    HAPTLV sessionControlTLV, selectedVideoTLV, selectedAudioTLV;
        sessionControlTLV.type = 1;
        selectedVideoTLV.type = 2;
        selectedAudioTLV.type = 3;

         HAPTLV* tlvs[] = {&sessionControlTLV, &selectedVideoTLV, &selectedAudioTLV, NULL};

        // Simply validate input.
        err = HAPTLVReaderGetAll(requestReader, tlvs);
        if (err) {
            HAPAssert(err == kHAPError_InvalidData);
            return err;
        }

        for (HAPTLV* const* tlvItem = tlvs; *tlvItem; tlvItem++) {
            HAPLogDebug(&kHAPLog_Default, "tlvType: %d, tlvSize: %lu\n", (*tlvItem)->type, (*tlvItem)->value.numBytes );
        } */
    // Get TLV item.
    // HAPTLV tlv;
    // bool valid;
    // err = HAPTLVReaderGetNext(&reader, &valid, &tlv);
    // HAPAssert(!err);
    // HAPAssert(valid);

    // Compare TLV item.
    // HAPAssert(tlv.type == (*tlvItem)->type);
    // HAPAssert(tlv.value.numBytes == (*tlvItem)->value.numBytes);
    // if (!(*tlvItem)->value.bytes) {
    //     HAPAssert(!tlv.value.bytes);
    // } else {
    //     HAPAssert(tlv.value.bytes);
    //     HAPAssert(HAPRawBufferAreEqual(
    //             HAPNonnullVoid(tlv.value.bytes), HAPNonnullVoid((*tlvItem)->value.bytes), tlv.value.numBytes));

    //     // Check for NULL-terminator after TLV.
    //     HAPAssert(!((const uint8_t*) tlv.value.bytes)[tlv.value.numBytes]);
    // }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleSetupEndpointsRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter HAP_UNUSED,
        void* _Nullable context) {

    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    HAPError err;
    AccessoryContext* myContext = context;
    // streamingSession* newSession = &(myContext->session);

    // HAPLogDebug(&kHAPLog_Default, "",
    // myContext->session.accessoryAddress.
    // )

    err = HAPTLVWriterEncode(responseWriter, &streamingSessionFormatRead, &(myContext->session));

    // err = handleSessionRead(responseWriter, &(myContext->session));

    return err;
    /*
        HAPError err;

        void* bytes;
        size_t maxBytes;
        HAPTLVWriterGetScratchBytes(responseWriter, &bytes, &maxBytes);

        err = HAPTLVWriterAppend(
                responseWriter,
                &(const HAPTLV) { .type = 2, // Status
                                  .value = { .bytes = &accessoryConfiguration.state.streaming, .numBytes = 1 } });

        err = HAPTLVWriterAppend(
                responseWriter,
                &(const HAPTLV) { .type = 3, // Address
                                  .value = { .bytes = &accessoryConfiguration.ipAddress, .numBytes = 9 } });

        HAPAssert(!err);
     */
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleSetupEndpointsWrite(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicWriteRequest* request HAP_UNUSED,
        HAPTLVReaderRef* requestReader,
        void* _Nullable context) {
    // HAPPrecondition(requestReader);

    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    HAPError err;
    AccessoryContext* myContext = context;
    streamingSession* newSession = &(myContext->session);
    // controllerAddressStruct* accesoryAddress = &(newSession->accessoryAddress);

    // err = handleSessionWrite(requestReader, newSession);
    err = HAPTLVReaderDecode(requestReader, &streamingSessionFormatWrite, newSession);

    newSession->accessoryAddress.audioPort = newSession->controllerAddress.audioPort;
    newSession->accessoryAddress.videoPort = newSession->controllerAddress.videoPort;
    newSession->setupWriteStatus = 0;

    return kHAPError_None;
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
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, *value ? "true" : "false");
    return kHAPError_None;
}

/**
 * @file streaming.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2021-01-20
 *
 * @copyright Copyright (c) 2021
 *
 */

// #include "streaming.h"
// #include "HAPTLV+Internal.h"
// #include <string.h>

// static bool isValid(void* unsused HAP_UNUSED) {
//     return true;
// }

// supportedAudioConfigStruct supportedAudioConfigValue =
// {
//     .audioCodecConfig =
//     {
//         .audioCodecType = 2,     // AAC-ELD
//         .audioCodecParams =
//         {
//             .audioChannels = 1,  // 1 channel
//             .bitRate = 0,        // Variable
//             .sampleRate = 1      // 16kHz 8 not supported on MBP
//         }
//     },
//     .comfortNoiseSupport = false
// };

// HAP_STRUCT_TLV_SUPPORT(void, SupportedAudioConfigFormat)
// HAP_STRUCT_TLV_SUPPORT(void, AudioCodecConfigFormat)
// HAP_STRUCT_TLV_SUPPORT(void, AudioCodecParamsFormat)

// const HAPUInt8TLVFormat audioCodecParamsAudioChannelsFormat = { .type = kHAPTLVFormatType_UInt8,
//                                                                 .constraints = { .minimumValue = 1, .maximumValue = 1
//                                                                 }, .callbacks = { .getDescription = NULL } };
// const HAPStructTLVMember audioCodecParamsAudioChannelsMember = {
//     .valueOffset = HAP_OFFSETOF(audioCodecParamsStruct, audioChannels),
//     .isSetOffset = 0,
//     .tlvType = 1,
//     .debugDescription = "Audio Codec Config Params Audio Channels",
//     .format = &audioCodecParamsAudioChannelsFormat,
//     .isOptional = false,
//     .isFlat = false
// };

// const HAPUInt8TLVFormat audioCodecParamsBitRateFormat = { .type = kHAPTLVFormatType_UInt8,
//                                                           .constraints = { .minimumValue = 0, .maximumValue = 2 },
//                                                           .callbacks = { .getDescription = NULL } };
// const HAPStructTLVMember audioCodecParamsBitRateMember = { .valueOffset = HAP_OFFSETOF(audioCodecParamsStruct,
// bitRate),
//                                                            .isSetOffset = 0,
//                                                            .tlvType = 2,
//                                                            .debugDescription = "Audio Codec Config Params Bit-Rate",
//                                                            .format = &audioCodecParamsBitRateFormat,
//                                                            .isOptional = false,
//                                                            .isFlat = false };

// // const HAPUInt8TLVFormat audioCodecParamsSampleRateFormat = { .type = kHAPTLVFormatType_UInt8,
// //                                                              .constraints = { .minimumValue = 0, .maximumValue = 2
// },
// //                                                              .callbacks = { .getDescription = NULL } };
// const HAPStructTLVMember audioCodecParamsSampleRateMember = { .valueOffset =
//                                                                       HAP_OFFSETOF(audioCodecParamsStruct,
//                                                                       sampleRate),
//                                                               .isSetOffset = 0,
//                                                               .tlvType = 3,
//                                                               .debugDescription =
//                                                                       "Audio Codec Config Params Sample Rate",
//                                                               .format = &audioCodecParamsSampleRateFormat,
//                                                               .isOptional = false,
//                                                               .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

// const HAPUInt16TLVFormat audioCodecTypeFormat = { .type = kHAPTLVFormatType_UInt16,
//                                                   .constraints = { .minimumValue = 0, .maximumValue = 6 },
//                                                   .callbacks = { .getDescription = NULL } };
// const HAPStructTLVMember audioCodecTypeMember = { .valueOffset = HAP_OFFSETOF(audioCodecConfigStruct,
// audioCodecType),
//                                                   .isSetOffset = 0,
//                                                   .tlvType = 1,
//                                                   .debugDescription = "Audio Codec Type",
//                                                   .format = &audioCodecTypeFormat,
//                                                   .isOptional = false,
//                                                   .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

// const AudioCodecConfigFormat audioCodecParamsFormat = {
//     .type = kHAPTLVFormatType_Struct,
//     .members = (const HAPStructTLVMember* const[]) { &audioCodecParamsAudioChannelsMember,
//                                                      &audioCodecParamsBitRateMember,
//                                                      &audioCodecParamsSampleRateMember,
//                                                      &audioCodecParamsRTPTimeMember,
//                                                      NULL },
//     .callbacks = { .isValid = isValid }
// };
// const HAPStructTLVMember audioCodecParamsMember = { .valueOffset =
//                                                             HAP_OFFSETOF(audioCodecConfigStruct, audioCodecParams),
//                                                     .isSetOffset = 0,
//                                                     .tlvType = 2,
//                                                     .debugDescription = "Audio Codec Parameters",
//                                                     .format = &audioCodecParamsFormat,
//                                                     .isOptional = false,
//                                                     .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

// // const AudioCodecConfigFormat audioCodecConfigFormat = {
// //     .type = kHAPTLVFormatType_Struct,
// //     .members = (const HAPStructTLVMember* const[]) { &audioCodecTypeMember, &audioCodecParamsMember, NULL },
// //     .callbacks = { .isValid = isValid }
// // };
// const HAPStructTLVMember audioCodecConfigMember = { .valueOffset =
//                                                             HAP_OFFSETOF(supportedAudioConfigStruct,
//                                                             audioCodecConfig),
//                                                     .isSetOffset = 0,
//                                                     .tlvType = 1,
//                                                     .debugDescription = "Audio Codec Config",
//                                                     .format = &audioCodecConfigFormat,
//                                                     .isOptional = false,
//                                                     .isFlat = false };

// const HAPUInt8TLVFormat comfortNoiseFormat = { .type = kHAPTLVFormatType_UInt8,
//                                                .constraints = { .minimumValue = 0, .maximumValue = 255 },
//                                                .callbacks = { .getDescription = NULL } };
// const HAPStructTLVMember comfortNoiseMember = { .valueOffset =
//                                                         HAP_OFFSETOF(supportedAudioConfigStruct,
//                                                         comfortNoiseSupport),
//                                                 .isSetOffset = 0,
//                                                 .tlvType = 2,
//                                                 .debugDescription = "Comfort Noise Support",
//                                                 .format = &comfortNoiseFormat,
//                                                 .isOptional = false,
//                                                 .isFlat = false };

// const SupportedAudioConfigFormat supportedAudioConfigFormat = {
//     .type = kHAPTLVFormatType_Struct,
//     .members = (const HAPStructTLVMember* const[]) { &audioCodecConfigMember, &comfortNoiseMember, NULL },
//     .callbacks = { .isValid = isValid }
// };

// supportedVideoConfigStruct supportedVideoConfigValue = {
//     .videoCodecConfig = { .videoCodecType = 0,
//                           .videoCodecParams = { .profileID = 1,
//                                                 .level = 2,
//                                                 .packetizationMode = 0,
//                                                 .CVOEnabled = 0 }, // TODO - Make enums for profileID, and level
//                           .videoAttributes = { .imageWidth = 1920, .imageHeight = 1080, .frameRate = 30 } }
// };

// HAP_STRUCT_TLV_SUPPORT(void, SupportedVideoConfigFormat)
// HAP_STRUCT_TLV_SUPPORT(void, VideoCodecConfigFormat)
// HAP_STRUCT_TLV_SUPPORT(void, VideoCodecParamsFormat)
// HAP_STRUCT_TLV_SUPPORT(void, VideoAttributesFormat)

// const HAPUInt8TLVFormat videoCodecParamsProfileIDFormat = { .type = kHAPTLVFormatType_UInt8,
//                                                             .constraints = { .minimumValue = 0, .maximumValue = 2 },
//                                                             .callbacks = { .getDescription = NULL } };
// const HAPStructTLVMember videoCodecParamsProfileIDMember = { .valueOffset =
//                                                                      HAP_OFFSETOF(videoCodecParamsStruct, profileID),
//                                                              .isSetOffset = 0,
//                                                              .tlvType = 1,
//                                                              .debugDescription = "Video Codec Config Params Profile
//                                                              ID", .format = &videoCodecParamsProfileIDFormat,
//                                                              .isOptional = false,
//                                                              .isFlat = false };

// const HAPUInt8TLVFormat videoCodecParamsLevelFormat = { .type = kHAPTLVFormatType_UInt8,
//                                                         .constraints = { .minimumValue = 0, .maximumValue = 2 },
//                                                         .callbacks = { .getDescription = NULL } };
// const HAPStructTLVMember videoCodecParamsLevelMember = { .valueOffset = HAP_OFFSETOF(videoCodecParamsStruct, level),
//                                                          .isSetOffset = 0,
//                                                          .tlvType = 2,
//                                                          .debugDescription = "Video Codec Config Params Level",
//                                                          .format = &videoCodecParamsLevelFormat,
//                                                          .isOptional = false,
//                                                          .isFlat = false };

// const HAPUInt8TLVFormat videoCodecParamsPacketizationModeFormat = { .type = kHAPTLVFormatType_UInt8,
//                                                                     .constraints = { .minimumValue = 0,
//                                                                                      .maximumValue = 2 },
//                                                                     .callbacks = { .getDescription = NULL } };
// const HAPStructTLVMember videoCodecParamsPacketizationModeMember = {
//     .valueOffset = HAP_OFFSETOF(videoCodecParamsStruct, packetizationMode),
//     .isSetOffset = 0,
//     .tlvType = 3,
//     .debugDescription = "Video Codec Config Packetization Mode",
//     .format = &videoCodecParamsPacketizationModeFormat,
//     .isOptional = false,
//     .isFlat = false
// };

// const HAPUInt8TLVFormat videoCodecParamsCVOEnabledFormat = { .type = kHAPTLVFormatType_UInt8,
//                                                              .constraints = { .minimumValue = 0, .maximumValue = 2 },
//                                                              .callbacks = { .getDescription = NULL } };
// const HAPStructTLVMember videoCodecParamsCVOEnabledMember = { .valueOffset =
//                                                                       HAP_OFFSETOF(videoCodecParamsStruct,
//                                                                       CVOEnabled),
//                                                               .isSetOffset = 0,
//                                                               .tlvType = 4,
//                                                               .debugDescription = "CVO Enabled",
//                                                               .format = &videoCodecParamsCVOEnabledFormat,
//                                                               .isOptional = true,
//                                                               .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

// const HAPUInt8TLVFormat videoCodecTypeFormat = { .type = kHAPTLVFormatType_UInt8,
//                                                  .constraints = { .minimumValue = 0, .maximumValue = 1 },
//                                                  .callbacks = { .getDescription = NULL } };
// const HAPStructTLVMember videoCodecTypeMember = { .valueOffset = HAP_OFFSETOF(videoCodecConfigStruct,
// videoCodecType),
//                                                   .isSetOffset = 0,
//                                                   .tlvType = 1,
//                                                   .debugDescription = "Video Codec Type",
//                                                   .format = &videoCodecTypeFormat,
//                                                   .isOptional = false,
//                                                   .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

// // const VideoCodecConfigFormat videoCodecParamsFormat = {
// //     .type = kHAPTLVFormatType_Struct,
// //     .members = (const HAPStructTLVMember* const[]) { &videoCodecParamsProfileIDMember,
// //                                                      &videoCodecParamsLevelMember,
// //                                                      &videoCodecParamsPacketizationModeMember,
// //                                                      //  &videoCodecParamsCVOEnabledMember,
// //                                                      NULL },
// //     .callbacks = { .isValid = isValid }
// // };
// const HAPStructTLVMember videoCodecParamsMember = { .valueOffset =
//                                                             HAP_OFFSETOF(videoCodecConfigStruct, videoCodecParams),
//                                                     .isSetOffset = 0,
//                                                     .tlvType = 2,
//                                                     .debugDescription = "Video Codec Parameters",
//                                                     .format = &videoCodecParamsFormat,
//                                                     .isOptional = false,
//                                                     .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

// const HAPUInt16TLVFormat videoAttributesImageWidthFormat = { .type = kHAPTLVFormatType_UInt16,
//                                                              .constraints = { .minimumValue = 0, .maximumValue = 4096
//                                                              }, .callbacks = { .getDescription = NULL } };
// const HAPStructTLVMember videoAttributesImageWidthMember = { .valueOffset =
//                                                                      HAP_OFFSETOF(videoAttributesStruct, imageWidth),
//                                                              .isSetOffset = 0,
//                                                              .tlvType = 1,
//                                                              .debugDescription = "Video Attributes Image Width",
//                                                              .format = &videoAttributesImageWidthFormat,
//                                                              .isOptional = false,
//                                                              .isFlat = false };

// const HAPUInt16TLVFormat videoAttributesImageHeightFormat = { .type = kHAPTLVFormatType_UInt16,
//                                                               .constraints = { .minimumValue = 0,
//                                                                                .maximumValue = 4096 },
//                                                               .callbacks = { .getDescription = NULL } };
// const HAPStructTLVMember videoAttributesImageHeightMember = { .valueOffset =
//                                                                       HAP_OFFSETOF(videoAttributesStruct,
//                                                                       imageHeight),
//                                                               .isSetOffset = 0,
//                                                               .tlvType = 2,
//                                                               .debugDescription = "Video Attributes Image Height",
//                                                               .format = &videoAttributesImageHeightFormat,
//                                                               .isOptional = false,
//                                                               .isFlat = false };

// const HAPUInt8TLVFormat videoAttributesFrameRateFormat = { .type = kHAPTLVFormatType_UInt8,
//                                                            .constraints = { .minimumValue = 0, .maximumValue = 255 },
//                                                            .callbacks = { .getDescription = NULL } };
// const HAPStructTLVMember videoAttributesFrameRateMember = { .valueOffset =
//                                                                     HAP_OFFSETOF(videoAttributesStruct, frameRate),
//                                                             .isSetOffset = 0,
//                                                             .tlvType = 3,
//                                                             .debugDescription = "Video Attributes Frame Rate",
//                                                             .format = &videoAttributesFrameRateFormat,
//                                                             .isOptional = false,
//                                                             .isFlat = false };

// const VideoAttributesFormat videoAttributesFormat = {
//     .type = kHAPTLVFormatType_Struct,
//     .members = (const HAPStructTLVMember* const[]) { &videoAttributesImageWidthMember,
//                                                      &videoAttributesImageHeightMember,
//                                                      &videoAttributesFrameRateMember,
//                                                      NULL },
//     .callbacks = { .isValid = isValid }
// };
// const HAPStructTLVMember videoAttributesMember = { .valueOffset = HAP_OFFSETOF(videoCodecConfigStruct,
// videoAttributes),
//                                                    .isSetOffset = 0,
//                                                    .tlvType = 3,
//                                                    .debugDescription = "Video Attributes",
//                                                    .format = &videoAttributesFormat,
//                                                    .isOptional = false,
//                                                    .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

// const VideoCodecConfigFormat videoCodecConfigFormat = {
//     .type = kHAPTLVFormatType_Struct,
//     .members = (const HAPStructTLVMember* const[]) { &videoCodecTypeMember,
//                                                      &videoCodecParamsMember,
//                                                      &videoAttributesMember,
//                                                      NULL },
//     .callbacks = { .isValid = isValid }
// };

// const HAPStructTLVMember videoCodecConfigMember = { .valueOffset =
//                                                             HAP_OFFSETOF(supportedVideoConfigStruct,
//                                                             videoCodecConfig),
//                                                     .isSetOffset = 0,
//                                                     .tlvType = 1,
//                                                     .debugDescription = "Video Codec Config",
//                                                     .format = &videoCodecConfigFormat,
//                                                     .isOptional = false,
//                                                     .isFlat = false };

/* ---------------------------------------------------------------------------------------------*/

// const SupportedVideoConfigFormat supportedVideoConfigFormat = {
//     .type = kHAPTLVFormatType_Struct,
//     .members = (const HAPStructTLVMember* const[]) { &videoCodecConfigMember, NULL },
//     .callbacks = { .isValid = isValid }
// };

/* ---------------------------------------------------------------------------------------------*/

/* const RTPParametersFormat rtpParametersFormat = {
    .type = kHAPTLVFormatType_Struct,
    .members = (const HAPStructTLVMember* const[]) { &payloadTypeMember,
                                                     &selectedSsrcTypeMember,
                                                     &maximumBitrateTypeMember,
                                                     &minRTCPIntervalTypeMember,
                                                     NULL },
    .callbacks = { .isValid = isValid }
}; */

/* const HAPStructTLVMember videoRtpParametersMember = { .valueOffset = HAP_OFFSETOF(videoRtpParameters,
   vRtpParameters), .isSetOffset = 0, .tlvType = 1, .debugDescription = "Video RTP Parameters Type", .format =
   &rtpParametersFormat, .isOptional = false, .isFlat = true }; */

/* const HAPStructTLVMember audioRtpParametersMember = { .valueOffset = HAP_OFFSETOF(audioRtpParameters, rtpParameters),
                                                      .isSetOffset = 0,
                                                      .tlvType = 1,
                                                      .debugDescription = "Audio RTP Parameters Type",
                                                      .format = &rtpParametersFormat,
                                                      .isOptional = false,
                                                      .isFlat = true }; */

/* ---------------------------------------------------------------------------------------------*/

// HAPError handleSessionRead(HAPTLVWriterRef* responseWriter, streamingSession* session) {
//     HAPError err;
//     err = HAPTLVWriterEncode(responseWriter, &streamingSessionFormatRead, session);
//     return err;
// };
// TODO - Remove this function as it's unnecessary.
// HAPError handleSessionWrite(HAPTLVReaderRef* responseReader, streamingSession* session) {
//     HAPError err;
//     err = HAPTLVReaderDecode(responseReader, &streamingSessionFormatWrite, session);
//     return err;
// };

// HAPError handleSelectedWrite(HAPTLVReaderRef* responseReader, selectedRTPStruct* selectedRTP) {
//     HAPError err;
//     err = HAPTLVReaderDecode(responseReader, &selectedRTPFormatWrite, selectedRTP);
//     return err;
// };

void checkFormats() {

    const HAPTLVFormat* formats[] = {
        &sessionControlTypeFormat, &selectedVideoParametersFormat, &selectedAudioParametersFormat, NULL
    };
    for (size_t i = 0; i < 3; i++) {
        bool valid;
        valid = HAPTLVFormatIsValid(formats[i]);
        HAPLogDebug(&kHAPLog_Default, "%zu: %d", i, valid);
    }
}

HAPError handleAudioRead(HAPTLVWriterRef* responseWriter) {
    return HAPTLVWriterEncode(responseWriter, &supportedAudioConfigFormat, &supportedAudioConfigValue);
}

HAPError handleVideoRead(HAPTLVWriterRef* responseWriter) {
    return HAPTLVWriterAppend(responseWriter, &videoConfig);
}
