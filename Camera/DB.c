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

#include "App.h"
#include "DB.h"
#include "App_Camera.h"
#include "App_Secure_Camera.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * IID constants.
 */
#define kIID_AccessoryInformation                 ((uint64_t) 0x0001)
#define kIID_AccessoryInformationIdentify         ((uint64_t) 0x0002)
#define kIID_AccessoryInformationManufacturer     ((uint64_t) 0x0003)
#define kIID_AccessoryInformationModel            ((uint64_t) 0x0004)
#define kIID_AccessoryInformationName             ((uint64_t) 0x0005)
#define kIID_AccessoryInformationSerialNumber     ((uint64_t) 0x0006)
#define kIID_AccessoryInformationFirmwareRevision ((uint64_t) 0x0007)
#define kIID_AccessoryInformationHardwareRevision ((uint64_t) 0x0008)
#define kIID_AccessoryInformationADKVersion       ((uint64_t) 0x0009)
#define kIID_AccessoryInformationProductData      ((uint64_t) 0x000A)

#define kIID_HAPProtocolInformation        ((uint64_t) 0x0010)
#define kIID_HAPProtocolInformationVersion ((uint64_t) 0x0011)

#define kIID_Pairing                ((uint64_t) 0x0020)
#define kIID_PairingPairSetup       ((uint64_t) 0x0022)
#define kIID_PairingPairVerify      ((uint64_t) 0x0023)
#define kIID_PairingPairingFeatures ((uint64_t) 0x0024)
#define kIID_PairingPairingPairings ((uint64_t) 0x0025)

#define kIID_RTPStream          ((uint64_t) 0x0030)
#define kIID_RTPStreamingStatus ((uint64_t) 0x0031)
#define kIID_RTPSelectedStream  ((uint64_t) 0x0032)
#define kIID_RTPSetupEndpoints  ((uint64_t) 0x0033)
#define kIID_RTPSupportedAudio  ((uint64_t) 0x0034)
#define kIID_RTPSupportedConfig ((uint64_t) 0x0035)
#define kIID_RTPSupportedVideo  ((uint64_t) 0x0037)
#define kIID_RTPActive          ((uint64_t) 0x0038)

#define kIID_Microphone     ((uint64_t) 0x0040)
#define kIID_MicrophoneMute ((uint64_t) 0x0041)

#define kIID_MotionSensor   ((uint64_t) 0x0050)
#define kIID_MotionDetected ((uint64_t) 0x0051)
#define kIID_MotionActive ((uint64_t) 0x0052)

#define kIID_CameraOperatingMode    ((uint64_t) 0x0060)
#define kIID_CameraEventSnapshots   ((uint64_t) 0x0061)
#define kIID_CameraHomekitCamActive ((uint64_t) 0x0062)
#define kIID_CameraPeriodicSnapshots   ((uint64_t) 0x0063)
#define kIID_CameraOperatingModeNightVision    ((uint64_t) 0x0064)
#define kIID_CameraOperatingModeIndicator    ((uint64_t) 0x0065)

#define kIID_CameraRecordingManagement     ((uint64_t) 0x0070)
#define kIID_SupportedCamRecordingConfig   ((uint64_t) 0x0071)
#define kIID_SupportedVidRecordingConfig   ((uint64_t) 0x0072)
#define kIID_SupportedAudioRecordingConfig ((uint64_t) 0x0073)
#define kIID_SelectedCamRecordingConfig    ((uint64_t) 0x0074)
#define kIID_CameraRecordingMgmtActive     ((uint64_t) 0x0075)
#define kIID_CameraRecordingMgmtRecAudioActive     ((uint64_t) 0x0076)

#define kIID_DataStreamManagement               ((uint64_t) 0x0080)
#define kIID_SupportedDataStreamTransportConfig ((uint64_t) 0x0081)
#define kIID_SetupDataStreamTransport           ((uint64_t) 0x0082)
#define kIID_DataStreamVersion           ((uint64_t) 0x0083)


