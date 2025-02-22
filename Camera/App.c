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


#include <stdlib.h>
#include <string.h>

#include "ingenicVideoPipeline.h"

#include "HAP.h"
#include "HAPTLV+Internal.h"
#include "HAPCharacteristicTypes.h"

#include "App_Camera.h"
#include "App_Secure_Camera.h"
#include "POSDataStream.h"
#include "POSCameraController.h"
#include "POSRecordingController.h"
#include "App.h"
#include "DB.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Domain used in the key value store for application data.
 *
 * Purged: On factory reset.
 */
#define kAppKeyValueStoreDomain_Configuration ((HAPPlatformKeyValueStoreDomain) 0x00)

/**
 * Key used in the key value store to store the configuration state.
 *
 * Purged: On factory reset.
 */
#define kAppKeyValueStoreKey_Configuration_State ((HAPPlatformKeyValueStoreDomain) 0x00)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

 AccessoryConfiguration accessoryConfiguration;

/**
 * Load the accessory state from persistent memory.
 */
static void LoadAccessoryState(void) {
    HAPPrecondition(accessoryConfiguration.keyValueStore);

    HAPError err;

    // Load persistent state if available
    bool found;
    size_t numBytes;

    err = HAPPlatformKeyValueStoreGet(
            accessoryConfiguration.keyValueStore,
            kAppKeyValueStoreDomain_Configuration,
            kAppKeyValueStoreKey_Configuration_State,
            &accessoryConfiguration.state,
            sizeof accessoryConfiguration.state,
            &numBytes,
            &found);

    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        HAPFatalError();
    }
    if (!found || numBytes != sizeof accessoryConfiguration.state) {
        if (found) {
            HAPLogError(&kHAPLog_Default, "Unexpected app state found in key-value store. Resetting to default.");
        }
        HAPRawBufferZero(&accessoryConfiguration.state, sizeof accessoryConfiguration.state);
        accessoryConfiguration.state.streaming = kHAPCharacteristicValue_StreamingStatus_Available;
        accessoryConfiguration.state.microphone.muted = true;
        accessoryConfiguration.state.motion.detected = false;
        accessoryConfiguration.state.motion.active = true;
        accessoryConfiguration.state.rtp.active = kHAPCharacteristicValue_Active_Active;
        accessoryConfiguration.state.operatingMode.nightVision = true;
        accessoryConfiguration.state.operatingMode.indicator = true;
        accessoryConfiguration.state.operatingMode.flip = false;
        accessoryConfiguration.state.operatingMode.homekitActive = true;
        accessoryConfiguration.state.operatingMode.recordingActive = kHAPCharacteristicValue_Active_Active;
        accessoryConfiguration.state.operatingMode.recordingAudioActive = kHAPCharacteristicValue_RecordingAudioActive_Include;
        accessoryConfiguration.state.operatingMode.periodicSnapshots = true;
        accessoryConfiguration.state.operatingMode.eventSnapshots = true;
        accessoryConfiguration.state.active = kHAPCharacteristicValue_Active_Active;
    }
    accessoryConfiguration.state.streaming = kHAPCharacteristicValue_StreamingStatus_Available;
    
}
/**
 * Save the accessory state to persistent memory.
 */
void SaveAccessoryState(void) {
    HAPPrecondition(accessoryConfiguration.keyValueStore);

    HAPError err;
    err = HAPPlatformKeyValueStoreSet(
            accessoryConfiguration.keyValueStore,
            kAppKeyValueStoreDomain_Configuration,
            kAppKeyValueStoreKey_Configuration_State,
            &accessoryConfiguration.state,
            sizeof accessoryConfiguration.state);
    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        HAPFatalError();
    }
}

//----------------------------------------------------------------------------------------------------------------------

/**
 * HomeKit accessory that provides the IP Camera service.
 *
 * Note: Not constant to enable BCT Manual Name Change.
 */
 HAPAccessory accessory = { .aid = 1,
                                  .category = kHAPAccessoryCategory_IPCameras,
                                  .name = "wyrecam",
                                  .manufacturer = "radredgreen",
                                  .model = "wyzec3",
                                  .serialNumber = "0001",
                                  .firmwareVersion = "0004", // this didn't work: V6.21.5.0_191008
                                  .hardwareVersion = "wyzec3",
                                  .services = (const HAPService* const[]) { &accessoryInformationService,
                                                                            &hapProtocolInformationService,
                                                                            &pairingService,
                                                                            &rtpStreamService,
                                                                            &microphoneService,
                                                                            &motionDetectService,
                                                                            &cameraOperatingModeService,
                                                                            &cameraRecordingManagementService,
                                                                            &dataStreamTransportManagementService,
                                                                            NULL },
                                  .callbacks = { .identify = IdentifyAccessory } };

//----------------------------------------------------------------------------------------------------------------------

HAP_RESULT_USE_CHECK
HAPError IdentifyAccessory(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPAccessoryIdentifyRequest* request HAP_UNUSED,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    return kHAPError_None;
}

void AccessoryNotification(
        const HAPAccessory* accessory,
        const HAPService* service,
        const HAPCharacteristic* characteristic,
        void* ctx HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "Accessory Notification");

    HAPAccessoryServerRaiseEvent(accessoryConfiguration.server, characteristic, service, accessory);
}

void AppCreate(HAPAccessoryServerRef* server, HAPPlatformKeyValueStoreRef keyValueStore) {
    HAPPrecondition(server);
    HAPPrecondition(keyValueStore);

    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    HAPRawBufferZero(&accessoryConfiguration, sizeof accessoryConfiguration);
    accessoryConfiguration.server = server;
    accessoryConfiguration.keyValueStore = keyValueStore;
    LoadAccessoryState();
    set_red_led(accessoryConfiguration.state.operatingMode.indicator);
    set_flip(accessoryConfiguration.state.operatingMode.flip);

}

void AppRelease(void) {
}

void AppAccessoryServerStart(void) {
    HAPAccessoryServerStart(accessoryConfiguration.server, &accessory);
}

//----------------------------------------------------------------------------------------------------------------------
void AccessoryServerHandleUpdatedState(HAPAccessoryServerRef* server, void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(server);
    // HAPPrecondition(!context);

    switch (HAPAccessoryServerGetState(server)) {
        case kHAPAccessoryServerState_Idle: {
            HAPLogInfo(&kHAPLog_Default, "Accessory Server State did update: Idle.");
            return;
        }
        case kHAPAccessoryServerState_Running: {
            HAPLogInfo(&kHAPLog_Default, "Accessory Server State did update: Running.");
            return;
        }
        case kHAPAccessoryServerState_Stopping: {
            HAPLogInfo(&kHAPLog_Default, "Accessory Server State did update: Stopping.");
            return;
        }
    }
    HAPFatalError();
}

const HAPAccessory* AppGetAccessoryInfo() {
    return &accessory;
}


void AppInitialize(
        HAPAccessoryServerOptions* hapAccessoryServerOptions HAP_UNUSED,
        HAPPlatform* hapPlatform HAP_UNUSED,
        HAPAccessoryServerCallbacks* hapAccessoryServerCallbacks HAP_UNUSED) {
    /*no-op*/
}

void ContextInitialize(AccessoryContext* context) {
    memset(context, 0, sizeof(context));

    POSDataStreamInit();
    StreamContextInitialize(context);
    initVideoPipeline(&accessoryConfiguration);    
    RecordingContextInitialize(context); // needs to init after initVideoPipeline
}

void ContextDeintialize(AccessoryContext* context) {
    /*no-op*/
}

void AppDeinitialize() {
    /*no-op*/
}
