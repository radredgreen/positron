// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

// An example that implements the light bulb HomeKit profile. It can serve as a basic implementation for
// any platform. The accessory logic implementation is reduced to internal state updates and log output.
//
// This implementation is platform-independent.
//
// The code consists of multiple parts:
//
//   1. The definition of the accessory configuration and its internal state.
//
//   2. Helper functions to load and save the state of the accessory.
//
//   3. The definitions for the HomeKit attribute database.
//
//   4. The callbacks that implement the actual behavior of the accessory, in this
//      case here they merely access the global accessory state variable and write
//      to the log to make the behavior easily observable.
//
//   5. The initialization of the accessory state.
//
//   6. Callbacks that notify the server in case their associated value has changed.

#include "HAP.h"
#include "HAPTLV+Internal.h"
#include "HAPCharacteristicTypes.h"

#include <string.h>
#include "App.h"
#include "DB.h"
#include <stdlib.h>
#include "ingenicVideoPipeline.h"
#include "App_Camera.h"
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
        accessoryConfiguration.state.operatingMode.homekitActive = true;
        accessoryConfiguration.state.operatingMode.recordingActive = kHAPCharacteristicValue_Active_Active;
        accessoryConfiguration.state.operatingMode.recordingAudioActive = kHAPCharacteristicValue_RecordingAudioActive_Include;
        accessoryConfiguration.state.active = kHAPCharacteristicValue_Active_Active;
    }
    accessoryConfiguration.state.streaming = kHAPCharacteristicValue_StreamingStatus_Available;
    
    HAPLogError(&kHAPLog_Default, "accessoryConfiguration.state.streaming address in LoadAccessoryState: %p\n", &accessoryConfiguration.state.streaming);
    HAPLogError(&kHAPLog_Default, "streaming state: %d", accessoryConfiguration.state.streaming);

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
                                                                            //&cameraOperatingModeService,
                                                                            //&cameraRecordingManagementService,
                                                                            //&dataStreamTransportManagementService,
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
    memset(context->session.sessionId, 0, UUIDLENGTH);
    context->session.status = kHAPCharacteristicValue_StreamingStatus_Available;
    accessoryConfiguration.state.streaming = kHAPCharacteristicValue_StreamingStatus_Available;

    HAPPlatformRandomNumberFill(&context->session.ssrcVideo , 4);
    do{
        HAPPlatformRandomNumberFill(&context->session.ssrcAudio , 4);
    } while( context->session.ssrcVideo == context->session.ssrcAudio );

    StreamContextInitialize(context);
    initVideoPipeline();
}

void ContextDeintialize(AccessoryContext* context) {
    /*no-op*/
}

void AppDeinitialize() {
    /*no-op*/
}
