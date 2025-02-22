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

#include <string.h> 

#include <HAP.h>
#include <HAP+Internal.h>

#include "POSDataStream.h"
#include "POSDataStreamParser.h"

#include "hexdump.h"

static const HAPLogObject logObject = { .subsystem = kHAP_LogSubsystem, .category = "POSDataStreamController" };


// note: the hap node js code has a comment at this point that says 'welcome to hell'!
// Not aiming for a full featured data stream parser, just byte banging the happy path for hksv

HAPError readNumberToInt64(uint8_t * bytes, size_t * idx, size_t maxlen, int64_t * output){
//printf("bytes: %02x, idx: %d, maxLen: %d, output: %d\n", *bytes, *idx, maxlen, *output);
  *output = 0;
  if((*idx)+1 > maxlen) return kHAPError_InvalidData;   //check 1
  if(bytes[(*idx)] == INT64LE) {
    (*idx) ++;
    if((*idx)+8 > maxlen) return kHAPError_InvalidData;   //check 8
    *output = HAPReadLittleInt64(&(bytes[(*idx)]));
    (*idx) += 8;
  }
  if(bytes[(*idx)] == INT32LE) {
    (*idx) ++;
    if((*idx)+4 > maxlen) return kHAPError_InvalidData;  //check 4
    *output = HAPReadLittleInt32(&(bytes[(*idx)]));
    (*idx) += 4;
  }
  if(bytes[(*idx)] == INT16LE) {
    (*idx) ++;
    if((*idx)+2 > maxlen) return kHAPError_InvalidData;  //check 2
    *output = HAPReadLittleInt16(&(bytes[(*idx)]));
    (*idx) += 2;
  }
  if(bytes[(*idx)] == INT8) {
    (*idx) ++;
    if((*idx)+1 > maxlen) return kHAPError_InvalidData;   //check 1
    *output = (uint8_t)(bytes[(*idx)+1]);
    (*idx) += 1;
  }
  if(bytes[(*idx)] >= INTEGER_RANGE_START_0 && bytes[(*idx)] <= INTEGER_RANGE_STOP_39){
    *output = (uint8_t)(bytes[(*idx)] - INTEGER_RANGE_START_0);
    (*idx) ++;
  }
  if(bytes[(*idx)] == INTEGER_MINUS_ONE) {
    *output = -1;
    (*idx) ++;

  }
  return kHAPError_None;
}

HAPError writeNumber(uint8_t * bytes, size_t * idx, size_t maxlen, int64_t input){
//printf("writeNumber %u, %lld\n", (uint32_t)*idx, input);
  if (input == -1) {
    if((*idx)+1 > maxlen) return kHAPError_InvalidData;   //check 1
    bytes[*idx] = INTEGER_MINUS_ONE;
    (*idx)++;
  } else if (input >= 0 && input <= 39) {
    if((*idx)+1 > maxlen) return kHAPError_InvalidData;   //check 1
    bytes[*idx] = (int8_t)(INTEGER_RANGE_START_0 + input);
    (*idx)++;
  } else if (input >= -128 && input <= 127) {
    if((*idx)+2 > maxlen) return kHAPError_InvalidData;   //check 2
    bytes[*idx] = INT8;
    (*idx)++;
    bytes[*idx] = (int8_t)input;
    (*idx)++;
  } else if (input >= -32768 && input <= 32767) {
    if((*idx)+3 > maxlen) return kHAPError_InvalidData;   //check 3
    bytes[*idx] = INT16LE;
    (*idx)++;
    HAPWriteLittleInt16(&(bytes[*idx]), input);
    (*idx) += 2;
  } else if (input >= -2147483648 && input <= 2147483647) {
    if((*idx)+5 > maxlen) return kHAPError_InvalidData;   //check 5
    bytes[*idx] = INT32LE;
    (*idx)++;
    HAPWriteLittleInt32(&(bytes[*idx]), input);
    (*idx) += 4;
  } else { 
    if((*idx)+9 > maxlen) return kHAPError_InvalidData;   //check 9
    bytes[*idx] = INT64LE;
    (*idx)++;
    HAPWriteLittleInt64(&(bytes[*idx]), input);
    (*idx) += 8;
  }
  return kHAPError_None;
}

HAPError writeDataLength(uint8_t * bytes, size_t * idx, size_t maxlen, int64_t input){
//printf("writeDataLength\n");
  if (input >= -128 && input <= 127) {
    if((*idx)+2 > maxlen) return kHAPError_InvalidData;   //check 2
    bytes[*idx] = DATA_LENGTH8;
    (*idx)++;
    bytes[*idx] = (int8_t)input;
    (*idx)++;
  } else if (input >= -32768 && input <= 32767) {
    if((*idx)+3 > maxlen) return kHAPError_InvalidData;   //check 3
    bytes[*idx] = DATA_LENGTH16LE;
    (*idx)++;
    HAPWriteLittleInt16(&(bytes[*idx]), input);
    (*idx) += 2;
  } else if (input >= -2147483648 && input <= 2147483647) {
    if((*idx)+5 > maxlen) return kHAPError_InvalidData;   //check 5
    bytes[*idx] = DATA_LENGTH32LE;
    (*idx)++;
    HAPWriteLittleInt32(&(bytes[*idx]), input);
    (*idx) += 4;
  } else { 
    if((*idx)+9 > maxlen) return kHAPError_InvalidData;   //check 9
    bytes[*idx] = DATA_LENGTH64LE;
    (*idx)++;
    HAPWriteLittleInt64(&(bytes[*idx]), input);
    (*idx) += 8;
  }
  return kHAPError_None;
}

