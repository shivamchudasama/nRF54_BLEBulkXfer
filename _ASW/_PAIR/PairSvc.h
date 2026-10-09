/**
 * @file          PairSvc.h
 * @brief         Header file containing the Pairing GATT service: the host's
 *                control point (CONTROL), the pairing status (STATUS, read and
 *                notify) and the characteristic the central writes over the
 *                secured peer link (SECURED). Wire contract:
 *                _DOC/Pairing/PROTOCOL.md §2.
 * @date          06/10/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _PAIR_SVC_H
#define _PAIR_SVC_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include <zephyr/bluetooth/uuid.h>
#include "BaseUUIDs.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           PART_UUID_DOMAIN_PAIR
 * @brief         UUID domain of the pairing service (same as SETU's).
 */
#define PART_UUID_DOMAIN_PAIR                (0xB1)

/**
 * @def           PART_UUID_SERVICE_PAIR
 * @brief         UUID service ID of the pairing service.
 */
#define PART_UUID_SERVICE_PAIR               (0xC1)

/**
 * @def           PAIR_UUID_VAL
 * @brief         128-bit value of a pairing service UUID with characteristic
 *                ID u16_char (0 for the service itself).
 */
#define PAIR_UUID_VAL(u16_char)              BT_UUID_128_ENCODE( \
                                                UUID_FIRST_PART_32BIT( \
                                                   PART_UUID_DOMAIN_PAIR, \
                                                   PART_UUID_SERVICE_PAIR, \
                                                   (u16_char)), \
                                                BASE_UUID_SECOND_PART_16BIT, \
                                                BASE_UUID_THIRD_PART_16BIT, \
                                                BASE_UUID_FOURTH_PART_16BIT, \
                                                BASE_UUID_FIFTH_PART_48BIT)

/**
 * @def           BT_UUID_PAIR_SVC_VAL
 * @brief         Pairing service, B1C10000-16A1-4812-AF35-F3F29A92F6CA.
 *                Advertised by a provisioned device.
 */
#define BT_UUID_PAIR_SVC_VAL                 PAIR_UUID_VAL(0x0000)

/**
 * @def           BT_UUID_PAIR_SVC
 * @brief         Pairing service UUID (const struct bt_uuid *).
 */
#define BT_UUID_PAIR_SVC                     BT_UUID_DECLARE_128(BT_UUID_PAIR_SVC_VAL)

/**
 * @def           BT_UUID_PAIR_CONTROL
 * @brief         CONTROL (Write): START / CANCEL / UNPAIR from the host.
 */
#define BT_UUID_PAIR_CONTROL                 BT_UUID_DECLARE_128(PAIR_UUID_VAL(0x0001))

/**
 * @def           BT_UUID_PAIR_STATUS
 * @brief         STATUS (Read, Notify): PAIR_STATUS_LEN bytes.
 */
#define BT_UUID_PAIR_STATUS                  BT_UUID_DECLARE_128(PAIR_UUID_VAL(0x0002))

/**
 * @def           BT_UUID_PAIR_SECURED
 * @brief         SECURED (Write, LE Secure Connections encryption required):
 *                written by the central over the paired link.
 */
#define BT_UUID_PAIR_SECURED                 BT_UUID_DECLARE_128(PAIR_UUID_VAL(0x0003))

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
struct bt_conn;

extern int gi_PairSvc_NotifyStatus(struct bt_conn *stpt_conn, const uint8_t *u8pt_status,
   uint16_t u16_len);

#endif //!_PAIR_SVC_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
