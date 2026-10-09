/**
 * @file          FileSysManager.c
 * @brief         Source file for File System Manager module which initializes
 *                the file system and runs the FSM.
 * @date          16/02/26
 * @author        Yash Sunil Giramkar [YSG]
 * @copyright     Copyright(c) Yash Sunil Giramkar (YSG) as an unpublished work.
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "FileSysManager.h"
#include "FileSysManagerFSM.h"
#include <ff.h>

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           FSMGR_THREAD_STACK_SIZE
 * @brief         Stack size for the file system manager thread
 */
#define FSMGR_THREAD_STACK_SIZE              4096

/**
 * @def           FSMGR_THREAD_PRIORITY
 * @brief         Priority for the file system manager thread
 */
#define FSMGR_THREAD_PRIORITY                5

/**
 * @def           FSMGR_MSG_Q for messages between FOTA FSM and File System Manager
 * @brief         This message queue is used for communication between the FOTA
 *                FSM and the File System Manager.
 */
K_MSGQ_DEFINE(FSMGR_MSG_Q, sizeof(FileSysMessage_T), 25, 4);

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
// Definition of all the enums
/**
 * @enum          <Enum name>
 * @brief         <Enum details>.
 */

// Declarations of all the enum variables
/**
 * @var           <Variable name>
 * @brief         <Variable details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
// Definition of all the structures
/**
 * @struct        <Structure name>
 * @brief         <Structure details>.
 */
/******************************************************************************/
/*                                                                            */
/*                                   UNIONS                                   */
/*                                                                            */
/******************************************************************************/
// Definition of all the unions
/**
 * @union         <Union name>
 * @brief         <Union details>.
 */

// Declarations of all the union variables
/**
 * @var           <Variable name>
 * @brief         <Variable details>.
 */

/******************************************************************************/
/*                                                                            */
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/*                                                                            */
/******************************************************************************/
static int si_FileSystemMount(void);

/******************************************************************************/
/*                                                                            */
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                              PUBLIC VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           <Variable name>
 * @brief         <Variable details>.
 */
uint8_t gu8_consumedBuff = 0;

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           st_FatFs
 * @brief         The FATFS structure for the file system.
 */
static FATFS st_FatFs;

/**
 * @var           sst_mountInfo
 * @brief         The mount information structure for the file system.
 */
static struct fs_mount_t sst_mountInfo = {
    .type = FS_FATFS,
    .fs_data = &st_FatFs,
    .storage_dev = (void *)FAT_DRIVE_NAME,
    .mnt_point = FAT_MOUNT_POINT
};

/**
 * @var           sst_FSMGRContext
 * @brief         Variable to hold the context of the File System Manager FSM.
 */
static FileSysManagerCTX_T sst_FSMGRContext;

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                        PRIVATE FUNCTION DEFINITIONS                        */
/*                                                                            */
/******************************************************************************/
/**
 * @private       si_FileSystemMount
 * @brief         Function to mount the file system, and format it if mounting fails.
 * @param[in]     None
 * @param[out]    None
 * @param[inout]  None
 * @return        0 on success, negative error code on failure.
 */
static int si_FileSystemMount(void)
{
   // Variable to hold return codes from file system operations
   int i_RetCode;

   // Attempt to mount the file system
   i_RetCode = fs_mount(&sst_mountInfo);

      // If mounting fails, attempt to format the drive and mount again
   if (i_RetCode < 0)
   {
      LOG_INF("Mount failed (%d), formatting...", i_RetCode);

      // Format the drive with FATFS
      i_RetCode = fs_mkfs(FS_FATFS, (uintptr_t)(FAT_DRIVE_NAME ":"), NULL, 0);
      // If formatting fails, log the error and exit
      if (i_RetCode < 0)
      {
         LOG_ERR("Format failed (%d)", i_RetCode);
         return i_RetCode;
      }

      // Try mounting again after formatting
      i_RetCode = fs_mount(&sst_mountInfo);
      // If mounting still fails after formatting, log the error and exit
      if (i_RetCode < 0)
      {
         LOG_ERR("Mount failed after format (%d)", i_RetCode);
         return i_RetCode;
      }
   }
   return i_RetCode;
}

/**
 * @private       sv_FSMGR_Thread
 * @brief         The thread function for the File System Manager FSM.
 *                It mounts the file system and then continuously processes
 *                incoming messages from the message queue.
 * @param[in]     vptr1 - Unused parameter for thread entry function.
 * @param[out]    None
 * @param[inout]  None
 * @return        0 on success, negative error code on failure.
 */
static void sv_FSMGR_Thread(void *vptr1, void *vptr2, void *vptr3)
{
   // Variable to hold return codes from file system operations
   int i_RetCode;

   // Mount the file system
   i_RetCode = si_FileSystemMount();
   if(i_RetCode < 0)
   {
      // Log that the FATFS has been successfully mounted
      LOG_ERR("FATFS Mounting Failed!");
   }
   else
   {
      // Log that the FATFS has been successfully mounted
      LOG_INF("FATFS Mounted!");
   }

   // Initialize the file system manager FSM
   gv_FileSysManagerFSMInit(&sst_FSMGRContext);

   while (1)
   {
      gu8_consumedBuff++;
      k_msgq_get(&FSMGR_MSG_Q, &sst_FSMGRContext.st_currentMsg, K_FOREVER);
      gv_FileSysManagerFSMRun(&sst_FSMGRContext);
   }
}

/**
 * @def           sv_FSMGR_Thread registeration
 * @brief         This macro registers the File System Manager thread with the
 *                Zephyr kernel.
 */
K_THREAD_DEFINE(fs_tid, FSMGR_THREAD_STACK_SIZE,
                sv_FSMGR_Thread, NULL, NULL, NULL,
                FSMGR_THREAD_PRIORITY, 0, 0);




/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gstpt_FSMGR_GetMsgQ
 * @brief         Returns a pointer to the File System Manager message queue.
 * @param[in]     None
 * @param[out]    None
 * @param[inout]  None
 * @return        Pointer to the File System Manager message queue.
 */
struct k_msgq *gstpt_FSMGR_GetMsgQ(void)
{
    return &FSMGR_MSG_Q;
}

/**
 * Copyright(c) Yash Sunil Giramkar (YSG) as an unpublished work.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION IS ALLOWED ONLY IN ACCORDANCE WITH
 * THE TERMS OF THE LICENSE
 *
 * @author:Yash Sunil Giramkar [YSG]
 */
