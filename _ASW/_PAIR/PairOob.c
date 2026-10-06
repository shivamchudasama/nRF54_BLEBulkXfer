/**
 * @file          PairOob.c
 * @brief         Source file containing the signed OOB data of certificate-based
 *                pairing. A device signs its LE Secure Connections OOB values
 *                with its device key (ECDSA P-256, SHA-256, raw r || s), over
 *
 *                   r || c || sender address || receiver address
 *
 *                and sends [r][c][signature]. The addresses are not sent: the
 *                receiver knows both, and binding them stops a frame made for
 *                one peer from being replayed to another. The peer verifies with
 *                the public key of the device certificate it has just verified.
 *
 *                Same content as the BG22/BG24 reference (st_SignOOBData /
 *                st_VerifyOOBData in Sample Code Device Cert Verification), which
 *                signed r || c only and returned an uninitialised status on a
 *                NULL argument.
 * @date          06/10/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "PairOob.h"
#include <stddef.h>
#include <string.h>

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           PAIR_OOB_ALG
 * @brief         Signature algorithm: ECDSA with SHA-256 over the message.
 */
#define PAIR_OOB_ALG                         (PSA_ALG_ECDSA(PSA_ALG_SHA_256))

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/

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
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/*                                                                            */
/******************************************************************************/
static void sv_BuildMessage(const uint8_t *u8pt_rand, const uint8_t *u8pt_confirm,
   const uint8_t *u8pt_sender, const uint8_t *u8pt_receiver, uint8_t *u8pt_msg);

/******************************************************************************/
/*                                                                            */
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                        PRIVATE FUNCTION DEFINITIONS                        */
/*                                                                            */
/******************************************************************************/
/**
 * @private       sv_BuildMessage
 * @brief         Lay out the signed message r || c || sender || receiver.
 * @param[in]     u8pt_rand OOB random value r (PAIR_OOB_VALUE_LEN bytes).
 * @param[in]     u8pt_confirm OOB confirm value c (PAIR_OOB_VALUE_LEN bytes).
 * @param[in]     u8pt_sender Address of the device that made r and c.
 * @param[in]     u8pt_receiver Address of the device they are for.
 * @param[out]    u8pt_msg Message, PAIR_OOB_SIGNED_LEN bytes.
 * @return        None.
 */
static void sv_BuildMessage(const uint8_t *u8pt_rand, const uint8_t *u8pt_confirm,
   const uint8_t *u8pt_sender, const uint8_t *u8pt_receiver, uint8_t *u8pt_msg)
{
   (void)memcpy(&u8pt_msg[0], u8pt_rand, PAIR_OOB_VALUE_LEN);
   (void)memcpy(&u8pt_msg[PAIR_OOB_VALUE_LEN], u8pt_confirm, PAIR_OOB_VALUE_LEN);
   (void)memcpy(&u8pt_msg[2U * PAIR_OOB_VALUE_LEN], u8pt_sender, PAIR_ADDR_LEN);
   (void)memcpy(&u8pt_msg[(2U * PAIR_OOB_VALUE_LEN) + PAIR_ADDR_LEN], u8pt_receiver,
      PAIR_ADDR_LEN);
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gt_PairOob_Sign
 * @brief         Build the OOB frame [r][c][signature] for the peer.
 * @param[in]     t_key Signing key (the device key, CSR_DEVICE_SIGNING_KEY_ID).
 * @param[in]     u8pt_rand Own OOB random value r.
 * @param[in]     u8pt_confirm Own OOB confirm value c.
 * @param[in]     u8pt_sender Own address (PAIR_ADDR_LEN bytes).
 * @param[in]     u8pt_receiver Peer address (PAIR_ADDR_LEN bytes).
 * @param[out]    u8pt_frame Frame, PAIR_OOB_FRAME_LEN bytes.
 * @return        PSA_SUCCESS; PSA_ERROR_INVALID_ARGUMENT for a NULL argument;
 *                otherwise the error of psa_sign_message(), or
 *                PSA_ERROR_CORRUPTION_DETECTED if the signature is not 64 bytes.
 */
psa_status_t gt_PairOob_Sign(psa_key_id_t t_key, const uint8_t *u8pt_rand,
   const uint8_t *u8pt_confirm, const uint8_t *u8pt_sender, const uint8_t *u8pt_receiver,
   uint8_t *u8pt_frame)
{
   uint8_t u8ar_msg[PAIR_OOB_SIGNED_LEN];
   size_t t_sigLen = 0U;
   psa_status_t t_status;

   // Check if every buffer is given
   if ((u8pt_rand == NULL) || (u8pt_confirm == NULL) || (u8pt_sender == NULL) ||
      (u8pt_receiver == NULL) || (u8pt_frame == NULL))
   {
      return PSA_ERROR_INVALID_ARGUMENT;
   }

   sv_BuildMessage(u8pt_rand, u8pt_confirm, u8pt_sender, u8pt_receiver, u8ar_msg);

   t_status = psa_sign_message(t_key, PAIR_OOB_ALG, u8ar_msg, sizeof(u8ar_msg),
      &u8pt_frame[2U * PAIR_OOB_VALUE_LEN], PAIR_OOB_SIG_LEN, &t_sigLen);

   // Check if a raw P-256 signature was made
   if (t_status != PSA_SUCCESS)
   {
      return t_status;
   }
   if (t_sigLen != PAIR_OOB_SIG_LEN)
   {
      return PSA_ERROR_CORRUPTION_DETECTED;
   }

   (void)memcpy(&u8pt_frame[0], u8pt_rand, PAIR_OOB_VALUE_LEN);
   (void)memcpy(&u8pt_frame[PAIR_OOB_VALUE_LEN], u8pt_confirm, PAIR_OOB_VALUE_LEN);

   return PSA_SUCCESS;
}

/**
 * @public        gt_PairOob_Verify
 * @brief         Check the signature of a peer's OOB frame.
 * @param[in]     t_peerKey Peer public key, from its verified device certificate
 *                (ge_VerifyRemoteDeviceCertificate()).
 * @param[in]     u8pt_frame Received frame [r][c][signature].
 * @param[in]     u8pt_sender Peer address (it made r and c).
 * @param[in]     u8pt_receiver Own address.
 * @return        PSA_SUCCESS if the signature verifies; PSA_ERROR_INVALID_ARGUMENT
 *                for a NULL argument; PSA_ERROR_INVALID_SIGNATURE if it does not
 *                verify; otherwise the error of psa_verify_message().
 */
psa_status_t gt_PairOob_Verify(psa_key_id_t t_peerKey, const uint8_t *u8pt_frame,
   const uint8_t *u8pt_sender, const uint8_t *u8pt_receiver)
{
   uint8_t u8ar_msg[PAIR_OOB_SIGNED_LEN];

   // Check if every buffer is given
   if ((u8pt_frame == NULL) || (u8pt_sender == NULL) || (u8pt_receiver == NULL))
   {
      return PSA_ERROR_INVALID_ARGUMENT;
   }

   sv_BuildMessage(&u8pt_frame[0], &u8pt_frame[PAIR_OOB_VALUE_LEN], u8pt_sender,
      u8pt_receiver, u8ar_msg);

   return psa_verify_message(t_peerKey, PAIR_OOB_ALG, u8ar_msg, sizeof(u8ar_msg),
      &u8pt_frame[2U * PAIR_OOB_VALUE_LEN], PAIR_OOB_SIG_LEN);
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
