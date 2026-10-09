/**
 * @file          FsCmd.h
 * @brief         Header file containing the file commands over BLE: the host (GUI
 *                Files page, setu_client.py fs) sends the File System Manager's
 *                commands as SETU short messages (appTypes 0x40-0x4F) and gets
 *                each one's result back. It replaces the FileSystemPoC's UART test
 *                harness. Contract: _DOC/FileSysManager/PROTOCOL.md.
 * @date          07/10/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _FS_CMD_H
#define _FS_CMD_H

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
 * @def           FSCMD_APP_TYPE_CMD
 * @brief         Short message (host -> device): [u8 seq][u8 op][arg].
 */
#define FSCMD_APP_TYPE_CMD                   (0x40U)

/**
 * @def           FSCMD_APP_TYPE_REPLY
 * @brief         Short message (device -> host) ending every command:
 *                [u8 seq][u8 op][u8 status][data].
 */
#define FSCMD_APP_TYPE_REPLY                 (0x41U)

/**
 * @def           FSCMD_APP_TYPE_ENTRY
 * @brief         Short message (device -> host), one per listed entry before the
 *                REPLY of LS: [u8 seq][u8 type][u32 LE size][path].
 */
#define FSCMD_APP_TYPE_ENTRY                 (0x42U)

/**
 * @def           FSCMD_APP_TYPE_LAST
 * @brief         Last appType of the range registered with the router.
 */
#define FSCMD_APP_TYPE_LAST                  (0x4FU)

/**
 * @def           FSCMD_ARG_MAX
 * @brief         Longest argument of a CMD (a path or the data of a write).
 */
#define FSCMD_ARG_MAX                        (240U)

/**
 * @def           FSCMD_READ_MAX
 * @brief         Most bytes one READ returns (what fits a REPLY).
 */
#define FSCMD_READ_MAX                       (239U)

/**
 * @def           FSCMD_ENTRY_PATH_MAX
 * @brief         Longest path an ENTRY carries; a longer one is cut.
 */
#define FSCMD_ENTRY_PATH_MAX                 (236U)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          FsCmdOp_E
 * @brief         Command codes on the wire (the UART harness's commands).
 */
typedef enum
{
   eFSOP_MKDIR   = 1,                        /**< arg: directory path.                 */
   eFSOP_CD      = 2,                        /**< arg: directory path.                 */
   eFSOP_OPENR   = 3,                        /**< arg: file path.                      */
   eFSOP_OPENW   = 4,                        /**< arg: file path.                      */
   eFSOP_WRITE   = 5,                        /**< arg: 1..FSCMD_ARG_MAX data bytes.    */
   eFSOP_READ    = 6,                        /**< arg: none or u16 LE byte count.      */
   eFSOP_LS      = 7,                        /**< no arg.                              */
   eFSOP_DELFILE = 8,                        /**< arg: file path.                      */
   eFSOP_DELDIR  = 9,                        /**< arg: directory path.                 */
   eFSOP_CLOSE   = 10,                       /**< no arg.                              */
   eFSOP_ABORT   = 11,                       /**< no arg.                              */
} FsCmdOp_E;

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
extern int gi_FsCmd_Init(void);

#endif //!_FS_CMD_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
