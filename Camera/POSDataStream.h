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

#ifndef POSDATASTREAM_H
#define POSDATASTREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#include "HAPPlatformTCPStreamManager.h"
#include "HAPPlatformTCPStreamManager+Init.h"
#include "HAPIP+ByteBuffer.h"
#include "HAPCrypto.h"

#include "App_Secure_Camera.h"

#define kDataStreamResponseTimeout ((HAPTime)(10 * HAPSecond))
#define CHACHA20_POLY1305_KEY_BYTES 32
#define DSACCSALTLENGTH 32
#define DSCONSALTLENGTH 32
#define FRAME_HEADER_LEN 4


typedef struct
{
  
    pthread_mutex_t mutex;
  
    uint16_t port;

    HAPPlatformTCPStreamEvent interests;
    
    //controllerSalt must be immediately before accessorySalt so a 64 byte read of controller yields the concatenation
    uint8_t controllerSalt [DSCONSALTLENGTH];
    uint8_t accessorySalt [DSACCSALTLENGTH];

    uint8_t AccessoryToControllerKey [CHACHA20_POLY1305_KEY_BYTES];
    uint8_t ControllertoAccessoryKey [CHACHA20_POLY1305_KEY_BYTES];
    
    HAP_chacha20_poly1305_ctx tx_ctx;
    uint64_t tx_nonce;
    HAP_chacha20_poly1305_ctx rx_ctx;
    uint64_t rx_nonce;
    
    HAPPlatformTCPStreamManager tcpStreamManager;
    HAPPlatformTCPStreamRef tcpStream;
    HAPPlatformTimerRef TCPTimer;

    int64_t requestId; // id sent in the header field
    int64_t streamId; // 

    bool timerSet;
    uint8_t datastreamState;
  
    HAPIPByteBuffer outboundBuffer;
    uint8_t outboundBytes[kHAPIPSession_DefaultOutboundBufferSize];
    HAPIPByteBuffer inboundBuffer;
    uint8_t inboundBytes[kHAPIPSession_DefaultInboundBufferSize];

} posDataStreamStruct;

#define DATASTREAM_MAX_CHUNK_SIZE 0x40000

//state
enum {
  IDLE,
  SETUP,
  ACCEPT,
  WAIT_FOR_HELLO,
  SEND_HELLO,
  WAIT_FOR_OPEN,
  SEND_OPEN,
  GET_CHUNK,
  SEND_CHUNK,
  WAIT_FOR_ACK,
  SEND_CLOSE,
  CLOSE,
};


void POSDataStreamInit();

        
void POSDataStreamSetup(setupDataStreamTransportWriteStruct setupDataStreamTransportWrite, 
                        setupDataStreamTransportReadStruct setupDataStreamTransportRead,
                        uint16_t * port,
                        uint8_t * sessionKey); 

void start10sTimer(posDataStreamStruct * datastream);
void UpdateInterests(posDataStreamStruct * datastream);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
