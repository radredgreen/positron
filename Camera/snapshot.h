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




#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAP.h"
#include "HAP+Internal.h"

#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <turbojpeg.h>

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

struct MemoryStruct {
    unsigned char* memory;
    size_t size;
};

int getSnapshot(unsigned long * outSize, uint8_t*  outBuffer, int, int);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif /* SNAPSHOT_H */
