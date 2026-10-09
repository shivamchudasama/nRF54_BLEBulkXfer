/**
 * @file          FileSysManager_Config.h
 * @brief         Tunables of the File System Manager. Each takes its Kconfig
 *                value (_DI/Kconfig, menu "File system manager") when the build
 *                defines it, otherwise the default below, so the library also
 *                builds without the application's Kconfig (host tests).
 *
 * @date          07/10/2026
 * @author        Shivam Chudasama [SC]
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef _FILE_SYS_MANAGER_CONFIG_H
#define _FILE_SYS_MANAGER_CONFIG_H

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           FSMGR_STACK_SIZE
 * @brief         Stack size of the File System Manager thread. FATFS calls and
 *                the path buffers of a listing run on it.
 */
#ifndef FSMGR_STACK_SIZE
#ifdef CONFIG_FSMGR_STACK_SIZE
#define FSMGR_STACK_SIZE                     CONFIG_FSMGR_STACK_SIZE
#else
#define FSMGR_STACK_SIZE                     (4096)
#endif // CONFIG_FSMGR_STACK_SIZE
#endif // FSMGR_STACK_SIZE

/**
 * @def           FSMGR_PRIORITY
 * @brief         Priority of the File System Manager thread. Below the BulkXfer
 *                engine (priority 5), so flash writes never delay the link.
 */
#ifndef FSMGR_PRIORITY
#ifdef CONFIG_FSMGR_PRIORITY
#define FSMGR_PRIORITY                       CONFIG_FSMGR_PRIORITY
#else
#define FSMGR_PRIORITY                       (10)
#endif // CONFIG_FSMGR_PRIORITY
#endif // FSMGR_PRIORITY

/**
 * @def           FSMGR_QUEUE_DEPTH
 * @brief         Messages the queue holds. A full queue makes gi_FSMGR_Submit()
 *                wait (or fail, with K_NO_WAIT).
 */
#ifndef FSMGR_QUEUE_DEPTH
#ifdef CONFIG_FSMGR_QUEUE_DEPTH
#define FSMGR_QUEUE_DEPTH                    CONFIG_FSMGR_QUEUE_DEPTH
#else
#define FSMGR_QUEUE_DEPTH                    (8)
#endif // CONFIG_FSMGR_QUEUE_DEPTH
#endif // FSMGR_QUEUE_DEPTH

/**
 * @def           FSMGR_WRITE_BUFFER_SIZE
 * @brief         Size of the RAM buffer that collects eFSC_WRITE_DATA payloads
 *                into larger fs_write() calls (CONFIG_FSMGR_BUFFERED_WRITE). 0
 *                writes each payload directly.
 */
#ifndef FSMGR_WRITE_BUFFER_SIZE
#if defined(CONFIG_FSMGR_BUFFERED_WRITE) && defined(CONFIG_FSMGR_WRITE_BUFFER_SIZE)
#define FSMGR_WRITE_BUFFER_SIZE              CONFIG_FSMGR_WRITE_BUFFER_SIZE
#else
#define FSMGR_WRITE_BUFFER_SIZE              (0)
#endif // CONFIG_FSMGR_BUFFERED_WRITE
#endif // FSMGR_WRITE_BUFFER_SIZE

/**
 * @def           FSMGR_MAX_PATH_LEN
 * @brief         Size of a path buffer, terminator included. A path that does
 *                not fit fails with -ENAMETOOLONG.
 */
#ifndef FSMGR_MAX_PATH_LEN
#define FSMGR_MAX_PATH_LEN                   (256U)
#endif // FSMGR_MAX_PATH_LEN

#endif //!_FILE_SYS_MANAGER_CONFIG_H
