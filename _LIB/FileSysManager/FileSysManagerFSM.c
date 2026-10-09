/**
 * @file          FileSysManagerFSM.c
 * @brief         File System Manager state machine (Zephyr SMF): directory,
 *                file, read, write, list, delete and rename operations on the FAT
 *                volume, one message at a time, on the manager thread.
 *
 *                Every message ends with exactly one final result through its
 *                callback (a listing sends one result per entry first). A command
 *                that is not allowed in the current state is refused with -EPERM.
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
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include "FileSysManagerFSM.h"
#include "AppLog.h"

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
/**
 * @enum          FsmgrState_E
 * @brief         States of the File System Manager FSM.
 */
typedef enum
{
   eFSM_IDLE,
   eFSM_CREATE_DIR,
   eFSM_CREATE_FILE,
   eFSM_DELETE,
   eFSM_WRITE_FILE,
   eFSM_READ_FILE,
   eFSM_CLOSE,
   eFSM_FAILED,
} FsmgrState_E;

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
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/*                                                                            */
/******************************************************************************/
static void sv_IdleEntry(void *vpt_obj);
static enum smf_state_result se_IdleRun(void *vpt_obj);
static void sv_CreateDirEntry(void *vpt_obj);
static void sv_CreateFileEntry(void *vpt_obj);
static void sv_DeleteEntry(void *vpt_obj);
static enum smf_state_result se_WriteFileRun(void *vpt_obj);
static enum smf_state_result se_ReadFileRun(void *vpt_obj);
static void sv_CloseFileEntry(void *vpt_obj);
static void sv_OpFailedEntry(void *vpt_obj);
static enum smf_state_result se_NoEventRun(void *vpt_obj);

static void sv_SetState(FileSysManagerCTX_T *stpt_ctx, FsmgrState_E e_state);
static uint32_t su32_Accepted(const FileSysManagerCTX_T *stpt_ctx);
static void sv_ReportEntry(FileSysManagerCTX_T *stpt_ctx, uint8_t u8_type, uint32_t u32_size,
   const char *cpt_path);
static void sv_Fail(FileSysManagerCTX_T *stpt_ctx, int i_status);
static void sv_CloseIfOpen(FileSysManagerCTX_T *stpt_ctx);
static void sv_ClearSavedContext(FileSysManagerCTX_T *stpt_ctx);
static int si_BuildPath(const FileSysManagerCTX_T *stpt_ctx, const uint8_t *u8pt_name,
   uint32_t u32_len, bool b_createDirectory, char *cpt_builtPath, size_t s_builtPathMaxSize);
static int si_BuildPathFromMsg(const FileSysManagerCTX_T *stpt_ctx, bool b_createDirectory,
   char *cpt_builtPath, size_t s_builtPathMaxSize);
static int si_FlushWriteCache(FileSysManagerCTX_T *stpt_ctx, bool b_syncFile);
static int si_WriteData(FileSysManagerCTX_T *stpt_ctx, const uint8_t *u8pt_data, uint32_t u32_len);
static int si_ListDrive(FileSysManagerCTX_T *stpt_ctx, uint32_t *u32pt_count);
static int si_CheckDIRAndUnlinkFiles(const char *ccpt_dirPath);
static int si_Rename(FileSysManagerCTX_T *stpt_ctx);

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
 * @var           scstar_fsmStates
 * @brief         State table of the File System Manager FSM. The transient
 *                states do their work in the entry action and leave it at once.
 */
static const struct smf_state scstar_fsmStates[] =
{
   [eFSM_IDLE]        = SMF_CREATE_STATE(sv_IdleEntry, se_IdleRun, NULL, NULL, NULL),
   [eFSM_CREATE_DIR]  = SMF_CREATE_STATE(sv_CreateDirEntry, se_NoEventRun, NULL, NULL, NULL),
   [eFSM_CREATE_FILE] = SMF_CREATE_STATE(sv_CreateFileEntry, se_NoEventRun, NULL, NULL, NULL),
   [eFSM_DELETE]      = SMF_CREATE_STATE(sv_DeleteEntry, se_NoEventRun, NULL, NULL, NULL),
   [eFSM_WRITE_FILE]  = SMF_CREATE_STATE(NULL, se_WriteFileRun, NULL, NULL, NULL),
   [eFSM_READ_FILE]   = SMF_CREATE_STATE(NULL, se_ReadFileRun, NULL, NULL, NULL),
   [eFSM_CLOSE]       = SMF_CREATE_STATE(sv_CloseFileEntry, se_NoEventRun, NULL, NULL, NULL),
   [eFSM_FAILED]      = SMF_CREATE_STATE(sv_OpFailedEntry, se_NoEventRun, NULL, NULL, NULL),
};

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
 * @private       sv_SetState
 * @brief         Transition to e_state (runs its entry action at once).
 * @param[in]     stpt_ctx FSM context.
 * @param[in]     e_state New state.
 * @return        None.
 */
static void sv_SetState(FileSysManagerCTX_T *stpt_ctx, FsmgrState_E e_state)
{
   smf_set_state(SMF_CTX(stpt_ctx), &scstar_fsmStates[e_state]);
}

/**
 * @private       su32_Accepted
 * @brief         Bytes written to the open file so far, buffered ones included.
 * @param[in]     stpt_ctx FSM context.
 * @return        Byte count.
 */
static uint32_t su32_Accepted(const FileSysManagerCTX_T *stpt_ctx)
{
#if FSMGR_WRITE_BUFFER_SIZE > 0
   return stpt_ctx->u32_byteWritten + stpt_ctx->u32_writeCacheFill;
#else
   return stpt_ctx->u32_byteWritten;
#endif // FSMGR_WRITE_BUFFER_SIZE
}