HAP_STATIC_ASSERT(kAttributeCount == 10 + 2 + 5 + 8 + 2 + 3 + 6 + 7 + 4, AttributeCount_mismatch);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const HAPBoolCharacteristic accessoryInformationIdentifyCharacteristic = {
    .format = kHAPCharacteristicFormat_Bool,
    .iid = kIID_AccessoryInformationIdentify,
    .characteristicType = &kHAPCharacteristicType_Identify,
    .debugDescription = kHAPCharacteristicDebugDescription_Identify,
    .manufacturerDescription = NULL,
    .properties = { .readable = false,
                    .writable = true,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = NULL, .handleWrite = HAPHandleAccessoryInformationIdentifyWrite }
};

const HAPStringCharacteristic accessoryInformationManufacturerCharacteristic = {
    .format = kHAPCharacteristicFormat_String,
    .iid = kIID_AccessoryInformationManufacturer,
    .characteristicType = &kHAPCharacteristicType_Manufacturer,
    .debugDescription = kHAPCharacteristicDebugDescription_Manufacturer,
    .manufacturerDescription = NULL,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .constraints = { .maxLength = 64 },
    .callbacks = { .handleRead = HAPHandleAccessoryInformationManufacturerRead, .handleWrite = NULL }
};

const HAPStringCharacteristic accessoryInformationModelCharacteristic = {
    .format = kHAPCharacteristicFormat_String,
    .iid = kIID_AccessoryInformationModel,
    .characteristicType = &kHAPCharacteristicType_Model,
    .debugDescription = kHAPCharacteristicDebugDescription_Model,
    .manufacturerDescription = NULL,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .constraints = { .maxLength = 64 },
    .callbacks = { .handleRead = HAPHandleAccessoryInformationModelRead, .handleWrite = NULL }
};

const HAPStringCharacteristic accessoryInformationNameCharacteristic = {
    .format = kHAPCharacteristicFormat_String,
    .iid = kIID_AccessoryInformationName,
    .characteristicType = &kHAPCharacteristicType_Name,
    .debugDescription = kHAPCharacteristicDebugDescription_Name,
    .manufacturerDescription = NULL,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .constraints = { .maxLength = 64 },
    .callbacks = { .handleRead = HAPHandleAccessoryInformationNameRead, .handleWrite = NULL }
};

const HAPStringCharacteristic accessoryInformationSerialNumberCharacteristic = {
    .format = kHAPCharacteristicFormat_String,
    .iid = kIID_AccessoryInformationSerialNumber,
    .characteristicType = &kHAPCharacteristicType_SerialNumber,
    .debugDescription = kHAPCharacteristicDebugDescription_SerialNumber,
    .manufacturerDescription = NULL,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .constraints = { .maxLength = 64 },
    .callbacks = { .handleRead = HAPHandleAccessoryInformationSerialNumberRead, .handleWrite = NULL }
};

const HAPStringCharacteristic accessoryInformationFirmwareRevisionCharacteristic = {
    .format = kHAPCharacteristicFormat_String,
    .iid = kIID_AccessoryInformationFirmwareRevision,
    .characteristicType = &kHAPCharacteristicType_FirmwareRevision,
    .debugDescription = kHAPCharacteristicDebugDescription_FirmwareRevision,
    .manufacturerDescription = NULL,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .constraints = { .maxLength = 64 },
    .callbacks = { .handleRead = HAPHandleAccessoryInformationFirmwareRevisionRead, .handleWrite = NULL }
};

const HAPStringCharacteristic accessoryInformationHardwareRevisionCharacteristic = {
    .format = kHAPCharacteristicFormat_String,
    .iid = kIID_AccessoryInformationHardwareRevision,
    .characteristicType = &kHAPCharacteristicType_HardwareRevision,
    .debugDescription = kHAPCharacteristicDebugDescription_HardwareRevision,
    .manufacturerDescription = NULL,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .constraints = { .maxLength = 64 },
    .callbacks = { .handleRead = HAPHandleAccessoryInformationHardwareRevisionRead, .handleWrite = NULL }
};

const HAPStringCharacteristic accessoryInformationADKVersionCharacteristic = {
    .format = kHAPCharacteristicFormat_String,
    .iid = kIID_AccessoryInformationADKVersion,
    .characteristicType = &kHAPCharacteristicType_ADKVersion,
    .debugDescription = kHAPCharacteristicDebugDescription_ADKVersion,
    .manufacturerDescription = NULL,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = true,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .constraints = { .maxLength = 64 },
    .callbacks = { .handleRead = HAPHandleAccessoryInformationADKVersionRead, .handleWrite = NULL }
};

