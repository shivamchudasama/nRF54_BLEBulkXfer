/**
 * @file          PairOob.h
 * @brief         Header file containing the signed OOB data of certificate-based
 *                pairing: the LE Secure Connections OOB values (random r and
 *                confirm c) of one device, signed with its device key so that
 *                the peer can tie them to the certificate it has verified.
 *                Wire contract: _DOC/Pairing/PROTOCOL.md §4.
 * @date          06/10/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _PAIR_OOB_H
#define _PAIR_OOB_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include <psa/crypto.h>

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           PAIR_OOB_VALUE_LEN
 * @brief         Length of the OOB random value r and of the confirm value c.
 */
#define PAIR_OOB_VALUE_LEN                   (16U)

/**
 * @def           PAIR_ADDR_LEN
 * @brief         Length of an LE address on the wire: [u8 type][6 B address,
 *                least significant byte first], as bt_addr_le_t in memory.
 */
#define PAIR_ADDR_LEN                        (7U)

/**
 * @def           PAIR_OOB_SIG_LEN
 * @brief         Length of an ECDSA P-256 signature, raw r || s.
 */
#define PAIR_OOB_SIG_LEN                     (64U)

/**
 * @def           PAIR_OOB_FRAME_LEN
 * @brief         Length of the OOB frame sent to the peer: [r][c][signature].
 */
#define PAIR_OOB_FRAME_LEN                   ((2U * PAIR_OOB_VALUE_LEN) + PAIR_OOB_SIG_LEN)

/**
 * @def           PAIR_OOB_SIGNED_LEN
 * @brief         Length of the signed message:
 *                r || c || sender address || receiver address.
 */
#define PAIR_OOB_SIGNED_LEN                  ((2U * PAIR_OOB_VALUE_LEN) + (2U * PAIR_ADDR_LEN))

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
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/
extern psa_status_t gt_PairOob_Sign(psa_key_id_t t_key, const uint8_t *u8pt_rand,
   const uint8_t *u8pt_confirm, const uint8_t *u8pt_sender, const uint8_t *u8pt_receiver,
   uint8_t *u8pt_frame);
extern psa_status_t gt_PairOob_Verify(psa_key_id_t t_peerKey, const uint8_t *u8pt_frame,
   const uint8_t *u8pt_sender, const uint8_t *u8pt_receiver);

#endif //!_PAIR_OOB_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