//  process the hello message payload
//    0000  2c e34772 65717565 73744568 656c6c6f 4870726f 746f636f 6c47636f 6e74726f    ,.GrequestEhelloHprotocolGcontro
//    0020  6c426964 3373af81 92000000 00e0                                            lBid3s........
//               i d 

HAPError processHelloPayload( uint8_t * payload, size_t payloadLen, int64_t * id ){
  size_t idx = 0;

  if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
  size_t headerLen = payload[idx]+1; // +1 is the headerlen
  idx++;

  if(headerLen >= payloadLen) return kHAPError_InvalidData;

  //expecting a dictionary of length 3
  if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
  if(payload[idx] != DICTIONARY_LENGTH_START + 3){
    HAPLogDebug(&kHAPLog_Default, "Expected a 3 deep dictionary in the hello header");
    return kHAPError_InvalidData;
  }
  idx++; // dict tag

  for(uint8_t dictIdx = 0 ; dictIdx < 3; dictIdx ++){
    //expecting 3 dict entries of string type
    if(idx+1 > headerLen) return kHAPError_InvalidData;   //check 1
    if(payload[idx] < UTF8_LENGTH_START || payload[idx] > UTF8_LENGTH_STOP){
      HAPLogDebug(&kHAPLog_Default, "Expected 3 text keys in the hello header");
      return kHAPError_InvalidData;
    }
    uint8_t keyLen = payload[idx] - UTF8_LENGTH_START;
    uint8_t valLen = 0;
    idx++; //utf8 len tag

    switch(payload[idx]){
      case 'r': //request
        if(idx+keyLen > headerLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen != 7){
          HAPLogDebug(&kHAPLog_Default, "Expected request text key in the hello header");
          return kHAPError_InvalidData;
        }
        idx += keyLen;
        if(idx+1 > headerLen) return kHAPError_InvalidData;   //check 1
        valLen = payload[idx] - UTF8_LENGTH_START;
        idx++;
        if(idx+valLen > headerLen) return kHAPError_InvalidData; // check valLen
        if(valLen != 5 || payload[idx] != 'h'){
          HAPLogDebug(&kHAPLog_Default, "Expected request key to have 'hello' value in the hello header");
          return kHAPError_InvalidData;
        }
        HAPLogInfo(&kHAPLog_Default, "Request good");
        idx += valLen;
      break;
      case 'p': //protocol
        if(idx+keyLen > headerLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen != 8){
          HAPLogDebug(&kHAPLog_Default, "Expected protocol text key in the hello header");
          return kHAPError_InvalidData;
        }
        idx += keyLen;
        if(idx+1 > headerLen) return kHAPError_InvalidData;   //check 1
        valLen = payload[idx] - UTF8_LENGTH_START;
        idx++;
        if(idx+valLen > headerLen) return kHAPError_InvalidData; // check valLen
        if(valLen != 7 || payload[idx] != 'c'){
          HAPLogDebug(&kHAPLog_Default, "Expected protocol key to have 'control' value in the hello header");
          return kHAPError_InvalidData;
        }
        HAPLogInfo(&kHAPLog_Default, "Protocol good");
        idx += valLen;
      break;
      case 'i': //id
        if(idx+keyLen > headerLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen != 2){
          HAPLogDebug(&kHAPLog_Default, "Expected id text key in the hello header");
          return kHAPError_InvalidData;
        }
        idx += keyLen;
        if(idx+1 > headerLen) return kHAPError_InvalidData;   //check 1
        if(payload[idx] != INT64LE && payload[idx] != INT32LE && payload[idx] != INT16LE && payload[idx] != INT8 && payload[idx] < INTEGER_MINUS_ONE && payload[idx] > INTEGER_RANGE_STOP_39){ //
          HAPLogDebug(&kHAPLog_Default, "Expected id key to be integer in the hello header, %02x", payload[idx]);
          return kHAPError_InvalidData;
        }
        //idx++; consumed in read number
        if( readNumberToInt64(payload, &idx, headerLen, id) ) return kHAPError_InvalidData;
        HAPLogInfo(&kHAPLog_Default, "ID good");
      break;
    }

  }
  return kHAPError_None;
}

HAPError makeHelloPayloadResponse( uint8_t * buffer, size_t maxLen, size_t * len, int64_t id ){

  //  0000  2de44872 6573706f 6e736544 6f70656e 4870726f 746f636f 6c476461 74615365    -.HresponseDopenHprotocolGdataSe
  //  0020  6e644269 64094673 74617475 7308e146 73746174 757308                        ndBid.Fstatus..Fstatus.

  uint8_t bytes[] =
  { 
  0xe4, // dict of 4
    0x48, // string of 8
      //0x72, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, //request
      0x72, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, //response
    0x45, // string of 5
      0x68, 0x65, 0x6c, 0x6c, 0x6f, //hello
    0x48, // string of 8
      0x70, 0x72, 0x6f, 0x74, 0x6f, 0x63, 0x6f, 0x6c, //protocol
    0x47, // string of 7
      0x63, 0x6f, 0x6e, 0x74, 0x72, 0x6f, 0x6c, // control
    0x42, // string of 2
      0x69, 0x64 // id
  }; 
  uint8_t bytes2[] =
  { 
    0x46, // string of 6
      0x73, 0x74, 0x61, 0x74, 0x75, 0x73, // status
    INTEGER_RANGE_START_0
  };

  size_t idx = 0;

  HAPPrecondition(1+sizeof(bytes)+1+8+sizeof(bytes2)+1 <= maxLen);
  idx = 1; //payload header

  HAPRawBufferCopyBytes(&(buffer[1]), &bytes, sizeof(bytes));
  idx += sizeof(bytes);
  
  // write a LE number

  if( writeNumber(buffer, &idx, maxLen, id) ) return kHAPError_InvalidData;

  HAPRawBufferCopyBytes(&(buffer[idx]), &bytes2, sizeof(bytes2));
  idx += sizeof(bytes2);

  // header over, write the payloadLen
  buffer[0] = (uint8_t)(idx) - 1; // exclude payloadLen byte of len 1

  //fill in the message part (0xe0) - 0 dict
  buffer[idx] = DICTIONARY_LENGTH_START + 0;
  idx++;

  *len = idx;

  return kHAPError_None;
}



