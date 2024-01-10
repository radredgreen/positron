// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

// Basic light bulb database example. This header file, and the corresponding DB.c implementation in the ADK, is
// platform-independent.

#ifndef DB_H
#define DB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAP.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

/**
 * Total number of services and characteristics contained in the accessory.
 */
#define kAttributeCount ((size_t) 40)

/**
 * HomeKit Accessory Information service.
 */
extern const HAPService accessoryInformationService;

/**
 * Characteristics to expose accessory information and configuration (associated with Accessory Information service).
 */
extern const HAPBoolCharacteristic accessoryInformationIdentifyCharacteristic;
extern const HAPStringCharacteristic accessoryInformationManufacturerCharacteristic;
extern const HAPStringCharacteristic accessoryInformationModelCharacteristic;
extern const HAPStringCharacteristic accessoryInformationNameCharacteristic;
extern const HAPStringCharacteristic accessoryInformationSerialNumberCharacteristic;
extern const HAPStringCharacteristic accessoryInformationFirmwareRevisionCharacteristic;
extern const HAPStringCharacteristic accessoryInformationHardwareRevisionCharacteristic;
extern const HAPStringCharacteristic accessoryInformationADKVersionCharacteristic;

/**
 * HAP Protocol Information service.
 */
extern const HAPService hapProtocolInformationService;

/**
 * Pairing service.
 */
extern const HAPService pairingService;

/**
 * IP Camera service.
 */
extern const HAPTLV8Characteristic streamingStatusCharacteristic;
extern const HAPTLV8Characteristic supportedAudioStreamCharacteristic;
extern const HAPTLV8Characteristic supportedVideoStreamCharacteristic;
extern const HAPTLV8Characteristic supportedRTPConfigurationCharacteristic;
extern const HAPTLV8Characteristic selectedRTPConfigurationCharacteristic;
extern const HAPTLV8Characteristic setupEndpointsCharacteristic;
extern const HAPUInt8Characteristic rtpActiveCharacteristic;
extern const HAPService rtpStreamService;

/**
 * Microphone service
 */
extern const HAPBoolCharacteristic microphoneMuteCharacteristic;
extern const HAPService microphoneService;

/**
 * Motion Detect service
 */
extern const HAPBoolCharacteristic motionDetectedCharacteristic;
extern const HAPService motionDetectService;

/**
 * Camera Operating Mode Service
 */
// required
extern const HAPBoolCharacteristic eventSnapshotsActiveCharacteristic;
extern const HAPBoolCharacteristic homekitCameraActiveCharacteristic;
extern const HAPService cameraOperatingModeService;

/**
 * Camera Recording Management Service
 */
// required
extern const HAPTLV8Characteristic supportedCameraRecordingConfigurationCharacteristic;
extern const HAPTLV8Characteristic supportedVideoRecordingConfigurationCharacteristic;
extern const HAPTLV8Characteristic supportedAudioRecordingConfigurationCharacteristic;
extern const HAPTLV8Characteristic selectedCameraRecordingConfigurationCharacteristic;
extern const HAPUInt8Characteristic cameraRecMgmtActiveCharacteristic;
extern const HAPService cameraRecordingManagementService;

/**
 * Data Stream Transport Management Service
 */
// required
extern const HAPTLV8Characteristic setupDataStreamTransportCharacteristic;
extern const HAPTLV8Characteristic supportedDataStreamTransportConfigurationCharacteristic;
extern const HAPService dataStreamTransportManagementService;

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
