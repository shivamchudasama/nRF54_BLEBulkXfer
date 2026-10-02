/* SPDX-License-Identifier: MIT */
/* Platform header TF-PSA-Crypto's key storage includes when the ITS is not its
   own file-based one (MBEDTLS_PSA_ITS_FILE_C off, see tf_psa_crypto_host_config.h):
   the PSA status type and codes. On the include path of the tfpsacrypto target only. */
#ifndef TFPSA_HOST_PSA_ERROR_H
#define TFPSA_HOST_PSA_ERROR_H
#include <psa/crypto_types.h>
#include <psa/crypto_values.h>
#endif /* TFPSA_HOST_PSA_ERROR_H */
