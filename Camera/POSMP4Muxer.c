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
 * 
 * 
 * Inspired by: https://github.com/aspt/mp4
 */

//mmap
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <stdio.h>

#include "HAPBase.h"
#include "HAPAssert.h"

#include "POSRingBufferVideoIn.h"
#include "POSMP4Muxer.h"

#include "hexdump.h"

// File timescale
#define MOOV_TIMESCALE 1000


/*
*   Endian-independent byte-write macros
*/
#define MP4_WR(x, n) HAPAssert((void *)(write_ptr+1)<(void *)(buf+maxSize));*write_ptr++ = (unsigned char)((x) >> 8*n)
#define WR1(x) MP4_WR(x,0);
#define WR2(x) MP4_WR(x,1); MP4_WR(x,0);
#define WR3(x) MP4_WR(x,2); MP4_WR(x,1); MP4_WR(x,0);
#define WR4(x) MP4_WR(x,3); MP4_WR(x,2); MP4_WR(x,1); MP4_WR(x,0);
// esc-coded OD length
#define MP4_WRITE_OD_LEN(size) if (size > 0x7F) do { size -= 0x7F; WR1(0x00ff);} while (size > 0x7F); WR1(size)

#define MP4_WRITE_STRING(s) {size_t len = strlen(s) + 1; HAPAssert((void *)(write_ptr+len)<(void *)(buf+maxSize)); memcpy(write_ptr, s, len); write_ptr += len;}

#define MP4_WR4_PTR(p,x) (p)[0]=(char)((x) >> 8*3); (p)[1]=(char)((x) >> 8*2); (p)[2]=(char)((x) >> 8*1); (p)[3]=(char)((x));

// Finish atom: update atom size field
#define MP4_END_ATOM --stack; MP4_WR4_PTR((unsigned char*)*stack, write_ptr - *stack);}

// Initiate atom: save position of size field on stack
#define MP4_ATOM(x)  {*stack++ = write_ptr; write_ptr += 4; WR4(x);

// Atom with 'FullAtomVersionFlags' field
#define MP4_FULL_ATOM(x, flag)  MP4_ATOM(x); WR4(flag);