/**
 * @private       sv_ReportEntry
 * @brief         Hand one listing entry (not final) to the message's callback.
 * @param[in]     stpt_ctx FSM context.
 * @param[in]     u8_type FSMGR_ENTRY_FILE or FSMGR_ENTRY_DIR.
 * @param[in]     u32_size File size.
 * @param[in]     cpt_path Full path of the entry.
 * @return        None.
 */
static void sv_ReportEntry(FileSysManagerCTX_T *stpt_ctx, uint8_t u8_type, uint32_t u32_size,
   const char *cpt_path)
{
   FsmgrResult_T st_res = { 0 };

   // Check if anyone wants the results of this message
   if (stpt_ctx->st_currentMsg.fpt_onResult != NULL)
   {
      st_res.e_command = stpt_ctx->st_currentMsg.e_command;
      st_res.b_final = false;
      st_res.u8_entryType = u8_type;
      st_res.u32_entrySize = u32_size;
      st_res.u8pt_data = (const uint8_t *)cpt_path;
      st_res.u32_len = (uint32_t)strlen(cpt_path);
      stpt_ctx->st_currentMsg.fpt_onResult(&st_res, stpt_ctx->st_currentMsg.vpt_user);
   }
}

/**
 * @private       sv_Fail
 * @brief         Report i_status as the final result and go to FAILED, which
 *                closes the file and returns to IDLE.
 * @param[in]     stpt_ctx FSM context.
 * @param[in]     i_status Negative errno.
 * @return        None.
 */
static void sv_Fail(FileSysManagerCTX_T *stpt_ctx, int i_status)
{
   gv_FileSysManagerFSMReport(stpt_ctx, i_status, NULL, 0U);
   sv_SetState(stpt_ctx, eFSM_FAILED);
}

/**
 * @private       sv_CloseIfOpen
 * @brief         Closes the file if it is open.
 * @param[in]     stpt_ctx FSM context.
 * @return        None.
 */
static void sv_CloseIfOpen(FileSysManagerCTX_T *stpt_ctx)
{
   // Check if the file is currently open
   if (stpt_ctx->b_fileOpenStatus)
   {
      // Close the file and update the open status
      (void)fs_close(&stpt_ctx->file);
      stpt_ctx->b_fileOpenStatus = false;
   }
}

/**
 * @private       sv_ClearSavedContext
 * @brief         Forget the file and the write buffer, and make the root the
 *                current directory again.
 * @param[in]     stpt_ctx FSM context.
 * @return        None.
 */
static void sv_ClearSavedContext(FileSysManagerCTX_T *stpt_ctx)
{
   stpt_ctx->b_fileOpenStatus = false;
   stpt_ctx->u32_byteWritten = 0U;
   (void)strncpy(stpt_ctx->as8_currentDir, FSMGR_MOUNT_POINT, sizeof(stpt_ctx->as8_currentDir) - 1U);
   stpt_ctx->as8_currentDir[sizeof(stpt_ctx->as8_currentDir) - 1U] = '\0';
   stpt_ctx->as8_activeFile[0] = '\0';
#if FSMGR_WRITE_BUFFER_SIZE > 0
   stpt_ctx->u32_writeCacheFill = 0U;
#endif // FSMGR_WRITE_BUFFER_SIZE
}

/**
 * @private       si_BuildPath
 * @brief         Build an absolute path from a name. "/DRIVE:/..." is used as is,
 *                "/x" is taken from the root, and a relative name from the root
 *                (directories) or the current directory (files).
 * @param[in]     stpt_ctx FSM context.
 * @param[in]     u8pt_name Name bytes (not terminated).
 * @param[in]     u32_len Name length.
 * @param[in]     b_createDirectory Whether the name is a directory to create or open.
 * @param[out]    cpt_builtPath Built path.
 * @param[in]     s_builtPathMaxSize Size of cpt_builtPath.
 * @return        0, -EINVAL (empty name or a NUL inside) or -ENAMETOOLONG.
 */
static int si_BuildPath(const FileSysManagerCTX_T *stpt_ctx, const uint8_t *u8pt_name,
   uint32_t u32_len, bool b_createDirectory, char *cpt_builtPath, size_t s_builtPathMaxSize)
{
   char c_tempCharString[FSMGR_MAX_PATH_LEN];
   const char *ccpt_nameStart;
   const char *ccpt_basePath;
   bool b_isAbsoluteFsPath;
   int i_retVal;

   // Check if the name is empty or holds a terminator
   if ((u32_len == 0U) || (memchr(u8pt_name, '\0', u32_len) != NULL))
   {
      return -EINVAL;
   }

   // Check if the name fits the temporary buffer (it used to be cut short)
   if (u32_len >= sizeof(c_tempCharString))
   {
      return -ENAMETOOLONG;
   }

   memcpy(c_tempCharString, u8pt_name, u32_len);
   c_tempCharString[u32_len] = '\0';

   // Check if the path is an absolute path (starts with '/' and contains ':')
   b_isAbsoluteFsPath = ((c_tempCharString[0] == '/') && (strchr(c_tempCharString, ':') != NULL));
   if (b_isAbsoluteFsPath)
   {
      i_retVal = snprintf(cpt_builtPath, s_builtPathMaxSize, "%s", c_tempCharString);
      return ((i_retVal > 0) && ((size_t)i_retVal < s_builtPathMaxSize)) ? 0 : -ENAMETOOLONG;
   }

   // Check if the path starts with '/': it is relative to the mount point
   if (c_tempCharString[0] == '/')
   {
      ccpt_nameStart = &c_tempCharString[1];
      ccpt_basePath = FSMGR_MOUNT_POINT;
   }
   else
   {
      // Directories are created or opened from the root, files from the current directory
      ccpt_nameStart = c_tempCharString;
      ccpt_basePath = b_createDirectory ? FSMGR_MOUNT_POINT : stpt_ctx->as8_currentDir;
   }

   // Check if the name part of the path is empty
   if (ccpt_nameStart[0] == '\0')
   {
      return -EINVAL;
   }

   i_retVal = snprintf(cpt_builtPath, s_builtPathMaxSize, "%s/%s", ccpt_basePath, ccpt_nameStart);

   return ((i_retVal > 0) && ((size_t)i_retVal < s_builtPathMaxSize)) ? 0 : -ENAMETOOLONG;
}

