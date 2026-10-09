/**
 * @file          SETU_Server.h
 * @brief         SETU Server role: receives bulk data that a remote GATT
 *                client writes into this device's GATT database.
 *
 *                The service (SETU_Uuid.h) is declared with the generic
 *                callbacks of GATT_GenericCallbacks.c:
 *
 *                  DATA (Write Without Response + Write), buffer >= SETU_MAX_FRAME_LEN:
 *                     gt_GATT_GenericWrite + fpt_customWriteCb that forwards to
 *                     gt_SETUS_DataWriteHook(). Carries START, DATA, ABORT and
 *                     client -> server short messages.
 *                  CTRL (Notify + CCC):
 *                     ACK, NACK, END, ABORT and server -> client short messages,
 *                     sent with bt_gatt_notify_cb(). Bulk data never goes here.
 *
 *                Typical integration:
 * @code
 *                // Generated service .c (hook name set in the configurator)
 *                static ssize_t st_OnSETUData(struct bt_conn *c,
 *                   const struct bt_gatt_attr *a, const void *b, uint16_t l,
 *                   uint16_t o, uint8_t f)
 *                {
 *                   return gt_SETUS_DataWriteHook(c, a, b, l, o, f);
 *                }
 *
 *                // Application
 *                gi_SETUS_Init(&srvCfg);
 *                // bt_conn_cb: connected    -> gv_SETUS_OnConnected(conn)
 *                //             disconnected -> gv_SETU_OnDisconnected(conn)
 * @endcode
 *
 *                One connection at a time; gi_SETUS_Rebind() moves the
 *                binding to another live connection.
 *
 * @date          24/09/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef _SETU_SERVER_H
#define _SETU_SERVER_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include "SETU_Types.h"

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
/**
 * @struct        SETUSrvCfg_T
 * @brief         Configuration passed to gi_SETUS_Init(). Copied internally.
 */
typedef struct
{
   /**
    * @brief      Value attribute of the CTRL (notify) characteristic. Find it
    *             with bt_gatt_find_by_uuid(svc.attrs, svc.attr_count,
    *             BT_UUID_SETU_CTRL).
    */
   const struct bt_gatt_attr *stpt_ctrlAttr;

   SETURxStart_F fpt_onRxStart;               /**< Optional: NULL accepts every transfer. */
   SETURxData_F fpt_onRxData;                 /**< Required to receive transfers.         */
   SETURxDone_F fpt_onRxDone;                 /**< Optional.                              */
   SETURxShort_F fpt_onRxShort;               /**< Optional: client -> server shorts.     */

   /**
    * @brief      When true, gv_SETUS_OnConnected() requests 2M PHY and maximum
    *             data length, and (with CONFIG_BT_GATT_CLIENT) an ATT MTU
    *             exchange.
    */
   bool b_autoTuneLink;
} SETUSrvCfg_T;

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
/* ---- Lifecycle ----------------------------------------------------------- */
extern int gi_SETUS_Init(const SETUSrvCfg_T *stpt_cfg);
extern void gv_SETUS_OnConnected(struct bt_conn *stpt_conn);
extern void gv_SETUS_OnDisconnected(struct bt_conn *stpt_conn);
extern int gi_SETUS_Rebind(struct bt_conn *stpt_conn);

/* ---- Server -> client ---------------------------------------------------- */
extern int gi_SETUS_SendShort(uint8_t u8_appType, const void *vpt_data, uint8_t u8_len,
   k_timeout_t t_timeout);
extern void gv_SETUS_AbortRx(void);

/* ---- Queries ------------------------------------------------------------- */
extern bool gb_SETUS_IsRxBusy(void);
extern uint16_t gu16_SETUS_GetMaxShortPayload(void);
extern void gv_SETUS_GetCaps(SETUCaps_T *stpt_caps);

/* ---- GATT hook (GATTCustomWriteCb_F signature) --------------------------- */
extern ssize_t gt_SETUS_DataWriteHook(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset, uint8_t u8_flags);

#endif // _SETU_SERVER_H