int POSWriteMoov(char * buf, size_t maxSize, POSMp4VideoTrack * vtrack, POSMp4AudioTrack * atrack){

    // atoms nesting stack
    unsigned char * stack_base[20];
    unsigned char ** stack = stack_base;

    // in-memory indexes
    unsigned char * write_base, * write_ptr;

    write_ptr = buf;
    write_base = write_ptr;

    unsigned int ntr, ntracks;

    ntracks = atrack->mute?1:2;
    
    int width = 1920; // todo
    int height = 1080;  //todo
    // todo: fix audio
    int dsi_bytes_cnt = 2;  // bytes count
    uint8_t dsi_buf[1024] = 	{0x12,0x88}; // TEMP

    char * language = "und";

    // hardcoded part of the file header
    static const unsigned char mp4e_box_ftyp[] = 
    {
        0,0,0,0x1c,'f','t','y','p',
        'm','p','4','2',0,0,0,1,
        'i','s','o','m','m','p','4','2','a','v','c','1', 
    };
    //fwrite(mp4e_box_ftyp, sizeof(mp4e_box_ftyp),1,mux);
    memcpy(buf, &mp4e_box_ftyp, sizeof(mp4e_box_ftyp));
    write_ptr += sizeof(mp4e_box_ftyp);

    // 
    // Write index atoms; order taken from Table 1 of [1]
    //
    MP4_ATOM(BOX_moov);
        MP4_FULL_ATOM(BOX_mvhd, 0);
            WR4(0); // creation_time
            WR4(0); // modification_time

            WR4(MOOV_TIMESCALE); // duration
            WR4(0); // duration

            WR4(0x00010000); // rate 
            WR2(0x0100); // volume
            WR2(0); // reserved
            WR4(0); // reserved
            WR4(0); // reserved

            // matrix[9]
            WR4(0x00010000);WR4(0);WR4(0);
            WR4(0);WR4(0x00010000);WR4(0);
            WR4(0);WR4(0);WR4(0x40000000);

            // pre_defined[6]
            WR4(0); WR4(0); WR4(0); 
            WR4(0); WR4(0); WR4(0); 

            //next_track_ID is a non-zero integer that indicates a value to use for the track ID of the next track to be
            //added to this presentation. Zero is not a valid track ID value. The value of next_track_ID shall be
            //larger than the largest track-ID in use.
            //WR4(ntracks+1); 
            WR4(0xFFFFFFFF); 
        MP4_END_ATOM;
    
    for (ntr = 0; ntr < ntracks; ntr++){
        unsigned handler_type;
        const char * handler_ascii = NULL;
        int samples_count = 0;
        int track_media_kind = ntr ? e_audio : e_video; // video first, audio second

        switch (track_media_kind){
            case e_audio:
                handler_type = MP4_HANDLER_TYPE_SOUN;
                handler_ascii = "Sound Handler";
                break;
            case e_video:
                handler_type = MP4_HANDLER_TYPE_VIDE;
                handler_ascii = "Video Handler";
                break;
            default:
                return -1;
        }

        MP4_ATOM(BOX_trak);
            MP4_FULL_ATOM(BOX_tkhd, 7); // flag: 1=trak enabled; 2=track in movie; 4=track in preview
                WR4(0);             // creation_time
                WR4(0);             // modification_time
                WR4(ntr+1);         // track_ID
                WR4(0);             // reserved
                WR4(0);             // duration
                WR4(0); WR4(0); // reserved[2]
                WR2(0);             // layer
                WR2(0);             // alternate_group
                if (track_media_kind == e_audio)
                {
                    WR2(0x0100);        // volume {if track_is_audio 0x0100 else 0}; 
                } else {
                    WR2(0x0000);        // volume {if track_is_audio 0x0100 else 0}; 
                }
                WR2(0);             // reserved

                // matrix[9]
                WR4(0x00010000);WR4(0);WR4(0);
                WR4(0);WR4(0x00010000);WR4(0);
                WR4(0);WR4(0);WR4(0x40000000);

                if (track_media_kind == e_audio)
                {
                    WR4(0); // width
                    WR4(0); // height
                }
                else
                {
                    WR4(width*0x10000); // width
                    WR4(height*0x10000); // height
                }
            MP4_END_ATOM;

            MP4_ATOM(BOX_mdia);
                MP4_FULL_ATOM(BOX_mdhd, 0);
                    WR4(0); // creation_time
                    WR4(0); // modification_time
                    if (track_media_kind == e_audio)
                    {
                        WR4(32000); // timescale
                    }
                    else
                    {
                        WR4(1000); // timescale
                    }
                    WR4(0); // duration
                    {
                        int lang_code = ((language[0]&31) << 10) | ((language[1]&31) << 5) | (language[2]&31);
                        WR2(lang_code); // language
                    }
                    WR2(0); // pre_defined
                MP4_END_ATOM;

                MP4_FULL_ATOM(BOX_hdlr, 0);
                    WR4(0); // pre_defined
                    WR4(handler_type); // handler_type
                    WR4(0); WR4(0); WR4(0); // reserved[3]
                    if (handler_ascii)
                    {
                        MP4_WRITE_STRING(handler_ascii);
                    }
                    else
                    {
                        WR4(0);
                    }
                MP4_END_ATOM;

                MP4_ATOM(BOX_minf);

                    if (track_media_kind == e_audio)
                    {
                        // Sound Media Header Box 
                        MP4_FULL_ATOM(BOX_smhd, 0);
                            WR2(0);   // balance
                            WR2(0);   // reserved
                        MP4_END_ATOM;
                    }
                    if (track_media_kind == e_video)
                    {
                        // mandatory Video Media Header Box 
                        MP4_FULL_ATOM(BOX_vmhd, 1);
                            WR2(0);   // graphicsmode
                            WR2(0); WR2(0); WR2(0); // opcolor[3]
                        MP4_END_ATOM;
                    }

                    MP4_ATOM(BOX_dinf);
                        MP4_FULL_ATOM(BOX_dref, 0);
                        WR4(1); // entry_count
                            // If the flag is set indicating that the data is in the same file as this box, then no string (not even an empty one)
                            // shall be supplied in the entry field.

                            // ASP the correct way to avoid supply the string, is to use flag 1 
                            // otherwise ISO reference demux crashes
                            MP4_FULL_ATOM(BOX_url, 1);
                            MP4_END_ATOM;
                        MP4_END_ATOM;
                    MP4_END_ATOM;

                    MP4_ATOM(BOX_stbl);
                        MP4_FULL_ATOM(BOX_stsd, 0);
                        WR4(1); // entry_count;

                        if (track_media_kind == e_audio)
                        {
                            // AudioSampleEntry() assume MP4E_HANDLER_TYPE_SOUN
                            unsigned box_name = (track_media_kind == e_audio) ? BOX_mp4a : BOX_mp4s;
                            MP4_ATOM(box_name);
                                // SampleEntry
                                WR4(0); WR2(0); // reserved[6]
                                WR2(1); // data_reference_index; - this is a tag for descriptor below

                                if (track_media_kind == e_audio)
                                {
                                    // AudioSampleEntry
                                    WR4(0); WR4(0); // reserved[2]
                                    WR2(1); // channelcount
                                    WR2(16); // samplesize
                                    WR4(0);  // pre_defined+reserved
                                    WR4((32000 << 16));  // samplerate == = {timescale of media}<<16;
                                }
                                //https://github.com/gliese1337/HLS.js
                                MP4_FULL_ATOM(BOX_esds, 0);
                                    if (dsi_bytes_cnt > 0)
                                    {
                                        int dsi_bytes = (int)(dsi_bytes_cnt); //  - two bytes size field
                                        int dsi_size_size = 1;
                                        for (int i = dsi_bytes; i > 0x7F; i -= 0x7F) dsi_size_size++;
                                        int dcd_bytes = dsi_bytes + dsi_size_size + 1 + (1+1+3+4+4);
                                        int dcd_size_size = 1;
                                        for (int i = dcd_bytes; i > 0x7F; i -= 0x7F) dcd_size_size++;
                                        int esd_bytes = dcd_bytes + dcd_size_size + 1 + 3 + 3;

                                        WR1(3); // OD_ES Descritption
                                        MP4_WRITE_OD_LEN(esd_bytes);
                                        WR2(0); // ES_ID(2) // TODO - what is this?
                                        WR1(0); // flags(1)

                                        WR1(4); // OD_DCD DecoderConfigDescrTag
                                        MP4_WRITE_OD_LEN(dcd_bytes);
                                        if (track_media_kind == e_audio)
                                        {
                                            WR1(0x40); // OD_DCD  == 0x40
                                            //WR1(MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3); // OD_DCD  
                                            WR1((5<<2) + 1); // stream_type == AudioStream  + 0x1 ObjectDescriptorStream 
                                        }
                                        else
                                        {
                                            // http://xhelmboyx.tripod.com/formats/mp4-layout.txt
                                            WR1(208); // 208 = private video
                                            WR1(32<<2); // stream_type == user private
                                        }
                                        WR3(0);// WR3(tr->info.u.a.channelcount * 6144/8); // bufferSizeDB in bytes, constant as in reference decoder
                                        WR4(64000); // maxBitrate TODO TEMP
                                        WR4(64000); // avg_bitrate_bps TODO TEMP

                                        WR1(5); // OD_DSI
                                        MP4_WRITE_OD_LEN(dsi_bytes); 
                                        for (int i = 0; i < dsi_bytes; i++)
                                        {
                                            WR1(dsi_buf[i]);
                                        }
                                        WR1(6); //SLConfigDescrTag
                                        WR1(1); // length
                                        WR1(2); // MP4 = 2
                                    }
                                MP4_END_ATOM; //esds
                            MP4_END_ATOM; //mp4a/mp4s
                        }

                        if (track_media_kind == e_video)
                        {
                            MP4_ATOM(BOX_avc1);
                            // VisualSampleEntry  8.16.2
                            // extends SampleEntry
                            WR2(0); // reserved
                            WR2(0); // reserved
                            WR2(0); // reserved
                            WR2(1); // data_reference_index
                            WR2(0); // pre_defined 
                            WR2(0); // reserved
                            WR4(0); // pre_defined 
                            WR4(0); // pre_defined 
                            WR4(0); // pre_defined 
                            WR2(width);
                            WR2(height);
                            WR4(0x00480000); // horizresolution = 72 dpi
                            WR4(0x00480000); // vertresolution  = 72 dpi
                            WR4(0); // reserved
                            WR2(1); // frame_count
                            WR1(4);
                            WR1('h');
                            WR1('2');
                            WR1('6');
                            WR1('4'); 
                            for (int i = 0; i < 27; i++)
                            {
                                WR1(0); //  compressorname
                            }
                            WR2(24); // depth
                            WR2(0xffff); // pre_defined

                                MP4_ATOM(BOX_avcC);
                                // AVCDecoderConfigurationRecord 5.2.4.1.1
                                WR1(1); // configurationVersion
                                WR1(vtrack->SPSNALU[1]); // 
                                WR1(vtrack->SPSNALU[2]); // 
                                WR1(vtrack->SPSNALU[3]); // 
                                WR1(255);                 // 0xfc + NALU_len-1  
                                WR1(0xe0 | 0x01);  //sps count
                                WR2(vtrack->SPSNALUNumBytes);
                                for (int i = 0; i < (int)vtrack->SPSNALUNumBytes; i++)
                                {
                                    WR1(vtrack->SPSNALU[i]);
                                }

                                WR1(1); // pps count
                                WR2(vtrack->PPSNALUNumBytes);
                                for (int i = 0; i < (int)vtrack->PPSNALUNumBytes; i++)
                                {
                                    WR1(vtrack->PPSNALU[i]);
                                }

                                MP4_END_ATOM;
                            MP4_END_ATOM;
                        }
                        MP4_END_ATOM;

                        /************************************************************************/
                        /*      indexes                                                         */
                        /************************************************************************/
                        // Sample Size Box 
                        MP4_FULL_ATOM(BOX_stsz, 0);
                        WR4(0); // sample_size  If this field is set to 0, then the samples have different sizes, and those sizes 
                                    //  are stored in the sample size table.
                        WR4(0);  // sample_count;
                        MP4_END_ATOM;


                        // Sample To Chunk Box 
                        MP4_FULL_ATOM(BOX_stsc, 0);
                            WR4(0); // entry_count
                        MP4_END_ATOM;


                        // Time to Sample Box 
                        MP4_FULL_ATOM(BOX_stts, 0);
                        {
                            WR1(0);
                            WR1(0);
                            WR1(0);
                            WR1(0);
                        }
                        MP4_END_ATOM;

                        // Chunk Offset Box 
                        MP4_FULL_ATOM(BOX_stco, 0);
                            WR4(0);
                        MP4_END_ATOM;
                    MP4_END_ATOM;
                MP4_END_ATOM;
            MP4_END_ATOM;
        MP4_END_ATOM;
    } // tracks loop

    MP4_ATOM(BOX_mvex);
    MP4_FULL_ATOM(BOX_mehd, 0);
        WR4(0);    // duration
    MP4_END_ATOM;
    for (int ntr = 0; ntr < ntracks; ntr++)
    {
        MP4_FULL_ATOM(BOX_trex, 0);
            WR4(ntr+1);             // track_ID
            WR4(1);                 // default_sample_description_index
            WR4(0);                 // default_sample_duration
            WR4(0);                 // default_sample_size
            WR4(0);                 // default_sample_flags 
        MP4_END_ATOM;
    }
    MP4_END_ATOM;

    MP4_END_ATOM;   // moov atom

    return (write_ptr - write_base);
}