/**
 * @private       si_BuildPathFromMsg
 * @brief         si_BuildPath() on the current message's payload.
 * @param[in]     stpt_ctx FSM context.
 * @param[in]     b_createDirectory Whether the name is a directory to create or open.
 * @param[out]    cpt_builtPath Built path.
 * @param[in]     s_builtPathMaxSize Size of cpt_builtPath.
 * @return        As si_BuildPath().
 */
static int si_BuildPathFromMsg(const FileSysManagerCTX_T *stpt_ctx, bool b_createDirectory,
   char *cpt_builtPath, size_t s_builtPathMaxSize)
{
   return si_BuildPath(stpt_ctx, stpt_ctx->st_currentMsg.u8_data,
      stpt_ctx->st_currentMsg.u32_sizeOfData, b_createDirectory, cpt_builtPath,
      s_builtPathMaxSize);
}

/**
 * @private       si_FlushWriteCache
 * @brief         Write out the buffered data and optionally sync the file.
 * @param[in]     stpt_ctx FSM context.
 * @param[in]     b_syncFile true to call fs_sync() after flushing.
 * @return        0 or a negative errno (-ENOSPC for a short write).
 */
static int si_FlushWriteCache(FileSysManagerCTX_T *stpt_ctx, bool b_syncFile)
{
   int i_retVal;

#if FSMGR_WRITE_BUFFER_SIZE > 0
   // Check if there is buffered data to write
   if (stpt_ctx->u32_writeCacheFill > 0U)
   {
      i_retVal = (int)fs_write(&stpt_ctx->file, stpt_ctx->au8_writeCache,
         stpt_ctx->u32_writeCacheFill);
      if (i_retVal < 0)
      {
         return i_retVal;
      }
      // A short write means the volume is full
      if ((uint32_t)i_retVal != stpt_ctx->u32_writeCacheFill)
      {
         return -ENOSPC;
      }

      stpt_ctx->u32_byteWritten += stpt_ctx->u32_writeCacheFill;
      stpt_ctx->u32_writeCacheFill = 0U;
   }
#endif // FSMGR_WRITE_BUFFER_SIZE

   if (b_syncFile)
   {
      i_retVal = fs_sync(&stpt_ctx->file);
      if (i_retVal < 0)
      {
         return i_retVal;
      }
   }

   return 0;
}

/**
 * @private       si_WriteData
 * @brief         Write a payload to the open file, through the write buffer when
 *                there is one.
 * @param[in]     stpt_ctx FSM context.
 * @param[in]     u8pt_data Data.
 * @param[in]     u32_len Data length.
 * @return        0 or a negative errno (-ENOSPC for a short write).
 */
static int si_WriteData(FileSysManagerCTX_T *stpt_ctx, const uint8_t *u8pt_data, uint32_t u32_len)
{
   int i_retVal = 0;

#if FSMGR_WRITE_BUFFER_SIZE > 0
   uint32_t u32_copyLen;

   while (u32_len > 0U)
   {
      u32_copyLen = MIN(u32_len, FSMGR_WRITE_BUFFER_SIZE - stpt_ctx->u32_writeCacheFill);
      memcpy(&stpt_ctx->au8_writeCache[stpt_ctx->u32_writeCacheFill], u8pt_data, u32_copyLen);
      stpt_ctx->u32_writeCacheFill += u32_copyLen;
      u8pt_data += u32_copyLen;
      u32_len -= u32_copyLen;

      // Check if the buffer is full: write it out
      if (stpt_ctx->u32_writeCacheFill == FSMGR_WRITE_BUFFER_SIZE)
      {
         i_retVal = si_FlushWriteCache(stpt_ctx, false);
         if (i_retVal < 0)
         {
            break;
         }
      }
   }
#else
   i_retVal = (int)fs_write(&stpt_ctx->file, u8pt_data, u32_len);
   if (i_retVal >= 0)
   {
      // A short write means the volume is full
      if ((uint32_t)i_retVal != u32_len)
      {
         stpt_ctx->u32_byteWritten += (uint32_t)i_retVal;
         i_retVal = -ENOSPC;
      }
      else
      {
         stpt_ctx->u32_byteWritten += u32_len;
         i_retVal = 0;
      }
   }
#endif // FSMGR_WRITE_BUFFER_SIZE

   return i_retVal;
}

/**
 * @private       si_ListDrive
 * @brief         Report and log every entry of the root and, for each directory
 *                there, its direct children. Deeper levels are not listed.
 * @param[in]     stpt_ctx FSM context.
 * @param[out]    u32pt_count Entries reported.
 * @return        0, or the error of opening or reading the root.
 */
