/**
 * @file          SETU_Frame.h
 * @brief         Wire format of the BLE bulk transfer (SETU) framework:
 *                frame layout, frame types, status codes and the pure
 *                encode / decode functions.
 *
 *                Every GATT write / notification carries exactly one frame:
 *
 * @code
 *                +--------+--------+---------------------------+
 *                | len(1) | type(1)| payload (len bytes)       |
 *                +--------+--------+---------------------------+
 * @endcode
 *
 *                - len  : payload length. len + 2 must equal the ATT length.
 *                - type : 0x00..0xEF application data type (single-frame
 *                         message, no ACK), 0xF0..0xFF framework control.
 *
 *                Multi-byte fields are little-endian.
 *
 *                This module has no Zephyr dependency so that it can be unit
 *                tested on the host.
 *
 * @date          22/09/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef _SETU_FRAME_H
#define _SETU_FRAME_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>
#include "SETU_Config.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           SETU_FRAME_HDR_LEN
 * @brief         Frame header: len(1) + type(1).
 */
#define SETU_FRAME_HDR_LEN                    (2U)

/**
 * @def           SETU_DATA_HDR_LEN
 * @brief         DATA frame header: len(1) + type(1) + xferId(1) + seq(1).
 */
#define SETU_DATA_HDR_LEN                     (4U)

/**
 * @def           SETU_MIN_FRAME_LEN
 * @brief         Smallest frame that can carry a DATA frame with one data byte.
 */
#define SETU_MIN_FRAME_LEN                    (SETU_DATA_HDR_LEN + 1U)

/**
 * @def           SETU_MAX_SHORT_PAYLOAD
 * @brief         Largest application payload of a single-frame (short) message.
 */
#define SETU_MAX_SHORT_PAYLOAD                (SETU_MAX_FRAME_LEN - SETU_FRAME_HDR_LEN)

/**
 * @def           SETU_MAX_CHUNK_LEN
 * @brief         Largest data chunk carried by one DATA frame.
 */
#define SETU_MAX_CHUNK_LEN                    (SETU_MAX_FRAME_LEN - SETU_DATA_HDR_LEN)

/**
 * @def           SETU_APP_TYPE_MAX
 * @brief         Highest type value available to the application.
 */
#define SETU_APP_TYPE_MAX                     (0xEFU)

/**
 * @def           SETU_START_PAYLOAD_LEN
 * @brief         Payload size of a START frame.
 */
#define SETU_START_PAYLOAD_LEN                (12U)

/**
 * @def           SETU_ACK_PAYLOAD_LEN
 * @brief         Payload size of an ACK frame.
 */
#define SETU_ACK_PAYLOAD_LEN                  (3U)

/**
 * @def           SETU_NACK_PAYLOAD_LEN
 * @brief         Payload size of a NACK frame.
 */
#define SETU_NACK_PAYLOAD_LEN                 (3U)

/**
 * @def           SETU_END_PAYLOAD_LEN
 * @brief         Payload size of an END frame.
 */
#define SETU_END_PAYLOAD_LEN                  (2U)

/**
 * @def           SETU_ABORT_PAYLOAD_LEN
 * @brief         Payload size of an ABORT frame.
 */
#define SETU_ABORT_PAYLOAD_LEN                (3U)

/**
 * @def           SETU_CTRL_FRAME_MAX_LEN
 * @brief         Largest control frame (START). Useful for stack buffers.
 */
#define SETU_CTRL_FRAME_MAX_LEN               (SETU_FRAME_HDR_LEN + SETU_START_PAYLOAD_LEN)

/**
 * @def           ENOTSUP
 * @brief         Some host C libraries (old MinGW) lack ENOTSUP; Zephyr
 *                defines it.
 */
#ifndef ENOTSUP
#define ENOTSUP                              (134)
#endif // ENOTSUP

#if (SETU_MAX_FRAME_LEN > 257U) || (SETU_MAX_FRAME_LEN < 20U)
#error "SETU_MAX_FRAME_LEN must be in the range 20..257"
#endif // SETU_MAX_FRAME_LEN