#define MOOF_TRUN_STACK_SIZE 4*32*2 // at most one trun per frame... 4 seconds per fragment * 24 fps * 2x margin

int POSWriteMoof(char * buf, size_t maxSize, POSMp4VideoTrack * vtrack, POSMp4AudioTrack * atrack, size_t * fragmentSize, size_t * mdatLen){

    // write the moof header
    // note: there MUST be one whole fragment to write in vtrack, or this will fail

    // atoms nesting stack
    unsigned char * stack_base[20];
    unsigned char ** stack = stack_base;

    unsigned char *pvideo_data_offset_stack_base [MOOF_TRUN_STACK_SIZE]; 
    unsigned char ** pvideo_data_offset_stack = pvideo_data_offset_stack_base;

    unsigned char *paudio_data_offset_stack_base [MOOF_TRUN_STACK_SIZE]; 
    unsigned char ** paudio_data_offset_stack = paudio_data_offset_stack_base;

    // in-memory indexes
    unsigned char * write_base, * write_ptr, * psample_count_offset;

    write_ptr = buf;
    write_base = write_ptr;

    int ntracks = 2; 

    uint32_t secondPassVTrunIndex = vtrack -> ring_trun_index;
    uint32_t secondPassATrunIndex = atrack -> ring_trun_index;
    unsigned flags;
 
    MP4_ATOM(BOX_moof)
        MP4_FULL_ATOM(BOX_mfhd, 0)
            WR4(vtrack->sequenceNumber); // start from 1
            vtrack->sequenceNumber ++;
        MP4_END_ATOM //mfhd

        int videoTrunAccumDuration = 0;
        MP4_ATOM(BOX_traf)
            flags =  0x20020;
            MP4_FULL_ATOM(BOX_tfhd, flags)
                WR4(1);       // track_ID
                WR4(0x1010000);     // default_sample_flags
            MP4_END_ATOM // tfhd
            flags =  0x01000000;
            MP4_FULL_ATOM(BOX_tfdt, flags)
                WR4(0);       // version??
                WR4(vtrack -> baseMediaDecodeTime);     // base media decode time //TEMP 
            MP4_END_ATOM //tfdt

            flags  = 0;
            flags |= 0x001;         // data-offset-present
            flags |= 0x100;         // sample-duration-present
            flags |= 0x200;         // sample-size-present

            int nalType = *(uint8_t *)(vtrack -> ring -> buffer[ vtrack -> ring_trun_index ].loc) & 0x1f;

            if( nalType == 5) // I-Frame
                flags |= 0x004;         // set first-sample-flags-present
            else
                flags &= 0xfffb;        // clear first-sample-flag-present

            MP4_FULL_ATOM(BOX_trun, flags)
                psample_count_offset = write_ptr; write_ptr += 4;
                //printf("buf: %16x, write_ptr: %16x, *pvideo_data_offset_stack: %16x\n", buf, write_ptr, pvideo_data_offset_stack);
                assert( (unsigned)(pvideo_data_offset_stack - pvideo_data_offset_stack_base) < MOOF_TRUN_STACK_SIZE );
                *pvideo_data_offset_stack++ = write_ptr; write_ptr += 4;   // save ptr to data_offset

                //hexDump("stack", pvideo_data_offset_stack_base, 4*24*2*4, 8);
                if(nalType == 5) {WR4(33554432)}; // ?? 0x02000000 matching supereg
                int trunSampleCount = 0;
                do{

                    //printf("Here: %d, trun_index: %d, loc byte: %d, len: %d\n", __LINE__,vtrack -> ring_trun_index, *(uint8_t *)(vtrack -> ring -> buffer[ vtrack -> ring_trun_index ].loc) & 0x1f ,vtrack -> ring -> buffer[ vtrack -> ring_trun_index ].len);
                    WR4(vtrack -> ring -> buffer[ vtrack -> ring_trun_index ].dur);    // sample_duration
                    WR4(vtrack -> ring -> buffer[ vtrack -> ring_trun_index ].len+4);    // sample_size + avcc header
                    trunSampleCount ++;
                    videoTrunAccumDuration += vtrack -> ring -> buffer[ vtrack -> ring_trun_index ].dur;

                    vtrack -> ring_trun_index = (vtrack -> ring_trun_index + 1) & RING_BUFFER_MASK(vtrack -> ring);
                    //printf("I-frame?: %d\n", (*(uint8_t *)(vtrack -> ring -> buffer[ vtrack -> ring_trun_index ].loc) & 0x1f) == 5);
                }
                while((*(uint8_t *)(vtrack -> ring -> buffer[ vtrack -> ring_trun_index ].loc) & 0x1f) != 5 && // I-Frame
                    vtrack -> ring_trun_index != (vtrack -> ring ->head_index -1) & RING_BUFFER_MASK(vtrack -> ring)); // ran out of ring buffer.  this should not be possible

                    HAPAssert(vtrack -> ring_trun_index != (vtrack -> ring ->head_index -1) & RING_BUFFER_MASK(vtrack -> ring));

                vtrack -> baseMediaDecodeTime += videoTrunAccumDuration;
                //write the sample count
                MP4_WR4_PTR(psample_count_offset, trunSampleCount);
            MP4_END_ATOM //trun
            //printf("Here: %d, loc byte: %d\n", __LINE__, (*(uint8_t *)(vtrack -> ring -> buffer[ vtrack -> ring_trun_index ].loc)) & 0x1f);

        MP4_END_ATOM //traf

        if(!atrack->mute)
        MP4_ATOM(BOX_traf) // audio
            flags =  0x20000;
            MP4_FULL_ATOM(BOX_tfhd, flags)
                WR4(2);       // track_ID
            MP4_END_ATOM // tfhd
            flags =  0x01000000;
            MP4_FULL_ATOM(BOX_tfdt, flags)
                WR4(0);       // version??
                WR4(atrack -> baseMediaDecodeTime);     // base media decode time //TEMP 
            MP4_END_ATOM //tfdt

            flags  = 0;
            flags |= 0x001;         // data-offset-present
            flags |= 0x100;         // sample-duration-present
            flags |= 0x200;         // sample-size-present
            MP4_FULL_ATOM(BOX_trun, flags)
                psample_count_offset = write_ptr; write_ptr += 4;
                assert( (unsigned)(paudio_data_offset_stack - paudio_data_offset_stack_base) < MOOF_TRUN_STACK_SIZE );
                *paudio_data_offset_stack++ = write_ptr; write_ptr += 4;   // save ptr to data_offset
                int audioTrunAccumDuration = 0;
                int trunSampleCount = 0;
                do{
                    WR4(atrack -> ring -> buffer[ atrack -> ring_trun_index ].dur);    // sample_duration
                    WR4(atrack -> ring -> buffer[ atrack -> ring_trun_index ].len);    
                    trunSampleCount ++;
                    audioTrunAccumDuration += atrack -> ring -> buffer[ atrack -> ring_trun_index ].dur;
                    atrack -> ring_trun_index = (atrack -> ring_trun_index + 1) & RING_BUFFER_MASK(atrack -> ring);
                }
                while(audioTrunAccumDuration < 32 * videoTrunAccumDuration && atrack -> ring_trun_index != (atrack -> ring ->head_index-1) & RING_BUFFER_MASK(atrack -> ring));
                atrack -> baseMediaDecodeTime += audioTrunAccumDuration;
                //write the sample count
                MP4_WR4_PTR(psample_count_offset, trunSampleCount);
            MP4_END_ATOM
        MP4_END_ATOM // traf 
    MP4_END_ATOM //moof

    // second pass. write the data offsets now that the end of the moof has been written and we know the moof len
    // if this algorithm is exactly the same as above, the indicies will land in the right places with the right values
    // 8 is the len of the mdat box head
    int mdatStart =  write_ptr - write_base; // the box header isn't included in the length
    int trunAccumLen = write_ptr - write_base + 8;  
    //printf("Moof beginning trunAccumLen: %d\n", trunAccumLen);

    //rewind trun index
    vtrack -> ring_trun_index = secondPassVTrunIndex;
    pvideo_data_offset_stack = pvideo_data_offset_stack_base;
    atrack -> ring_trun_index = secondPassATrunIndex;
    paudio_data_offset_stack = paudio_data_offset_stack_base;

    int videoTrunAccumDuration = 0;
    int audioTrunAccumDuration = 0;

    MP4_WR4_PTR( *pvideo_data_offset_stack, trunAccumLen ); 

    HAPAssert( (unsigned)(pvideo_data_offset_stack - pvideo_data_offset_stack_base) < MOOF_TRUN_STACK_SIZE );
    pvideo_data_offset_stack++;
    
    do{
        //printf("vtrack -> ring_trun_index: %d, .len+4: %d, trunAccumLen: %d\n", vtrack -> ring_trun_index,vtrack -> ring -> buffer[ vtrack -> ring_trun_index ].len+4, trunAccumLen);

        videoTrunAccumDuration += vtrack -> ring -> buffer[ vtrack -> ring_trun_index ].dur;
        trunAccumLen += vtrack -> ring -> buffer[ vtrack -> ring_trun_index ].len + 4; //add the avcc header len
        vtrack -> ring_trun_index = (vtrack -> ring_trun_index + 1) & RING_BUFFER_MASK(vtrack -> ring);
    }
    while((*(uint8_t *)(vtrack -> ring -> buffer[ vtrack -> ring_trun_index ].loc) & 0x1f) != 5 && // I-Frame
        vtrack -> ring_trun_index != (vtrack -> ring ->head_index -1) & RING_BUFFER_MASK(vtrack -> ring)); // ran out of ring buffer.  this should not be possible

    if(!atrack->mute){
        //write the audio base offset
        MP4_WR4_PTR( *paudio_data_offset_stack, trunAccumLen ); 
        assert( (unsigned)(paudio_data_offset_stack - paudio_data_offset_stack_base) < MOOF_TRUN_STACK_SIZE );
        paudio_data_offset_stack++;
        do{
            audioTrunAccumDuration += atrack -> ring -> buffer[ atrack -> ring_trun_index ].dur;
            trunAccumLen += atrack -> ring -> buffer[ atrack -> ring_trun_index ].len;
            atrack -> ring_trun_index = (atrack -> ring_trun_index + 1) & RING_BUFFER_MASK(atrack -> ring);
        }
        while(audioTrunAccumDuration < 32 * videoTrunAccumDuration && // 32 = 32000 (audio clock) / 1000 (video sample rate)
            atrack -> ring_trun_index != (atrack -> ring ->head_index-1) & RING_BUFFER_MASK(atrack -> ring));
    }

    *fragmentSize = trunAccumLen;
    *mdatLen =  trunAccumLen - mdatStart;
    return (write_ptr - write_base);

}

