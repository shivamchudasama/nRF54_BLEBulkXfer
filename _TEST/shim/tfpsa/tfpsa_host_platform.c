/**
 * @file          tfpsa_host_platform.c
 * @brief         What the host build of TF-PSA-Crypto expects the platform to
 *                provide (see tf_psa_crypto_host_config.h):
 *                - an in-memory PSA ITS, where persistent keys are stored (the
 *                  device uses Zephyr's secure storage); compiled against
 *                  TF-PSA-Crypto's own ITS header (core/psa_crypto_its.h), whose
 *                  lengths are uint32_t, so its ABI matches on 64-bit hosts;
 *                - the random generator (MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG): a
 *                  fixed-seed xorshift, enough for key import and signature
 *                  verification, which is all the test uses. Not for any real
 *                  use;
 *                - mbedtls_platform_zeroize() (MBEDTLS_PLATFORM_ZEROIZE_ALT),
 *                  through a volatile pointer so the stores are kept.
 *                gv_HostItsClear() empties the ITS (between tests).
 * @date          02/10/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#include <string.h>
#include <psa/crypto.h>
#include "psa_crypto_its.h"

#define HOST_ITS_SLOTS                       (8U)
#define HOST_ITS_MAX_DATA                    (2048U)

typedef struct
{
   int i_used;
   psa_storage_uid_t t_uid;
   uint32_t u32_len;
   psa_storage_create_flags_t t_flags;
   uint8_t u8ar_data[HOST_ITS_MAX_DATA];
} HostItsEntry_T;

static HostItsEntry_T sstar_hostIts[HOST_ITS_SLOTS];
static uint64_t su64_rngState = 0x9E3779B97F4A7C15ULL;

void gv_HostItsClear(void);

static HostItsEntry_T *sstpt_HostItsFind(psa_storage_uid_t t_uid)
{
   uint32_t i;

   for (i = 0U; i < HOST_ITS_SLOTS; i++)
   {
      if (sstar_hostIts[i].i_used && (sstar_hostIts[i].t_uid == t_uid)) { return &sstar_hostIts[i]; }
   }
   return NULL;
}

void gv_HostItsClear(void)
{
   memset(sstar_hostIts, 0, sizeof(sstar_hostIts));
}

psa_status_t psa_its_set(psa_storage_uid_t uid, uint32_t data_length, const void *p_data,
   psa_storage_create_flags_t create_flags)
{
   HostItsEntry_T *e = sstpt_HostItsFind(uid);
   uint32_t i;

   if (data_length > HOST_ITS_MAX_DATA) { return PSA_ERROR_INSUFFICIENT_STORAGE; }
   for (i = 0U; (e == NULL) && (i < HOST_ITS_SLOTS); i++)
   {
      if (!sstar_hostIts[i].i_used) { e = &sstar_hostIts[i]; }
   }
   if (e == NULL) { return PSA_ERROR_INSUFFICIENT_STORAGE; }
   e->i_used = 1;
   e->t_uid = uid;
   e->u32_len = data_length;
   e->t_flags = create_flags;
   memcpy(e->u8ar_data, p_data, data_length);
   return PSA_SUCCESS;
}

psa_status_t psa_its_get(psa_storage_uid_t uid, uint32_t data_offset, uint32_t data_length,
   void *p_data, size_t *p_data_length)
{
   HostItsEntry_T *e = sstpt_HostItsFind(uid);
   uint32_t u32_n;

   if (e == NULL) { return PSA_ERROR_DOES_NOT_EXIST; }
   if (data_offset > e->u32_len) { return PSA_ERROR_INVALID_ARGUMENT; }
   u32_n = e->u32_len - data_offset;
   if (u32_n > data_length) { u32_n = data_length; }
   memcpy(p_data, &e->u8ar_data[data_offset], u32_n);
   *p_data_length = u32_n;
   return PSA_SUCCESS;
}

psa_status_t psa_its_get_info(psa_storage_uid_t uid, struct psa_storage_info_t *p_info)
{
   HostItsEntry_T *e = sstpt_HostItsFind(uid);

   if (e == NULL) { return PSA_ERROR_DOES_NOT_EXIST; }
   p_info->size = e->u32_len;
   p_info->flags = e->t_flags;
   return PSA_SUCCESS;
}

psa_status_t psa_its_remove(psa_storage_uid_t uid)
{
   HostItsEntry_T *e = sstpt_HostItsFind(uid);

   if (e == NULL) { return PSA_ERROR_DOES_NOT_EXIST; }
   e->i_used = 0;
   return PSA_SUCCESS;
}

psa_status_t mbedtls_psa_external_get_random(mbedtls_psa_external_random_context_t *context,
   uint8_t *output, size_t output_size, size_t *output_length)
{
   size_t i;

   (void)context;
   for (i = 0U; i < output_size; i++)
   {
      su64_rngState ^= su64_rngState << 13;
      su64_rngState ^= su64_rngState >> 7;
      su64_rngState ^= su64_rngState << 17;
      output[i] = (uint8_t)(su64_rngState >> 32);
   }
   *output_length = output_size;
   return PSA_SUCCESS;
}

void mbedtls_platform_zeroize(void *buf, size_t len)
{
   volatile uint8_t *u8pt_p = (volatile uint8_t *)buf;

   while (len-- > 0U) { *u8pt_p++ = 0U; }
}
