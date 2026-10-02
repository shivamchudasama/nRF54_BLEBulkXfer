/**
 * @file          tf_psa_crypto_host_config.h
 * @brief         TF_PSA_CRYPTO_USER_CONFIG_FILE for the host build of Mbed TLS /
 *                TF-PSA-Crypto used by test_devicecert_verify.c. Applied on top
 *                of the default configuration, it removes what the host cannot
 *                or should not provide, the same on Windows (gcc 4.4) and Linux:
 *                - assembly and CPU-specific AES (old gcc cannot build AES-NI;
 *                  the test does not use AES);
 *                - the OS entropy source (old MinGW has no bcrypt.h): the test
 *                  supplies the random generator (tfpsa_host_platform.c);
 *                - the file-based ITS: keys live in an in-memory ITS
 *                  (tfpsa_host_platform.c), as in Zephyr's secure storage;
 *                - calendar time: the device has no wall clock and ignores the
 *                  validity period (DeviceCert_Verify.c);
 *                - the OS secure-zeroize call (old MinGW lacks SecureZeroMemory):
 *                  tfpsa_host_platform.c provides mbedtls_platform_zeroize().
 * @date          02/10/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#undef MBEDTLS_AESNI_C
#undef MBEDTLS_AESCE_C
#undef MBEDTLS_PADLOCK_C
#undef MBEDTLS_HAVE_ASM
#undef MBEDTLS_PSA_BUILTIN_GET_ENTROPY
#define MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG
#undef MBEDTLS_PSA_ITS_FILE_C
#undef MBEDTLS_HAVE_TIME_DATE
#define MBEDTLS_PLATFORM_ZEROIZE_ALT