static int si_ListDrive(FileSysManagerCTX_T *stpt_ctx, uint32_t *u32pt_count)
{
   struct fs_dir_t st_rootDir;
   struct fs_dir_t st_childDir;
   struct fs_dirent st_rootEntry;
   struct fs_dirent st_childEntry;
   char as8_dirPath[FSMGR_MAX_PATH_LEN];
   char as8_childPath[FSMGR_MAX_PATH_LEN];
   int i_retVal;

   *u32pt_count = 0U;
   LOG_INF("[DBG] Listing drive from: %s", FSMGR_MOUNT_POINT);

   fs_dir_t_init(&st_rootDir);
   i_retVal = fs_opendir(&st_rootDir, FSMGR_MOUNT_POINT);
   if (i_retVal != 0)
   {
      LOG_ERR("[DBG][0] opendir failed (%d): %s", i_retVal, FSMGR_MOUNT_POINT);
      return i_retVal;
   }

   while (true)
   {
      i_retVal = fs_readdir(&st_rootDir, &st_rootEntry);
      if (i_retVal != 0)
      {
         LOG_ERR("[DBG][0] readdir failed (%d): %s", i_retVal, FSMGR_MOUNT_POINT);
         break;
      }

      // Check if the end of the directory is reached
      if (st_rootEntry.name[0] == '\0')
      {
         break;
      }

      if ((size_t)snprintf(as8_dirPath, sizeof(as8_dirPath), "%s/%s", FSMGR_MOUNT_POINT,
         st_rootEntry.name) >= sizeof(as8_dirPath))
      {
         LOG_WRN("[DBG][0] path too long: %s", st_rootEntry.name);
         continue;
      }

      // Check if the entry is a file
      if (st_rootEntry.type != FS_DIR_ENTRY_DIR)
      {
         LOG_INF("[DBG][0][FILE] %s (%u bytes)", as8_dirPath, (uint32_t)st_rootEntry.size);
         sv_ReportEntry(stpt_ctx, FSMGR_ENTRY_FILE, (uint32_t)st_rootEntry.size, as8_dirPath);
         (*u32pt_count)++;
         continue;
      }

      LOG_INF("[DBG][0][DIR] %s", as8_dirPath);
      sv_ReportEntry(stpt_ctx, FSMGR_ENTRY_DIR, 0U, as8_dirPath);
      (*u32pt_count)++;

      fs_dir_t_init(&st_childDir);
      if (fs_opendir(&st_childDir, as8_dirPath) != 0)
      {
         LOG_ERR("[DBG][1] opendir failed: %s", as8_dirPath);
         continue;
      }

      while (fs_readdir(&st_childDir, &st_childEntry) == 0)
      {
         // Check if the end of the directory is reached
         if (st_childEntry.name[0] == '\0')
         {
            break;
         }

         if ((size_t)snprintf(as8_childPath, sizeof(as8_childPath), "%s/%s", as8_dirPath,
            st_childEntry.name) >= sizeof(as8_childPath))
         {
            LOG_WRN("[DBG][1] path too long: %s", st_childEntry.name);
            continue;
         }

         // Check if the child is a directory: listed, but its content is not
         if (st_childEntry.type == FS_DIR_ENTRY_DIR)
         {
            LOG_WRN("[DBG][1][DIR] %s (ignored: depth>1 not supported)", as8_childPath);
            sv_ReportEntry(stpt_ctx, FSMGR_ENTRY_DIR, 0U, as8_childPath);
         }
         else
         {
            LOG_INF("[DBG][1][FILE] %s (%u bytes)", as8_childPath, (uint32_t)st_childEntry.size);
            sv_ReportEntry(stpt_ctx, FSMGR_ENTRY_FILE, (uint32_t)st_childEntry.size,
               as8_childPath);
         }
         (*u32pt_count)++;
      }

      (void)fs_closedir(&st_childDir);
   }

   (void)fs_closedir(&st_rootDir);

   return i_retVal;
}

/**
 * @private       si_CheckDIRAndUnlinkFiles
 * @brief         Delete every file in a directory, so the directory itself can
 *                be deleted.
 * @param[in]     ccpt_dirPath Path of the directory.
 * @return        0, -ENOTSUP if it holds a subdirectory (no recursive delete),
 *                -ENAMETOOLONG, or the error of a file system call.
 */
static int si_CheckDIRAndUnlinkFiles(const char *ccpt_dirPath)
{
   struct fs_dir_t st_dir;
   struct fs_dirent st_entry;
   char as8_entryPath[FSMGR_MAX_PATH_LEN];
   int i_retVal;

   fs_dir_t_init(&st_dir);
   i_retVal = fs_opendir(&st_dir, ccpt_dirPath);
   if (i_retVal != 0)
   {
      return i_retVal;
   }

   while (true)
   {
      i_retVal = fs_readdir(&st_dir, &st_entry);
      if (i_retVal != 0)
      {
         break;
      }

      // Check if the end of the directory is reached
      if (st_entry.name[0] == '\0')
      {
         break;
      }

      // Check if the entry is a directory: recursive deletion is not supported
      if (st_entry.type == FS_DIR_ENTRY_DIR)
      {
         i_retVal = -ENOTSUP;
         break;
      }

      if ((size_t)snprintf(as8_entryPath, sizeof(as8_entryPath), "%s/%s", ccpt_dirPath,
         st_entry.name) >= sizeof(as8_entryPath))
      {
         i_retVal = -ENAMETOOLONG;
         break;
      }

      i_retVal = fs_unlink(as8_entryPath);
      if (i_retVal != 0)
      {
         break;
      }
   }

   (void)fs_closedir(&st_dir);

   return i_retVal;
}