HAPError processDataSendOpen( uint8_t * payload, size_t payloadLen, int64_t * id , int64_t * streamId ){

  //  0000  24e34772 65717565 7374446f 70656e48 70726f74 6f636f6c 48646174 6153656e    $.GrequestDopenHprotocolHdataSen
  //  0020  64426964 09e44873 74726561 6d496409 44747970 65526970 63616d65 72612e72    dBid..HstreamId.DtypeRipcamera.r
  //  0040  65636f72 64696e67 46746172 6765744a 636f6e74 726f6c6c 65724672 6561736f    ecordingFtargetJcontrollerFreaso
  //  0060  6e467265 636f7264                                                          nFrecord

  size_t idx = 0;

  if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
  size_t headerLen = payload[idx]+1;
  idx++;

  if(headerLen >= payloadLen) return kHAPError_InvalidData;

  //expecting a dictionary of length 3
  if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
  if(payload[idx] != DICTIONARY_LENGTH_START + 3){
    HAPLogDebug(&kHAPLog_Default, "Expected a 3 deep dictionary in the datasend open header");
    return kHAPError_InvalidData;
  }
  idx++; // dict tag

  for(uint8_t dictIdx = 0 ; dictIdx < 3; dictIdx ++){
    //expecting 3 dict entries of string type
    if(idx+1 > headerLen) return kHAPError_InvalidData;   //check 1
    if(payload[idx] < UTF8_LENGTH_START || payload[idx] > UTF8_LENGTH_STOP){
      HAPLogDebug(&kHAPLog_Default, "Expected 3 text keys in the datasend open header");
      return kHAPError_InvalidData;
    }
    uint8_t keyLen = payload[idx] - UTF8_LENGTH_START;
    uint8_t valLen = 0;
    idx++; //utf8 len tag

    switch(payload[idx]){
      case 'r': //request
        if(idx+keyLen > headerLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen != 7){
          HAPLogDebug(&kHAPLog_Default, "Expected request text key in the datasend open header");
          return kHAPError_InvalidData;
        }
        idx += keyLen;
        if(idx+1 > headerLen) return kHAPError_InvalidData;   //check 1
        valLen = payload[idx] - UTF8_LENGTH_START;
        idx++;
        if(idx+valLen > headerLen) return kHAPError_InvalidData; // check valLen
        if(valLen != 4 || payload[idx] != 'o'){
          HAPLogDebug(&kHAPLog_Default, "Expected request key to have 'open' value in the datasend open header");
          return kHAPError_InvalidData;
        }
        HAPLogInfo(&kHAPLog_Default, "Request good");
        idx += valLen;
      break;
      case 'p': //protocol
        if(idx+keyLen > payloadLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen != 8){
          HAPLogDebug(&kHAPLog_Default, "Expected protocol text key in the datasend open message");
          return kHAPError_InvalidData;
        }
        idx += keyLen;
        if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
        valLen = payload[idx] - UTF8_LENGTH_START;
        idx++;
        if(idx+valLen > payloadLen) return kHAPError_InvalidData; // check valLen
        if(valLen != 8 || payload[idx] != 'd'){
          HAPLogDebug(&kHAPLog_Default, "Expected protocol key to have 'datastream' value in the datasend open message");
          return kHAPError_InvalidData;
        }
        HAPLogInfo(&kHAPLog_Default, "Protocol good");
        idx += valLen;
      break;
      case 'i': //id
        if(idx+keyLen > headerLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen != 2){
          HAPLogDebug(&kHAPLog_Default, "Expected id text key in the datasend open header");
          return kHAPError_InvalidData;
        }
        idx += keyLen;
        if(idx+1 > headerLen) return kHAPError_InvalidData;   //check 1
        if(payload[idx] != INT64LE && payload[idx] != INT32LE && payload[idx] != INT16LE && payload[idx] != INT8 && payload[idx] < INTEGER_MINUS_ONE && payload[idx] > INTEGER_RANGE_STOP_39){ //
          HAPLogDebug(&kHAPLog_Default, "Expected id key to be integer in the datasend open header, %02x", payload[idx]);
          return kHAPError_InvalidData;
        }
        //idx++; consumed in read number
        if( readNumberToInt64(payload, &idx, headerLen, id) ) return kHAPError_InvalidData;
        HAPLogInfo(&kHAPLog_Default, "ID good: %d", *id);
      break;
      default:
        return kHAPError_InvalidData;
    }

  }

  // messege

  size_t messegeLen = payloadLen - headerLen;

  //expecting a dictionary of length at least 3
  if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
  if(payload[idx] <= DICTIONARY_LENGTH_START + 3 || payload[idx] >= DICTIONARY_LENGTH_STOP){
    HAPLogDebug(&kHAPLog_Default, "Expected at least 3 deep dictionary in the datasend open message");
    return kHAPError_InvalidData;
  }
  uint8_t dictMax = payload[idx] - DICTIONARY_LENGTH_START;
  idx++; // dict tag

  for(uint8_t dictIdx = 0 ; dictIdx < dictMax; dictIdx ++){
    //expecting 3 dict entries of string type
    if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
    if(payload[idx] < UTF8_LENGTH_START || payload[idx] > UTF8_LENGTH_STOP){
      HAPLogDebug(&kHAPLog_Default, "Expected 3 text keys in the datasend open message");
      return kHAPError_InvalidData;
    }
    uint8_t keyLen = payload[idx] - UTF8_LENGTH_START;
    uint8_t valLen = 0;
    idx++; //utf8 len tag

    switch(payload[idx]){
      case 't': //type or target
        if(idx+keyLen > payloadLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen == 4){
          idx += keyLen;
          if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
          valLen = payload[idx] - UTF8_LENGTH_START;
          idx++;
          if(idx+valLen > payloadLen) return kHAPError_InvalidData; // check valLen
          if(valLen != 18 || payload[idx] != 'i'){ //ipcamera.recording
            HAPLogDebug(&kHAPLog_Default, "Expected type key to have 'ipcamera.recording' value in the datasend open message");
            return kHAPError_InvalidData;
          }
          HAPLogInfo(&kHAPLog_Default, "type good");
          idx += valLen;
        }else if(keyLen == 6) {
          idx += keyLen;
          if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
          valLen = payload[idx] - UTF8_LENGTH_START;
          idx++;
          if(idx+valLen > payloadLen) return kHAPError_InvalidData; // check valLen
          if(valLen != 10 || payload[idx] != 'c'){ // controller
            HAPLogDebug(&kHAPLog_Default, "Expected target key to have 'controller' value in the datasend open message");
            return kHAPError_InvalidData;
          }
          HAPLogInfo(&kHAPLog_Default, "target good");
          idx += valLen;
        }else{
          HAPLogDebug(&kHAPLog_Default, "Expected type text/target key in the datasend open message");
          return kHAPError_InvalidData;

        }
      break;
      case 's': //streamId
        if(idx+keyLen > payloadLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen != 8){
          HAPLogDebug(&kHAPLog_Default, "Expected id text key in the datasend open message");
          return kHAPError_InvalidData;
        }
        idx += keyLen;
        if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
        if(payload[idx] != INT64LE && payload[idx] != INT32LE && payload[idx] != INT16LE && payload[idx] != INT8 && payload[idx] < INTEGER_MINUS_ONE && payload[idx] > INTEGER_RANGE_STOP_39){ //
          HAPLogDebug(&kHAPLog_Default, "Expected streamId key to be integer in the datasend open messege, %02x", payload[idx]);
          return kHAPError_InvalidData;
        }
        //idx++; consumed in read number
        if( readNumberToInt64(payload, &idx, payloadLen, streamId) ) return kHAPError_InvalidData;
        HAPLogInfo(&kHAPLog_Default, "streamId good: %d", *streamId);
      break;
      default:
        if(idx+keyLen > payloadLen) return kHAPError_InvalidData; //check keyLen
        idx += keyLen;
        if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
        if(payload[idx] < UTF8_LENGTH_START || payload[idx] > UTF8_LENGTH_STOP) return kHAPError_InvalidData;
        valLen = payload[idx] - UTF8_LENGTH_START;
        idx++;
        if(idx+valLen > payloadLen) return kHAPError_InvalidData; // check valLen
        HAPLogInfo(&kHAPLog_Default, "Passed on an unknown utf8 key:value (probably reason:recording)");
        idx += valLen;
      break;
    }

  }
  return kHAPError_None;  
}

