/**
 * @file          FileSysManager.c
 * @brief         File System Manager: mounts the FAT volume (formatting it if
 *                mounting fails), then executes the queued messages one by one
 *                through the FSM on its own thread.
 *
 *                Ported from the FileSystemPoC (Sample Code FS). Changes are
 *                listed in _DOC/FileSysManager/README.md.
 *
 * @date          07/10/2026
 * @author        Yash Sunil Giramkar [YSG], Shivam Chudasama [SC]
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <errno.h>
#include <string.h>
#include <zephyr/fs/fs.h>
#include <zephyr/sys/atomic.h>
#include <ff.h>
#include "FileSysManager.h"
#include "FileSysManagerFSM.h"
#include "AppLog.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           FSMGR_FLASH_IF_NAME
 * @brief         Name of the external flash interface, for the mount log line.
 */
#if defined(CONFIG_FSMGR_FLASH_IF_SQSPI)
#define FSMGR_FLASH_IF_NAME                  "sQSPI"
#else
#define FSMGR_FLASH_IF_NAME                  "SPI"
#endif // CONFIG_FSMGR_FLASH_IF_SQSPI

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
 * @struct        FsmgrCall_T
 * @brief         What gi_FSMGR_Call() waits on: the final result's status.
 */
typedef struct
{
   struct k_sem st_done;                     /**< Given by the final result.           */
   int i_status;                             /**< Status of the final result.          */
} FsmgrCall_T;

/******************************************************************************/
/*                                                                            */
/*                                   UNIONS                                   */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/*                                                                            */
/******************************************************************************/
static int si_FileSystemMount(void);
static bool sb_ProcessNext(k_timeout_t t_timeout);
static void sv_CallDone(const FsmgrResult_T *stpt_result, void *vpt_user);
static void sv_FSMGR_Thread(void *vpt_p1, void *vpt_p2, void *vpt_p3);

/******************************************************************************/
/*                                                                            */
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           sst_fatFs
 * @brief         FATFS work area of the volume.
 */
static FATFS sst_fatFs;

/**
 * @var           sst_mountInfo
 * @brief         Mount description of the volume.
 */
static struct fs_mount_t sst_mountInfo =
{
   .type = FS_FATFS,
   .fs_data = &sst_fatFs,
   .storage_dev = (void *)FSMGR_DRIVE_NAME,
   .mnt_point = FSMGR_MOUNT_POINT,
};

/**
 * @var           sst_FSMGRContext
 * @brief         Context of the File System Manager FSM.
 */
static FileSysManagerCTX_T sst_FSMGRContext;

/**
 * @var           sb_mounted
 * @brief         The volume is mounted. Until then (or if mounting failed) every
 *                command fails with -ENODEV.
 */
static volatile bool sb_mounted = false;

/**
 * @var           st_started
 * @brief         gi_FSMGR_Start() has started the thread.
 */
static atomic_t st_started = ATOMIC_INIT(0);

/**
 * @var           sst_fsmgrMsgQ
 * @brief         Messages waiting for the File System Manager thread.
 */
K_MSGQ_DEFINE(sst_fsmgrMsgQ, sizeof(FileSysMessage_T), FSMGR_QUEUE_DEPTH, 4);

/**
 * @var           sst_fsmgrThread
 * @brief         The File System Manager thread, started by gi_FSMGR_Start().
 */
K_THREAD_DEFINE(sst_fsmgrThread, FSMGR_STACK_SIZE, sv_FSMGR_Thread, NULL, NULL, NULL,
   FSMGR_PRIORITY, 0, SYS_FOREVER_MS);

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
 * @brief         Mount the volume; if that fails, format it and mount again.
 * @return        0 on success, otherwise the error of the last step.
 */