int POSWriteMdat(char * buf, size_t maxSize, POSMp4VideoTrack * vtrack, POSMp4AudioTrack * atrack, size_t fragmentSize, size_t mdatLen, bool * mdatDone){
    // this function needs to be reentrant and continue writing the mdat box after each entry

    // in-memory indexes
    unsigned char * write_base, * write_ptr, * sample_avcc_header_offset;

    write_ptr = buf;
    write_base = write_ptr;

    //MP4_ATOM(BOX_mdat)
    // Q: how do we know if we need to start a new box?
    // A: If the vtrack mdat_index is still pointing at the last I-Frame, start a new box
    //printf("Start a new MDAT?: NAL type: %d, mdat_idx: %d, trun_idx: %d\n", (*(uint8_t *)(vtrack -> ring -> buffer[ vtrack -> ring_mdat_index ].loc) & 0x1f), vtrack -> ring_mdat_index, vtrack -> ring_trun_index);
    if( vtrack -> ring_mdat_index != vtrack -> ring_trun_index &&  
        (*(uint8_t *)(vtrack -> ring -> buffer[ vtrack -> ring_mdat_index ].loc) & 0x1f) == 5){
        //printf("start a new mdat box\n");
        HAPAssert(maxSize > 8);
        WR4(mdatLen);
        WR4(BOX_mdat);
    }
    int sampAccumDuration = 0;

    //video track
    //there needs to be space for the first frame, or this code needs to be changed to segment each frame
    HAPAssert((size_t)write_ptr + vtrack -> ring -> buffer[ vtrack -> ring_mdat_index ].len+4 < (size_t)buf + maxSize);
    do{
        sample_avcc_header_offset = write_ptr; write_ptr += 4;

        memcpy(write_ptr, 
            vtrack -> ring -> buffer[ vtrack -> ring_mdat_index ].loc, 
            vtrack -> ring -> buffer[ vtrack -> ring_mdat_index ].len); // stream cipher to go here in the future

        write_ptr += vtrack -> ring -> buffer[ vtrack -> ring_mdat_index ].len;
        MP4_WR4_PTR( sample_avcc_header_offset, vtrack -> ring -> buffer[ vtrack -> ring_mdat_index ].len ); // write the sample avcc header            

        //printf("Here: %d, mdat_idx: %d, trun_idx: %d, .len+4: %d, size: %d\n", __LINE__, vtrack -> ring_mdat_index, vtrack -> ring_trun_index, vtrack -> ring -> buffer[ vtrack -> ring_mdat_index ].len+4, write_ptr - write_base);

        vtrack -> ring_mdat_index = (vtrack -> ring_mdat_index + 1) & RING_BUFFER_MASK(vtrack -> ring);
    }while( vtrack -> ring_mdat_index != vtrack -> ring_trun_index &&
        (size_t) write_ptr + vtrack -> ring -> buffer[ vtrack -> ring_mdat_index ].len+4 < (size_t) buf + maxSize &&
        vtrack -> ring_mdat_index != (vtrack -> ring ->head_index -1) & RING_BUFFER_MASK(vtrack -> ring)); // ran out of ring buffer.  this should not be possible

    //printf("Here: %d, size: %d\n", __LINE__, write_ptr - write_base);

    //audio track
    //there needs to be space for the first frame, or this code needs to be changed to segment each frame
    if(!atrack->mute){
        HAPAssert((size_t)write_ptr + atrack -> ring -> buffer[ atrack -> ring_mdat_index ].len < (size_t)buf + maxSize);
        do{
            memcpy(write_ptr, 
                atrack -> ring -> buffer[ atrack -> ring_mdat_index ].loc, 
                atrack -> ring -> buffer[ atrack -> ring_mdat_index ].len); // stream cipher to go here

            // todo: check len        
            write_ptr += atrack -> ring -> buffer[ atrack -> ring_mdat_index ].len;

            atrack -> ring_mdat_index = (atrack -> ring_mdat_index + 1) & RING_BUFFER_MASK(atrack -> ring);
        }
        while(atrack->ring_mdat_index != atrack->ring_trun_index && 
        (size_t)(write_ptr + atrack -> ring -> buffer[ atrack -> ring_mdat_index ].len) < (size_t)buf + maxSize &&
            atrack -> ring_mdat_index != (atrack -> ring ->head_index-1) & RING_BUFFER_MASK(atrack -> ring));

    }
    //MP4_END_ATOM //mdat

    if(!atrack->mute){
        * mdatDone = (vtrack -> ring_mdat_index == vtrack -> ring_trun_index) && (atrack->ring_mdat_index == atrack->ring_trun_index);
    } else {
        * mdatDone = (vtrack -> ring_mdat_index == vtrack -> ring_trun_index);
    }
    //printf("write_ptr: %p, write_base: %p, (write_ptr - write_base): %d\n",write_ptr, write_base, write_ptr - write_base);
    //printf("mdatLen: %d\n", mdatLen);

    //if((vtrack -> ring_mdat_index == vtrack -> ring_trun_index)) {
    //    printf("mdatDone ending Moof\n");
    //}
    return (write_ptr - write_base);
}