/**
 * @private       si_Rename
 * @brief         Rename the "old\0new" pair in the current message. Both names
 *                are file paths (relative ones from the current directory).
 * @param[in]     stpt_ctx FSM context.
 * @return        0 or a negative errno.
 */
static int si_Rename(FileSysManagerCTX_T *stpt_ctx)
{
   const uint8_t *u8pt_data = stpt_ctx->st_currentMsg.u8_data;
   uint32_t u32_len = stpt_ctx->st_currentMsg.u32_sizeOfData;
   const uint8_t *u8pt_sep;
   uint32_t u32_oldLen;
   char as8_old[FSMGR_MAX_PATH_LEN];
   char as8_new[FSMGR_MAX_PATH_LEN];
   int i_retVal;

   u8pt_sep = (u32_len > 0U) ? memchr(u8pt_data, '\0', u32_len) : NULL;

   // Check if the payload is "old\0new"
   if (u8pt_sep == NULL)
   {
      return -EINVAL;
   }

   u32_oldLen = (uint32_t)(u8pt_sep - u8pt_data);
   i_retVal = si_BuildPath(stpt_ctx, u8pt_data, u32_oldLen, false, as8_old, sizeof(as8_old));
   if (i_retVal == 0)
   {
      i_retVal = si_BuildPath(stpt_ctx, u8pt_sep + 1, u32_len - u32_oldLen - 1U, false, as8_new,
         sizeof(as8_new));
   }

   if (i_retVal == 0)
   {
      i_retVal = fs_rename(as8_old, as8_new);
      if (i_retVal == 0)
      {
         LOG_INF("Renamed %s to %s", as8_old, as8_new);
      }
      else
      {
         LOG_ERR("Rename failed (%d): %s", i_retVal, as8_old);
      }
   }

   return i_retVal;
}

/******************************************************************************/
/*                                                                            */
/*                        PRIVATE FUNCTIONS OF THE FSM                        */
/*                                                                            */
/******************************************************************************/
/**
 * @private       se_NoEventRun
 * @brief         Run action of the transient states, which never stay current.
 * @param[in]     vpt_obj FSM context.
 * @return        SMF_EVENT_HANDLED.
 */
static enum smf_state_result se_NoEventRun(void *vpt_obj)
{
   ARG_UNUSED(vpt_obj);
   return SMF_EVENT_HANDLED;
}

/**
 * @private       sv_IdleEntry
 * @brief         Entry action of IDLE: nothing to do.
 * @param[in]     vpt_obj FSM context.
 * @return        None.
 */
static void sv_IdleEntry(void *vpt_obj)
{
   ARG_UNUSED(vpt_obj);
}

/**
 * @private       se_IdleRun
 * @brief         IDLE: start a directory, open, delete, list or rename operation.
 *                Read and write need an open file.
 * @param[in]     vpt_obj FSM context.
 * @return        SMF_EVENT_HANDLED.
 */
static enum smf_state_result se_IdleRun(void *vpt_obj)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vpt_obj;
   FsmgrResult_T st_res = { 0 };
   uint32_t u32_count = 0U;
   int i_retVal;

   switch (stpt_ctx->st_currentMsg.e_command)
   {
      case eFSC_OPEN_DIR:
      case eFSC_MAKE_DIR:
         sv_SetState(stpt_ctx, eFSM_CREATE_DIR);
         break;

      case eFSC_OPEN_FILE_READ:
      case eFSC_OPEN_FILE_WRITE:
         sv_SetState(stpt_ctx, eFSM_CREATE_FILE);
         break;

      case eFSC_READ_FILE:
         LOG_WRN("Open a file first, then use read");
         gv_FileSysManagerFSMReport(stpt_ctx, -EPERM, NULL, 0U);
         break;

      case eFSC_WRITE_DATA:
         LOG_WRN("Open a file first, then use write");
         gv_FileSysManagerFSMReport(stpt_ctx, -EPERM, NULL, 0U);
         break;

      case eFSC_DEBUG_LIST_DRIVE:
         i_retVal = si_ListDrive(stpt_ctx, &u32_count);
         if (stpt_ctx->st_currentMsg.fpt_onResult != NULL)
         {
            st_res.e_command = eFSC_DEBUG_LIST_DRIVE;
            st_res.i_status = i_retVal;
            st_res.b_final = true;
            st_res.u32_total = u32_count;
            stpt_ctx->st_currentMsg.fpt_onResult(&st_res, stpt_ctx->st_currentMsg.vpt_user);
         }
         stpt_ctx->b_reported = true;
         break;

      case eFSC_DELETE_FILE:
      case eFSC_DELETE_DIR:
         sv_SetState(stpt_ctx, eFSM_DELETE);
         break;

      case eFSC_RENAME:
         gv_FileSysManagerFSMReport(stpt_ctx, si_Rename(stpt_ctx), NULL, 0U);
         break;

      case eFSC_CLOSE_FILE:
         sv_CloseIfOpen(stpt_ctx);
         gv_FileSysManagerFSMReport(stpt_ctx, 0, NULL, 0U);
         break;

      case eFSC_ABORT:
         sv_CloseIfOpen(stpt_ctx);
         sv_ClearSavedContext(stpt_ctx);
         gv_FileSysManagerFSMReport(stpt_ctx, 0, NULL, 0U);
         break;

      default:
         LOG_ERR("Unexpected command %d in IDLE", stpt_ctx->st_currentMsg.e_command);
         gv_FileSysManagerFSMReport(stpt_ctx, -EINVAL, NULL, 0U);
         break;
   }

   return SMF_EVENT_HANDLED;
}

