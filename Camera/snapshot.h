/**
 * @file snapshot.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2021-01-20
 *
 * @copyright Copyright (c) 2021
 *
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
