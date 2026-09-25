/**
 * @file          DataStore.h
 * @brief         Header file containing the receive-side data store: binds the BulkXfer
 *                Server to a RAM buffer that holds one hex segment and logs it on the
 *                serial terminal once it has been received and CRC-verified (every
 *                byte with CONFIG_DS_HEX_DUMP, otherwise one summary line).
 * @date          25/09/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _DATA_STORE_H
#define _DATA_STORE_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <zephyr/kernel.h>

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           DS_BUF_SIZE
 * @brief         Largest segment (data bytes, without the address header) that one
 *                transfer may carry. The client splits longer segments.
 */
#define DS_BUF_SIZE                          (65536U)

/**
 * @def           DS_ADDR_HDR_LEN
 * @brief         Length of the little-endian start address in front of the segment data.
 */
#define DS_ADDR_HDR_LEN                      (4U)

/**
 * @def           DS_APP_TYPE_SEGMENT
 * @brief         BulkXfer application type of a hex segment transfer (client -> server).
 */
#define DS_APP_TYPE_SEGMENT                  (0x10U)

/**
 * @def           DS_APP_TYPE_RESULT
 * @brief         Short message type of the per-transfer result (server -> client).
 */
#define DS_APP_TYPE_RESULT                   (0x01U)

/**
 * @def           DS_APP_TYPE_STORED
 * @brief         Short message type sent once a segment has been dumped and the buffer
 *                is free again (server -> client).
 */
#define DS_APP_TYPE_STORED                   (0x11U)

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
extern int gi_DataStore_Init(void);

#endif //!_DATA_STORE_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
