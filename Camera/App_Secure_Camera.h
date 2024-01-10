/*
 * App_Secure_Camera.h
 *
 *  Created on: Dec 29, 2020
 *      Author: joe
 */

#ifndef APPLICATIONS_CAMERA_APP_SECURE_CAMERA_H_
#define APPLICATIONS_CAMERA_APP_SECURE_CAMERA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "HAP.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

HAP_RESULT_USE_CHECK
HAPError HandleEventSnapActiveRead(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicReadRequest* request,
        bool* value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleEventSnapActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleHomeKitCamActiveRead(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicReadRequest* request,
        bool* value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSelectedCamRecConfRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSelectedCamRecConfWrite(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicWriteRequest* request,
        HAPTLVReaderRef* requestReader,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleCamRecMgmtActiveRead(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicReadRequest* request,
        HAPCharacteristicValue_Active* value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleCamRecMgmtActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicWriteRequest* request,
        HAPCharacteristicValue_Active value,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSetupDSTransportRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSetupDSTransportWrite(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicWriteRequest* request,
        HAPTLVReaderRef* requestReader,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSupportedAudioConfRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSupportedCamRecConfRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSupportedDSTransportConfRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleSupportedVidRecConfRead(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicReadRequest* request,
        HAPTLVWriterRef* responseWriter,
        void* _Nullable context);

HAP_RESULT_USE_CHECK
HAPError HandleRTPActiveRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPUInt8CharacteristicReadRequest* request HAP_UNUSED,
        HAPCharacteristicValue_Active* value,
        void* _Nullable context HAP_UNUSED);

HAP_RESULT_USE_CHECK
HAPError HandleRTPActiveWrite(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicWriteRequest* request,
        HAPCharacteristicValue_Active value,
        void* _Nullable context HAP_UNUSED);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_CAMERA_APP_SECURE_CAMERA_H_ */
