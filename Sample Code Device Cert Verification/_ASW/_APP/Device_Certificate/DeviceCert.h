/**
 * @file          DeviceCert.h
 * @brief         Header file containing device certificate.
 * @date          05/09/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _DEVICE_CERT_H
#define _DEVICE_CERT_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include "nvm3.h"
#include "sl_status.h"
#include "NVM3_Handler.h"
#include "CSR_Generator.h"
#include "CSR_Generator_Types.h"
#include "DeviceCert_Types.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/oid.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/base64.h"
#include "sl_bt_root_cert.h"
#include "sl_bt_api.h"
#include "gatt_db.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           CSR_GENERATOR_NVM3_REGION
 * @brief         The NVM3 key region which will be used to save security keys and certificates.
 *                This is the default NVM3 region for Bluetooth NVM3
 *                key space.
 */
#define CSR_GENERATOR_NVM3_REGION            (0x40000)

/**
 * @def           PUB_KEY_OFFSET
 * @brief         Public key offset in the certificate structure.
 */
#define PUB_KEY_OFFSET                       (26)

/**
 * @def           EC_PUB_KEY_LEN
 * @brief         Public key length in the certificate structure.
 */
#define EC_PUB_KEY_LEN                       (65)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          DeviceProvisioningState_E
 * @brief         Enums of different states for device provisioning.
 */
typedef enum
{
   // eDPS_WAIT_FOR_CERT_GENERATION = 0,        /**< Waiting for certificate to be generated */
   // eDPS_WAIT_FOR_CERT_LENGTH,                /**< Wait for certificate length */
   // eDPS_WAIT_FOR_CERT,                       /**< Wait for certificate */
   eDPS_WAIT_FOR_WRITING_DEVICE_CERT,        /**< Wait for writing device certificate */
   eDPS_DEVICE_PROVISIONING_COMPLETE         /**< Device provisioning complete */
} DeviceProvisioningState_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        <Structure name>
 * @brief         <Structure details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                   UNIONS                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @union         <Union name>
 * @brief         <Union details>.
 */

/******************************************************************************/
/*                                                                            */
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/
extern mbedtls_svc_key_id_t gt_remotePubKeyID;

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/
extern sl_status_t gt_VerifyOwnDeviceCertificate(void);
extern sl_status_t gt_VerifyRemoteDeviceCertificate(void);
extern sl_status_t gt_GetCertificate(uint8_t *u8pt_cert, uint32_t *u32pt_certSize, \
   uint32_t u32_certRegion, uint32_t u32_certTag);
extern sl_status_t gt_StoreCertificate(uint8_t *u8pt_cert, uint32_t u32_certSize, \
   uint32_t u32_certRegion, uint32_t u32_certTag);
// extern sl_status_t gt_DeviceProvisioningFSM(sl_bt_msg_t *stpt_evt);

#endif //!_DEVICE_CERT_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
