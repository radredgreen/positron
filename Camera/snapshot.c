/**
 * @file streaming.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2021-01-20
 *
 * @copyright Copyright (c) 2021
 *
 */

#include "snapshot.h"
#include <stdio.h>
#include <errno.h>

extern pthread_mutex_t snapshot_mutex;

static const HAPLogObject logObject = { .subsystem = kHAP_LogSubsystem, .category = "CameraGetSnapshot" };

// The hardware video pipeline is setup to create jpeg snapshots at 640x480 (2hz)
// This handles the majority of apple devices I've seen.
// The apple watch, however, requests images at 320x240. This function will resize 
// down (in software) if a request for an image smaller than 640x480 occurs.
int getSnapshot(unsigned long * jpegSize, uint8_t* jpegBuf, int width, int height) {

	FILE *file=NULL;  
	unsigned char *srcbuf=NULL;
	size_t srcsize;
    
    struct timespec wait;
    wait.tv_sec = 0;
    wait.tv_nsec = 100000; //100 ms

	HAPLogInfo(&logObject, "Locking snapshot mutex");
	if ( pthread_mutex_timedlock(&snapshot_mutex, &wait) != 0 ){
		HAPLogError(&logObject, "Locking snapshot mutex failed: %s.  Exiting.\n", strerror(errno));
        *jpegSize = 0;
        return -1;
	}	
    
    file=fopen("/tmp/snap.jpg", "rb");

	fseek(file, 0, SEEK_END);
	srcsize=ftell(file);
	if( srcsize > *jpegSize ){
            HAPLogError(&logObject, "No room in jpg buffer, reading partial file");
            srcsize = *jpegSize;
	}
	srcbuf=(unsigned char *)malloc(srcsize);
	fseek(file, 0, SEEK_SET);
	fread(srcbuf, srcsize, 1, file);
	fclose(file);  
	file=NULL;
    
    HAPLogInfo(&logObject, "Unocking snapshot mutex");
	if ( pthread_mutex_unlock(&snapshot_mutex) != 0 ){
		HAPLogError(&logObject, "Unlocking snapshot mutex failed: %s.  Exiting.\n", strerror(errno));
        *jpegSize = 0;
        return -1;
	}	


    HAPLogInfo(&logObject, "%lu bytes retrieved\n", (unsigned long) srcsize);

    tjhandle tjInstanceIn = NULL;
    int flags = 0, snapWidth, snapHeight, snapSubSamp, snapColorspace;
    int pixelFormat = TJPF_BGRX;

    if ((tjInstanceIn = tjInitDecompress()) == NULL)
        HAPLogError(&logObject, "%s", tjGetErrorStr());

    if (tjDecompressHeader3(tjInstanceIn, srcbuf, srcsize, &snapWidth, &snapHeight, &snapSubSamp, &snapColorspace) < 0){
        HAPLogError(&logObject, "%s", tjGetErrorStr());
    }
    HAPLogInfo(
            &logObject,
            "Snapshot image: Width: %d, Height: %d, Sub: %d, Colorspace: %d",
            snapWidth,
            snapHeight,
            snapSubSamp,     // TJSAMP_420
            snapColorspace); // TJCS_YCbCr
    // Don't resize if requested size is greater than snapshot from camera.
    if (height >= snapHeight) {
        HAPLogDebug(&logObject, "%s", "Snapshot not scaled.");
        HAPRawBufferCopyBytes(jpegBuf, srcbuf, srcsize);
        *jpegSize = srcsize;
        free(srcbuf);

    } else {
        HAPLogDebug(&logObject, "%s", "Scaling snapshot.");
        tjhandle tjInstanceOut = NULL;
        unsigned char* dstBuf = NULL; /* Dynamically allocate the JPEG buffer */
        unsigned long dstSize = 0;

        dstSize = width * height * tjPixelSize[pixelFormat];

        if ((dstBuf = (unsigned char*) tjAlloc(dstSize)) == NULL)
            HAPLogError(&logObject, "%s", "Couldn't allocate dstBuf.");
        // tjDecompress2 also handles the resizing.
        if (tjDecompress2(tjInstanceIn, srcbuf, srcsize, dstBuf, width, 0, height, pixelFormat, flags) < 0)
            HAPLogError(&logObject, "tjDecompress2: %s", tjGetErrorStr());

        free(srcbuf);

        if ((tjInstanceOut = tjInitCompress()) == NULL)
            HAPLogError(&logObject, "%s", tjGetErrorStr());
        if (tjCompress2(
                    tjInstanceOut,
                    dstBuf,
                    width,
                    0,
                    height,
                    pixelFormat,
                    &jpegBuf,
                    jpegSize, //contains the buffer size in and the image size out
                    snapSubSamp,
                    95,
                    flags |= TJFLAG_NOREALLOC) < 0){
            HAPLogError(&logObject, "%s", tjGetErrorStr());
        }
        if (tjDecompressHeader3(tjInstanceIn, jpegBuf, *jpegSize, &snapWidth, &snapHeight, &snapSubSamp, &snapColorspace) < 0){
            HAPLogError(&logObject, "tjDecompressHeader3: %s", tjGetErrorStr());
        }
        HAPLogInfo(
                &logObject,
                "Snapshot image resized: Width: %d, Height: %d, Sub: %d, Colorspace: %d",
                snapWidth,
                snapHeight,
                snapSubSamp,     // TJSAMP_420
                snapColorspace); // TJCS_YCbCr

        tjFree(dstBuf);
        dstBuf = NULL;
        tjDestroy(tjInstanceOut);
        tjInstanceOut = NULL;
    }
    tjDestroy(tjInstanceIn);
    tjInstanceIn = NULL;
    HAPLogInfo(&logObject, "Successful %s.\n\n", __func__);
    return 0;
}
