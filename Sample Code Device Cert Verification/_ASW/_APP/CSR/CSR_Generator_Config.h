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
/**
 * @def           KEY_USAGE_SIGN_VERIFY
 * @brief         Key usage for signing and verifying messages.
 */
#define KEY_USAGE_SIGN_VERIFY                (PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_VERIFY_MESSAGE | \
                                             PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_VERIFY_HASH)

/**
 * @def           KEY_USAGE_DERIVE
 * @brief         Key usage for key derivation.
 */
#define KEY_USAGE_DERIVE                     (PSA_KEY_USAGE_DERIVE)

/**
 * @def           SIGN_VERIFY_ALGO
 * @brief         Signature and verification algorithm.
 */
#define SIGN_VERIFY_ALGO                     (PSA_ALG_ECDSA(PSA_ALG_SHA_256))

/**
 * @def           CSR_PROTOCOL_BLE
 * @brief         CSR protocol used for BLE.
 */
#define CSR_PROTOCOL_BLE                     (0)

/**
 * @def           CSR_PROTOCOL_BTMESH
 * @brief         CSR protocol used for Bluetooth Mesh.
 */
#define CSR_PROTOCOL_BTMESH                  (1)

/**
 * @def           CSR_GENERATOR_CONFIG_GENERATE_STATIC_AUTH
 * @brief         Generating Static Authentication Data.
 */
#define CSR_GENERATOR_CONFIG_GENERATE_STATIC_AUTH \
                                             (0)

/**
 * @def           CSR_GENERATOR_CONFIG_GENERATE_EC_KEY
 * @brief         Generating device EC key.
 */
// <q CSR_GENERATOR_CONFIG_GENERATE_EC_KEY> Device EC key
// <i> Generating device EC key.
#define CSR_GENERATOR_CONFIG_GENERATE_EC_KEY (1)

/**
 * @def           CSR_GENERATOR_CONFIG_CERTIFICATE_ON_DEVICE
 * @brief         Should the device hold the certificate or not.
 */
#define CSR_GENERATOR_CONFIG_CERTIFICATE_ON_DEVICE \
                                             (0)

/**
 * @def           CSR_GENERATOR_CONFIG_CSR_ON_DEVICE
 * @brief         Should the device hold the CSR or not.
 */
#define CSR_GENERATOR_CONFIG_CSR_ON_DEVICE   (1)

/**
 * @def           CSR_GENERATOR_CSR_RAM_ADDRESS
 * @brief         The RAM address where the Certificate Request is stored.
 */
#define CSR_GENERATOR_CSR_RAM_ADDRESS        (0x20007C00)

/**
 * @def           CSR_GENERATOR_NVM3_REGION
 * @brief         The NVM3 key region which will be used to save security keys and certificates.
 *                This is the default NVM3 region for Bluetooth NVM3
 *                key space.
 */
#define CSR_GENERATOR_NVM3_REGION            (0x40000)

/**
 * @def           CSR_GENERATOR_EC_KEY_USAGE
 * @brief         Key Usage for EC key pair.
 *                KEY_USAGE_SIGN_VERIFY: Use for message signing and verification.
 *                KEY_USAGE_DERIVE: Use for key derivation.
 *                Default: KEY_USAGE_SIGN_VERIFY
 */
#define CSR_GENERATOR_EC_KEY_USAGE           (KEY_USAGE_SIGN_VERIFY)

/**
 * @def           CSR_GENERATOR_EC_KEY_ALGO
 * @brief         Key algorithm.
 *                SIGN_VERIFY_ALGO: Algorithm for sign and verification.
 *                PSA_ALG_ECDH: Algorithm for key derivation.
 *                Default: SIGN_VERIFY_ALGO
 */
#define CSR_GENERATOR_EC_KEY_ALGO            (SIGN_VERIFY_ALGO)

// Certification Subject Data
/**
 * @def           CSR_GENERATOR_SUBJECT_COUNTRY
 * @brief         Country Identifier for CSR generation.
 */
#define CSR_GENERATOR_SUBJECT_COUNTRY        ("IN")

/**
 * @def           CSR_GENERATOR_SUBJECT_STATE
 * @brief         State Identifier for CSR generation.
 */
#define CSR_GENERATOR_SUBJECT_STATE          ("Maharashtra")

/**
 * @def           CSR_GENERATOR_SUBJECT_LOCALITY
 * @brief         Locality Identifier for CSR generation.
 */
#define CSR_GENERATOR_SUBJECT_LOCALITY       ("Pune")

/**
 * @def           CSR_GENERATOR_SUBJECT_ORGANIZATION
 * @brief         Organization Identifier for CSR generation.
 */
#define CSR_GENERATOR_SUBJECT_ORGANIZATION   ("Bajaj Auto Technology Limited")

/**
 * @def           CSR_GENERATOR_SUBJECT_ORGANIZATION_UNIT
 * @brief         Organization Unit Identifier for CSR generation.
 */
#define CSR_GENERATOR_SUBJECT_ORGANIZATION_UNIT \
                                             ("IS Team")

/**
 * @def           CSR_GENERATOR_KEY_LOCATION
 * @brief         Key location for CSR generation.
 *                PSA_KEY_LOCATION_SLI_SE_OPAQUE: Vault.
 *                PSA_KEY_LOCATION_LOCAL_STORAGE: Local Storage.
 *                Default: PSA_KEY_LOCATION_LOCAL_STORAGE
 */
#define CSR_GENERATOR_KEY_LOCATION           (PSA_KEY_LOCATION_SLI_SE_OPAQUE)

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
