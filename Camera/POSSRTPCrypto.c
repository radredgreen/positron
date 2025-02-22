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


#include "POSSRTPCrypto.h"
#include <string.h>
#include "mbedtls/aes.h"
#include "mbedtls/sha1.h"

// debugging
// #include <stdio.h>

void srtp_session_key(char *subKeyOut, uint32_t subKeySize, const char *masterKey, uint32_t masterKeySize,
                      const char *salt, uint8_t keyIndex)

{
  // https://datatracker.ietf.org/doc/html/rfc3711
  uint8_t iv[16];

  memcpy(iv, salt, 14);
  iv[14] = 0;
  iv[15] = 0;
  iv[7] ^= keyIndex;

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, masterKey, 8 * masterKeySize);

  size_t nc_off = 0;
  uint8_t stream_block[16];
  memset(stream_block, 0, 16);

  memset(subKeyOut, 0, subKeySize);
  mbedtls_aes_crypt_ctr(&aes, subKeySize, &nc_off, iv, stream_block, subKeyOut, subKeyOut);

  return;
}

void srtp_setupContext(srtp_ctx *srtp_ctxx, srtp_ctx *srtcp_ctx, const char *key,
                       uint32_t keySize, const char *salt, uint32_t tagSize, uint32_t ssrc)

{
  uint32_t bsssrc = __builtin_bswap32(ssrc);
  uint32_t *a;
  if (srtp_ctxx != NULL)
  {
    srtp_ctxx->key_size = keySize;
    srtp_ctxx->tag_size = tagSize;
    if (keySize != 0)
    {
      srtp_session_key(srtp_ctxx->encr_key, keySize, key, keySize, salt, 0);
      srtp_session_key(srtp_ctxx->auth_key, 0x14, key, keySize, salt, 1);
      srtp_session_key(srtp_ctxx->salt_key, 0xe, key, keySize, salt, 2);
      a = (uint32_t *)(srtp_ctxx->salt_key + 4);
      *a = *(a) ^ bsssrc;
    }
  }
  if (srtcp_ctx != NULL)
  {
    srtcp_ctx->key_size = keySize;
    srtcp_ctx->tag_size = tagSize;
    if (keySize != 0)
    {
      srtp_session_key(srtcp_ctx->encr_key, keySize, key, keySize, salt, 3);
      srtp_session_key(srtcp_ctx->auth_key, 0x14, key, keySize, salt, 4);
      srtp_session_key(srtcp_ctx->salt_key, 0xe, key, keySize, salt, 5);
      a = (uint32_t *)(srtcp_ctx->salt_key + 4);
      *a = *(a) ^ bsssrc;
    }
  }
  return;
}

void srtp_encrypt(
    const srtp_ctx *srtp_ctx,
    uint8_t *packet,
    const uint8_t *data_bytes,
    size_t num_header_bytes,
    size_t num_data_bytes,
    uint32_t index)
{
  uint8_t iv[16];
//  printf("srtp_encrypt: num_header_bytes, %d, num_data_bytes %d, index: %lu\n", num_header_bytes, num_data_bytes, index);
//  hexDump("srtp_ctx->encr_key",&srtp_ctx->encr_key, srtp_ctx->key_size, 16);
//  hexDump("srtp_ctx->salt_key",&srtp_ctx->salt_key, 14, 16);
  
  memset(&iv, 0, 16);
  memcpy(&iv, srtp_ctx->salt_key, 14);

  iv[10] = (uint8_t)(index >> 24) ^ iv[10];
  iv[11] = (uint8_t)(index >> 16) ^ iv[11];
  iv[12] = (uint8_t)(index >> 8) ^ iv[12];
  iv[13] = (uint8_t)index ^ iv[13];

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, srtp_ctx->encr_key, 8 * srtp_ctx->key_size);

  size_t nc_off = 0;
  uint8_t stream_block[16];
  memset(stream_block, 0, 16);

  if (num_header_bytes != 0)
  {
    mbedtls_aes_crypt_ctr(&aes, num_header_bytes, &nc_off, iv, stream_block, packet, packet);
  }
  mbedtls_aes_crypt_ctr(&aes, num_data_bytes, &nc_off, iv, stream_block, data_bytes, packet + num_header_bytes);
  return;
}

void srtp_decrypt(srtp_ctx *srtp_ctx, uint8_t *data, uint8_t *packet_bytes,
                  uint32_t num_packet_bytes, uint32_t index)

