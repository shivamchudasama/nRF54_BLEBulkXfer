/* SPDX-License-Identifier: MIT */
/* Platform header TF-PSA-Crypto's key storage includes when the ITS is not its
   own file-based one: TF-PSA-Crypto's ITS interface (core/psa_crypto_its.h,
   uint32_t lengths), implemented in RAM by tfpsa_host_platform.c. On the include
   path of the tfpsacrypto target only. */
#ifndef TFPSA_HOST_PSA_ITS_H
#define TFPSA_HOST_PSA_ITS_H
#include "psa_crypto_its.h"
#endif /* TFPSA_HOST_PSA_ITS_H */