HAPError makeDataSendOpenResponse( uint8_t * buffer, size_t maxLen, size_t * len, int64_t requestId, int64_t streamId ){

  //  0000  2de44872 6573706f 6e736544 6f70656e 4870726f 746f636f 6c476461 74615365    -.HresponseDopenHprotocolGdataSe
  //  0020  6e644269 64094673 74617475 7308e146 73746174 757308                        ndBid.Fstatus..Fstatus.

  uint8_t bytes[] =
  { 
  0xe4, // dict of 4
    0x48, // string of 8
      0x72, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, //response
    0x44, // string of 4
      0x6f, 0x70, 0x65, 0x6e, // open
    0x48, // string of 8
      0x70, 0x72, 0x6f, 0x74, 0x6f, 0x63, 0x6f, 0x6c, //protocol
    0x48, // string of 8
      0x64, 0x61, 0x74, 0x61, 0x53, 0x65, 0x6e, 0x64, // dataSend
    0x42, // string of 2
      0x69, 0x64 // id
  }; 
  uint8_t bytes2[] =
  { 
    0x46, // string of 6
      0x73, 0x74, 0x61, 0x74, 0x75, 0x73, // status
    INTEGER_RANGE_START_0
  };

  size_t idx = 0;

  HAPPrecondition(100+sizeof(bytes)+sizeof(bytes2) <= maxLen); //todo: fix
  idx = 1; //payload header

  //write status:0
  HAPRawBufferCopyBytes(&(buffer[1]), &bytes, sizeof(bytes));
  idx += sizeof(bytes);
  
  // write a LE number
  if( writeNumber(buffer,&idx, maxLen,requestId)  ) return kHAPError_InvalidData;

  HAPRawBufferCopyBytes(&(buffer[idx]), &bytes2, sizeof(bytes2));
  idx += sizeof(bytes2);

  // header over, write the payloadLen
  buffer[0] = (uint8_t)(idx) - 1; // exclude payloadLen byte of len 1

  //fill in the message part (0xe1) - 1 dict
  buffer[idx] = DICTIONARY_LENGTH_START + 1;
  idx++;

  //write status:0
  HAPRawBufferCopyBytes(&(buffer[idx]), &bytes2, sizeof(bytes2));
  idx += sizeof(bytes2);

  *len = idx;
  return kHAPError_None;
}


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
){
  size_t idx = 0;
  idx = 1; //payload header

  uint8_t header[] =
  { 
  0xe2, // dict of 2
    0x48, // string of 8
      0x70, 0x72, 0x6f, 0x74, 0x6f, 0x63, 0x6f, 0x6c, //protocol
    0x48, // string of 8
      0x64, 0x61, 0x74, 0x61, 0x53, 0x65, 0x6e, 0x64, // dataSend
    0x45, // string of 5
      0x65, 0x76, 0x65, 0x6E, 0x74, //event
    0x44, // string of 4
      0x64, 0x61, 0x74, 0x61, // data
  }; 
  if(idx+sizeof(header) > maxLen) return kHAPError_InvalidData;   //check
  HAPRawBufferCopyBytes(&(buffer[idx]), &header, sizeof(header));
  idx += sizeof(header);

  // header over, write the payloadLen
  buffer[0] = (uint8_t)(idx) - 1; // exclude payloadLen byte of len 1

  if(idx+1 > maxLen) return kHAPError_InvalidData;   //check 1
  if (endOfStream)
    buffer[idx] = 0xe3; // dict of 3 including endOfStream
  else 
    buffer[idx] = 0xe2;
  idx++;

  uint8_t message1[] =
  { 
    0x48, // string of 8
      0x73, 0x74, 0x72, 0x65, 0x61, 0x6D, 0x49, 0x64, // streamId
  };

  if(idx+sizeof(message1) > maxLen) return kHAPError_InvalidData;   //check
  HAPRawBufferCopyBytes(&(buffer[idx]), &message1, sizeof(message1));
  idx += sizeof(message1);

  // write streamId number
  if ( writeNumber(buffer, &idx, maxLen, streamId) ) return kHAPError_InvalidData;

  uint8_t message1b[] =
  {
    0x4b, //string of 11
      0x65, 0x6E, 0x64, 0x4F, 0x66, 0x53, 0x74, 0x72, 0x65, 0x61, 0x6D, //endOfStream
    0x01,
  };

  if(endOfStream){
    if(idx+sizeof(message1b) > maxLen) return kHAPError_InvalidData;   //check
    HAPRawBufferCopyBytes(&(buffer[idx]), &message1b, sizeof(message1b));
    idx += sizeof(message1b);
  }

  uint8_t message2[] =
  { 
    0x47, // string of 7
      0x70, 0x61, 0x63, 0x6B, 0x65, 0x74, 0x73, // packets
    0xd1, // array of 1
      0xe2,  // dict of 2 containing metadata and data
        0x48, // string of 8
          0x6D, 0x65, 0x74, 0x61, 0x64, 0x61, 0x74, 0x61, //metadata
  };

  uint8_t message2b[] =
  {
          0x48, //string of 8
            0x64, 0x61, 0x74, 0x61, 0x54, 0x79, 0x70, 0x65, //datatype
  };

  if(idx+sizeof(message2) > maxLen) return kHAPError_InvalidData;   //check
  HAPRawBufferCopyBytes(&(buffer[idx]), &message2, sizeof(message2));
  idx += sizeof(message2);

  if(idx+1 > maxLen) return kHAPError_InvalidData;   //check 1

  if (dataChunkSequenceNumber == 1)
    buffer[idx] = 0xe5; // dict of 5 including dataTotalSize
  else 
    buffer[idx] = 0xe4;
  idx++;

  if(idx+sizeof(message2b) > maxLen) return kHAPError_InvalidData;   //check
  HAPRawBufferCopyBytes(&(buffer[idx]), &message2b, sizeof(message2b));
  idx += sizeof(message2b);

  // write the datatype string
  size_t dataTypeLen = strnlen(dataType, 0x20);

  if(idx+1 > maxLen) return kHAPError_InvalidData;   //check 1
  buffer[idx] = 0x40 + dataTypeLen;
  idx++;
  strncpy(&buffer[idx], dataType, 0x20);
  idx += dataTypeLen;

  uint8_t message3[] =
  { 

          0x52, // string of 18
            0x64, 0x61, 0x74, 0x61, 0x53, 0x65, 0x71, 0x75, 0x65, 0x6E, 0x63, 0x65, 0x4E, 0x75, 0x6D, 0x62, 0x65, 0x72, //dataSequenceNumber
  };

  if(idx+sizeof(message3) > maxLen) return kHAPError_InvalidData;   //check
  HAPRawBufferCopyBytes(&(buffer[idx]), &message3, sizeof(message3));
  idx += sizeof(message3);

  // write dataSequenceNumber number
  if ( writeNumber(buffer, &idx, maxLen, dataSequenceNr) ) return kHAPError_InvalidData;

  uint8_t message4[] =
  { 
          0x4f, // string of 15
            0x69, 0x73, 0x4C, 0x61, 0x73, 0x74, 0x44, 0x61, 0x74, 0x61, 0x43, 0x68, 0x75, 0x6E, 0x6B, // isLastDataChunk
  };

  if(idx+sizeof(message4) > maxLen) return kHAPError_InvalidData;   //check
  HAPRawBufferCopyBytes(&(buffer[idx]), &message4, sizeof(message4));
  idx += sizeof(message4);

  // isLastDataChunk bool
  if(idx+1 > maxLen) return kHAPError_InvalidData;   //check
  if(isLastDataChunk) buffer[idx] = TRUE;
  else buffer[idx] = FALSE;
  idx++;

  uint8_t message5[] =
  { 
          0x57, // string of 23
            0x64, 0x61, 0x74, 0x61, 0x43, 0x68, 0x75, 0x6E, 0x6B, 0x53, 0x65, 0x71, 0x75, 0x65, 0x6E, 0x63, 0x65, 0x4E, 0x75, 0x6D, 0x62, 0x65, 0x72, // dataChunkSequenceNumber
  };

  if(idx+sizeof(message5) > maxLen) return kHAPError_InvalidData;   //check
  HAPRawBufferCopyBytes(&(buffer[idx]), &message5, sizeof(message5));
  idx += sizeof(message5);

  // write dataChunkSequenceNumber number
  if ( writeNumber(buffer, &idx, maxLen, dataChunkSequenceNumber) ) return kHAPError_InvalidData;

  uint8_t message5b[] =
  { 

          0x4d, // string of 13
            0x64, 0x61, 0x74, 0x61, 0x54, 0x6F, 0x74, 0x61, 0x6c, 0x53, 0x69, 0x7a, 0x65 //dataTotalSize
  };
  if (dataChunkSequenceNumber == 1){

    if(idx+sizeof(message5b) > maxLen) return kHAPError_InvalidData;   //check
    HAPRawBufferCopyBytes(&(buffer[idx]), &message5b, sizeof(message5b));
    idx += sizeof(message5b);

    // write dataTotalSize number
    if ( writeNumber(buffer, &idx, maxLen, dataTotalSize) ) return kHAPError_InvalidData;
  }

  uint8_t message6[] =
  { 
      0x44, // string of 4
        0x64, 0x61, 0x74, 0x61, // data
  };

  if(idx+sizeof(message6) > maxLen) return kHAPError_InvalidData;   //check
  HAPRawBufferCopyBytes(&(buffer[idx]), &message6, sizeof(message6));
  idx += sizeof(message6);

  // don't forget to write
  // data tag and then the data
  // in the buffer after this function

  *len = idx;
  return kHAPError_None;
}


