/**
 * @file          SETU.h
 * @brief         Umbrella header of the BLE bulk transfer (SETU) framework.
 *
 *                SETU moves arbitrarily large objects from a GATT client
 *                to a GATT server:
 *
 *                  Client (TX, SETU_Client.h): writes START / DATA frames
 *                     into the peer's DATA characteristic with Write Without
 *                     Response.
 *                  Server (RX, SETU_Server.h): hosts DATA + CTRL, answers
 *                     with ACK / NACK / END notifications on CTRL.
 *
 *                A device that needs both directions runs both roles; each
 *                binds its own connection. Reliability: windowed cumulative
 *                ACKs + Go-Back-N retransmit + CRC-32 over the whole object.
 *                See SETU_Frame.h for the wire format.
 *
 *                Roles are selected at build time with SETU_ENABLE_SERVER /
 *                SETU_ENABLE_CLIENT (SETU_Config.h).
 *
 * @date          24/09/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef _SETU_H
#define _SETU_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <zephyr/bluetooth/conn.h>
#include "SETU_Types.h"
#include "SETU_Uuid.h"
#if SETU_ENABLE_SERVER
#include "SETU_Server.h"
#endif // SETU_ENABLE_SERVER
#if SETU_ENABLE_CLIENT
#include "SETU_Client.h"
#endif // SETU_ENABLE_CLIENT

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
extern void gv_SETU_OnDisconnected(struct bt_conn *stpt_conn);

#endif // _SETU_H
