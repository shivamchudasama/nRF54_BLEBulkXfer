/**
 * @file          SETU_Types.h
 * @brief         Types shared by the SETU Server and Client roles:
 *                application callbacks, data source and capability record.
 *
 *                Threading: every callback in SETUSrvCfg_T / SETUCliCfg_T runs
 *                on the SETU engine thread (never in BLE stack context),
 *                one at a time. Callbacks may call the SETU API (e.g.
 *                start the next transfer from fpt_onTxDone). Long-running
 *                callbacks (a slow flash write in fpt_onRxData) throttle the
 *                link naturally: the receiver simply ACKs later.
 *
 * @date          22/09/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef _SETU_TYPES_H
#define _SETU_TYPES_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/bluetooth/gatt.h>
#include "SETU_Frame.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           SETU_PROTOCOL_VERSION
 * @brief         Protocol version reported in SETUCaps_T. Version 2: the sender
 *                is the GATT client (writes DATA), the receiver the GATT
 *                server (notifies CTRL).
 */
#define SETU_PROTOCOL_VERSION                 (2U)

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
 * @typedef       SETUSourceRead_F
 * @brief         Sender data provider. Copies u16_len bytes starting at
 *                u32_offset of the object being sent into u8pt_buf.
 *
 *                The same range may be requested more than once (retransmit
 *                after NACK / timeout), so the source must stay readable until
 *                fpt_onTxDone is called.
 *
 * @param[in]     vpt_ctx Context pointer from SETUSource_T.
 * @param[in]     u32_offset Byte offset into the object.
 * @param[out]    u8pt_buf Destination (u16_len bytes).
 * @param[in]     u16_len Number of bytes to copy.
 * @return        0 on success, negative errno to abort the transfer with
 *                eBS_SOURCE_ERROR.
 */
typedef int (*SETUSourceRead_F)(void *vpt_ctx, uint32_t u32_offset,
   uint8_t *u8pt_buf, uint16_t u16_len);

/**
 * @struct        SETUSource_T
 * @brief         Data source of an outgoing multi-frame transfer.
 */
typedef struct
{
   SETUSourceRead_F fpt_read;                 /**< Data provider. Must not be NULL.       */
   void *vpt_ctx;                            /**< Passed back to fpt_read unchanged.     */
} SETUSource_T;

/**
 * @typedef       SETURxStart_F
 * @brief         A peer announced an incoming transfer (START frame).
 * @param[in]     u8_appType Application type of the object.
 * @param[in]     u32_totalLen Total size in bytes.
 * @return        0 to accept, non-zero to reject (peer gets eBS_REJECTED).
 */
typedef int (*SETURxStart_F)(uint8_t u8_appType, uint32_t u32_totalLen);

/**
 * @typedef       SETURxData_F
 * @brief         In-order chunk of an incoming transfer. Offsets are strictly
 *                increasing and contiguous; no chunk is delivered twice.
 * @param[in]     u8_appType Application type of the object.
 * @param[in]     u32_offset Offset of this chunk within the object.
 * @param[in]     u8pt_data Chunk data (valid only during the call).
 * @param[in]     u16_len Chunk length.
 * @return        0 to continue, non-zero to abort (eBS_SINK_ERROR).
 */
typedef int (*SETURxData_F)(uint8_t u8_appType, uint32_t u32_offset,
   const uint8_t *u8pt_data, uint16_t u16_len);

/**
 * @typedef       SETURxDone_F
 * @brief         An incoming transfer finished. Only eBS_OK means the
 *                data delivered through SETURxData_F is complete and verified.
 * @param[in]     u8_appType Application type of the object.
 * @param[in]     e_status Result.
 * @param[in]     u32_totalLen Size announced in START.
 */
typedef void (*SETURxDone_F)(uint8_t u8_appType, SETUStatus_E e_status,
   uint32_t u32_totalLen);

/**
 * @typedef       SETURxShort_F
 * @brief         A single-frame application message was received.
 * @param[in]     u8_appType Application type (0x00..SETU_APP_TYPE_MAX).
 * @param[in]     u8pt_data Payload (valid only during the call).
 * @param[in]     u8_len Payload length.
 */
typedef void (*SETURxShort_F)(uint8_t u8_appType, const uint8_t *u8pt_data,
   uint8_t u8_len);

/**
 * @typedef       SETUTxDone_F
 * @brief         An outgoing transfer finished.
 * @param[in]     u8_appType Application type passed to gi_SETUC_Send().
 * @param[in]     e_status Result reported by the receiver, or a local error.
 */
typedef void (*SETUTxDone_F)(uint8_t u8_appType, SETUStatus_E e_status);

/**
 * @struct        SETUCaps_T
 * @brief         Capabilities exposed by the Server through the optional,
 *                read-only Caps characteristic (served by gt_GATT_GenericRead).
 */
typedef struct __packed
{
   uint8_t u8_protocolVersion;               /**< SETU_PROTOCOL_VERSION.                  */
   uint8_t u8_maxFrameLen;                   /**< SETU_MAX_FRAME_LEN.                     */
   uint8_t u8_window;                        /**< SETU_WINDOW_DEFAULT.                    */
   uint8_t u8_reserved;                      /**< 0.                                     */
} SETUCaps_T;

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

#endif // _SETU_TYPES_H
