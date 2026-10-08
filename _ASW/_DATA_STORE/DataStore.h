/**
 * @file          DataStore.h
 * @brief         Header file containing the receive-side data store: registers the hex
 *                upload appTypes (0x10-0x1F) with the BulkXfer router (BulkRouter.h),
 *                receives into a RAM buffer that holds one hex segment and logs it on the
 *                serial terminal once it has been received and CRC-verified (every
 *                byte with CONFIG_DS_HEX_DUMP, otherwise one summary line). Between
 *                BEGIN and COMMIT, each segment is also stored as a record of one file
 *                on the external flash (_LIB/FileSysManager).
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
#include <stdbool.h>
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

/**
 * @def           DS_APP_TYPE_BEGIN
 * @brief         Short message (client -> server) that starts storing the upload in a
 *                file; its payload is the file name.
 */
#define DS_APP_TYPE_BEGIN                    (0x12U)

/**
 * @def           DS_APP_TYPE_COMMIT
 * @brief         Short message (client -> server) that ends the upload and gives the
 *                file its name.
 */
#define DS_APP_TYPE_COMMIT                   (0x13U)

/**
 * @def           DS_APP_TYPE_FILE
 * @brief         Short message (server -> client) answering BEGIN and COMMIT:
 *                [u8 op][u8 status][u32 LE file size][u32 LE file CRC-32].
 */
#define DS_APP_TYPE_FILE                     (0x14U)

/**
 * @def           DS_APP_TYPE_LAST
 * @brief         Last appType of the hex upload range registered with the router.
 */
#define DS_APP_TYPE_LAST                     (0x1FU)

/**
 * @def           DS_NAME_MAX
 * @brief         Longest file name BEGIN accepts (characters A-Z a-z 0-9 . _ -).
 */
#define DS_NAME_MAX                          (32U)

/**
 * @def           DS_FILE_DIR
 * @brief         Directory of the stored uploads.
 */
#define DS_FILE_DIR                          "/FLASH_DISK:/FW"

/**
 * @def           DS_TEMP_NAME
 * @brief         Name of the file an upload is written to until COMMIT renames it.
 */
#define DS_TEMP_NAME                         "UPLOAD.TMP"

/**
 * @def           DS_RECORD_HDR_LEN
 * @brief         Header of each record in the file: [u32 LE address][u32 LE length].
 */
#define DS_RECORD_HDR_LEN                    (8U)

/**
 * @def           DS_FILE_REPLY_LEN
 * @brief         Payload length of the FILE short message.
 */
#define DS_FILE_REPLY_LEN                    (10U)

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
extern bool gb_DataStore_UploadOpen(void);

#endif //!_DATA_STORE_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
