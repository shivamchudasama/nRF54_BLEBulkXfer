/**
 * @file          Prov.h
 * @brief         Header file containing device provisioning over SETU: hands
 *                the CSR to the provisioner (PC acting as the CA), receives the CA
 *                certificate and the device certificate, verifies both, stores
 *                them in ITS and deletes the CSR. Provisioning is one-time until a
 *                wipe (DEPROVISION or the DK button). Wire contract:
 *                _DOC/Provisioning/PROTOCOL.md.
 * @date          01/10/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _PROV_H
#define _PROV_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           PROV_APP_TYPE_FIRST
 * @brief         First SETU appType of the provisioning range.
 */
#define PROV_APP_TYPE_FIRST                  (0x20U)

/**
 * @def           PROV_APP_TYPE_LAST
 * @brief         Last SETU appType of the provisioning range (inclusive).
 */
#define PROV_APP_TYPE_LAST                   (0x2FU)

/**
 * @def           PROV_APP_TYPE_GET_STATUS
 * @brief         Short message, provisioner -> device, no payload: asks for STATUS.
 */
#define PROV_APP_TYPE_GET_STATUS             (0x20U)

/**
 * @def           PROV_APP_TYPE_STATUS
 * @brief         Short message, device -> provisioner:
 *                [u8 state][u8 flags][u16 LE CSR length][32 B SHA-256(public key)].
 */
#define PROV_APP_TYPE_STATUS                 (0x21U)

/**
 * @def           PROV_APP_TYPE_CSR_REQ
 * @brief         Short message, provisioner -> device, no payload: asks the device
 *                to send its CSR (as a PROV_APP_TYPE_CSR transfer).
 */
#define PROV_APP_TYPE_CSR_REQ                (0x22U)

/**
 * @def           PROV_APP_TYPE_CSR
 * @brief         Transfer, device -> provisioner: the DER CSR.
 */
#define PROV_APP_TYPE_CSR                    (0x23U)

/**
 * @def           PROV_APP_TYPE_CA_CERT
 * @brief         Transfer, provisioner -> device: the DER CA certificate.
 */
#define PROV_APP_TYPE_CA_CERT                (0x24U)

/**
 * @def           PROV_APP_TYPE_DEV_CERT
 * @brief         Transfer, provisioner -> device: the DER device certificate.
 */
#define PROV_APP_TYPE_DEV_CERT               (0x25U)

/**
 * @def           PROV_APP_TYPE_RESULT
 * @brief         Short message, device -> provisioner: [u8 refAppType][u8 status].
 */
#define PROV_APP_TYPE_RESULT                 (0x26U)

/**
 * @def           PROV_APP_TYPE_DEPROVISION
 * @brief         Short message, provisioner -> device, no payload: wipe the key,
 *                the CSR and both certificates, then generate a fresh key and CSR
 *                (state KEY_READY). Answered with RESULT.
 */
#define PROV_APP_TYPE_DEPROVISION            (0x27U)

/**
 * @def           PROV_STATUS_LEN
 * @brief         Payload length of the STATUS short message.
 */
#define PROV_STATUS_LEN                      (36U)

/**
 * @def           PROV_RESULT_LEN
 * @brief         Payload length of the RESULT short message.
 */
#define PROV_RESULT_LEN                      (2U)

/**
 * @def           PROV_FLAG_CSR_TX_BUSY
 * @brief         STATUS flags bit: the CSR is being sent.
 */
#define PROV_FLAG_CSR_TX_BUSY                (0x01U)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          ProvState_E
 * @brief         Provisioning state, sent in STATUS. Derived at boot from ITS:
 *                PROVISIONED when a stored certificate pair re-verifies, otherwise
 *                KEY_READY (a CA held in RAM in CA_OK is lost on reset).
 */
typedef enum
{
   ePS_NO_KEY = 0,                           /**< No key/CSR (generation failed) */
   ePS_KEY_READY = 1,                        /**< Key and CSR ready              */
   ePS_CA_OK = 2,                            /**< CA certificate verified (RAM)  */
   ePS_PROVISIONED = 3,                      /**< Both certificates stored; CSR
                                                  deleted; final until a wipe    */
} ProvState_E;

/**
 * @enum          ProvStatus_E
 * @brief         RESULT status codes. The certificate checks share their values
 *                with DeviceCertStatus_E (DeviceCert_Types.h).
 */
typedef enum
{
   ePRS_OK = 0x00,                           /**< Done                                  */
   ePRS_BAD_STATE = 0x01,                    /**< Not allowed in the current state, or
                                                  busy                                  */
   ePRS_TOO_LARGE = 0x02,                    /**< Object empty or above 1024 bytes      */
   ePRS_PARSE = 0x03,                        /**< Not a parsable DER certificate        */
   ePRS_NOT_CA = 0x04,                       /**< Not a self-signed CA certificate      */
   ePRS_BAD_SIG = 0x05,                      /**< Signature does not verify             */
   ePRS_KEY_MISMATCH = 0x06,                 /**< Not this device's public key          */
   ePRS_SUBJECT_MISMATCH = 0x07,             /**< Subject differs from the CSR          */
   ePRS_BAD_PROFILE = 0x08,                  /**< Algorithm, CA flag or KeyUsage        */
   ePRS_NO_PEER_SVC = 0x09,                  /**< Provisioner hosts no SETU service */
   ePRS_INTERNAL = 0x0A,                     /**< Crypto, storage, memory or SETU
                                                  failure                               */
   ePRS_TRANSFER = 0x0B,                     /**< CSR transfer failed                   */
} ProvStatus_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                                   UNIONS                                   */
/*                                                                            */
/******************************************************************************/

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
extern int gi_Prov_Init(void);
extern ProvState_E ge_Prov_GetState(void);
extern void gv_Prov_RequestWipe(void);

#endif //!_PROV_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
