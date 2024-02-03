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


#ifndef POSRTPCRYPTO_H
#define POSRTPCRYPTO_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t key_size;
    uint32_t tag_size;
    uint8_t encr_key[32];
    uint8_t auth_key[20];
    uint8_t salt_key[14];
} srtp_ctx;

void srtp_session_key
               (char * subKeyOut, uint32_t subKeySize, const char * masterKey, uint32_t masterKeySize,
               const char * salt, uint8_t keyIndex);

void srtp_setupContext
               (srtp_ctx *srtp_ctxx,srtp_ctx *srtcp_ctx,const char *key,
               uint32_t keySize,const char *salt,uint32_t tagSize,uint32_t ssrc);

void srtp_encrypt(
    const srtp_ctx *srtp_ctx,
    uint8_t *packet,
    const uint8_t *data_bytes,
    size_t num_header_bytes,
    size_t num_data_bytes,
    uint32_t index);

void srtp_decrypt
               (srtp_ctx *srtp_ctx,uint8_t *data,uint8_t *packet_bytes,
               uint32_t num_packet_bytes,uint32_t index);

void store_bigendian(uint32_t *output,uint32_t input);


void hmac_sha1_aad
               (uint8_t *r,uint8_t *key,uint32_t key_len,uint8_t *in,uint32_t in_len,uint8_t *aad,
               uint32_t aad_len);

void srtp_authenticate(
    const srtp_ctx *srtp_ctx,
    uint8_t *tag,
    const uint8_t *bytes,
    uint32_t num_bytes,
    uint32_t index);

int srtp_verifyAuthentication
                  (srtp_ctx *srtp_ctx,uint8_t *tag,uint8_t *bytes,uint32_t num_bytes,
                   uint32_t index);



#ifdef __cplusplus
}
#endif

#endif