const HAPService accessoryInformationService = { .iid = kIID_AccessoryInformation,
                                                 .serviceType = &kHAPServiceType_AccessoryInformation,
                                                 .debugDescription = kHAPServiceDebugDescription_AccessoryInformation,
                                                 .name = NULL,
                                                 .properties = { .primaryService = false, .hidden = false },
                                                 .linkedServices = NULL,
                                                 .characteristics = (const HAPCharacteristic* const[]) {
                                                         &accessoryInformationIdentifyCharacteristic,
                                                         &accessoryInformationManufacturerCharacteristic,
                                                         &accessoryInformationModelCharacteristic,
                                                         &accessoryInformationNameCharacteristic,
                                                         &accessoryInformationSerialNumberCharacteristic,
                                                         &accessoryInformationFirmwareRevisionCharacteristic,
                                                         &accessoryInformationHardwareRevisionCharacteristic,
                                                         &accessoryInformationADKVersionCharacteristic,
                                                         NULL } };

//----------------------------------------------------------------------------------------------------------------------

static const HAPStringCharacteristic hapProtocolInformationVersionCharacteristic = {
    .format = kHAPCharacteristicFormat_String,
    .iid = kIID_HAPProtocolInformationVersion,
    .characteristicType = &kHAPCharacteristicType_Version,
    .debugDescription = kHAPCharacteristicDebugDescription_Version,
    .manufacturerDescription = NULL,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .constraints = { .maxLength = 64 },
    .callbacks = { .handleRead = HAPHandleHAPProtocolInformationVersionRead, .handleWrite = NULL }
};

const HAPService hapProtocolInformationService = {
    .iid = kIID_HAPProtocolInformation,
    .serviceType = &kHAPServiceType_HAPProtocolInformation,
    .debugDescription = kHAPServiceDebugDescription_HAPProtocolInformation,
    .name = NULL,
    .properties = { .primaryService = false, .hidden = false },
    .linkedServices = NULL,
    .characteristics = (const HAPCharacteristic* const[]) { &hapProtocolInformationVersionCharacteristic, NULL }
};

//----------------------------------------------------------------------------------------------------------------------

static const HAPTLV8Characteristic pairingPairSetupCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_PairingPairSetup,
    .characteristicType = &kHAPCharacteristicType_PairSetup,
    .debugDescription = kHAPCharacteristicDebugDescription_PairSetup,
    .manufacturerDescription = NULL,
    .properties = { .readable = false,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = true } },
    .callbacks = { .handleRead = HAPHandlePairingPairSetupRead, .handleWrite = HAPHandlePairingPairSetupWrite }
};

static const HAPTLV8Characteristic pairingPairVerifyCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_PairingPairVerify,
    .characteristicType = &kHAPCharacteristicType_PairVerify,
    .debugDescription = kHAPCharacteristicDebugDescription_PairVerify,
    .manufacturerDescription = NULL,
    .properties = { .readable = false,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = true } },
    .callbacks = { .handleRead = HAPHandlePairingPairVerifyRead, .handleWrite = HAPHandlePairingPairVerifyWrite }
};

static const HAPUInt8Characteristic pairingPairingFeaturesCharacteristic = {
    .format = kHAPCharacteristicFormat_UInt8,
    .iid = kIID_PairingPairingFeatures,
    .characteristicType = &kHAPCharacteristicType_PairingFeatures,
    .debugDescription = kHAPCharacteristicDebugDescription_PairingFeatures,
    .manufacturerDescription = NULL,
    .properties = { .readable = false,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .units = kHAPCharacteristicUnits_None,
    .constraints = { .minimumValue = 0,
                     .maximumValue = UINT8_MAX,
                     .stepValue = 0,
                     .validValues = NULL,
                     .validValuesRanges = NULL },
    .callbacks = { .handleRead = HAPHandlePairingPairingFeaturesRead, .handleWrite = NULL }
};

static const HAPTLV8Characteristic pairingPairingPairingsCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_PairingPairingPairings,
    .characteristicType = &kHAPCharacteristicType_PairingPairings,
    .debugDescription = kHAPCharacteristicDebugDescription_PairingPairings,
    .manufacturerDescription = NULL,
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = true } },
    .callbacks = { .handleRead = HAPHandlePairingPairingPairingsRead,
                   .handleWrite = HAPHandlePairingPairingPairingsWrite }
};

