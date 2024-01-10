/*
 * App_Secure_Camera.c
 *
 *  Created on: Dec 29, 2020
 *      Author: joe
 */

#include "HAP.h"
#include "HAPTLV+Internal.h"

#include "App_Secure_Camera.h"
#include "App.h"
#include "DB.h"

HAP_RESULT_USE_CHECK
HAPError HandleEventSnapActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value HAP_UNUSED,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleEventSnapActiveWrite(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicWriteRequest* request HAP_UNUSED,
        bool value HAP_UNUSED,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleHomeKitCamActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED) {
    value = &accessoryConfiguration.state.operatingMode.homekitActive;
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, *value);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleSelectedCamRecConfRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(responseWriter);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
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
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleCamRecMgmtActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPUInt8CharacteristicReadRequest* request HAP_UNUSED,
        HAPCharacteristicValue_Active* value,
        void* _Nullable context HAP_UNUSED) {
    value = &accessoryConfiguration.state.operatingMode.recordingActive;
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
HAPError HandleSetupDSTransportRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(responseWriter);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleSetupDSTransportWrite(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicWriteRequest* request HAP_UNUSED,
        HAPTLVReaderRef* requestReader,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(requestReader);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleSupportedAudioConfRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(responseWriter);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleSupportedCamRecConfRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(responseWriter);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleSupportedDSTransportConfRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPTLV8CharacteristicReadRequest* request HAP_UNUSED,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context HAP_UNUSED) {
    HAPPrecondition(responseWriter);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
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
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleRTPActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPUInt8CharacteristicReadRequest* request HAP_UNUSED,
        HAPCharacteristicValue_Active* value,
        void* _Nullable context HAP_UNUSED) {
    value = &accessoryConfiguration.state.active;
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
    if (accessoryConfiguration.state.active != value) {
        accessoryConfiguration.state.active = value;

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }
    return kHAPError_None;
}
