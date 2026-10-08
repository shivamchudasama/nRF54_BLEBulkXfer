/**
 * @file          FileSysManagerFSM.h
 * @brief         Header file for File System Manager FSM
 * @date          20/02/26
 * @author        Yash Sunil Giramkar [YSG]
 * @copyright     Copyright(c) Yash Sunil Giramkar (YSG) as an unpublished work.
 */

#ifndef FS_MGR_FSM_H
#define FS_MGR_FSM_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/smf.h>
#include <zephyr/fs/fs.h>
#include "AppLog.h"
#include "TransferMsgTypes.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           <Define name>
 * @brief         <Define details>.
 */
// FATFS Drive Name
#define FAT_DRIVE_NAME                       "FLASH_DISK"
// Mount point for FATFS
#define FAT_MOUNT_POINT                      "/" FAT_DRIVE_NAME ":"
/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          <Enum name>
 * @brief         <Enum details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        <Structure name>
 * @brief         <Structure details>.
 */
typedef struct{
   struct smf_ctx smf;                       /** Mandatory field for Zephyr SMF */

   /* Custom Data begins here*/
   struct fs_file_t file;                    /** File Pointer and information */
   FileSysMessage_T st_currentMsg;           /** Current Command and Data */
   uint32_t u32_byteWritten;                 /** Data bytes received so far*/
   uint32_t u32_totalExpectedBytes;          /** Total number of bytes expected*/
   bool b_fileOpenStatus;                    /** File Open Status */
   char as8_currentDir[FS_MAX_CHUNK_SIZE];   /** Active directory for relative file paths */
   char as8_activeFile[FS_MAX_CHUNK_SIZE];   /** Last opened file path */
#ifdef CONFIG_FSMGR_BUFFERED_WRITE
   uint8_t au8_writeCache[CONFIG_FSMGR_WRITE_BUFFER_SIZE]; /** Aggregation buffer for writes */
   uint32_t u32_writeCacheFill;               /** Valid bytes currently in write cache */
#endif
}FileSysManagerCTX_T;

/******************************************************************************/
/*                                                                            */
/*                                   UNIONS                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @union         <Union name>
 * @brief         <Union details>.
 */

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
void gv_FileSysManagerFSMInit(FileSysManagerCTX_T *ctx);
void gv_FileSysManagerFSMRun(FileSysManagerCTX_T *ctx);

#endif //!FS_MGR_FSM_H

/**
 * Copyright(c) Yash Sunil Giramkar (YSG) as an unpublished work.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION IS ALLOWED ONLY IN ACCORDANCE WITH
 * THE TERMS OF THE LICENSE
 *
 * @author:Yash Sunil Giramkar [YSG]
 */