/**
 * @private       sv_CreateDirEntry
 * @brief         CREATE_DIR: create (MAKE_DIR, an existing one is fine) or check
 *                (OPEN_DIR) a directory and make it current, unless the message
 *                has FSMGR_MSG_KEEP_DIR. Reports the directory path.
 * @param[in]     vpt_obj FSM context.
 * @return        None.
 */
static void sv_CreateDirEntry(void *vpt_obj)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vpt_obj;
   struct fs_dirent st_dirEntry;
   char as8_targetDir[FSMGR_MAX_PATH_LEN];
   int i_retVal;

   i_retVal = si_BuildPathFromMsg(stpt_ctx, true, as8_targetDir, sizeof(as8_targetDir));
   if (i_retVal != 0)
   {
      LOG_ERR("Directory path build failed (%d)", i_retVal);
      sv_Fail(stpt_ctx, i_retVal);
      return;
   }

   if (stpt_ctx->st_currentMsg.e_command == eFSC_MAKE_DIR)
   {
      i_retVal = fs_mkdir(as8_targetDir);
      if ((i_retVal != 0) && (i_retVal != -EEXIST))
      {
         LOG_ERR("Directory create failed (%d): %s", i_retVal, as8_targetDir);
         sv_Fail(stpt_ctx, i_retVal);
         return;
      }
   }
   else
   {
      i_retVal = fs_stat(as8_targetDir, &st_dirEntry);
      if ((i_retVal != 0) || (st_dirEntry.type != FS_DIR_ENTRY_DIR))
      {
         LOG_ERR("Directory open failed (%d): %s", i_retVal, as8_targetDir);
         sv_Fail(stpt_ctx, (i_retVal != 0) ? i_retVal : -ENOTDIR);
         return;
      }
   }

   // Check if the directory becomes the current one
   if ((stpt_ctx->st_currentMsg.u8_flags & FSMGR_MSG_KEEP_DIR) == 0U)
   {
      (void)strncpy(stpt_ctx->as8_currentDir, as8_targetDir, sizeof(stpt_ctx->as8_currentDir) - 1U);
      stpt_ctx->as8_currentDir[sizeof(stpt_ctx->as8_currentDir) - 1U] = '\0';
      LOG_INF("Directory active: %s", stpt_ctx->as8_currentDir);
   }

   gv_FileSysManagerFSMReport(stpt_ctx, 0, (const uint8_t *)as8_targetDir,
      (uint32_t)strlen(as8_targetDir));
   sv_SetState(stpt_ctx, eFSM_IDLE);
}

/**
 * @private       sv_CreateFileEntry
 * @brief         CREATE_FILE: open a file for reading, or open or create it for
 *                writing (not truncated), and go to READ_FILE or WRITE_FILE.
 *                Reports the file path.
 * @param[in]     vpt_obj FSM context.
 * @return        None.
 */
static void sv_CreateFileEntry(void *vpt_obj)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vpt_obj;
   fs_mode_t t_fileOpenMode;
   FsmgrState_E e_nextState;
   int i_retVal;

   i_retVal = si_BuildPathFromMsg(stpt_ctx, false, stpt_ctx->as8_activeFile,
      sizeof(stpt_ctx->as8_activeFile));
   if (i_retVal != 0)
   {
      LOG_ERR("Unable to build path (%d)", i_retVal);
      stpt_ctx->as8_activeFile[0] = '\0';
      sv_Fail(stpt_ctx, i_retVal);
      return;
   }

   if (stpt_ctx->st_currentMsg.e_command == eFSC_OPEN_FILE_READ)
   {
      t_fileOpenMode = FS_O_READ;
      e_nextState = eFSM_READ_FILE;
   }
   else
   {
      t_fileOpenMode = FS_O_CREATE | FS_O_WRITE;
      e_nextState = eFSM_WRITE_FILE;
   }

   sv_CloseIfOpen(stpt_ctx);
   fs_file_t_init(&stpt_ctx->file);

   i_retVal = fs_open(&stpt_ctx->file, stpt_ctx->as8_activeFile, t_fileOpenMode);
   if (i_retVal != 0)
   {
      LOG_ERR("File open failed (%d): %s", i_retVal, stpt_ctx->as8_activeFile);
      sv_Fail(stpt_ctx, i_retVal);
      return;
   }

   stpt_ctx->b_fileOpenStatus = true;
   stpt_ctx->u32_byteWritten = 0U;
#if FSMGR_WRITE_BUFFER_SIZE > 0
   stpt_ctx->u32_writeCacheFill = 0U;
#endif // FSMGR_WRITE_BUFFER_SIZE
   LOG_INF("File opened: %s", stpt_ctx->as8_activeFile);

   gv_FileSysManagerFSMReport(stpt_ctx, 0, (const uint8_t *)stpt_ctx->as8_activeFile,
      (uint32_t)strlen(stpt_ctx->as8_activeFile));
   sv_SetState(stpt_ctx, e_nextState);
}

/**
 * @private       sv_DeleteEntry
 * @brief         DELETE: delete a file, or a directory after deleting the files
 *                in it.
 * @param[in]     vpt_obj FSM context.
 * @return        None.
 */
