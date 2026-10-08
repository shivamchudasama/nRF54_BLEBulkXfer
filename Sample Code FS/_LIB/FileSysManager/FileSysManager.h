/**
 * @file          FileSysManager.h
 * @brief         Header file for File System Manager module
 * @date          16/02/26
 * @author        Yash Sunil Giramkar [YSG]
 * @copyright     Copyright(c) Yash Sunil Giramkar (YSG) as an unpublished work.
 */

#ifndef FS_MGR_H
#define FS_MGR_H
/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/sys/printk.h>
#include <errno.h>
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
struct k_msgq *gstpt_FSMGR_GetMsgQ(void);

#endif //!FS_MGR_H

/**
 * Copyright(c) Yash Sunil Giramkar (YSG) as an unpublished work.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION IS ALLOWED ONLY IN ACCORDANCE WITH
 * THE TERMS OF THE LICENSE
 *
 * @author:Yash Sunil Giramkar [YSG]
 */
