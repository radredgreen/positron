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

#include <string.h> // for memset
#include <pthread.h>

#include <HAP.h>
#include <HAP+Internal.h>

#include "POSDataStream.h"
#include "POSDataStreamParser.h"

#include "POSRecordingController.h"

#include "hexdump.h"

/*
State machine design
SETUP:
  Setup call -> ACCEPT
              Timeout (10s) -> CLOSE
  Rx Callback - N/A
  Tx Callback - N/A
  Recording Thread - N/A

ACCEPT:
  POSDataStreamListenerCallBack - Find an available datastream slot -> WAIT_FOR_HELLO
              No slots -> Close the listener (no slot was opened)
  Tx Callback - N/A
  Recording Thread - N/A

WAIT_FOR_HELLO:
  Rx Callback - Get Hello -> SEND_HELLO
              Need more data -> WAIT_FOR_HELLO
              Timeout (10s) -> CLOSE
  Tx Callback - N/A
  Recording Thread - N/A

SEND_HELLO:
  Rx Callback - N/A
  Tx Callback - Finish Send -> WAIT_FOR_OPEN
              More to send -> SEND_HELLO
              Tx Err -> CLOSE
  Recording Thread - N/A

WAIT_FOR_OPEN:
  Rx Callback - Get Open -> SEND_OPEN
              Need more data -> WAIT_FOR_OPEN
              Timeout (10s) -> CLOSE
  Tx Callback - N/A
  Recording Thread - N/A

SEND_OPEN:
  Rx Callback - N/A
  Tx Callback - Finsh Send -> GET_CHUNK
              More to send -> SEND_OPEN
              Tx Err -> CLOSE
              MUTEX
              Connect to RecordingController,  datastream and recordingBuffer share state
              RecordingController already in use -> CLOSE
              posRecordingBuffer.isCtxValid = true
              posRecordingBuffer.newChunkNeeded = true
  Recording Thread - N/A

GET_CHUNK:
  Rx Callback - Get Ack -> CLOSE
              MUTEX
              Get Close -> request endOfStream
              Need more data -> Nop (state will move between GET_CHUNK and SEND_CHUNK and more data will come)
  Tx Callback - 
              N/A
              MUTEX
  Rec Thred - Generate a new chunk and put it in the recordingBuffer
              endOfStream sent -> WAIT_FOR_ACK
              not endOfStream -> SEND_CHUNK
              BufferOverflow -> CLOSE
              posRecordingBuffer.newChunkNeeded = false

SEND_CHUNK:
  Rx Callback - Get Ack -> CLOSE
              MUTEX
              Get Close -> request endOfStream
              Need more data -> Nop (state will move between GET_CHUNK and SEND_CHUNK and more data will come)
  Tx Callback - Finish Send -> GET_CHUNK
              More to send -> SEND_CHUNK
              Tx Err -> CLOSE
  Recording Thread - N/A

WAIT_FOR_ACK:
  Rx Callback - Get Ack -> SEND_CLOSE
                Get Close -> SEND_CLOSE
                Need more data -> WAIT_FOR_ACK
                Timeout (10s) -> CLOSE
  Tx Callback - N/A
  Recording Thread - N/A

SEND_CLOSE:
  Rx Callback - N/A
  Tx Callback - Finish Send -> CLOSE
                More to send -> SEND_CLOSE
              Tx Err -> CLOSE
  Recording Thread - N/A

CLOSE:
              MUTEX
  Rx Callback - Close listener
                Close Stream
                Close Interest
                Invalidate HDS state
  Tx Callback - Close Interest
  Rec Thred - Disconnect Recording Buffer from HDS Ctx
              Reset Transmit Recording State

*/


static const HAPLogObject logObject = { .subsystem = kHAP_LogSubsystem, .category = "POSDataStreamController" };


extern posRecordingBufferStruct posRecordingBuffer;

#define NUM_SUPPORTED_DATASTREAMS 8
posDataStreamStruct posDataStream[NUM_SUPPORTED_DATASTREAMS];


static void POSDataStreamListenerCallBack(
  HAPPlatformTCPStreamManagerRef tcpStreamManager,
  void* _Nullable context);

static void POSDataStreamHandleTimeout(HAPPlatformTimerRef timer, void* _Nullable context);

