/**
 * @file          CSR_Generator.h
 * @brief         Header file containing CSR generator functionality.
 * @date          27/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _CSR_GENERATOR_H
#define _CSR_GENERATOR_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include "sl_status.h"
#include "sl_component_catalog.h"
#include "app_assert.h"
#include "app_log.h"
#include "sl_common.h"
#include "em_system.h"
#include "psa/crypto.h"
#include "psa/crypto_values.h"
#include "CSR_Generator_Config.h"
#include "CSR_Generator_Types.h"
#include "NVM3_Handler.h"
#include "DeviceCert.h"
#include "BALOS_Macros.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           UUID_LEN
 * @brief         Length of UUID.
 */
#define UUID_LEN                             (16)

/**
 * @def           CRYPTO_EC_PUBLIC_KEY_LEN
 * @brief         Length of 256-bits EC public key (in bytes).
 */
#define CRYPTO_EC_PUBLIC_KEY_LEN             (64)

/**
 * @def           CRYPTO_AUTH_256_LEN
 * @brief         Length of 256-bits authentication data (in bytes).
 */
#define CRYPTO_AUTH_256_LEN                  (32)

/**
 * @def           CRYPTO_SHA_1_LEN
 * @brief         Length of SHA-1.
 */
#define CRYPTO_SHA_1_LEN                     (20)

/**
 * @def           CRYPTO_EC_PRIVATE_KEY_LEN
 * @brief         Length of 256-bits EC private key (in bytes).
 */
#define CRYPTO_EC_PRIVATE_KEY_LEN            (32)

/**
 * @def           DEVICE_CERTIFICATE_NVM3_START_KEY
 * @brief         NVM3 key for device certificate.
 */
#define DEVICE_CERTIFICATE_NVM3_START_KEY    (1)

/**
 * @def           DEVICE_CSR_NVM3_START_KEY
 * @brief         NVM3 key for device certificate.
 */
#define DEVICE_CSR_NVM3_START_KEY            (9)

/**
 * @def           CSR_GENERATOR_STATIC_AUTH_DATA_NVM3_TAG
 * @brief         NVM3 tag for static authentication data.
 */
#define CSR_GENERATOR_STATIC_AUTH_DATA_NVM3_TAG \
                                             (0x00FD)

/**
 * @def           CSR_GENERATOR_DEVICE_EC_KEY_NVM3_TAG
 * @brief         NVM3 tag for device EC key.
 */
#define CSR_GENERATOR_DEVICE_EC_KEY_NVM3_TAG (0x00FE)

/**
 * @def           DEVICE_EC_KEY_ID
 * @brief         NVM3 key for device EC key.
 */
#define DEVICE_EC_KEY_ID                     (CSR_GENERATOR_NVM3_REGION | CSR_GENERATOR_DEVICE_EC_KEY_NVM3_TAG)

/**
 * @def           STATIC_AUTH_DATA_ID
 * @brief         NVM3 key for static authentication data.
 */
#define STATIC_AUTH_DATA_ID                  (CSR_GENERATOR_NVM3_REGION | CSR_GENERATOR_STATIC_AUTH_DATA_NVM3_TAG)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          <Enum name>
 * @brief         <Enum details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        CSROutput_T
 * @brief         Structure containing the generated CSR.
 */
typedef struct __attribute__((__packed__))
{
   uint8_t u8_isCSRGenerated;                /**< Flag indicating if the CSR is
                                                   generated */
   uint8_t u8ar_staticAuthData[CRYPTO_AUTH_256_LEN];
                                             /**< Static authentication data */
   uint16_t u16_CSRLen;                      /**< Length of generated CSR data.
                                                   DER is binary format so length is
                                                   required */
   uint8_t u8ar_CSR[];                       /**< Generated CSR */
} CSROutput_T;

/**
 * @struct        SubjectNameField_T
 * @brief         Structure containing the subject name field.
 */
typedef struct
{
   size_t t_nameLen;
   size_t t_valueLen;
   const char *cpt_name;
   char *cpt_value;
} SubjectNameField_T;

/**
 * @struct        CSRConfig_T
 * @brief         Structure containing the CSR configuration.
 */
typedef struct
{
   bool b_isStaticAuth2Generated;            /**< If True, generate 256-bit
                                                   static authentication data */
   bool b_isStaticAuthOnDevice;              /**< If True, mark static authentication
                                                   data presence on device in control block */
   bool b_isKey2Generated;                   /**< If True, generate device EC key
                                                   and corresponding CSR */
   bool b_isKeyAvailable;                    /**< If True, mark device EC key
                                                   presence on device in control block */
   bool b_isCertAvailable;                   /**< If True, mark certificate presence
                                                   on device in control block */
   bool b_isCSRAvailable;                    /**< If true, mark CSR presence on
                                                   device in control block */
   uint8_t u8_certPositionOnNVM3;            /**< For recording certificate position
                                                   in NVM3 to control block */
   uint8_t u8_CSRPositionOnNVM3;             /**< For recording CSR position in
                                                   NVM3 to control block */
   size_t t_subjectNameFieldCnt;             /**< CSR subject name, excluding CN (see below) */
   const SubjectNameField_T *stpt_subjectNameField;
                                             /**< Pointer to the subject name fields */
} CSRConfig_T;

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
extern CSROutput_T gst_CSROutput;
extern CSRConfig_T gst_CSRConfig;
extern DeviceProvisioningRCB_T gst_deviceProvisioningRCB;
extern uint8_t gu8ar_deviceCertDER[];
extern uint32_t gu32_deviceCertDERLen;
extern bool gb_isCSRAvailable;
extern bool gb_isDeviceCertAvailable;
extern bool gb_isDeviceCertVerified;

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/
extern void gv_GenerateCSR(void);
extern sl_status_t gt_CalculateSHA1(const uint8_t *u8pt_data, size_t t_len, uint8_t *u8pt_hash);
extern sl_status_t gt_ExportPubKey(mbedtls_svc_key_id_t key, uint8_t *data, size_t data_size, size_t *data_length);
extern sl_status_t gt_ReadDeviceProvisioningRCB(DeviceProvisioningRCB_T *stpt_RCB);
extern void gv_UpdateDeviceProvisioningRCB(int32_t i32_idx, bool b_isPresent, uint8_t u8_NVM3ID);
extern sl_status_t st_WriteDeviceProvisioningRCB(DeviceProvisioningRCB_T *stpt_block);
extern sl_status_t gt_CryptoInit(void);
extern sl_status_t gt_PSAStatus2SLStatus(psa_status_t t_retVal);

#endif //!_CSR_GENERATOR_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