static int si_FileSystemMount(void)
{
   int i_retCode;

   i_retCode = fs_mount(&sst_mountInfo);

   // Check if mounting failed: format the drive and mount again
   if (i_retCode < 0)
   {
      LOG_INF("Mount failed (%d), formatting...", i_retCode);

      i_retCode = fs_mkfs(FS_FATFS, (uintptr_t)(FSMGR_DRIVE_NAME ":"), NULL, 0);
      if (i_retCode < 0)
      {
         LOG_ERR("Format failed (%d)", i_retCode);
         return i_retCode;
      }

      i_retCode = fs_mount(&sst_mountInfo);
      if (i_retCode < 0)
      {
         LOG_ERR("Mount failed after format (%d)", i_retCode);
         return i_retCode;
      }
   }

   return i_retCode;
}

/**
 * @private       sb_ProcessNext
 * @brief         Execute the next queued message: through the FSM, or with
 *                -ENODEV while the volume is not mounted.
 * @param[in]     t_timeout How long to wait for a message.
 * @return        true if a message was executed.
 */
static bool sb_ProcessNext(k_timeout_t t_timeout)
{
   // Check if a message arrived
   if (k_msgq_get(&sst_fsmgrMsgQ, &sst_FSMGRContext.st_currentMsg, t_timeout) != 0)
   {
      return false;
   }

   if (sb_mounted)
   {
      gv_FileSysManagerFSMRun(&sst_FSMGRContext);
   }
   else
   {
      gv_FileSysManagerFSMReport(&sst_FSMGRContext, -ENODEV, NULL, 0U);
   }

   return true;
}

/**
 * @private       sv_CallDone
 * @brief         FsmgrResult_F of gi_FSMGR_Call(): keep the final status and wake
 *                the caller.
 * @param[in]     stpt_result Result.
 * @param[in]     vpt_user The caller's FsmgrCall_T.
 * @return        None.
 */
static void sv_CallDone(const FsmgrResult_T *stpt_result, void *vpt_user)
{
   FsmgrCall_T *stpt_call = (FsmgrCall_T *)vpt_user;

   // Check if this is the final result (listing entries are not)
   if (stpt_result->b_final)
   {
      stpt_call->i_status = stpt_result->i_status;
      k_sem_give(&stpt_call->st_done);
   }
}

/**
 * @private       sv_FSMGR_Thread
 * @brief         Mount the volume, then execute queued messages for ever.
 * @param[in]     vpt_p1 Unused.
 * @param[in]     vpt_p2 Unused.
 * @param[in]     vpt_p3 Unused.
 * @return        None.
 */