const HAPService pairingService = { .iid = kIID_Pairing,
                                    .serviceType = &kHAPServiceType_Pairing,
                                    .debugDescription = kHAPServiceDebugDescription_Pairing,
                                    .name = NULL,
                                    .properties = { .primaryService = false, .hidden = false },
                                    .linkedServices = NULL,
                                    .characteristics =
                                            (const HAPCharacteristic* const[]) { &pairingPairSetupCharacteristic,
                                                                                 &pairingPairVerifyCharacteristic,
                                                                                 &pairingPairingFeaturesCharacteristic,
                                                                                 &pairingPairingPairingsCharacteristic,
                                                                                 NULL } };

//----------------------------------------------------------------------------------------------------------------------

/**
 * The 'Streaming Status' characteristic of the RTP Streaming service.
 */
const HAPTLV8Characteristic streamingStatusCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_RTPStreamingStatus,
    .characteristicType = &kHAPCharacteristicType_StreamingStatus,
    .debugDescription = kHAPCharacteristicDebugDescription_StreamingStatus,
    .manufacturerDescription = NULL,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleStreamingStatusRead, .handleWrite = NULL }
};

/**
 * The 'Supported Audio' characteristic of the RTP Streaming service.
 */
const HAPTLV8Characteristic supportedAudioStreamCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_RTPSupportedAudio,
    .characteristicType = &kHAPCharacteristicType_SupportedAudioStreamConfiguration,
    .debugDescription = kHAPCharacteristicDebugDescription_SupportedAudioStreamConfiguration,
    .manufacturerDescription = NULL,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleSupportedAudioRead, .handleWrite = NULL }
};

/**
 * The 'Supported Video' characteristic of the RTP Streaming service.
 */
const HAPTLV8Characteristic supportedVideoStreamCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_RTPSupportedVideo,
    .characteristicType = &kHAPCharacteristicType_SupportedVideoStreamConfiguration,
    .debugDescription = kHAPCharacteristicDebugDescription_SupportedVideoStreamConfiguration,
    .manufacturerDescription = NULL,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleSupportedVideoRead, .handleWrite = NULL }
};

/**
 * The 'Supported RTP configuration' characteristic of the RTP Streaming service.
 */
const HAPTLV8Characteristic supportedRTPConfigurationCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_RTPSupportedConfig,
    .characteristicType = &kHAPCharacteristicType_SupportedRTPConfiguration,
    .debugDescription = kHAPCharacteristicDebugDescription_SupportedRTPConfiguration,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleSupportedRTPConfigRead, .handleWrite = NULL }
};

/**
 * The 'Selected RTP Stream configuration' characteristic of the RTP Streaming service.
 */
const HAPTLV8Characteristic selectedRTPConfigurationCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_RTPSelectedStream,
    .characteristicType = &kHAPCharacteristicType_SelectedRTPStreamConfiguration,
    .debugDescription = kHAPCharacteristicDebugDescription_SelectedRTPStreamConfiguration,
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = true, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleSelectedRTPConfigRead, .handleWrite = HandleSelectedRTPConfigWrite }
};

/**
 * The 'setup endpoints' characteristic of the RTP Streaming service.
 */
const HAPTLV8Characteristic setupEndpointsCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_RTPSetupEndpoints,
    .characteristicType = &kHAPCharacteristicType_SetupEndpoints,
    .debugDescription = kHAPCharacteristicDebugDescription_SetupEndpoints,
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = true,
                            .supportsWriteResponse = false } }, // controlpoint == true to prevent read on pair.
    .callbacks = { .handleRead = HandleSetupEndpointsRead, .handleWrite = HandleSetupEndpointsWrite }
};

/**
 * The 'active' characteristic of the RTP Streaming service. Needed for secure video
 */
const HAPUInt8Characteristic rtpActiveCharacteristic = {
    .format = kHAPCharacteristicFormat_UInt8,
    .iid = kIID_RTPActive,
    .characteristicType = &kHAPCharacteristicType_Active,
    .debugDescription = "rtp-active",
    .constraints = { .minimumValue = 0,
                     .maximumValue = 1,
                     .stepValue = 0,
                     .validValues = NULL,
                     .validValuesRanges = NULL },
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleRTPActiveRead, .handleWrite = HandleRTPActiveWrite }
};