static void sv_DeleteEntry(void *vpt_obj)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vpt_obj;
   bool b_isDir = (stpt_ctx->st_currentMsg.e_command == eFSC_DELETE_DIR);
   struct fs_dirent st_entry;
   char as8_targetPath[FSMGR_MAX_PATH_LEN];
   int i_retVal;

   i_retVal = si_BuildPathFromMsg(stpt_ctx, b_isDir, as8_targetPath, sizeof(as8_targetPath));
   if (i_retVal != 0)
   {
      LOG_ERR("Delete path build failed (%d)", i_retVal);
      sv_Fail(stpt_ctx, i_retVal);
      return;
   }

   i_retVal = fs_stat(as8_targetPath, &st_entry);
   if (i_retVal != 0)
   {
      LOG_ERR("Delete target not found (%d): %s", i_retVal, as8_targetPath);
      sv_Fail(stpt_ctx, i_retVal);
      return;
   }

   // Check if the target has the requested type
   if (!b_isDir && (st_entry.type != FS_DIR_ENTRY_FILE))
   {
      LOG_ERR("Delete file requested for non-file: %s", as8_targetPath);
      sv_Fail(stpt_ctx, -EISDIR);
      return;
   }
   if (b_isDir && (st_entry.type != FS_DIR_ENTRY_DIR))
   {
      LOG_ERR("Delete dir requested for non-dir: %s", as8_targetPath);
      sv_Fail(stpt_ctx, -ENOTDIR);
      return;
   }

   // Check if a directory must be emptied first
   if (b_isDir)
   {
      i_retVal = si_CheckDIRAndUnlinkFiles(as8_targetPath);
      if (i_retVal != 0)
      {
         LOG_ERR("Failed to clear directory contents (%d): %s", i_retVal, as8_targetPath);
         sv_Fail(stpt_ctx, i_retVal);
         return;
      }
      LOG_INF("Directory cleared successfully: %s", as8_targetPath);
   }

   i_retVal = fs_unlink(as8_targetPath);
   if (i_retVal != 0)
   {
      LOG_ERR("Delete failed (%d): %s", i_retVal, as8_targetPath);
      sv_Fail(stpt_ctx, i_retVal);
      return;
   }

   LOG_INF("Delete successful: %s", as8_targetPath);
   gv_FileSysManagerFSMReport(stpt_ctx, 0, NULL, 0U);
   sv_SetState(stpt_ctx, eFSM_IDLE);
}

/**
 * @private       se_WriteFileRun
 * @brief         WRITE_FILE: write data, close, or abort. A failed or empty write
 *                closes the file (FAILED).
 * @param[in]     vpt_obj FSM context.
 * @return        SMF_EVENT_HANDLED.
 */
static enum smf_state_result se_WriteFileRun(void *vpt_obj)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vpt_obj;
   uint32_t u32_size = stpt_ctx->st_currentMsg.u32_sizeOfData;
   int i_retVal;

   switch (stpt_ctx->st_currentMsg.e_command)
   {
      case eFSC_WRITE_DATA:
         // Check if the size is valid
         if ((u32_size == 0U) || (u32_size > FS_MAX_CHUNK_SIZE))
         {
            LOG_ERR("Invalid write size: %u", u32_size);
            sv_Fail(stpt_ctx, -EINVAL);
            break;
         }

         i_retVal = si_WriteData(stpt_ctx, stpt_ctx->st_currentMsg.u8_data, u32_size);
         if (i_retVal < 0)
         {
            LOG_ERR("File write failed (%d)", i_retVal);
            sv_Fail(stpt_ctx, i_retVal);
            break;
         }

         LOG_DBG("Written %u bytes, total %u", u32_size, su32_Accepted(stpt_ctx));
         gv_FileSysManagerFSMReport(stpt_ctx, 0, NULL, 0U);
         break;

      case eFSC_CLOSE_FILE:
         sv_SetState(stpt_ctx, eFSM_CLOSE);
         break;

      case eFSC_ABORT:
         // Buffered data is dropped
         sv_CloseIfOpen(stpt_ctx);
#if FSMGR_WRITE_BUFFER_SIZE > 0
         stpt_ctx->u32_writeCacheFill = 0U;
#endif // FSMGR_WRITE_BUFFER_SIZE
         gv_FileSysManagerFSMReport(stpt_ctx, 0, NULL, 0U);
         sv_SetState(stpt_ctx, eFSM_IDLE);
         break;

      default:
         LOG_ERR("Unexpected command %d in WRITE", stpt_ctx->st_currentMsg.e_command);
         gv_FileSysManagerFSMReport(stpt_ctx, -EPERM, NULL, 0U);
         break;
   }

   return SMF_EVENT_HANDLED;
}

/**
 * @private       se_ReadFileRun
 * @brief         READ_FILE: read up to the requested count (default and maximum
 *                FS_MAX_CHUNK_SIZE; 0 bytes at the end of the file), close, or abort.
 * @param[in]     vpt_obj FSM context.
 * @return        SMF_EVENT_HANDLED.
 */
