/**
 * @file          SETU_Config.h
 * @brief         Compile-time configuration of the BLE bulk transfer (SETU)
 *                framework.
 *
 *                Every value is wrapped in #ifndef so it can be overridden from
 *                the application's CMakeLists.txt, e.g.:
 *
 * @code
 *                zephyr_compile_definitions(SETU_WINDOW_DEFAULT=32)
 * @endcode
 *
 *                This header is intentionally free of Zephyr includes so that
 *                SETU_Frame.c can be unit-tested on any host. Kconfig
 *                symbols (CONFIG_*) are still visible: Zephyr injects them
 *                into every translation unit.
 *
 * @date          22/09/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef _SETU_CONFIG_H
#define _SETU_CONFIG_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           SETU_ENABLE_SERVER
 * @brief         1 builds the Server role (receives bulk data written into this
 *                device's GATT database).
 */
#ifndef SETU_ENABLE_SERVER
#define SETU_ENABLE_SERVER                    (1)
#endif // SETU_ENABLE_SERVER

/**
 * @def           SETU_ENABLE_CLIENT
 * @brief         1 builds the Client role (sends bulk data by writing into the
 *                peer's GATT database). Needs CONFIG_BT_GATT_CLIENT.
 */
#ifndef SETU_ENABLE_CLIENT
#if defined(CONFIG_BT_GATT_CLIENT)
#define SETU_ENABLE_CLIENT                    (1)
#else
#define SETU_ENABLE_CLIENT                    (0)
#endif // CONFIG_BT_GATT_CLIENT
#endif // SETU_ENABLE_CLIENT

/**
 * @def           SETU_MAX_FRAME_LEN
 * @brief         Largest frame (len + type + payload) the framework will build
 *                or accept, in bytes.
 *
 *                244 = 251 (LL payload with DLE) - 4 (L2CAP hdr) - 3 (ATT hdr),
 *                i.e. exactly one Link Layer packet at ATT_MTU 247. The frame
 *                used on a given link is min(ATT_MTU - 3, SETU_MAX_FRAME_LEN).
 *                Must not exceed 257 (1-byte length field + 2-byte header).
 *                The DATA characteristic buffer must be at least this large.
 */
#ifndef SETU_MAX_FRAME_LEN
#define SETU_MAX_FRAME_LEN                    (244U)
#endif // SETU_MAX_FRAME_LEN

/**
 * @def           SETU_WINDOW_DEFAULT
 * @brief         Number of DATA frames that may be in flight without an ACK.
 *                The effective window is the minimum of both peers' values.
 *                Must be in the range 2..128 (8-bit sequence numbers).
 */
#ifndef SETU_WINDOW_DEFAULT
#define SETU_WINDOW_DEFAULT                   (16U)
#endif // SETU_WINDOW_DEFAULT

/**
 * @def           SETU_RX_POOL_DEPTH
 * @brief         Server: frames that can be queued between the BLE RX thread
 *                (DATA write hook) and the engine thread. Should be at least
 *                SETU_WINDOW_DEFAULT + a few. Each entry costs roughly
 *                SETU_MAX_FRAME_LEN + 12 bytes of RAM.
 */
#ifndef SETU_RX_POOL_DEPTH
#define SETU_RX_POOL_DEPTH                    (SETU_WINDOW_DEFAULT + 4U)
#endif // SETU_RX_POOL_DEPTH

/**
 * @def           SETU_CLI_CTRL_POOL_DEPTH
 * @brief         Client: CTRL notifications that can be queued between the BLE
 *                RX thread and the engine thread. Control traffic is about
 *                one frame per window/2 DATA frames, so a few entries suffice.
 *                A dropped ACK is covered by the next one; a dropped NACK or
 *                END by the ACK timeout.
 */
#ifndef SETU_CLI_CTRL_POOL_DEPTH
#define SETU_CLI_CTRL_POOL_DEPTH              (4U)
#endif // SETU_CLI_CTRL_POOL_DEPTH

/**
 * @def           SETU_SRV_NOTIFY_INFLIGHT_MAX
 * @brief         Server: CTRL notifications (control frames and server -> client
 *                short messages) handed to the host but not yet completed.
 */