{
  uint8_t iv[16];
  // printf("srtp_decrypt: num_packet_bytes %d, index %lu\n", num_packet_bytes, index);
  memset(&iv, 0, 16);
  memcpy(&iv, srtp_ctx->salt_key, 14);

  iv[10] = (uint8_t)(index >> 24) ^ iv[10];
  iv[11] = (uint8_t)(index >> 16) ^ iv[11];
  iv[12] = (uint8_t)(index >> 8) ^ iv[12];
  iv[13] = (uint8_t)index ^ iv[13];

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, srtp_ctx->encr_key, 8 * srtp_ctx->key_size);

  size_t nc_off = 0;
  uint8_t stream_block[16];
  memset(stream_block, 0, 16);

  mbedtls_aes_crypt_ctr(&aes, num_packet_bytes, &nc_off, iv, stream_block, packet_bytes, data);
  return;
}

void store_bigendian(uint32_t *output, uint32_t input)

{
  uint8_t *pout = (uint8_t *)output;
  *(pout + 3) = (uint8_t)input;
  *(pout + 2) = ((uint8_t)(input >> 8));
  *(pout + 1) = ((uint8_t)(input >> 16));
  *pout = (uint8_t)(input >> 24);
  return;
}

void hmac_sha1_aad(uint8_t *r, uint8_t *key, uint32_t key_len, uint8_t *in, uint32_t in_len, uint8_t *aad,
                   uint32_t aad_len)

{
  // https://datatracker.ietf.org/doc/html/rfc2104
  uint32_t idx;
  mbedtls_sha1_context sha_ctx;
  uint8_t BByteString[0x40];

  for (idx = 0; idx != key_len; idx++)
  {
    *(uint8_t *)(BByteString + idx) = *(key + idx) ^ 0x36;
  }
  for (; idx != 0x40; idx++)
  {
    *(uint8_t *)(BByteString + idx) = 0x36;
  }
  mbedtls_sha1_init(&sha_ctx);
  mbedtls_sha1_starts(&sha_ctx);
  mbedtls_sha1_update(&sha_ctx, BByteString, 0x40);
  mbedtls_sha1_update(&sha_ctx, in, in_len);
  if (aad != 0)
  {
    // printf("hmac_sha1_aad: aad != 0, aad = %16x\n", *(uint32_t *)aad);
    mbedtls_sha1_update(&sha_ctx, aad, aad_len);
  }
  mbedtls_sha1_finish(&sha_ctx, r);
  mbedtls_sha1_free(&sha_ctx);
  for (idx = 0; idx != key_len; idx++)
  {
    *(uint8_t *)(BByteString + idx) = *(key + idx) ^ 0x5c;
  }
  for (; idx != 0x40; idx++)
  {
    *(uint8_t *)(BByteString + idx) = 0x5c;
  }
  mbedtls_sha1_init(&sha_ctx);
  mbedtls_sha1_starts(&sha_ctx);
  mbedtls_sha1_update(&sha_ctx, BByteString, 0x40);
  mbedtls_sha1_update(&sha_ctx, r, 0x14);
  mbedtls_sha1_finish(&sha_ctx, r);
  mbedtls_sha1_free(&sha_ctx);
  return;
}

void srtp_authenticate(
    srtp_ctx *srtp_ctx,
    uint8_t *tag,
    uint8_t *bytes,
    uint32_t num_bytes,
    uint32_t index)
{
  uint8_t result[20];
  uint32_t beIndex;
//  printf("srtp_authenticate(srtp_ctx *srtp_ctx = %8x,uint8_t *tag = %8x,uint8_t *bytes = %8x,uint32_t num_bytes = %d,uint32_t index = %d)\n",srtp_ctx, tag, bytes, num_bytes, index);
//  printf("srtp_authenticate( index = %d )\n", index);
//  hexDump("srtp_ctx->auth_key",&srtp_ctx->auth_key, 20, 16);

  store_bigendian(&beIndex, index);
  hmac_sha1_aad(result, srtp_ctx->auth_key, 20, bytes, num_bytes, (uint8_t *)&beIndex, 4);
  memcpy(tag, result, srtp_ctx->tag_size);
  return;
}

int srtp_verifyAuthentication(srtp_ctx *srtp_ctx, uint8_t *tag, uint8_t *bytes, uint32_t num_bytes,
                              uint32_t index)
{
  uint8_t result[20];
  uint32_t beIndex;
  uint32_t output;
  // printf("srtp_verifyAuthentication(srtp_ctx *srtp_ctx = %8x,uint8_t *tag = %8x,uint8_t *bytes = %8x,uint32_t num_bytes = %d,uint32_t index = %d)\n",srtp_ctx, tag, bytes, num_bytes, index);
  store_bigendian(&beIndex, index);
  hmac_sha1_aad(result, srtp_ctx->auth_key, 20, bytes, num_bytes, (uint8_t *)&beIndex, 4);
  output = 0 == memcmp(tag, result, srtp_ctx->tag_size);
  return (output);
}