#if (SETU_WINDOW_DEFAULT < 2U) || (SETU_WINDOW_DEFAULT > 128U)
#error "SETU_WINDOW_DEFAULT must be in the range 2..128"
#endif // SETU_WINDOW_DEFAULT

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          SETUFrameType_E
 * @brief         Framework-reserved frame types (0xF0..0xFF).
 *
 *                Sender -> receiver : START, DATA, ABORT(eBAD_BY_SENDER)
 *                Receiver -> sender : ACK, NACK, END, ABORT(eBAD_BY_RECEIVER)
 */
typedef enum
{
   /** xferId(1) appType(1) totalLen(4) chunkSize(1) window(1) crc32(4) */
   eBFT_START = 0xF0,                        /**< START frame */
   /** xferId(1) seq(1) data(1..chunkSize) */
   eBFT_DATA  = 0xF1,                        /**< DATA frame */
   /** xferId(1) nextExpectedSeq(1) window(1) - cumulative acknowledgement */
   eBFT_ACK   = 0xF2,                        /**< ACK frame */
   /** xferId(1) nextExpectedSeq(1) reason(1) - go back to nextExpectedSeq */
   eBFT_NACK  = 0xF3,                        /**< NACK frame */
   /** xferId(1) status(1) - final result, sent by the receiver */
   eBFT_END   = 0xF4,                        /**< END frame */
   /** xferId(1) reason(1) direction(1) */
   eBFT_ABORT = 0xF5,                        /**< ABORT frame (direction distinguishes sender / receiver) */
} SETUFrameType_E;

/**
 * @enum          SETUStatus_E
 * @brief         Transfer result. Also used on the wire as the END status and
 *                the ABORT / NACK reason.
 */
typedef enum
{
   eBS_OK              = 0x00,               /**< Transfer complete, CRC matched.   */
   eBS_CRC_ERROR       = 0x01,               /**< All data received, CRC mismatch.  */
   eBS_TIMEOUT         = 0x02,               /**< Peer stopped responding.          */
   eBS_ABORTED         = 0x03,               /**< Aborted by the local application. */
   eBS_REMOTE_ABORTED  = 0x04,               /**< Aborted by the peer.              */
   eBS_REJECTED        = 0x05,               /**< Receiver refused the transfer.    */
   eBS_DISCONNECTED    = 0x06,               /**< Link lost during the transfer.    */
   eBS_SOURCE_ERROR    = 0x07,               /**< Sender's data source failed.      */
   eBS_SINK_ERROR      = 0x08,               /**< Receiver's data sink failed.      */
   eBS_PROTOCOL_ERROR  = 0x09,               /**< Malformed / unexpected frame.     */
   eBS_NO_RESOURCES    = 0x0A,               /**< RX queue overflow (NACK reason).  */
   eBS_OUT_OF_ORDER    = 0x0B,               /**< Sequence gap (NACK reason).       */
} SETUStatus_E;

/**
 * @enum          SETUAbortDir_E
 * @brief         Which side of the transfer an ABORT frame refers to. Both
 *                peers may run a TX and an RX transfer at the same time, so the
 *                xferId alone is not enough to identify the session.
 */
typedef enum
{
   eBAD_BY_SENDER   = 0x00,                  /**< Sender cancels its outgoing transfer. */
   eBAD_BY_RECEIVER = 0x01,                  /**< Receiver cancels an incoming transfer.*/
} SETUAbortDir_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        SETUFrame_T
 * @brief         Decoded view of a received frame. Pointer members reference
 *                the buffer passed to gi_SETU_FrameParse() (no copy).
 */