HAPError makeDataSendCloseEvent( uint8_t * buffer, size_t maxLen, size_t * len, int64_t streamId ){
  size_t idx = 0;
  idx = 1; //payload header

  uint8_t header[] =
  { 
  0xe2, // dict of 2
    0x48, // string of 8
      0x70, 0x72, 0x6f, 0x74, 0x6f, 0x63, 0x6f, 0x6c, //protocol
    0x48, // string of 8
      0x64, 0x61, 0x74, 0x61, 0x53, 0x65, 0x6e, 0x64, // dataSend
    0x45, // string of 5
      0x65, 0x76, 0x65, 0x6E, 0x74, //event
    0x45, // string of 5
      0x63, 0x6c, 0x6f, 0x73, 0x65, // close
  }; 
  if(idx+sizeof(header) > maxLen) return kHAPError_InvalidData;   //check
  HAPRawBufferCopyBytes(&(buffer[idx]), &header, sizeof(header));
  idx += sizeof(header);

  // header over, write the payloadLen
  buffer[0] = (uint8_t)(idx) - 1; // exclude payloadLen byte of len 1


  uint8_t message1[] =
  { 
  0xe2, // dict of 2
    0x48, // string of 8
      0x73, 0x74, 0x72, 0x65, 0x61, 0x6D, 0x49, 0x64, // streamId
  };

  if(idx+sizeof(message1) > maxLen) return kHAPError_InvalidData;   //check
  HAPRawBufferCopyBytes(&(buffer[idx]), &message1, sizeof(message1));
  idx += sizeof(message1);

  // write streamId number
  if ( writeNumber(buffer, &idx, maxLen, streamId) ) return kHAPError_InvalidData;

  uint8_t message2[] =
  { 
    0x46, // string of 6
      0x73, 0x74, 0x61, 0x74, 0x75, 0x73, // status
    INTEGER_RANGE_START_0
  };

  if(idx+sizeof(message2) > maxLen) return kHAPError_InvalidData;   //check
  HAPRawBufferCopyBytes(&(buffer[idx]), &message2, sizeof(message2));
  idx += sizeof(message2);


  *len = idx;
  return kHAPError_None;
}