static void POSDataStreamHandleTCPStreamEvent(
        HAPPlatformTCPStreamManagerRef tcpStreamManager,
        HAPPlatformTCPStreamRef tcpStream,
        HAPPlatformTCPStreamEvent event,
        void* _Nullable context);
static void POSDataStreamHandleTCPStreamEventRx(
        HAPPlatformTCPStreamManagerRef tcpStreamManager,
        HAPPlatformTCPStreamRef tcpStream,
        HAPPlatformTCPStreamEvent event,
        void* _Nullable context);
static void POSDataStreamHandleTCPStreamEventTx(
        HAPPlatformTCPStreamManagerRef tcpStreamManager,
        HAPPlatformTCPStreamRef tcpStream,
        HAPPlatformTCPStreamEvent event,
        void* _Nullable context);

void UpdateInterests(posDataStreamStruct * datastream){
  //HAPLogError(&logObject,"UpdateInterests(hasbytes = %d, hasspace = %d)", datastream->interests.hasBytesAvailable, datastream->interests.hasSpaceAvailable);
  HAPPlatformTCPStreamUpdateInterests(
          &datastream->tcpStreamManager,
          datastream->tcpStream,
          datastream->interests,
          POSDataStreamHandleTCPStreamEvent,
          (void* ) datastream);
}

void POSDataStreamInit(){
  memset((uint8_t *)&posDataStream, 0, NUM_SUPPORTED_DATASTREAMS*sizeof(posDataStreamStruct));

  for( uint8_t idx = 0 ; idx < NUM_SUPPORTED_DATASTREAMS; idx ++){
    posDataStream[idx].outboundBuffer.data = (uint8_t *)&(posDataStream[idx].outboundBytes);
    posDataStream[idx].outboundBuffer.capacity = kHAPIPSession_DefaultOutboundBufferSize; 
    HAPIPByteBufferClear(&(posDataStream[idx].outboundBuffer));
    posDataStream[idx].outboundBuffer.limit = posDataStream[idx].outboundBuffer.position;

    posDataStream[idx].inboundBuffer.data = (uint8_t *)&(posDataStream[idx].inboundBytes);
    posDataStream[idx].inboundBuffer.capacity = kHAPIPSession_DefaultInboundBufferSize; 
    HAPIPByteBufferClear(&(posDataStream[idx].inboundBuffer));
    posDataStream[idx].inboundBuffer.limit = posDataStream[idx].inboundBuffer.position;
    // /posDataStream[idx].mutex= PTHREAD_MUTEX_INITIALIZER;
    posDataStream[idx].datastreamState = IDLE;
    pthread_mutex_init(&posDataStream[idx].mutex, NULL);

    HAPPlatformTCPStreamManagerCreate(
              &(posDataStream[idx].tcpStreamManager),
              &(const HAPPlatformTCPStreamManagerOptions) {
                      .interfaceName = NULL,       // Listen on all available network interfaces.
                      .port = 0, // Listen on unused port number from the ephemeral port range.
                      .maxConcurrentTCPStreams = 1 });
  }
  
}

void POSCloseDataStream(posDataStreamStruct * datastream){
  if (datastream -> timerSet) HAPPlatformTimerDeregister(datastream->TCPTimer);
  datastream -> timerSet = false;

  datastream->interests.hasBytesAvailable = false;
  datastream->interests.hasSpaceAvailable = false;
  UpdateInterests(datastream);
  if (HAPPlatformTCPStreamManagerIsListenerOpen(&(datastream->tcpStreamManager)))
    HAPPlatformTCPStreamManagerCloseListener(&(datastream->tcpStreamManager));
  
  HAPPlatformTCPStreamClose(&(datastream->tcpStreamManager), datastream->tcpStream);
  HAPIPByteBufferClear(&(datastream->outboundBuffer));
  datastream->outboundBuffer.limit = datastream->outboundBuffer.position;
  HAPIPByteBufferClear(&(datastream->inboundBuffer));
  datastream->inboundBuffer.limit = datastream->inboundBuffer.position;

  if(posRecordingBuffer.datastreamCtx == datastream){
    posRecordingBuffer.isDatastreamCtxValid = false;
    posRecordingBuffer.datastreamCtx == (posDataStreamStruct *) NULL;

    HAPIPByteBufferClear(&(posRecordingBuffer.chunkBuffer));
    posRecordingBuffer.chunkBuffer.limit = posRecordingBuffer.chunkBuffer.position;

    posRecordingBuffer.newChunkNeeded = false;
    posRecordingBuffer.isInitializationSent = false;
    posRecordingBuffer.closeRequested = false;
  }
  datastream->datastreamState = IDLE;

}

