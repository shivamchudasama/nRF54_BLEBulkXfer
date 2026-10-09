/**
 * @file          FileSysManager.h
 * @brief         File System Manager: mounts the FAT volume on the external
 *                flash (formatting it if it cannot be mounted) and executes file
 *                system commands, one at a time, on its own thread. Users submit
 *                FileSysMessage_T commands and get the results through a callback
 *                (gi_FSMGR_Submit), or wait for them (gi_FSMGR_Call).
 *
 *                Ported from the FileSystemPoC (Sample Code FS). See
 *                _DOC/FileSysManager/API_REFERENCE.md for the full contract.
 *
 * @date          07/10/2026
 * @author        Yash Sunil Giramkar [YSG], Shivam Chudasama [SC]
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef _FILE_SYS_MANAGER_H
#define _FILE_SYS_MANAGER_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <zephyr/kernel.h>
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
extern int gi_FSMGR_Start(void);
extern int gi_FSMGR_Submit(const FileSysMessage_T *stpt_msg, FsmgrResult_F fpt_onResult,
   void *vpt_user, k_timeout_t t_timeout);
extern int gi_FSMGR_Call(const FileSysMessage_T *stpt_msg);
extern bool gb_FSMGR_IsMounted(void);
extern bool gb_FSMGR_IsFileOpen(void);
extern uint8_t gu8_FSMGR_StatusCode(int i_status);

#endif //!_FILE_SYS_MANAGER_H
