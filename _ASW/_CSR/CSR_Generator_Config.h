/**
 * @file          CSR_Generator_Config.h
 * @brief         Header file containing CSR generator configuration details.
 * @date          28/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _CSR_GENERATOR_CONFIG_H
#define _CSR_GENERATOR_CONFIG_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <string.h>

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
// Certification Subject Data. Values come from Kconfig (_DI/Kconfig, menu
// "Device certificate subject (CSR)"); the CN is generated at runtime.
/**
 * @def           CSR_GENERATOR_SUBJECT_COUNTRY
 * @brief         Country Identifier for CSR generation.
 */
#define CSR_GENERATOR_SUBJECT_COUNTRY        (CONFIG_CSR_SUBJ_C)

/**
 * @def           CSR_GENERATOR_SUBJECT_STATE
 * @brief         State Identifier for CSR generation.
 */
#define CSR_GENERATOR_SUBJECT_STATE          (CONFIG_CSR_SUBJ_ST)

/**
 * @def           CSR_GENERATOR_SUBJECT_LOCALITY
 * @brief         Locality Identifier for CSR generation.
 */
#define CSR_GENERATOR_SUBJECT_LOCALITY       (CONFIG_CSR_SUBJ_L)

/**
 * @def           CSR_GENERATOR_SUBJECT_ORGANIZATION
 * @brief         Organization Identifier for CSR generation.
 */
#define CSR_GENERATOR_SUBJECT_ORGANIZATION   (CONFIG_CSR_SUBJ_O)

/**
 * @def           CSR_GENERATOR_SUBJECT_ORGANIZATION_UNIT
 * @brief         Organization Unit Identifier for CSR generation.
 */
#define CSR_GENERATOR_SUBJECT_ORGANIZATION_UNIT \
                                             (CONFIG_CSR_SUBJ_OU)

/**
 * @def           CSR_GENERATOR_CONFIG_GENERATE_STATIC_AUTH
 * @brief         Generating Static Authentication Data.
 */
#define CSR_GENERATOR_CONFIG_GENERATE_STATIC_AUTH \
                                             (0)

/**
 * @def           CSR_GENERATOR_CONFIG_STORE_STATIC_AUTH
 * @brief         Store Static Authentication Data on device.
 */
#define CSR_GENERATOR_CONFIG_STORE_STATIC_AUTH \
                                             (0)

/**
 * @def           CSR_GENERATOR_CONFIG_GENERATE_SIGNING_KEY
 * @brief         Generating device signing key.
 */
#define CSR_GENERATOR_CONFIG_GENERATE_SIGNING_KEY \
                                             (1)

/**
 * @def           CSR_GENERATOR_CONFIG_STORE_SIGNING_KEY
 * @brief         Store ECC key on device.
 */
#define CSR_GENERATOR_CONFIG_STORE_SIGNING_KEY \
                                             (1)

/**
 * @def           CSR_GENERATOR_CONFIG_CERTIFICATE_ON_DEVICE
 * @brief         Should the device hold the certificate or not.
 */
#define CSR_GENERATOR_CONFIG_CERTIFICATE_ON_DEVICE \
                                             (1)

/**
 * @def           CSR_GENERATOR_CONFIG_CSR_ON_DEVICE
 * @brief         Should the device hold the CSR or not.
 */
#define CSR_GENERATOR_CONFIG_CSR_ON_DEVICE   (1)

/**
 * @def           DEVICE_CERTIFICATE_ITS_OFFSET
 * @brief         ITS offset for device certificate.
 */
#define DEVICE_CERTIFICATE_ITS_OFFSET        (1)

/**
 * @def           DEVICE_CSR_ITS_OFFSET
 * @brief         ITS offset for device CSR.
 */
#define DEVICE_CSR_ITS_OFFSET                (2)

/**
 * @def           DEVICE_CA_CERT_ITS_OFFSET
 * @brief         ITS offset for the CA certificate (same UID base as the device
 *                certificate).
 */
#define DEVICE_CA_CERT_ITS_OFFSET            (3)

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

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/
#endif //!_CSR_GENERATOR_CONFIG_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