void start10sTimer(posDataStreamStruct * datastream){
  // 10s timeout

  HAPError err= HAPPlatformTimerRegister(
          &(datastream->TCPTimer),
          HAPPlatformClockGetCurrent() + kDataStreamResponseTimeout,
          &POSDataStreamHandleTimeout,
          (void*) datastream);
  if (err) {
      HAPAssert(err == kHAPError_OutOfResources);
      HAPLogError(&logObject, "Not enough resources to allocate datastream server callback timer.");
      HAPFatalError();
  }
  datastream -> timerSet = true;
}

void POSDataStreamSetup(setupDataStreamTransportWriteStruct setupDataStreamTransportWrite, 
                        setupDataStreamTransportReadStruct setupDataStreamTransportRead,
                        uint16_t * port,
                        uint8_t * sessionKey){ 

  HAPLogInfo(&kHAPLog_Default, "%s", __func__);

  //find an available datastream slot
  int selectedDataStream = -1;
  posDataStreamStruct * datastream = NULL;
  uint8_t idx = 0;
  for( ; idx < NUM_SUPPORTED_DATASTREAMS; idx ++){
    if ( posDataStream[idx].datastreamState == IDLE ) {
      selectedDataStream = idx;
      posDataStream[idx].datastreamState = SETUP;
      datastream = &(posDataStream[selectedDataStream]);
      HAPLogInfo(&logObject, "Datastream slot %d being used.", idx);
      break;
    }
  }
  if (idx ==  NUM_SUPPORTED_DATASTREAMS) {
    HAPLogError(&logObject, "No DataStream slot available to open a new listener");
    return; 
  }

  // copy salts to form key
  HAPRawBufferCopyBytes(datastream->accessorySalt, setupDataStreamTransportRead.accessorySalt, 32);
  HAPRawBufferCopyBytes(datastream->controllerSalt, setupDataStreamTransportWrite.controllerSalt, 32);


  //HAPLogBuffer(&kHAPLog_Default, sessionKey, CHACHA20_POLY1305_KEY_BYTES, "Session Key");

  // generate the accessory to controller key
  HAP_hkdf_sha512(
    (uint8_t *)&(datastream->AccessoryToControllerKey), // result
    CHACHA20_POLY1305_KEY_BYTES, // result size
    sessionKey, // current hap session shared secret
    X25519_BYTES, 
    (uint8_t *)&datastream->controllerSalt, // controller salt, accessory salt concatenation
    DSCONSALTLENGTH + DSACCSALTLENGTH,
    "HDS-Read-Encryption-Key",
    23
  );

  // generate the controller to accessory key
  HAP_hkdf_sha512(
    (uint8_t *)&(datastream->ControllertoAccessoryKey), // result
    CHACHA20_POLY1305_KEY_BYTES, // result size
    sessionKey, // current hap session shared secret
    X25519_BYTES, 
    (uint8_t *)&datastream->controllerSalt, // controller salt, accessory salt concatenation
    DSCONSALTLENGTH + DSACCSALTLENGTH,
    "HDS-Write-Encryption-Key",
    24
  );

  datastream->tx_nonce = 0;
  datastream->rx_nonce = 0;  
  

  HAPPlatformTCPStreamManagerOpenListener(
        &(datastream->tcpStreamManager),
        &POSDataStreamListenerCallBack,
        (void*) datastream);


  if ( !HAPPlatformTCPStreamManagerIsListenerOpen(&(datastream->tcpStreamManager))){
    HAPLogError(&logObject, "Failed to open a TCP stream manager");
    datastream -> datastreamState = IDLE;
  }

  // 10s timeout
  start10sTimer(datastream);

  *port = HAPPlatformTCPStreamManagerGetListenerPort(&(datastream->tcpStreamManager));
  HAPLogInfo(&kHAPLog_Default, "Listening port: %d", *port);

  datastream -> datastreamState = ACCEPT;
}