typedef struct
{
   uint8_t u8_type;                          /**< Frame type (app type or SETUFrameType_E). */
   uint8_t u8_payloadLen;                    /**< Payload length (the len byte).           */
   const uint8_t *u8pt_payload;              /**< Payload (short app frames).              */

   union
   {
      /* Members are ordered by alignment (widest first) to avoid padding,
       * not in wire order. Access them by name only. */
      struct
      {
         uint32_t u32_totalLen;              /**< Total bytes in the transfer.             */
         uint32_t u32_crc32;                 /**< CRC-32 over the complete data.           */
         uint8_t u8_xferId;                  /**< Transfer ID chosen by the sender.        */
         uint8_t u8_appType;                 /**< Application type of the assembled data.  */
         uint8_t u8_chunkSize;               /**< Max data bytes per DATA frame.           */
         uint8_t u8_window;                  /**< Sender's proposed window (frames).       */
      } st_start;                            /**< eBFT_START fields.                       */

      struct
      {
         const uint8_t *u8pt_data;           /**< Chunk data (points into the rx buffer).  */
         uint8_t u8_xferId;                  /**< Transfer ID.                             */
         uint8_t u8_seq;                     /**< Sequence number of this chunk.           */
         uint8_t u8_dataLen;                 /**< Chunk length in bytes.                   */
      } st_data;                             /**< eBFT_DATA fields.                        */

      struct
      {
         uint8_t u8_xferId;                  /**< Transfer ID.                             */
         uint8_t u8_seq;                     /**< Next expected seq (cumulative ACK).      */
         uint8_t u8_window;                  /**< Receiver's granted window (frames).      */
      } st_ack;                              /**< eBFT_ACK fields.                         */

      struct
      {
         uint8_t u8_xferId;                  /**< Transfer ID.                             */
         uint8_t u8_seq;                     /**< Next expected seq; resend from here.     */
         uint8_t u8_reason;                  /**< Reason (SETUStatus_E).                    */
      } st_nack;                             /**< eBFT_NACK fields.                        */

      struct
      {
         uint8_t u8_xferId;                  /**< Transfer ID.                             */
         uint8_t u8_status;                  /**< Final result (SETUStatus_E).              */
      } st_end;                              /**< eBFT_END fields.                         */

      struct
      {
         uint8_t u8_xferId;                  /**< Transfer ID.                             */
         uint8_t u8_reason;                  /**< Reason (SETUStatus_E).                    */
         uint8_t u8_dir;                     /**< Aborting side (SETUAbortDir_E).           */
      } st_abort;                            /**< eBFT_ABORT fields.                       */
   } u_body;                                 /**< Type-specific decoded fields.            */
} SETUFrame_T;

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
/* ---- Decoding ------------------------------------------------------------ */
extern int gi_SETU_FrameParse(const uint8_t *u8pt_buf, uint16_t u16_len,
   SETUFrame_T *stpt_frame);

/* ---- Encoding (return total frame length, or 0 if the buffer is too small) */
extern uint16_t gu16_SETU_EncodeShort(uint8_t *u8pt_buf, uint16_t u16_bufLen,
   uint8_t u8_appType, const uint8_t *u8pt_data, uint16_t u16_dataLen);
extern uint16_t gu16_SETU_EncodeStart(uint8_t *u8pt_buf, uint16_t u16_bufLen,
   uint8_t u8_xferId, uint8_t u8_appType, uint32_t u32_totalLen,
   uint8_t u8_chunkSize, uint8_t u8_window, uint32_t u32_crc32);
extern uint16_t gu16_SETU_EncodeDataHeader(uint8_t *u8pt_buf, uint16_t u16_bufLen,
   uint8_t u8_xferId, uint8_t u8_seq, uint16_t u16_dataLen);
extern uint16_t gu16_SETU_EncodeAck(uint8_t *u8pt_buf, uint16_t u16_bufLen,
   uint8_t u8_xferId, uint8_t u8_seq, uint8_t u8_window);
extern uint16_t gu16_SETU_EncodeNack(uint8_t *u8pt_buf, uint16_t u16_bufLen,
   uint8_t u8_xferId, uint8_t u8_seq, uint8_t u8_reason);
extern uint16_t gu16_SETU_EncodeEnd(uint8_t *u8pt_buf, uint16_t u16_bufLen,
   uint8_t u8_xferId, uint8_t u8_status);
extern uint16_t gu16_SETU_EncodeAbort(uint8_t *u8pt_buf, uint16_t u16_bufLen,
   uint8_t u8_xferId, uint8_t u8_reason, uint8_t u8_dir);

#endif // _SETU_FRAME_H
