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

#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#include "HAP.h"

#include "App_Camera.h"
#include "App_Secure_Camera.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

/**
 * Global accessory configuration.
 */
typedef struct {
    struct {
        struct {
            bool muted;
        } microphone;
        struct {
            bool detected;
            bool active;
        } motion;
        struct {
            HAPCharacteristicValue_Active active;
        } rtp;
        HAPCharacteristicValue_StreamingStatus streaming;
        struct {
            bool homekitActive;
            bool nightVision;
            bool indicator;
            bool flip;
            HAPCharacteristicValue_Active recordingActive;
            HAPCharacteristicValue_Active recordingAudioActive;
            bool periodicSnapshots;
            bool eventSnapshots;
        } operatingMode;
        HAPCharacteristicValue_Active active;
    } state;
    HAPAccessoryServerRef* server;
    HAPPlatformKeyValueStoreRef keyValueStore;
} AccessoryConfiguration;


typedef struct {
    streamingSession session;
    recordingSession recording;
} AccessoryContext;

/**
 * Identify routine. Used to locate the accessory.
 */
HAP_RESULT_USE_CHECK
HAPError IdentifyAccessory(
        HAPAccessoryServerRef* server,
        const HAPAccessoryIdentifyRequest* request,
        void* _Nullable context);

void SaveAccessoryState(void);

/**
 * Initialize the application.
 */
void AppCreate(HAPAccessoryServerRef* server, HAPPlatformKeyValueStoreRef keyValueStore);

/**
 * Deinitialize the application.
 */
void AppRelease(void);

/**
 * Start the accessory server for the app.
 */
void AppAccessoryServerStart(void);

/**
 * Handle the updated state of the Accessory Server.
 */
void AccessoryServerHandleUpdatedState(HAPAccessoryServerRef* server, void* _Nullable context);

void AccessoryServerHandleSessionAccept(HAPAccessoryServerRef* server, HAPSessionRef* session, void* _Nullable context);

void AccessoryServerHandleSessionInvalidate(
        HAPAccessoryServerRef* server,
        HAPSessionRef* session,
        void* _Nullable context);

/**
 * Restore platform specific factory settings.
 */
void RestorePlatformFactorySettings(void);

/**
 * Returns pointer to accessory information
 */
const HAPAccessory* AppGetAccessoryInfo();

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