/**
 * The RTP Stream service.
 */
const HAPService rtpStreamService = { .iid = kIID_RTPStream,
                                      .serviceType = &kHAPServiceType_CameraRTPStreamManagement,
                                      .debugDescription = kHAPServiceDebugDescription_CameraRTPStreamManagement,
                                      .name = NULL,
                                      .properties = { .primaryService = false, .hidden = false },
                                      .linkedServices = NULL,
                                      .characteristics = (const HAPCharacteristic* const[]) {
                                              &selectedRTPConfigurationCharacteristic,
                                              &streamingStatusCharacteristic,
                                              &supportedAudioStreamCharacteristic,
                                              &supportedVideoStreamCharacteristic,
                                              &supportedRTPConfigurationCharacteristic,
                                              &setupEndpointsCharacteristic,
                                              &rtpActiveCharacteristic,
                                              NULL } };

const HAPBoolCharacteristic microphoneMuteCharacteristic = {
    .format = kHAPCharacteristicFormat_Bool,
    .iid = kIID_MicrophoneMute,
    .characteristicType = &kHAPCharacteristicType_Mute,
    .debugDescription = kHAPCharacteristicDebugDescription_Mute,
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleMicMuteRead, .handleWrite = HandleMicMuteWrite }
};

const HAPService microphoneService = {
    .iid = kIID_Microphone,
    .serviceType = &kHAPServiceType_Microphone,
    .debugDescription = kHAPServiceDebugDescription_Microphone,
    .name = NULL, // Optional
    .properties = { .primaryService = false, .hidden = false },
    .linkedServices = NULL,
    .characteristics = (const HAPCharacteristic* const[]) { &microphoneMuteCharacteristic, NULL }
};

const HAPBoolCharacteristic motionDetectedCharacteristic = {
    .format = kHAPCharacteristicFormat_Bool,
    .iid = kIID_MotionDetected,
    .characteristicType = &kHAPCharacteristicType_MotionDetected,
    .debugDescription = kHAPCharacteristicDebugDescription_MotionDetected,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleMotionDetectedRead, .handleWrite = NULL }
};

const HAPBoolCharacteristic motionActiveCharacteristic = {
    .format = kHAPCharacteristicFormat_Bool,
    .iid = kIID_MotionActive,
    .characteristicType = &kHAPCharacteristicType_StatusActive,
    .debugDescription = "motion-active",
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleMotionActiveRead, .handleWrite = HandleMotionActiveWrite }
};

const HAPService motionDetectService = {
    .iid = kIID_MotionSensor,
    .serviceType = &kHAPServiceType_MotionSensor,
    .debugDescription = kHAPServiceDebugDescription_MotionSensor,
    .name = NULL, // Optional
    .properties = { .primaryService = false, .hidden = false },
    .linkedServices = NULL,
    .characteristics = (const HAPCharacteristic* const[]) { 
                                        &motionDetectedCharacteristic, 
                                        &motionActiveCharacteristic,
                                        NULL }
};

/**
 * Camera Operating Mode Service
 */