#ifndef SETU_SRV_NOTIFY_INFLIGHT_MAX
#define SETU_SRV_NOTIFY_INFLIGHT_MAX          (2U)
#endif // SETU_SRV_NOTIFY_INFLIGHT_MAX

/**
 * @def           SETU_CLI_WRITE_INFLIGHT_MAX
 * @brief         Client: Write Without Response PDUs handed to the host but not
 *                yet completed. Together with SETU_SRV_NOTIFY_INFLIGHT_MAX it
 *                must stay below CONFIG_BT_ATT_TX_COUNT, so the host never
 *                blocks on ATT buffer allocation.
 */
#ifndef SETU_CLI_WRITE_INFLIGHT_MAX
#define SETU_CLI_WRITE_INFLIGHT_MAX           (6U)
#endif // SETU_CLI_WRITE_INFLIGHT_MAX

/**
 * @def           SETU_TX_ACK_TIMEOUT_MS
 * @brief         Sender: time without ACK progress after which unacknowledged
 *                frames are retransmitted (Go-Back-N) or START is resent.
 */
#ifndef SETU_TX_ACK_TIMEOUT_MS
#define SETU_TX_ACK_TIMEOUT_MS                (1000U)
#endif // SETU_TX_ACK_TIMEOUT_MS

/**
 * @def           SETU_TX_MAX_RETRIES
 * @brief         Sender: consecutive ACK timeouts before the transfer is
 *                aborted with eBS_TIMEOUT.
 */
#ifndef SETU_TX_MAX_RETRIES
#define SETU_TX_MAX_RETRIES                   (5U)
#endif // SETU_TX_MAX_RETRIES

/**
 * @def           SETU_RX_ACK_DELAY_MS
 * @brief         Receiver: maximum delay before acknowledging received frames
 *                when fewer than window/2 frames have arrived since the last
 *                ACK (e.g. the sender's source is slow).
 */
#ifndef SETU_RX_ACK_DELAY_MS
#define SETU_RX_ACK_DELAY_MS                  (20U)
#endif // SETU_RX_ACK_DELAY_MS

/**
 * @def           SETU_RX_IDLE_TIMEOUT_MS
 * @brief         Receiver: time without any frame of the active transfer
 *                after which the transfer is dropped with eBS_TIMEOUT.
 */
#ifndef SETU_RX_IDLE_TIMEOUT_MS
#define SETU_RX_IDLE_TIMEOUT_MS               (5000U)
#endif // SETU_RX_IDLE_TIMEOUT_MS

/**
 * @def           SETU_CTRL_TX_TIMEOUT_MS
 * @brief         Maximum time to wait for a free credit when sending a
 *                control frame (ACK / NACK / END / ABORT).
 */
#ifndef SETU_CTRL_TX_TIMEOUT_MS
#define SETU_CTRL_TX_TIMEOUT_MS               (200U)
#endif // SETU_CTRL_TX_TIMEOUT_MS

/**
 * @def           SETU_WRITE_RETRY_MS
 * @brief         Client: back-off before retrying when the host reports it is
 *                temporarily out of buffers (-ENOMEM / -EAGAIN).
 */
#ifndef SETU_WRITE_RETRY_MS
#define SETU_WRITE_RETRY_MS                   (5U)
#endif // SETU_WRITE_RETRY_MS

/**
 * @def           SETU_THREAD_STACK_SIZE
 * @brief         Stack size of the SETU engine thread. Application
 *                callbacks (sink / source / done) run on this stack.
 */
#ifndef SETU_THREAD_STACK_SIZE
#define SETU_THREAD_STACK_SIZE                (2048U)
#endif // SETU_THREAD_STACK_SIZE

/**
 * @def           SETU_THREAD_PRIORITY
 * @brief         Priority of the SETU engine thread (preemptible).
 */
#ifndef SETU_THREAD_PRIORITY
#define SETU_THREAD_PRIORITY                  (5)
#endif // SETU_THREAD_PRIORITY

#if (SETU_ENABLE_SERVER == 0) && (SETU_ENABLE_CLIENT == 0)
#error "SETU: enable at least one of SETU_ENABLE_SERVER / SETU_ENABLE_CLIENT"
#endif

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

#endif // _SETU_CONFIG_H
