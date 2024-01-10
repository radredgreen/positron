#include "HAP.h"

// uint8_t numBytes = 123;

uint8_t selectedRTP[] = {
    0x01, 0x15,                                     // Session Control
    0x02, 0x01, 0x01,                               // Session Command
    0x01, 0x10,                                     // Session ID
    0xF3, 0x54, 0x6A, 0x47, 0x5D, 0x1F, 0x4E, 0xDB, // 16-bit
    0x9B, 0xB3, 0x23, 0x99, 0x4E, 0x6A, 0x90, 0x4C, // UUID
    0x02, 0x34,                                     // Selected Video Params
    0x01, 0x01, 0x00,                               // Video Codec Type 0 - H264
    0x02, 0x09,                                     // Codec Params
    0x01, 0x01, 0x02,                               // 2 - High Profile
    0x02, 0x01, 0x02,                               // 2 - Level 4
    0x03, 0x01, 0x00,                               // 0 - non-interleaved packet mode
    0x03, 0x0B,                                     // Video Attributes
    0x01, 0x02, 0x00, 0x05,                         // Width - 1280
    0x02, 0x02, 0xD0, 0x02,                         // Height - 720
    0x03, 0x01, 0x1E,                               // Framerate - 30
    0x04, 0x17,                                     // Video RTP
    0x01, 0x01, 0x63,                               // Payload type
    0x02, 0x04, 0x87, 0x61, 0xC2, 0xB8,             // SSRC
    0x03, 0x02, 0x2B, 0x01,                         // Max bitrate
    0x04, 0x04, 0x00, 0x00, 0x00, 0x3F,             // Min RTCP Interval
    0x05, 0x02, 0x62, 0x05,                         // Max MTU
    0x03, 0x2C,                                     // Selected Audio Params
    0x01, 0x01, 0x02,                               // Audio Codec Type 2 - AAC-ELD
    0x02, 0x0C,                                     // Codec Params
    0x01, 0x01, 0x01,                               // Audio channels - 1
    0x02, 0x01, 0x00,                               // 0 - variable bit rate
    0x03, 0x01, 0x01,                               // 1 - 16 kHz Audio. 8 wouldn't work when sending supported
    0x04, 0x01, 0x1E,                               // 30 - RTP time in ms.
    0x03, 0x16,                                     // Audio RTP
    0x01, 0x01, 0x6E,                               // Payload Type
    0x02, 0x04, 0x69, 0x21, 0xB1, 0xAA,             // SSRC
    0x03, 0x02, 0x18, 0x00,                         // Maximum bitrate
    0x04, 0x04, 0x00, 0x00, 0xA0, 0x40,             // Min RTCP Interval
    0x06, 0x01, 0x0D,                               // Comfort Noise Payload Type
    0x04, 0x01, 0x00,                               // Comfort noise
};