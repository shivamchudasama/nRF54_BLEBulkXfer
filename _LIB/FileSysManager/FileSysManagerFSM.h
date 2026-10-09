/**
 * @file          FileSysManagerFSM.h
 * @brief         State machine of the File System Manager (Zephyr SMF). It runs
 *                on the manager thread only: one context, one open file. Used by
 *                FileSysManager.c; applications use FileSysManager.h instead.
 *
 *                Ported from the FileSystemPoC (Sample Code FS); see
 *                _DOC/FileSysManager/README.md for what changed.
 *
 * @date          07/10/2026
 * @author        Yash Sunil Giramkar [YSG], Shivam Chudasama [SC]
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef _FILE_SYS_MANAGER_FSM_H
#define _FILE_SYS_MANAGER_FSM_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/smf.h>
#include <zephyr/fs/fs.h>
#include "FileSysManager_Types.h"
#include "FileSysManager_Config.h"

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
 * @struct        FileSysManagerCTX_T
 * @brief         Context of the File System Manager FSM.
 */
typedef struct
{
   struct smf_ctx smf;                       /**< Mandatory first field for Zephyr SMF. */

   struct fs_file_t file;                    /**< The open file.                       */
   FileSysMessage_T st_currentMsg;           /**< Message being executed.              */
   uint32_t u32_byteWritten;                 /**< Bytes written to the open file.      */
   bool b_fileOpenStatus;                    /**< A file is open.                      */
   bool b_reported;                          /**< The current message has its final result. */
   char as8_currentDir[FSMGR_MAX_PATH_LEN];  /**< Base of relative file paths.         */
   char as8_activeFile[FSMGR_MAX_PATH_LEN];  /**< Path of the last opened file.        */
#if FSMGR_WRITE_BUFFER_SIZE > 0
   uint8_t au8_writeCache[FSMGR_WRITE_BUFFER_SIZE]; /**< Collects write payloads.    */
   uint32_t u32_writeCacheFill;              /**< Valid bytes in au8_writeCache.       */
#endif // FSMGR_WRITE_BUFFER_SIZE
} FileSysManagerCTX_T;

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
extern void gv_FileSysManagerFSMInit(FileSysManagerCTX_T *stpt_ctx);
extern void gv_FileSysManagerFSMRun(FileSysManagerCTX_T *stpt_ctx);
extern void gv_FileSysManagerFSMReport(FileSysManagerCTX_T *stpt_ctx, int i_status,
   const uint8_t *u8pt_data, uint32_t u32_len);

#endif //!_FILE_SYS_MANAGER_FSM_H