static void sv_FSMGR_Thread(void *vpt_p1, void *vpt_p2, void *vpt_p3)
{
   ARG_UNUSED(vpt_p1);
   ARG_UNUSED(vpt_p2);
   ARG_UNUSED(vpt_p3);

   // Check if the volume can be mounted
   if (si_FileSystemMount() < 0)
   {
      LOG_ERR("FATFS Mounting Failed!");
   }
   else
   {
      sb_mounted = true;
      LOG_INF("FATFS Mounted! %s on external flash (%s)", FSMGR_MOUNT_POINT, FSMGR_FLASH_IF_NAME);
   }

   gv_FileSysManagerFSMInit(&sst_FSMGRContext);

   while (true)
   {
      (void)sb_ProcessNext(K_FOREVER);
   }
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gi_FSMGR_Start
 * @brief         Start the File System Manager thread, which mounts the volume.
 *                Messages may be submitted before; they wait in the queue.
 * @return        0, or -EALREADY if it was started before.
 */
int gi_FSMGR_Start(void)
{
   // Check if the thread runs already
   if (!atomic_cas(&st_started, 0, 1))
   {
      return -EALREADY;
   }

   k_thread_start(sst_fsmgrThread);

   return 0;
}

/**
 * @public        gi_FSMGR_Submit
 * @brief         Queue a copy of a message. Its results go to fpt_onResult on
 *                the File System Manager thread: listing entries, then exactly
 *                one final result.
 * @param[in]     stpt_msg Message (e_command, u8_flags, u32_sizeOfData, u8_data).
 * @param[in]     fpt_onResult Result callback, or NULL.
 * @param[in]     vpt_user Passed to fpt_onResult.
 * @param[in]     t_timeout How long to wait for room in the queue.
 * @return        0, -EINVAL (no message, unknown command, payload too long),
 *                or k_msgq_put()'s error when the queue stays full.
 */
int gi_FSMGR_Submit(const FileSysMessage_T *stpt_msg, FsmgrResult_F fpt_onResult,
   void *vpt_user, k_timeout_t t_timeout)
{
   FileSysMessage_T st_msg;

   // Check if the message is valid
   if ((stpt_msg == NULL) || ((uint32_t)stpt_msg->e_command >= (uint32_t)eFSC_COUNT))
   {
      return -EINVAL;
   }

   // A read gives the wanted byte count, the others a payload that must fit
   if ((stpt_msg->e_command != eFSC_READ_FILE) && (stpt_msg->u32_sizeOfData > FS_MAX_CHUNK_SIZE))
   {
      return -EINVAL;
   }

   st_msg = *stpt_msg;
   st_msg.fpt_onResult = fpt_onResult;
   st_msg.vpt_user = vpt_user;

   return k_msgq_put(&sst_fsmgrMsgQ, &st_msg, t_timeout);
}

/**
 * @public        gi_FSMGR_Call
 * @brief         Submit a message and wait for its final result. Listing
 *                entries and data are not returned. Never call it from the File
 *                System Manager thread itself (a result callback): it would wait
 *                for ever.
 * @param[in]     stpt_msg Message.
 * @return        The final result's status (0 or a negative errno), or
 *                gi_FSMGR_Submit()'s error.
 */
int gi_FSMGR_Call(const FileSysMessage_T *stpt_msg)
{
   FsmgrCall_T st_call;
   int i_ret;

   k_sem_init(&st_call.st_done, 0, 1);
   st_call.i_status = -EIO;

   i_ret = gi_FSMGR_Submit(stpt_msg, sv_CallDone, &st_call, K_FOREVER);
   if (i_ret == 0)
   {
      (void)k_sem_take(&st_call.st_done, K_FOREVER);
      i_ret = st_call.i_status;
   }

   return i_ret;
}

/**
 * @public        gb_FSMGR_IsMounted
 * @brief         Whether the volume is mounted.
 * @return        true once mounted.
 */
bool gb_FSMGR_IsMounted(void)
{
   return sb_mounted;
}

/**
 * @public        gb_FSMGR_IsFileOpen
 * @brief         Whether the FSM holds an open file (between a successful open
 *                and its close, abort or failure). A snapshot, read from any thread.
 * @return        true while a file is open.
 */
bool gb_FSMGR_IsFileOpen(void)
{
   return sst_FSMGRContext.b_fileOpenStatus;
}

/**
 * @public        gu8_FSMGR_StatusCode
 * @brief         Map a result's status (0 or a negative errno) to a portable
 *                FsmgrStatus_E code for a peer.
 * @param[in]     i_status Status.
 * @return        FsmgrStatus_E value.
 */
uint8_t gu8_FSMGR_StatusCode(int i_status)
{
   FsmgrStatus_E e_code;

   switch (i_status)
   {
      case 0:              e_code = eFSS_OK;            break;
      case -ENOENT:        e_code = eFSS_NOT_FOUND;     break;
      case -EEXIST:        e_code = eFSS_EXISTS;        break;
      case -ENOTEMPTY:     e_code = eFSS_NOT_EMPTY;     break;
      case -ENOSPC:        e_code = eFSS_NO_SPACE;      break;
      case -EINVAL:
      case -ENAMETOOLONG:  e_code = eFSS_BAD_ARG;       break;
      case -EPERM:
      case -EBADF:         e_code = eFSS_BAD_STATE;     break;
      case -EBUSY:
      case -ENOMSG:
      case -EAGAIN:        e_code = eFSS_BUSY;          break;
      case -ENODEV:        e_code = eFSS_NOT_MOUNTED;   break;
      case -ENOTSUP:       e_code = eFSS_NOT_SUPPORTED; break;
      case -ENOTDIR:
      case -EISDIR:        e_code = eFSS_WRONG_TYPE;    break;
      case -EACCES:        e_code = eFSS_DENIED;        break;
      default:             e_code = eFSS_IO;            break;
   }

   return (uint8_t)e_code;
}