static void POSDataStreamHandleTimeout(HAPPlatformTimerRef timer, void* _Nullable context) {
  HAPPrecondition(context);
  posDataStreamStruct * datastream = ( posDataStreamStruct * ) context;
  HAPLogInfo(&kHAPLog_Default, "%s", __func__);
  POSCloseDataStream(datastream);
}

static void stopTimer(posDataStreamStruct * datastream){
// stop the connection timer
  if (datastream -> timerSet) HAPPlatformTimerDeregister(datastream->TCPTimer);
  datastream -> timerSet = false;
}

static void POSDataStreamListenerCallBack(
  HAPPlatformTCPStreamManagerRef tcpStreamManager,
  void* _Nullable context)
{
  // a connection has been made  
  HAPPrecondition(context);
  posDataStreamStruct * datastream = ( posDataStreamStruct * ) context;

  HAPLogInfo(&kHAPLog_Default, "%s", __func__);

  stopTimer(datastream);
  
  // accept the stream
  HAPError err;
  err = HAPPlatformTCPStreamManagerAcceptTCPStream(&datastream->tcpStreamManager, &datastream->tcpStream);
  if (err) {
      HAPLogError(&kHAPLog_Default, "HAPPlatformTCPStreamManagerAcceptTCPStream error.");
      // close the listener
      POSCloseDataStream(datastream);
      return;
  }
  // close the listener, we're only accepting one connection per setupDataStream characteristic write/read
  HAPPlatformTCPStreamManagerCloseListener(&datastream->tcpStreamManager);

  // 10s timeout
  start10sTimer(datastream);
  datastream->datastreamState = WAIT_FOR_HELLO; // new connection

  // let select know that we're interested in a call back when data is available
  datastream->interests.hasBytesAvailable = true;
  datastream->interests.hasSpaceAvailable = false;
  UpdateInterests(datastream);
}

static void POSDataStreamHandleTCPStreamEvent(
        HAPPlatformTCPStreamManagerRef tcpStreamManager,
        HAPPlatformTCPStreamRef tcpStream,
        HAPPlatformTCPStreamEvent event,
        void* _Nullable context) {
  HAPPrecondition(context);
  HAPLogInfo(&kHAPLog_Default, "%s", __func__);

  posDataStreamStruct * datastream = ( posDataStreamStruct * ) context;


  pthread_mutex_lock(&datastream->mutex);
  if (event.hasSpaceAvailable)
    POSDataStreamHandleTCPStreamEventTx(
        tcpStreamManager,
        tcpStream,
        event,
        context);
  if (event.hasBytesAvailable)
      POSDataStreamHandleTCPStreamEventRx(
        tcpStreamManager,
        tcpStream,
        event,
        context);
  pthread_mutex_unlock(&datastream->mutex);

}