HAPError processDataSendAck( uint8_t * payload, size_t payloadLen, int64_t * streamId){

  //  0000  1de24565 76656e74 4361636b 4870726f 746f636f 6c486461 74615365 6e64e24b    ..EeventCackHprotocolHdataSend.K
  //  0020  656e644f 66537472 65616d01 48737472 65616d49 6409                          endOfStream.HstreamId.


  size_t idx = 0;

  if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
  size_t headerLen = payload[idx]+1;
  idx++;

  if(headerLen >= payloadLen) return kHAPError_InvalidData;

  //expecting a dictionary of length 2
  if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
  if(payload[idx] != DICTIONARY_LENGTH_START + 2){
    HAPLogDebug(&kHAPLog_Default, "Expected a 2 deep dictionary in the datasend ack header");
    return kHAPError_InvalidData;
  }
  idx++; // dict tag

  for(uint8_t dictIdx = 0 ; dictIdx < 2; dictIdx ++){
    //expecting 2 dict entries of string type
    if(idx+1 > headerLen) return kHAPError_InvalidData;   //check 1
    if(payload[idx] < UTF8_LENGTH_START || payload[idx] > UTF8_LENGTH_STOP){
      HAPLogDebug(&kHAPLog_Default, "Expected 2 text keys in the datasend ack header");
      return kHAPError_InvalidData;
    }
    uint8_t keyLen = payload[idx] - UTF8_LENGTH_START;
    uint8_t valLen = 0;
    idx++; //utf8 len tag

    switch(payload[idx]){
      case 'e': //event
        if(idx+keyLen > headerLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen != 5){
          HAPLogDebug(&kHAPLog_Default, "Expected event text key in the datasend event header");
          return kHAPError_InvalidData;
        }
        idx += keyLen;
        if(idx+1 > headerLen) return kHAPError_InvalidData;   //check 1
        valLen = payload[idx] - UTF8_LENGTH_START;
        idx++;
        if(idx+valLen > headerLen) return kHAPError_InvalidData; // check valLen
        if(valLen != 3 || payload[idx] != 'a'){
          HAPLogDebug(&kHAPLog_Default, "Expected event key to have 'ack' value in the datasend ack header");
          return kHAPError_InvalidData;
        }
        HAPLogInfo(&kHAPLog_Default, "Event good");
        idx += valLen;
      break;
      case 'p': //protocol
        if(idx+keyLen > payloadLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen != 8){
          HAPLogDebug(&kHAPLog_Default, "Expected protocol text key in the datasend ack message");
          return kHAPError_InvalidData;
        }
        idx += keyLen;
        if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
        valLen = payload[idx] - UTF8_LENGTH_START;
        idx++;
        if(idx+valLen > payloadLen) return kHAPError_InvalidData; // check valLen
        if(valLen != 8 || payload[idx] != 'd'){
          HAPLogDebug(&kHAPLog_Default, "Expected protocol key to have 'datastream' value in the datasend ack message");
          return kHAPError_InvalidData;
        }
        HAPLogInfo(&kHAPLog_Default, "Protocol good");
        idx += valLen;
      break;
      default:
        return kHAPError_InvalidData;
    }

  }

  // messege

  size_t messegeLen = payloadLen - headerLen;

  //expecting a dictionary of length at least 2
  if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
  if(payload[idx] != DICTIONARY_LENGTH_START + 2 ){
    HAPLogDebug(&kHAPLog_Default, "Expected at 2 deep dictionary in the datasend ack message");
    return kHAPError_InvalidData;
  }
  uint8_t dictMax = payload[idx] - DICTIONARY_LENGTH_START;
  idx++; // dict tag

  for(uint8_t dictIdx = 0 ; dictIdx < dictMax; dictIdx ++){
    //expecting 2 dict entries of string type
    if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
    if(payload[idx] < UTF8_LENGTH_START || payload[idx] > UTF8_LENGTH_STOP){
      HAPLogDebug(&kHAPLog_Default, "Expected 2 text keys in the datasend ack message");
      return kHAPError_InvalidData;
    }
    uint8_t keyLen = payload[idx] - UTF8_LENGTH_START;
    uint8_t valLen = 0;
    idx++; //utf8 len tag

    switch(payload[idx]){
      case 'e': //endOfStream
        if(idx+keyLen > payloadLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen == 11){
          idx += keyLen;
          if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
          if(payload[idx] != TRUE && payload[idx] != FALSE){ //
            HAPLogDebug(&kHAPLog_Default, "Expected endOfStream key to be integer in the datasend ack messege, %02x", payload[idx]);
            return kHAPError_InvalidData;
          }
          //idx++; consumed in read number
          idx += 1; // bool = true
        }
      break;
      case 's': //streamId
        if(idx+keyLen > payloadLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen != 8){
          HAPLogDebug(&kHAPLog_Default, "Expected id text key in the datasend ack message");
          return kHAPError_InvalidData;
        }
        idx += keyLen;
        if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
        if(payload[idx] != INT64LE && payload[idx] != INT32LE && payload[idx] != INT16LE && payload[idx] != INT8 && payload[idx] < INTEGER_MINUS_ONE && payload[idx] > INTEGER_RANGE_STOP_39){ //
          HAPLogDebug(&kHAPLog_Default, "Expected streamId key to be integer in the datasend open messege, %02x", payload[idx]);
          return kHAPError_InvalidData;
        }
        //idx++; consumed in read number
        if( readNumberToInt64(payload, &idx, payloadLen, streamId) ) return kHAPError_InvalidData;
        HAPLogInfo(&kHAPLog_Default, "streamId good: %d", *streamId);
      break;
      default:
        return kHAPError_InvalidData;
      break;
    }

  }
  return kHAPError_None;  
}

