/**
 * @file          internal_trusted_storage.h
 * @brief         Host-test stand-in for the PSA Internal Trusted Storage API,
 *                with Zephyr secure storage's signatures (size_t lengths):
 *                the types CSR_Generator.h and DeviceCert.h name. The functions
 *                are declared only; a test that reaches them defines them.
 * @date          01/10/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef _SHIM_PSA_ITS_H
#define _SHIM_PSA_ITS_H

#include "psa/crypto.h"

typedef uint64_t psa_storage_uid_t;
typedef uint32_t psa_storage_create_flags_t;
struct psa_storage_info_t { size_t size; psa_storage_create_flags_t flags; };

#define PSA_STORAGE_FLAG_NONE                ((psa_storage_create_flags_t)0)

extern psa_status_t psa_its_set(psa_storage_uid_t uid, size_t data_length, const void *p_data,
   psa_storage_create_flags_t create_flags);
extern psa_status_t psa_its_get(psa_storage_uid_t uid, size_t data_offset, size_t data_size,
   void *p_data, size_t *p_data_length);
extern psa_status_t psa_its_get_info(psa_storage_uid_t uid, struct psa_storage_info_t *p_info);
extern psa_status_t psa_its_remove(psa_storage_uid_t uid);

#endif //!_SHIM_PSA_ITS_H