static void POSDataStreamHandleTCPStreamEventRx(
        HAPPlatformTCPStreamManagerRef tcpStreamManager,
        HAPPlatformTCPStreamRef tcpStream,
        HAPPlatformTCPStreamEvent event,
        void* _Nullable context) {

  // data is ready to be read
  HAPPrecondition(context);
  HAPLogInfo(&kHAPLog_Default, "%s", __func__);

  posDataStreamStruct * datastream = ( posDataStreamStruct * ) context;
  HAPError err;

  HAPLogInfo(&kHAPLog_Default, "%s", __func__);

  stopTimer(datastream);

  HAPIPByteBuffer* bi;
  bi = &(datastream->inboundBuffer);
  HAPAssert(bi->data);
  HAPAssert(bi->position <= bi->limit);
  HAPAssert(bi->limit <= bi->capacity);

  size_t numBytes;
  uint32_t payloadLen;

  // the approach here needs to be to read the the header, then figure out how much more to read

  if( bi->position == bi->limit){
    err = HAPPlatformTCPStreamRead(
            tcpStreamManager,
            tcpStream,
            &bi->data[bi->limit],
            4, //bi->capacity - bi->limit,
            &numBytes);
  }else{
    payloadLen = (uint32_t)(bi->data[bi->position + 3]) + 
                ((uint32_t)(bi->data[bi->position + 2]) << 8) + 
                ((uint32_t)(bi->data[bi->position + 1]) << 16);

    err = HAPPlatformTCPStreamRead(
            tcpStreamManager,
            tcpStream,
            &bi->data[bi->limit],
            payloadLen + FRAME_HEADER_LEN + CHACHA20_POLY1305_TAG_BYTES - bi->limit, //bi->capacity - bi->limit,
            &numBytes);   
  }


  HAPLogInfo(&logObject, "%d bytes received on the data stream connection", numBytes);

  if (err == kHAPError_Unknown) {
    HAPLogError(&logObject, "error:Function 'HAPPlatformTCPStreamRead' failed.");
    POSCloseDataStream(datastream);
    return;
  } else if (err == kHAPError_Busy) {
    return;
  }

  HAPAssert(!err);
  if(numBytes == 0){
    HAPLogInfo(&kHAPLog_Default, "%s: remote side closed the connection", __func__);
    POSCloseDataStream(datastream);
    return;
  }
  HAPAssert(numBytes <= bi->capacity - bi->limit);
  bi->limit += numBytes;

  payloadLen = (uint32_t)(bi->data[bi->position + 3]) + 
                       ((uint32_t)(bi->data[bi->position + 2]) << 8) + 
                       ((uint32_t)(bi->data[bi->position + 1]) << 16);
  HAPLogBufferDebug(&kHAPLog_Default, bi->data, bi->limit, "Rx Raw Bytes");

  if( bi->data[bi->position] != 1 ){
    HAPLogError(&logObject, "Unknown frame format type.  Closing datastream.");
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    POSCloseDataStream(datastream);
    return;
  }


  if(datastream->datastreamState == WAIT_FOR_HELLO){
    if(payloadLen > 128 || payloadLen < 20){
      HAPLogError(&logObject, "Received initial frame with unexpected length (%d)", numBytes);
      HAPLogInfo(&kHAPLog_Default, "%s", __func__);
      POSCloseDataStream(datastream);
      return;
    }
  }
  
  if ( bi->limit < payloadLen + FRAME_HEADER_LEN + CHACHA20_POLY1305_TAG_BYTES){
    HAPLogInfo(&logObject, "Read: %d bytes, not done reading the hds frame, retruning to wait for wait for more bytes.", numBytes);
    start10sTimer(datastream);
    return; // not finished reading the hds frame
  }

  // one frame to read from the inboundBuffer
  uint64_t nonce_buffer;
  HAPWriteLittleUInt64(&nonce_buffer, datastream->rx_nonce);
  datastream->rx_nonce ++;

  int e = HAP_chacha20_poly1305_decrypt_aad(
          (uint8_t *)&bi->data[payloadLen+FRAME_HEADER_LEN], //(uint8_t *)&bytes[numBytes - CHACHA20_POLY1305_TAG_BYTES], //tag
          (uint8_t *)&bi->data[FRAME_HEADER_LEN], //(uint8_t *)&plainText, //&bytes[FRAME_HEADER_LEN], //plaintextBytes, start of the payload, overwrite
          (uint8_t *)&bi->data[FRAME_HEADER_LEN], //encryptedBytes, start of the payload, overwrite
          payloadLen, //numBytes - CHACHA20_POLY1305_TAG_BYTES - FRAME_HEADER_LEN, //cypher text len
          bi->data, //aadBytes
          FRAME_HEADER_LEN, //numAADBytes
          (uint8_t *)&nonce_buffer,
          sizeof nonce_buffer,
          datastream->ControllertoAccessoryKey);
  if (e) {
      HAPAssert(e == -1);
      HAPLogError(&kHAPLog_Default, "Decryption of message %llu failed.", (unsigned long long) datastream->rx_nonce);
      HAPLogBufferDebug(&kHAPLog_Default, datastream->ControllertoAccessoryKey, sizeof datastream->ControllertoAccessoryKey, "Decryption key");
      HAPLogBufferDebug(&kHAPLog_Default, datastream->accessorySalt, sizeof datastream->accessorySalt, "Accessory Salt");
      HAPLogBufferDebug(&kHAPLog_Default, datastream->controllerSalt, sizeof datastream->controllerSalt, "Controller Salt");
      HAPLogBufferDebug(&kHAPLog_Default, bi->data, payloadLen+CHACHA20_POLY1305_TAG_BYTES+FRAME_HEADER_LEN, "Message");
      HAPLogBufferDebug(&kHAPLog_Default, &bi->data[FRAME_HEADER_LEN], payloadLen, "Plain Text");
      HAPLogBufferDebug(&kHAPLog_Default, (uint8_t *)&bi->data[payloadLen+FRAME_HEADER_LEN], CHACHA20_POLY1305_TAG_BYTES, "Tag");

      POSCloseDataStream(datastream);
      return;
  }
  uint8_t * plainText = &bi->data[FRAME_HEADER_LEN];
  
  HAPLogInfo(&kHAPLog_Default, "Decryption success (%llu).", (unsigned long long) datastream->rx_nonce);
  HAPLogBufferDebug(&kHAPLog_Default, plainText, payloadLen, "Rx Plain Text");


  // numBytes = packet received from OS
  // payloadlen = len received in the frame len field at the head after the type (excl. type, len, auth tag)
  // headerLen = length of the header in the payload
  // messageLen = length of the message field in the payload

  uint8_t headerLen = plainText[0];
  if (headerLen > payloadLen) {
    HAPLogError(&logObject, "Invalid headerLen (%d)", headerLen);
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    POSCloseDataStream(datastream);
    return;
  }

  uint32_t messageLen = (payloadLen - headerLen) - 1;

  HAPIPByteBuffer* bo;
  bo = &(datastream->outboundBuffer);
  HAPAssert(bo->data);
  HAPAssert(bo->position <= bo->limit);
  HAPAssert(bo->limit <= bo->capacity);
  size_t len;


  switch(datastream->datastreamState){
    case(WAIT_FOR_HELLO):
      HAPLogInfo(&kHAPLog_Default, "Finished read in WAIT_FOR_HELLO state");
      err = processHelloPayload( plainText, payloadLen, &(datastream->requestId) );
      if (err != kHAPError_None){
        HAPLogError(&kHAPLog_Default, "Failed to processHelloPayload");
        POSCloseDataStream(datastream);
        return;
      }
      HAPLogInfo(&kHAPLog_Default, "hello request id: (%d)", datastream->requestId);

      HAPLogInfo(&kHAPLog_Default, "sending hello response id: (%d)", datastream->requestId);
      bo->position += 4; // reserve space for the tag and length field
      bo->limit = bo->position;
      err = makeHelloPayloadResponse( &bo->data[bo->position], bo->capacity - bo->position, &len, datastream->requestId );
      if (err != kHAPError_None){
        HAPLogError(&kHAPLog_Default, "Failed to makeHelloPayloadResponse");
        POSCloseDataStream(datastream);
        return;
      }

      HAPLogInfo(&kHAPLog_Default, "made makeHelloPayloadResponse");
      HAPLogBufferDebug(&kHAPLog_Default, &bo->data[bo->position], len, "Tx Plain Text");

      bo->limit = bo->position + len;
      HAPAssert(bo->position <= bo->limit);
      bo->position -= 4; // reserve space for the tag and length field
      bo->data[bo->position] = 0x01; // datastream type
      bo->data[bo->position+1] = (uint8_t)((len >> 16) & 0xff); // len MSB
      bo->data[bo->position+2] = (uint8_t)((len >> 8) & 0xff); // len
      bo->data[bo->position+3] = (uint8_t)((len) & 0xff); // len LSB
      HAPAssert(bo->limit <= bo->capacity-16); // check room for the tag

      HAPWriteLittleUInt64(&nonce_buffer, datastream->tx_nonce);
      datastream->tx_nonce ++;

      // encrypt the plainText
      HAP_chacha20_poly1305_encrypt_aad(
              &bo->data[bo->limit], // tag location
              &bo->data[4], // encryptedBytes
              &bo->data[4], // plaintextBytes
              bo->limit-4, // numPlaintextBytes
              bo->data, //aadBytes
              4, // numAADBytes
              (uint8_t *)&nonce_buffer,
              sizeof(nonce_buffer),
              datastream->AccessoryToControllerKey);
      bo->limit += 16; //tag size

      datastream->datastreamState = SEND_HELLO;

      // trigger sending
      datastream->interests.hasSpaceAvailable = true;
      datastream->interests.hasBytesAvailable = false;
      UpdateInterests(datastream);

      break;
    case(WAIT_FOR_OPEN):
      HAPLogInfo(&kHAPLog_Default, "Finished read in WAIT_FOR_OPEN state");
      err = processDataSendOpen( plainText, payloadLen, &(datastream->requestId), &(datastream->streamId) );
      if (err != kHAPError_None){
        HAPLogError(&kHAPLog_Default, "Failed to processDataSendOpen");
        POSCloseDataStream(datastream);
        return;
      }
      HAPLogInfo(&kHAPLog_Default, "datasend open id: (%lld), stream id: (%lld)", datastream->requestId, datastream->streamId);
      HAPLogInfo(&kHAPLog_Default, "sending datasend open response id: (%d)", datastream->requestId);
      bo->position += 4; // reserve space for the tag and length field
      bo->limit = bo->position;
      err = makeDataSendOpenResponse( &bo->data[bo->position], bo->capacity - bo->position, &len, datastream->requestId , datastream->streamId);
      if (err != kHAPError_None){
        HAPLogError(&kHAPLog_Default, "Failed to makeDataSendOpenResponse");
        POSCloseDataStream(datastream);
        return;
      }

      HAPLogInfo(&kHAPLog_Default, "made makeDataSendOpenResponse");
      HAPLogBufferDebug(&kHAPLog_Default, &bo->data[bo->position], len, "Tx Plain Text");

      bo->limit = bo->position + len;
      HAPAssert(bo->position <= bo->limit);
      bo->position -= 4; // reserve space for the tag and length field
      bo->data[bo->position] = 0x01; // datastream type
      bo->data[bo->position+1] = (uint8_t)((len >> 16) & 0xff); // len MSB
      bo->data[bo->position+2] = (uint8_t)((len >> 8) & 0xff); // len
      bo->data[bo->position+3] = (uint8_t)((len) & 0xff); // len LSB
      HAPAssert(bo->limit <= bo->capacity-16); // check room for the tag

      HAPWriteLittleUInt64(&nonce_buffer, datastream->tx_nonce);
      datastream->tx_nonce ++;

      // encrypt the plainText
      HAP_chacha20_poly1305_encrypt_aad(
              &bo->data[bo->limit], // tag location
              &bo->data[4], // encryptedBytes
              &bo->data[4], // plaintextBytes
              bo->limit-4, // numPlaintextBytes
              bo->data, //aadBytes
              4, // numAADBytes
              (uint8_t *)&nonce_buffer,
              sizeof(nonce_buffer),
              datastream->AccessoryToControllerKey);
      bo->limit += 16; //tag size

      datastream->datastreamState = SEND_OPEN;

      // trigger sending
      datastream->interests.hasSpaceAvailable = true;
      datastream->interests.hasBytesAvailable = false;
      UpdateInterests(datastream);

      break;
    case(GET_CHUNK):
      //fall through
    case(SEND_CHUNK):
      //fall through
    case(WAIT_FOR_ACK):
      HAPLogInfo(&kHAPLog_Default, "Finished read in GET_CHUNK/SEND_CHUNK/WAIT_FOR_ACK state");

      uint64_t closeStreamId, closeReason;
      if( processDataSendClose( plainText, payloadLen, &closeStreamId, &closeReason ) == kHAPError_None){
        HAPLogInfo(&kHAPLog_Default, "Close received on stream: %llu, Reason: %llu", closeStreamId, closeReason);        
        //if(closeReason == 6) HAPFatalError();
        if(datastream->datastreamState != WAIT_FOR_ACK) posRecordingBuffer.closeRequested = true;

      } else if ( processDataSendAck( plainText, payloadLen, &closeStreamId ) != kHAPError_None ){
        // unexpected receive message type if not close or ack
        POSCloseDataStream(datastream);
        return;
      }
      // received an ack message, or a close in wait_for_ack
      HAPLogInfo(&kHAPLog_Default, "Ack endOfStream received on stream: %llu", closeStreamId);        

      POSCloseDataStream(datastream);
      return;

      break;
      // don't bother parsing the ack
    default: // shouldn't be reading in any other state
      HAPLogError(&kHAPLog_Default, "Finished a receive in an unexpected datastream state. %d", datastream->datastreamState);
      POSCloseDataStream(datastream);
      return;
      break;
  }
  HAPIPByteBufferClear(bi);
  bi->limit = bi->position;
}


