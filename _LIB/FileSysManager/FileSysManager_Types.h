/**
 * @file          FileSysManager_Types.h
 * @brief         Types shared by the File System Manager and its users: the
 *                commands, the message a user submits, the result it gets back,
 *                and the portable status codes for reporting a result to a peer.
 *
 *                Ported from the FileSystemPoC (Sample Code FS,
 *                _LIB/Common/TransferMsgTypes.h). Added: eFSC_RENAME, the
 *                message flags, the result callback and FsmgrStatus_E.
 *
 * @date          07/10/2026
 * @author        Yash Sunil Giramkar [YSG], Shivam Chudasama [SC]
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef _FILE_SYS_MANAGER_TYPES_H
#define _FILE_SYS_MANAGER_TYPES_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include <stdbool.h>

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           FS_MAX_CHUNK_SIZE
 * @brief         Largest payload of one message: a path, a "old\0new" rename
 *                pair, or the data of one write. Equal to the SETU short
 *                message payload (SETU_MAX_SHORT_PAYLOAD), so one short message
 *                from a peer fits one message.
 */
#define FS_MAX_CHUNK_SIZE                    (242U)

/**
 * @def           FSMGR_DRIVE_NAME
 * @brief         Disk name of the FAT volume: the disk-name of the
 *                zephyr,flash-disk node and CONFIG_FS_FATFS_CUSTOM_MOUNT_POINTS.
 */
#define FSMGR_DRIVE_NAME                     "FLASH_DISK"

/**
 * @def           FSMGR_MOUNT_POINT
 * @brief         Mount point of the FAT volume. Absolute paths start with it.
 */
#define FSMGR_MOUNT_POINT                    "/" FSMGR_DRIVE_NAME ":"

/**
 * @def           FSMGR_MSG_KEEP_DIR
 * @brief         Message flag: eFSC_MAKE_DIR creates the directory without
 *                making it the current directory.
 */
#define FSMGR_MSG_KEEP_DIR                   (0x01U)

/**
 * @def           FSMGR_ENTRY_FILE
 * @brief         FsmgrResult_T.u8_entryType of a file.
 */
#define FSMGR_ENTRY_FILE                     (0U)

/**
 * @def           FSMGR_ENTRY_DIR
 * @brief         FsmgrResult_T.u8_entryType of a directory.
 */
#define FSMGR_ENTRY_DIR                      (1U)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          FileSysCommand_E
 * @brief         Commands executed by the File System Manager FSM. The payload
 *                (FileSysMessage_T.u8_data) of each is given in the API reference.
 */
typedef enum
{
   eFSC_OPEN_DIR,                            /**< Make an existing directory current. */
   eFSC_MAKE_DIR,                            /**< Create a directory (and make it current). */
   eFSC_OPEN_FILE_READ,                      /**< Open a file for reading.             */
   eFSC_OPEN_FILE_WRITE,                     /**< Open or create a file for writing.   */
   eFSC_WRITE_DATA,                          /**< Write to the file open for writing.  */
   eFSC_READ_FILE,                           /**< Read from the file open for reading. */
   eFSC_DEBUG_LIST_DRIVE,                    /**< List the root and one level below.   */
   eFSC_DELETE_FILE,                         /**< Delete a file.                       */
   eFSC_DELETE_DIR,                          /**< Delete a directory and its files.    */
   eFSC_CLOSE_FILE,                          /**< Flush, sync and close the open file. */
   eFSC_ABORT,                               /**< Close without flushing, reset.       */
   eFSC_RENAME,                              /**< Rename "old\0new".                   */
   eFSC_COUNT                                /**< Number of commands (not a command).  */
} FileSysCommand_E;

/**
 * @enum          FsmgrStatus_E
 * @brief         Portable result codes. A result carries a negative errno, whose
 *                values differ between C libraries; gu8_FSMGR_StatusCode() maps it
 *                to one of these for reporting to a peer.
 */
typedef enum
{
   eFSS_OK              = 0,                 /**< Success.                             */
   eFSS_NOT_FOUND       = 1,                 /**< No such file or directory.           */
   eFSS_EXISTS          = 2,                 /**< Already exists.                      */
   eFSS_NOT_EMPTY       = 3,                 /**< Directory not empty.                 */
   eFSS_NO_SPACE        = 4,                 /**< Volume full.                         */
   eFSS_BAD_ARG         = 5,                 /**< Bad path, name or size.              */
   eFSS_BAD_STATE       = 6,                 /**< Command not allowed now (no open file, wrong mode). */
   eFSS_BUSY            = 7,                 /**< Another command or owner holds the file system. */
   eFSS_NOT_MOUNTED     = 8,                 /**< The volume could not be mounted.     */
   eFSS_IO              = 9,                 /**< Any other error.                     */
   eFSS_NOT_SUPPORTED   = 10,                /**< Not supported (deep directory delete). */
   eFSS_WRONG_TYPE      = 11,                /**< A file where a directory was expected, or the reverse. */
   eFSS_DENIED          = 12,                /**< Access denied by the file system.    */
} FsmgrStatus_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        FsmgrResult_T
 * @brief         The result of a command, or one directory entry of a listing.
 *                Valid only during the callback.
 */
typedef struct
{
   FileSysCommand_E e_command;               /**< Command this result belongs to.      */
   int i_status;                             /**< 0 or a negative errno.               */
   bool b_final;                             /**< false: a listing entry, more follow. */
   uint8_t u8_entryType;                     /**< Entry: FSMGR_ENTRY_FILE / _DIR.      */
   uint32_t u32_entrySize;                   /**< Entry: file size in bytes.           */
   const uint8_t *u8pt_data;                 /**< Read data, path, or NULL.            */
   uint32_t u32_len;                         /**< Length of u8pt_data.                 */
   uint32_t u32_total;                       /**< Bytes written so far, or entry count. */
} FsmgrResult_T;

/**
 * @typedef       FsmgrResult_F
 * @brief         Receives the results of a submitted message, on the File System
 *                Manager thread: zero or more listing entries, then exactly one
 *                final result.
 * @param[in]     stpt_result Result (valid only during the call).
 * @param[in]     vpt_user The user pointer given with the message.
 */
typedef void (*FsmgrResult_F)(const FsmgrResult_T *stpt_result, void *vpt_user);

/**
 * @struct        FileSysMessage_T
 * @brief         One command for the File System Manager, copied into its queue.
 */
typedef struct
{
   FileSysCommand_E e_command;               /**< Command.                             */
   uint8_t u8_flags;                         /**< FSMGR_MSG_* flags.                   */
   uint32_t u32_sizeOfData;                  /**< Payload length (read: bytes wanted). */
   uint8_t u8_data[FS_MAX_CHUNK_SIZE];       /**< Path, rename pair or write data.     */
   FsmgrResult_F fpt_onResult;               /**< Set by gi_FSMGR_Submit().            */
   void *vpt_user;                           /**< Set by gi_FSMGR_Submit().            */
} FileSysMessage_T;

#endif //!_FILE_SYS_MANAGER_TYPES_H
