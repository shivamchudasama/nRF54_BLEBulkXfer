/**
 * @file          DeviceCert_Types.h
 * @brief         Header file containing device certificate types.
 * @date          05/09/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _DEVICE_CERT_TYPES_H
#define _DEVICE_CERT_TYPES_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include <stddef.h>

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           DEVICE_CERT_MAX_DER_LEN
 * @brief         Maximum length of generated device certificate in DER format.
 */
#define DEVICE_CERT_MAX_DER_LEN              (1024)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          DeviceCertStatus_E
 * @brief         Result of a certificate verification. The values are the
 *                provisioning RESULT status codes on the wire
 *                (_DOC/Provisioning/PROTOCOL.md), so they must not be renumbered.
 *                Codes 1, 2 and 9 belong to the provisioning flow, not to
 *                verification, and are never returned by DeviceCert_Verify.c.
 */
typedef enum
{
   eDCS_OK = 0x00,                           /**< Certificate verified */
   eDCS_PARSE = 0x03,                        /**< Not a parsable X.509 DER certificate */
   eDCS_NOT_CA = 0x04,                       /**< CA certificate is not a CA, is not
                                                  self-signed, or no CA is set yet */
   eDCS_BAD_SIG = 0x05,                      /**< Signature does not verify against
                                                  the CA (or itself, for the CA) */
   eDCS_KEY_MISMATCH = 0x06,                 /**< Device certificate carries another
                                                  public key than the device's own */
   eDCS_SUBJECT_MISMATCH = 0x07,             /**< Device certificate subject differs
                                                  from the CSR subject */
   eDCS_BAD_PROFILE = 0x08,                  /**< Key/signature algorithm, CA flag or
                                                  KeyUsage outside the profile */
   eDCS_INTERNAL = 0x0A,                     /**< Crypto library or memory failure */
} DeviceCertStatus_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        CACertData_T
 * @brief         Structure containing the CA certificate. Same layout as
 *                DeviceCertData_T, so both are stored the same way (DeviceCert.c).
 */
typedef struct __attribute__((__packed__))
{
   uint8_t u8_isCACertReceived;              /**< Flag indicating if the CA
                                                   certificate is received */
   uint16_t u16_CACertLen;                   /**< Length of the CA certificate (DER) */
   uint8_t u8ar_CACert[DEVICE_CERT_MAX_DER_LEN];
                                             /**< CA certificate */
} CACertData_T;

/**
 * @struct        DeviceCertData_T
 * @brief         Structure containing the device certificate.
 */
typedef struct __attribute__((__packed__))
{
   uint8_t u8_isDeviceCertGenerated;         /**< Flag indicating if the device
                                                   certificate is generated */
   uint16_t u16_deviceCertLen;               /**< Length of generated device certificate.
                                                   DER is binary format so length is
                                                   required */
   uint8_t u8ar_DeviceCert[DEVICE_CERT_MAX_DER_LEN];
                                             /**< Device certificate max length */
} DeviceCertData_T;

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

#endif //!_DEVICE_CERT_TYPES_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