static void POSDataStreamHandleTCPStreamEventTx(
        HAPPlatformTCPStreamManagerRef tcpStreamManager,
        HAPPlatformTCPStreamRef tcpStream,
        HAPPlatformTCPStreamEvent event,
        void* _Nullable context) {
  // there is room to transmit
  HAPPrecondition(context);
  posDataStreamStruct * datastream = ( posDataStreamStruct * ) context;
  HAPLogInfo(&kHAPLog_Default, "%s, state: %d", __func__, datastream->datastreamState);

  HAPError err;

  HAPIPByteBuffer* b;
  if(datastream->datastreamState == SEND_CHUNK){
    HAPLogInfo(&kHAPLog_Default, "Using the posRecordingBuffer");
    b = &(posRecordingBuffer.chunkBuffer);
  } else {
    HAPLogInfo(&kHAPLog_Default, "Using the outboundBuffer");
    b = &(datastream->outboundBuffer);
  }

  HAPAssert(b->data);
  HAPAssert(b->position <= b->limit);
  HAPAssert(b->limit <= b->capacity);

  size_t numBytes;
  err = HAPPlatformTCPStreamWrite(
          HAPNonnull(tcpStreamManager),
          tcpStream,
          /* bytes: */ &b->data[b->position],
          /* maxBytes: */ b->limit - b->position,
          &numBytes);

  if (err == kHAPError_Unknown) {
    HAPLogError(&logObject, "error:Function 'HAPPlatformTCPStreamWrite' failed.");
    POSCloseDataStream(datastream);
    return;
  } else if (err == kHAPError_Busy) {
    return;
  }

  HAPAssert(!err);
  if (numBytes == 0) {
      HAPLogError(&logObject, "error:Function 'HAPPlatformTCPStreamWrite' failed: 0 bytes written.");
      POSCloseDataStream(datastream);
      return;
  }
  HAPAssert(numBytes <= b->limit - b->position);
  b->position += numBytes;
  if (b->position != b->limit){
    HAPLogInfo(&logObject, "Wrote: %d bytes, not done writing, returning to wait for more space available.", numBytes);
    return; // not finished writing the requested data
  }
  HAPIPByteBufferClear(b);
  b->limit = b->position;

  datastream->interests.hasSpaceAvailable = false;
  UpdateInterests(datastream);

  switch(datastream->datastreamState){
    case SEND_HELLO: // finished sending hello
      HAPLogInfo(&logObject, "finished tx in State: SEND_HELLO");

      datastream->datastreamState = WAIT_FOR_OPEN;
      start10sTimer(datastream);

      // trigger sending
      datastream->interests.hasSpaceAvailable = false;
      datastream->interests.hasBytesAvailable = true;
      UpdateInterests(datastream);
      break;
    case SEND_OPEN: // finished sending open
      HAPLogInfo(&logObject, "finished tx in State: SEND_OPEN");

      // connect to the recordingBuffer
      if(posRecordingBuffer.isDatastreamCtxValid){
        HAPLogError(&logObject, "another datastream is using the recording controller");
        POSCloseDataStream(datastream);
        return;
      }
      //todo: race condition here between controllers?
      posRecordingBuffer.streamId = datastream->streamId;
      posRecordingBuffer.datastreamCtx = datastream;
      posRecordingBuffer.newChunkNeeded = true;
      posRecordingBuffer.isInitializationSent = false;
      posRecordingBuffer.isOverflowed = false;
      posRecordingBuffer.isDatastreamCtxValid = true;
      datastream->datastreamState = GET_CHUNK;

      datastream->interests.hasSpaceAvailable = false;
      datastream->interests.hasBytesAvailable = true;
      UpdateInterests(datastream);
      break;
    case SEND_CHUNK:  //finished sending a chunk
      HAPLogInfo(&logObject, "finished tx in State: SEND_CHUNK");

      datastream->datastreamState = GET_CHUNK;

      posRecordingBuffer.newChunkNeeded = true;
      datastream->interests.hasSpaceAvailable = false;
      datastream->interests.hasBytesAvailable = true;
      UpdateInterests(datastream);
      break;
    case SEND_CLOSE: //finished sending close
      HAPLogError(&logObject, "finished tx in State: SEND_CLOSE");
      POSCloseDataStream(datastream);
      break;
    default: // shouldn't be sending in any other state
      HAPLogError(&kHAPLog_Default, "Finished a send in an unexpected datastream state. %d", datastream->datastreamState);
      POSCloseDataStream(datastream);
      return;
      break;

  }

}