HAPError processDataSendClose( uint8_t * payload, size_t payloadLen, int64_t * streamId,  int64_t * reason){

  //  0000  1fe24565 76656e74 45636c6f 73654870 726f746f 636f6c48 64617461 53656e64    ..EeventEcloseHprotocolHdataSend
  //  0020  e2487374 7265616d 49640946 72656173 6f6e08                                 .HstreamId.Freason.
  size_t idx = 0;

  if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
  size_t headerLen = payload[idx]+1;
  idx++;

  if(headerLen >= payloadLen) return kHAPError_InvalidData;

  //expecting a dictionary of length 2
  if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
  if(payload[idx] != DICTIONARY_LENGTH_START + 2){
    HAPLogDebug(&kHAPLog_Default, "Expected a 2 deep dictionary in the datasend close header");
    return kHAPError_InvalidData;
  }
  idx++; // dict tag

  for(uint8_t dictIdx = 0 ; dictIdx < 2; dictIdx ++){
    //expecting 2 dict entries of string type
    if(idx+1 > headerLen) return kHAPError_InvalidData;   //check 1
    if(payload[idx] < UTF8_LENGTH_START || payload[idx] > UTF8_LENGTH_STOP){
      HAPLogDebug(&kHAPLog_Default, "Expected 2 text keys in the datasend close header");
      return kHAPError_InvalidData;
    }
    uint8_t keyLen = payload[idx] - UTF8_LENGTH_START;
    uint8_t valLen = 0;
    idx++; //utf8 len tag

    switch(payload[idx]){
      case 'e': //event
        if(idx+keyLen > headerLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen != 5){
          HAPLogDebug(&kHAPLog_Default, "Expected event text key in the datasend close header");
          return kHAPError_InvalidData;
        }
        idx += keyLen;
        if(idx+1 > headerLen) return kHAPError_InvalidData;   //check 1
        valLen = payload[idx] - UTF8_LENGTH_START;
        idx++;
        if(idx+valLen > headerLen) return kHAPError_InvalidData; // check valLen
        if(valLen != 5 || payload[idx] != 'c'){
          HAPLogDebug(&kHAPLog_Default, "Expected request key to have 'close' value in the datasend close header");
          return kHAPError_InvalidData;
        }
        HAPLogInfo(&kHAPLog_Default, "Event good");
        idx += valLen;
      break;
      case 'p': //protocol
        if(idx+keyLen > payloadLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen != 8){
          HAPLogDebug(&kHAPLog_Default, "Expected protocol text key in the datasend close message");
          return kHAPError_InvalidData;
        }
        idx += keyLen;
        if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
        valLen = payload[idx] - UTF8_LENGTH_START;
        idx++;
        if(idx+valLen > payloadLen) return kHAPError_InvalidData; // check valLen
        if(valLen != 8 || payload[idx] != 'd'){
          HAPLogDebug(&kHAPLog_Default, "Expected protocol key to have 'datastream' value in the datasend close message");
          return kHAPError_InvalidData;
        }
        HAPLogInfo(&kHAPLog_Default, "Protocol good");
        idx += valLen;
      break;
      default:
        return kHAPError_InvalidData;
    }

  }

  // messege

  size_t messegeLen = payloadLen - headerLen;

  //expecting a dictionary of length at least 2
  if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
  if(payload[idx] != DICTIONARY_LENGTH_START + 2 ){
    HAPLogDebug(&kHAPLog_Default, "Expected at 2 deep dictionary in the datasend close message");
    return kHAPError_InvalidData;
  }
  uint8_t dictMax = payload[idx] - DICTIONARY_LENGTH_START;
  idx++; // dict tag

  for(uint8_t dictIdx = 0 ; dictIdx < dictMax; dictIdx ++){
    //expecting 2 dict entries of string type
    if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
    if(payload[idx] < UTF8_LENGTH_START || payload[idx] > UTF8_LENGTH_STOP){
      HAPLogDebug(&kHAPLog_Default, "Expected 2 text keys in the datasend close message");
      return kHAPError_InvalidData;
    }
    uint8_t keyLen = payload[idx] - UTF8_LENGTH_START;
    uint8_t valLen = 0;
    idx++; //utf8 len tag

    switch(payload[idx]){
      case 'r': //reason
        if(idx+keyLen > payloadLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen == 6){
          idx += keyLen;
          if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
          if(payload[idx] != INT64LE && payload[idx] != INT32LE && payload[idx] != INT16LE && payload[idx] != INT8 && payload[idx] < INTEGER_MINUS_ONE && payload[idx] > INTEGER_RANGE_STOP_39){ //
            HAPLogDebug(&kHAPLog_Default, "Expected reason key to be integer in the datasend close messege, %02x", payload[idx]);
            return kHAPError_InvalidData;
          }
          //idx++; consumed in read number
          if( readNumberToInt64(payload, &idx, payloadLen, reason) ) return kHAPError_InvalidData;
          HAPLogInfo(&kHAPLog_Default, "reason good: %d", *reason);
          idx += valLen;
        }else{
          HAPLogDebug(&kHAPLog_Default, "Expected reason key in the datasend close message");
          return kHAPError_InvalidData;

        }
      break;
      case 's': //streamId
        if(idx+keyLen > payloadLen) return kHAPError_InvalidData; //check keyLen
        if(keyLen != 8){
          HAPLogDebug(&kHAPLog_Default, "Expected id text key in the datasend close message");
          return kHAPError_InvalidData;
        }
        idx += keyLen;
        if(idx+1 > payloadLen) return kHAPError_InvalidData;   //check 1
        if(payload[idx] != INT64LE && payload[idx] != INT32LE && payload[idx] != INT16LE && payload[idx] != INT8 && payload[idx] < INTEGER_MINUS_ONE && payload[idx] > INTEGER_RANGE_STOP_39){ //
          HAPLogDebug(&kHAPLog_Default, "Expected streamId key to be integer in the datasend open messege, %02x", payload[idx]);
          return kHAPError_InvalidData;
        }
        //idx++; consumed in read number
        if( readNumberToInt64(payload, &idx, payloadLen, streamId) ) return kHAPError_InvalidData;
        HAPLogInfo(&kHAPLog_Default, "streamId good: %d", *streamId);
      break;
      default:
        return kHAPError_InvalidData;
      break;
    }

  }
  return kHAPError_None;  
}

