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

#ifndef POSDATASTREAMPARSER_H
#define POSDATASTREAMPARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAP.h"


#define  INVALID 0x00 
#define  TRUE 0x01 
#define  FALSE 0x02 
#define  TERMINATOR 0x03 
#define  NULLOBJECT 0x04 
#define  UUID 0x05 
#define  DATE 0x06 
#define  INTEGER_MINUS_ONE 0x07 
#define  INTEGER_RANGE_START_0 0x08 
#define  INTEGER_RANGE_STOP_39 0x2E 
#define  INT8 0x30 
#define  INT16LE 0x31 
#define  INT32LE 0x32 
#define  INT64LE 0x33 
#define  FLOAT32LE 0x35 
#define  FLOAT64LE 0x36 
#define  UTF8_LENGTH_START 0x40 
#define  UTF8_LENGTH_STOP 0x60 
#define  UTF8_LENGTH8 0x61 
#define  UTF8_LENGTH16LE 0x62 
#define  UTF8_LENGTH32LE 0x63 
#define  UTF8_LENGTH64LE 0x64 
#define  UTF8_NULL_TERMINATED 0x6F 
#define  DATA_LENGTH_START 0x70 
#define  DATA_LENGTH_STOP 0x90 
#define  DATA_LENGTH8 0x91 
#define  DATA_LENGTH16LE 0x92 
#define  DATA_LENGTH32LE 0x93 
#define  DATA_LENGTH64LE 0x94 
#define  DATA_TERMINATED 0x9F 
#define  COMPRESSION_START 0xA0 
#define  COMPRESSION_STOP 0xCF 
#define  ARRAY_LENGTH_START 0xD0 
#define  ARRAY_LENGTH_STOP 0xDE 
#define  ARRAY_TERMINATED 0xDF 
#define  DICTIONARY_LENGTH_START 0xE0 
#define  DICTIONARY_LENGTH_STOP 0xEE 
#define  DICTIONARY_TERMINATED 0xEF


HAPError processHelloPayload( uint8_t * payload, size_t headerLen, int64_t * id );
HAPError makeHelloPayloadResponse( uint8_t * buffer, size_t maxLen, size_t * len, int64_t id );
HAPError processDataSendOpen( uint8_t * payload, size_t payloadLen, int64_t * id , int64_t * streamId );
HAPError makeDataSendOpenResponse( uint8_t * buffer, size_t maxLen, size_t * len, int64_t requestId, int64_t streamId );
HAPError makeDataSendEventStart( 
          uint8_t * buffer, 
          size_t maxLen, 
          size_t * len, 
          int64_t streamId, 
          char * dataType, 
          int64_t dataSequenceNr, 
          bool isLastDataChunk, 
          int64_t dataChunkSequenceNumber,
          int64_t dataTotalSize,
          bool endOfStream 
);
HAPError writeDataLength(uint8_t * bytes, size_t * idx, size_t maxlen, int64_t input);
HAPError processDataSendAck( uint8_t * payload, size_t payloadLen, int64_t * streamId);
HAPError processDataSendClose( uint8_t * payload, size_t payloadLen, int64_t * streamId,  int64_t * reason);
HAPError makeDataSendCloseEvent( uint8_t * buffer, size_t maxLen, size_t * len, int64_t streamId );


#ifdef __cplusplus
}
#endif


#endif //POSDATASTREAMPARSER_H