// required
const HAPBoolCharacteristic eventSnapshotsActiveCharacteristic = {
    .format = kHAPCharacteristicFormat_Bool,
    .iid = kIID_CameraEventSnapshots,
    .characteristicType = &kHAPCharacteristicType_EventSnapshotsActive,
    .debugDescription = kHAPCharacteristicDebugDescription_EventSnapshotsActive,
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = true,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleEventSnapActiveRead, .handleWrite = HandleEventSnapActiveWrite }
};
const HAPBoolCharacteristic homekitCameraActiveCharacteristic = {
    .format = kHAPCharacteristicFormat_Bool,
    .iid = kIID_CameraHomekitCamActive,
    .characteristicType = &kHAPCharacteristicType_HomeKitCameraActive,
    .debugDescription = kHAPCharacteristicDebugDescription_HomeKitCameraActive,
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = true,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleHomeKitCamActiveRead, .handleWrite = HandleHomeKitCamActiveWrite }
};
const HAPBoolCharacteristic periodicSnapshotsActiveCharacteristic = {
    .format = kHAPCharacteristicFormat_Bool,
    .iid = kIID_CameraPeriodicSnapshots,
    .characteristicType = &kHAPCharacteristicType_PeriodicSnapshotsActive,
    .debugDescription = kHAPCharacteristicDebugDescription_PeriodicSnapshotsActive,
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = true,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandlePeriodicSnapActiveRead, .handleWrite = HandlePeriodicSnapActiveWrite }
};
const HAPBoolCharacteristic nightVisionCharacteristic = {
    .format = kHAPCharacteristicFormat_Bool,
    .iid = kIID_CameraOperatingModeNightVision,
    .characteristicType = &kHAPCharacteristicType_NightVision,
    .debugDescription = kHAPCharacteristicDebugDescription_NightVision,
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = true,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleNightVisionRead, .handleWrite = HandleNightVisionWrite }
};
const HAPBoolCharacteristic operatingModeIndicatorCharacteristic = {
    .format = kHAPCharacteristicFormat_Bool,
    .iid = kIID_CameraOperatingModeIndicator,
    .characteristicType = &kHAPCharacteristicType_CameraOperatingModeIndicator,
    .debugDescription = kHAPCharacteristicDebugDescription_CameraOperatingModeIndicator,
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = true,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleOperatingModeIndicatorRead, .handleWrite = HandleOperatingModeIndicatorWrite }
};
const HAPService cameraOperatingModeService = { .iid = kIID_CameraOperatingMode,
                                                .serviceType = &kHAPServiceType_CameraOperatingMode,
                                                .debugDescription = kHAPServiceDebugDescription_CameraOperatingMode,
                                                .name = NULL, // Optional
                                                .properties = { .primaryService = false, .hidden = false },
                                                .linkedServices = NULL, //(uint16_t const[]) { kIID_RTPStream, 0 },
                                                .characteristics = (const HAPCharacteristic* const[]) {
                                                        &eventSnapshotsActiveCharacteristic,
                                                        &homekitCameraActiveCharacteristic,
                                                        &periodicSnapshotsActiveCharacteristic,
                                                        &nightVisionCharacteristic,
                                                        &operatingModeIndicatorCharacteristic,
                                                        NULL } };

/**
 * Camera Recording Management Service
 */
// required
const HAPTLV8Characteristic supportedCameraRecordingConfigurationCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_SupportedCamRecordingConfig,
    .characteristicType = &kHAPCharacteristicType_SupportedCameraRecordingConfiguration,
    .debugDescription = kHAPCharacteristicDebugDescription_SupportedCameraRecordingConfiguration,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleSupportedCamRecConfRead, .handleWrite = NULL }
};
const HAPTLV8Characteristic supportedVideoRecordingConfigurationCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_SupportedVidRecordingConfig,
    .characteristicType = &kHAPCharacteristicType_SupportedVideoRecordingConfiguration,
    .debugDescription = kHAPCharacteristicDebugDescription_SupportedVideoRecordingConfiguration,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleSupportedVidRecConfRead, .handleWrite = NULL }
};
const HAPTLV8Characteristic supportedAudioRecordingConfigurationCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_SupportedAudioRecordingConfig,
    .characteristicType = &kHAPCharacteristicType_SupportedAudioRecordingConfiguration,
    .debugDescription = kHAPCharacteristicDebugDescription_SupportedAudioRecordingConfiguration,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleSupportedAudioConfRead, .handleWrite = NULL }
};
const HAPTLV8Characteristic selectedCameraRecordingConfigurationCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_SelectedCamRecordingConfig,
    .characteristicType = &kHAPCharacteristicType_SelectedCameraRecordingConfiguration,
    .debugDescription = kHAPCharacteristicDebugDescription_SelectedCameraRecordingConfiguration,
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleSelectedCamRecConfRead, .handleWrite = HandleSelectedCamRecConfWrite }
};

const HAPUInt8Characteristic cameraRecMgmtActiveCharacteristic = {
    .format = kHAPCharacteristicFormat_UInt8,
    .iid = kIID_CameraRecordingMgmtActive,
    .characteristicType = &kHAPCharacteristicType_Active,
    .debugDescription = "recording-active",
    .constraints = { .minimumValue = 0,
                    .maximumValue = 1,
                    .stepValue = 0 ,
                    .validValues = NULL,
                    .validValuesRanges = NULL },

    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleCamRecMgmtActiveRead, .handleWrite = HandleCamRecMgmtActiveWrite }
};