static enum smf_state_result se_ReadFileRun(void *vpt_obj)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vpt_obj;
   uint8_t au8_readBuf[FS_MAX_CHUNK_SIZE];
   uint32_t u32_lenToRead;
   int i_retVal;

   switch (stpt_ctx->st_currentMsg.e_command)
   {
      case eFSC_READ_FILE:
         u32_lenToRead = stpt_ctx->st_currentMsg.u32_sizeOfData;
         if ((u32_lenToRead == 0U) || (u32_lenToRead > FS_MAX_CHUNK_SIZE))
         {
            u32_lenToRead = FS_MAX_CHUNK_SIZE;
         }

         i_retVal = (int)fs_read(&stpt_ctx->file, au8_readBuf, u32_lenToRead);
         if (i_retVal < 0)
         {
            LOG_ERR("File read failed (%d)", i_retVal);
            sv_Fail(stpt_ctx, i_retVal);
            break;
         }

         if (i_retVal == 0)
         {
            LOG_INF("End of file reached: %s", stpt_ctx->as8_activeFile);
         }
         else
         {
            LOG_HEXDUMP_DBG(au8_readBuf, (uint32_t)i_retVal, "Data content (hex):");
         }
         gv_FileSysManagerFSMReport(stpt_ctx, 0, au8_readBuf, (uint32_t)i_retVal);
         break;

      case eFSC_CLOSE_FILE:
         sv_SetState(stpt_ctx, eFSM_CLOSE);
         break;

      case eFSC_ABORT:
         sv_CloseIfOpen(stpt_ctx);
         gv_FileSysManagerFSMReport(stpt_ctx, 0, NULL, 0U);
         sv_SetState(stpt_ctx, eFSM_IDLE);
         break;

      default:
         LOG_ERR("Unexpected command %d in READ", stpt_ctx->st_currentMsg.e_command);
         gv_FileSysManagerFSMReport(stpt_ctx, -EPERM, NULL, 0U);
         break;
   }

   return SMF_EVENT_HANDLED;
}

/**
 * @private       sv_CloseFileEntry
 * @brief         CLOSE: write out the buffered data, sync and close the file.
 *                Reports the bytes written.
 * @param[in]     vpt_obj FSM context.
 * @return        None.
 */
static void sv_CloseFileEntry(void *vpt_obj)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vpt_obj;
   int i_retVal = 0;

   // Check if the file was open for writing (a read-only file needs no flush)
   if ((stpt_ctx->file.flags & FS_O_WRITE) != 0U)
   {
      i_retVal = si_FlushWriteCache(stpt_ctx, true);
   }
   if (i_retVal < 0)
   {
      LOG_ERR("File flush/sync failed (%d)", i_retVal);
      sv_Fail(stpt_ctx, i_retVal);
      return;
   }

   sv_CloseIfOpen(stpt_ctx);
   LOG_INF("File closed: %s, %u bytes written", stpt_ctx->as8_activeFile, stpt_ctx->u32_byteWritten);
   gv_FileSysManagerFSMReport(stpt_ctx, 0, NULL, 0U);
   sv_SetState(stpt_ctx, eFSM_IDLE);
}

/**
 * @private       sv_OpFailedEntry
 * @brief         FAILED: the error is already reported; close the file, drop
 *                the buffered data and return to IDLE.
 * @param[in]     vpt_obj FSM context.
 * @return        None.
 */
static void sv_OpFailedEntry(void *vpt_obj)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vpt_obj;

   sv_CloseIfOpen(stpt_ctx);
#if FSMGR_WRITE_BUFFER_SIZE > 0
   stpt_ctx->u32_writeCacheFill = 0U;
#endif // FSMGR_WRITE_BUFFER_SIZE
   LOG_ERR("FSM operation failed, returning to IDLE");
   sv_SetState(stpt_ctx, eFSM_IDLE);
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gv_FileSysManagerFSMReport
 * @brief         Hand the final result of the current message to its callback.
 *                u32_total is the bytes written to the open file so far.
 * @param[in]     stpt_ctx FSM context.
 * @param[in]     i_status 0 or a negative errno.
 * @param[in]     u8pt_data Data (read bytes, a path) or NULL.
 * @param[in]     u32_len Data length.
 * @return        None.
 */
void gv_FileSysManagerFSMReport(FileSysManagerCTX_T *stpt_ctx, int i_status,
   const uint8_t *u8pt_data, uint32_t u32_len)
{
   FsmgrResult_T st_res = { 0 };

   stpt_ctx->b_reported = true;

   // Check if anyone wants the results of this message
   if (stpt_ctx->st_currentMsg.fpt_onResult != NULL)
   {
      st_res.e_command = stpt_ctx->st_currentMsg.e_command;
      st_res.i_status = i_status;
      st_res.b_final = true;
      st_res.u8pt_data = u8pt_data;
      st_res.u32_len = u32_len;
      st_res.u32_total = su32_Accepted(stpt_ctx);
      stpt_ctx->st_currentMsg.fpt_onResult(&st_res, stpt_ctx->st_currentMsg.vpt_user);
   }
}

/**
 * @public        gv_FileSysManagerFSMInit
 * @brief         Reset the context (root as current directory, no file) and
 *                start in IDLE.
 * @param[in]     stpt_ctx FSM context.
 * @return        None.
 */
void gv_FileSysManagerFSMInit(FileSysManagerCTX_T *stpt_ctx)
{
   sv_ClearSavedContext(stpt_ctx);
   stpt_ctx->b_reported = false;
   smf_set_initial(SMF_CTX(stpt_ctx), &scstar_fsmStates[eFSM_IDLE]);
}

/**
 * @public        gv_FileSysManagerFSMRun
 * @brief         Execute stpt_ctx->st_currentMsg. Ends with exactly one final
 *                result; a path that would end without one reports -EIO.
 * @param[in]     stpt_ctx FSM context.
 * @return        None.
 */
void gv_FileSysManagerFSMRun(FileSysManagerCTX_T *stpt_ctx)
{
   stpt_ctx->b_reported = false;

   (void)smf_run_state(SMF_CTX(stpt_ctx));

   // Check if the message ended without a result (a defect): report one
   if (!stpt_ctx->b_reported)
   {
      LOG_ERR("Command %d ended without a result", stpt_ctx->st_currentMsg.e_command);
      gv_FileSysManagerFSMReport(stpt_ctx, -EIO, NULL, 0U);
   }
}
