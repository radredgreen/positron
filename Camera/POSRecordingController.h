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


#ifndef POSRECORDINGCONTROLLER_H
#define POSRECORDINGCONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#include "HAP.h"
#include "HAPBase.h"

#include "App.h"
#include "POSDataStream.h"

//0x40000
#define kHAPDataStream_ChunkBufferSize 0x50000 

typedef struct
{
  uint64_t streamId;
  HAPIPByteBuffer chunkBuffer;
  uint8_t chunkBufferData[kHAPDataStream_ChunkBufferSize];
  posDataStreamStruct * datastreamCtx;


  bool isDatastreamCtxValid;
  bool newChunkNeeded;
  bool isInitializationSent;
  bool closeRequested;
  bool isLastDataChunk;
  bool endOfStreamSent;
  bool isOverflowed;  //set if the recording ring buffer overwrites the data being sent
  uint64_t dataSequenceNumber;
  uint64_t dataChunkSequenceNumber;
  uint64_t dataTotalSize;
  uint64_t sentDataSize;

} posRecordingBufferStruct;


void RecordingContextInitialize(AccessoryContext* context);
void RecordingContextDeintialize(AccessoryContext* context); 
void posStartRecord(AccessoryContext* context HAP_UNUSED);
void posStopRecord(AccessoryContext* context HAP_UNUSED);
void posReconfigureRecord(AccessoryContext* context HAP_UNUSED);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
