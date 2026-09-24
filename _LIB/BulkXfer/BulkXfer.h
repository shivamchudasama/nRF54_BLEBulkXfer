/**
 * @file          BulkXfer.h
 * @brief         Umbrella header of the BLE bulk transfer (BulkXfer) framework.
 *
 *                BulkXfer moves arbitrarily large objects from a GATT client
 *                to a GATT server:
 *
 *                  Client (TX, BulkXfer_Client.h): writes START / DATA frames
 *                     into the peer's DATA characteristic with Write Without
 *                     Response.
 *                  Server (RX, BulkXfer_Server.h): hosts DATA + CTRL, answers
 *                     with ACK / NACK / END notifications on CTRL.
 *
 *                A device that needs both directions runs both roles; each
 *                binds its own connection. Reliability: windowed cumulative
 *                ACKs + Go-Back-N retransmit + CRC-32 over the whole object.
 *                See BulkXfer_Frame.h for the wire format.
 *
 *                Roles are selected at build time with BLK_ENABLE_SERVER /
 *                BLK_ENABLE_CLIENT (BulkXfer_Config.h).
 *
 * @date          24/09/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef _BULK_XFER_H
#define _BULK_XFER_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <zephyr/bluetooth/conn.h>
#include "BulkXfer_Types.h"
#include "BulkXfer_Uuid.h"
#if BLK_ENABLE_SERVER
#include "BulkXfer_Server.h"
#endif // BLK_ENABLE_SERVER
#if BLK_ENABLE_CLIENT
#include "BulkXfer_Client.h"
#endif // BLK_ENABLE_CLIENT

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/

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
extern void gv_BLK_OnDisconnected(struct bt_conn *stpt_conn);

#endif // _BULK_XFER_H
