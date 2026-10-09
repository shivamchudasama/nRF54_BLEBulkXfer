/**
 * @file          SETU_Core_Priv.h
 * @brief         Internal interface between the SETU engine core and the
 *                Server / Client roles. Not for applications.
 *
 *                The core owns the engine thread, its wake semaphore, the lock
 *                and the event bits. Each role owns its session, connection,
 *                credits, timers and incoming-frame queue, and plugs into the
 *                engine pass through gv_SETUx_EnginePre() / gv_SETUx_EnginePost().
 *
 * @date          24/09/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef _SETU_CORE_PRIV_H
#define _SETU_CORE_PRIV_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/sys/atomic.h>
#include "SETU_Types.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           SETU_SEQ_HALF_RANGE
 * @brief         Half of the 8-bit sequence space: separates "ahead" from
 *                "behind".
 */
#define SETU_SEQ_HALF_RANGE                   (128U)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          SETUEvent_E
 * @brief         Bit positions in gt_SETU_events.
 */
typedef enum
{
   eBE_SRV_ACK_DUE = 0,                      /**< Server delayed-ACK timer expired.       */
   eBE_SRV_IDLE,                             /**< Server inactivity timer expired.        */
   eBE_SRV_OVERFLOW,                         /**< DATA hook dropped a frame (no block).   */
   eBE_SRV_ABORT_REQ,                        /**< Application called gv_SETUS_AbortRx().   */
   eBE_CLI_TIMEOUT,                          /**< Client ACK timer expired.               */
   eBE_CLI_RETRY,                            /**< Host was out of buffers, retry pumping. */
   eBE_CLI_ABORT_REQ,                        /**< Application called gv_SETUC_AbortTx().   */
   eBE_CLI_ATTACH_DONE,                      /**< Discovery / subscription finished.      */
} SETUEvent_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        SETUFrameBlock_T
 * @brief         One received frame queued from BLE RX context to the engine.
 */
typedef struct
{
   void *vpt_fifoReserved;                   /**< Required by k_fifo (first word).        */
   atomic_val_t t_connGen;                   /**< Role's connection generation.           */
   uint16_t u16_len;                         /**< Valid bytes in u8ar_data.               */
   uint8_t u8ar_data[SETU_MAX_FRAME_LEN];     /**< Raw frame.                              */
} SETUFrameBlock_T;

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
extern struct k_mutex gst_SETU_lock;
extern atomic_t gt_SETU_events;

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/
/* ---- Core ---------------------------------------------------------------- */
extern void gv_SETU_EngineStart(void);
extern void gv_SETU_Kick(void);
extern void gv_SETU_TuneLink(struct bt_conn *stpt_conn);
extern bool gb_SETU_FrameLenValid(const uint8_t *u8pt_buf, uint16_t u16_len);
extern uint16_t gu16_SETU_FrameCapacity(struct bt_conn *stpt_conn);
extern uint32_t gu32_SETU_FrameCount(uint32_t u32_totalLen, uint8_t u8_chunkSize);
extern uint32_t gu32_SETU_SeqToAbs(uint8_t u8_seq, uint32_t u32_base);

/* ---- Role hooks, called by the engine with gst_SETU_lock held ------------- */
#if SETU_ENABLE_SERVER
extern void gv_SETUS_EnginePre(void);
extern void gv_SETUS_EnginePost(void);
#endif // SETU_ENABLE_SERVER
#if SETU_ENABLE_CLIENT
extern void gv_SETUC_EnginePre(void);
extern void gv_SETUC_EnginePost(void);
#endif // SETU_ENABLE_CLIENT

#endif // _SETU_CORE_PRIV_H
