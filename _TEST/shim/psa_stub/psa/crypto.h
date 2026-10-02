/**
 * @file          crypto.h
 * @brief         Host-test stand-in for the PSA Crypto API (psa/crypto.h): the
 *                types, constants and functions the _ASW provisioning code uses.
 *                Values match PSA Crypto 1.x. The functions are declared only;
 *                each test defines the ones it needs (scripted results), so no
 *                real cryptography runs on the host. The key attribute setters
 *                are inline and just record the values, so a test can check
 *                the policy a key was created with.
 *
 *                This directory (shim/psa_stub) is on the include path only of
 *                the tests that stub PSA; the certificate verification test
 *                links the real Mbed TLS / TF-PSA-Crypto instead.
 * @date          01/10/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef _SHIM_PSA_CRYPTO_H
#define _SHIM_PSA_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

typedef int32_t psa_status_t;
typedef uint32_t psa_key_id_t;
typedef psa_key_id_t mbedtls_svc_key_id_t;
typedef uint32_t psa_algorithm_t;
typedef uint16_t psa_key_type_t;
typedef uint8_t psa_ecc_family_t;
typedef uint32_t psa_key_usage_t;
typedef uint32_t psa_key_lifetime_t;
typedef struct
{
   psa_key_type_t type;
   size_t bits;
   psa_key_usage_t usage;
   psa_algorithm_t alg;
   psa_key_lifetime_t lifetime;
   psa_key_id_t id;
} psa_key_attributes_t;

#define PSA_KEY_ATTRIBUTES_INIT              { 0, 0, 0, 0, 0, 0 }

#define PSA_SUCCESS                          ((psa_status_t)0)
#define PSA_ERROR_GENERIC_ERROR              ((psa_status_t)-132)
#define PSA_ERROR_NOT_PERMITTED              ((psa_status_t)-133)
#define PSA_ERROR_NOT_SUPPORTED              ((psa_status_t)-134)
#define PSA_ERROR_INVALID_ARGUMENT           ((psa_status_t)-135)
#define PSA_ERROR_INVALID_HANDLE             ((psa_status_t)-136)
#define PSA_ERROR_BAD_STATE                  ((psa_status_t)-137)
#define PSA_ERROR_BUFFER_TOO_SMALL           ((psa_status_t)-138)
#define PSA_ERROR_ALREADY_EXISTS             ((psa_status_t)-139)
#define PSA_ERROR_DOES_NOT_EXIST             ((psa_status_t)-140)
#define PSA_ERROR_INSUFFICIENT_MEMORY        ((psa_status_t)-141)
#define PSA_ERROR_INSUFFICIENT_STORAGE       ((psa_status_t)-142)
#define PSA_ERROR_STORAGE_FAILURE            ((psa_status_t)-146)
#define PSA_ERROR_HARDWARE_FAILURE           ((psa_status_t)-147)
#define PSA_ERROR_INVALID_SIGNATURE          ((psa_status_t)-149)
#define PSA_ERROR_DATA_CORRUPT               ((psa_status_t)-152)

#define PSA_ALG_SHA_1                        ((psa_algorithm_t)0x02000005)
#define PSA_ALG_SHA_256                      ((psa_algorithm_t)0x02000009)
#define PSA_ALG_ECDSA(hash_alg)              ((psa_algorithm_t)(0x06000600 | ((hash_alg) & 0xFF)))

#define PSA_ECC_FAMILY_SECP_R1               ((psa_ecc_family_t)0x12)
#define PSA_KEY_TYPE_ECC_KEY_PAIR(curve)     ((psa_key_type_t)(0x7100 | (curve)))
#define PSA_KEY_TYPE_ECC_PUBLIC_KEY(curve)   ((psa_key_type_t)(0x4100 | (curve)))

#define PSA_KEY_USAGE_EXPORT                 ((psa_key_usage_t)0x00000001)
#define PSA_KEY_USAGE_SIGN_MESSAGE           ((psa_key_usage_t)0x00000400)
#define PSA_KEY_USAGE_VERIFY_MESSAGE         ((psa_key_usage_t)0x00000800)
#define PSA_KEY_USAGE_SIGN_HASH              ((psa_key_usage_t)0x00001000)
#define PSA_KEY_USAGE_VERIFY_HASH            ((psa_key_usage_t)0x00002000)

#define PSA_KEY_LIFETIME_VOLATILE            ((psa_key_lifetime_t)0x00000000)
#define PSA_KEY_LIFETIME_PERSISTENT          ((psa_key_lifetime_t)0x00000001)

static inline void psa_set_key_type(psa_key_attributes_t *a, psa_key_type_t t) { a->type = t; }
static inline void psa_set_key_bits(psa_key_attributes_t *a, size_t b) { a->bits = b; }
static inline void psa_set_key_usage_flags(psa_key_attributes_t *a, psa_key_usage_t u) { a->usage = u; }
static inline void psa_set_key_algorithm(psa_key_attributes_t *a, psa_algorithm_t g) { a->alg = g; }
static inline void psa_set_key_lifetime(psa_key_attributes_t *a, psa_key_lifetime_t l) { a->lifetime = l; }
static inline void psa_set_key_id(psa_key_attributes_t *a, psa_key_id_t i) { a->id = i; }

extern psa_status_t psa_crypto_init(void);
extern psa_status_t psa_generate_key(const psa_key_attributes_t *attributes, psa_key_id_t *key);
extern psa_status_t psa_import_key(const psa_key_attributes_t *attributes, const uint8_t *data,
   size_t data_length, psa_key_id_t *key);
extern psa_status_t psa_get_key_attributes(psa_key_id_t key, psa_key_attributes_t *attributes);
extern void psa_reset_key_attributes(psa_key_attributes_t *attributes);
extern psa_status_t psa_destroy_key(psa_key_id_t key);
extern psa_status_t psa_export_public_key(psa_key_id_t key, uint8_t *data, size_t data_size,
   size_t *data_length);
extern psa_status_t psa_hash_compute(psa_algorithm_t alg, const uint8_t *input, size_t input_length,
   uint8_t *hash, size_t hash_size, size_t *hash_length);
extern psa_status_t psa_sign_message(psa_key_id_t key, psa_algorithm_t alg, const uint8_t *input,
   size_t input_length, uint8_t *signature, size_t signature_size, size_t *signature_length);

#endif //!_SHIM_PSA_CRYPTO_H