const HAPUInt8Characteristic cameraRecMgmtRecAudioActiveCharacteristic = {
    .format = kHAPCharacteristicFormat_UInt8,
    .iid = kIID_CameraRecordingMgmtRecAudioActive,
    .characteristicType = &kHAPCharacteristicType_RecordingAudioActive,
    .debugDescription = kHAPCharacteristicDebugDescription_RecordingAudioActive,
    .constraints = { .minimumValue = 0,
                    .maximumValue = 1,
                    .stepValue = 0 ,
                    .validValues = NULL,
                    .validValuesRanges = NULL },
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = true,
                    .hidden = false,
                    .requiresTimedWrite = true,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleCamRecMgmtRecAudioActiveRead, .handleWrite = HandleCamRecMgmtRecAudioActiveWrite }
};

const HAPService cameraRecordingManagementService = {
    .iid = kIID_CameraRecordingManagement,
    .serviceType = &kHAPServiceType_CameraRecordingManagement,
    .debugDescription = kHAPServiceDebugDescription_CameraRecordingManagement,
    .name = NULL, // Optional
    .properties = { .primaryService = false, .hidden = false },
    .linkedServices = (uint16_t const[]) { kIID_MotionSensor, kIID_DataStreamManagement, 0 },
    .characteristics = (const HAPCharacteristic* const[]) { &supportedCameraRecordingConfigurationCharacteristic,
                                                            &supportedVideoRecordingConfigurationCharacteristic,
                                                            &supportedAudioRecordingConfigurationCharacteristic,
                                                            &selectedCameraRecordingConfigurationCharacteristic,
                                                            &cameraRecMgmtActiveCharacteristic,
                                                            &cameraRecMgmtRecAudioActiveCharacteristic,
                                                            NULL }
};

/**
 * Data Stream Transport Management Service
 */
// required
const HAPTLV8Characteristic setupDataStreamTransportCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_SetupDataStreamTransport,
    .characteristicType = &kHAPCharacteristicType_SetupDataStreamTransport,
    .debugDescription = kHAPCharacteristicDebugDescription_SetupDataStreamTransport,
    .properties = { .readable = true,
                    .writable = true,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = true } },
    .callbacks = { .handleRead = HandleSetupDSTransportRead, .handleWrite = HandleSetupDSTransportWrite }
};
const HAPTLV8Characteristic supportedDataStreamTransportConfigurationCharacteristic = {
    .format = kHAPCharacteristicFormat_TLV8,
    .iid = kIID_SupportedDataStreamTransportConfig,
    .characteristicType = &kHAPCharacteristicType_SupportedDataStreamTransportConfiguration,
    .debugDescription = kHAPCharacteristicDebugDescription_SupportedDataStreamTransportConfiguration,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .callbacks = { .handleRead = HandleSupportedDSTransportConfRead, .handleWrite = NULL }
};

const HAPStringCharacteristic dataStreamVersionCharacteristic = {
    .format = kHAPCharacteristicFormat_String,
    .iid = kIID_DataStreamVersion,
    .characteristicType = &kHAPCharacteristicType_Version,
    .debugDescription = "datastream-version",
    .manufacturerDescription = NULL,
    .properties = { .readable = true,
                    .writable = false,
                    .supportsEventNotification = false,
                    .hidden = false,
                    .requiresTimedWrite = false,
                    .supportsAuthorizationData = false,
                    .ip = { .controlPoint = false, .supportsWriteResponse = false } },
    .constraints = { .maxLength = 64 },
    .callbacks = { .handleRead = HandleDSVersionRead, .handleWrite = NULL }
};

const HAPService dataStreamTransportManagementService = {
    .iid = kIID_DataStreamManagement,
    .serviceType = &kHAPServiceType_DataStreamTransportManagement,
    .debugDescription = kHAPServiceDebugDescription_DataStreamTransportManagement,
    .name = NULL, // Optional
    .properties = { .primaryService = false, .hidden = false },
    .linkedServices = (uint16_t const[]) { kIID_CameraRecordingManagement, 0 },
    .characteristics = (const HAPCharacteristic* const[]) { &setupDataStreamTransportCharacteristic,
                                                            &supportedDataStreamTransportConfigurationCharacteristic,
                                                            &dataStreamVersionCharacteristic,
                                                            NULL }
};
