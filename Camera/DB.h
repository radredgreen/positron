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
#define kAttributeCount ((size_t) 47)

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
extern const HAPBoolCharacteristic motionActiveCharacteristic;
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
extern const HAPUInt8Characteristic cameraRecMgmtRecAudioActiveCharacteristic;
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
