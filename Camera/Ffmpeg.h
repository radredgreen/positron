/**
 * @file Ffmpeg.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2021-01-26
 *
 * @copyright Copyright (c) 2021
 *
 */
#ifndef FFMPEG_H
#define FFMPEG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAP.h"
//#include <libavformat/avformat.h>
//#include <openssl/rand.h>
//#include <libavutil/base64.h>
//#include <libavcodec/avcodec.h>
#include <unistd.h>
//#include <libavutil/time.h>
//#include <libavutil/mathematics.h>

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif
/*
typedef struct {
    AVFormatContext* format_context;
    AVCodecContext* codec_context;
    AVCodec* decoder;
    AVStream* strcm;
    AVPacket* packet;
    AVDictionary* opts;
    const char* in_filename;
} file_context;

typedef struct {
    AVFormatContext* format_context;
    AVCodecContext* codec_context;
    const AVCodec* decoder;
    AVStream* strm;
    AVPacket* packet;
    AVDictionary* opts;
    const char* url;
    int width;
    int height;
    enum AVPixelFormat pix_fmt;
} rtsp_context;

typedef struct {
    AVFormatContext* format_context;
    AVCodecContext* codec_context;
    const AVCodec* encoder;
    AVStream* strm;
    AVPacket* packet;
    AVDictionary* opts;
    // unsigned char key[16];
    // unsigned char salt[14];
    // char *srtp_encoded;
    // const char *url;
} srtp_context;
*/
void* startInStream(void* context);
void* startOutStream(void* context);
void StreamContextInitialize(void* context);
void StreamContextDeintialize(void* context);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif /* FFMPEG_H */
