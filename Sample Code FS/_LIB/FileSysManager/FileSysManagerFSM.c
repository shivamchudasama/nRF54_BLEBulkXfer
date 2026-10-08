/**
 * @file          FileSysManagerFSM.c
 * @brief         File system manager state machine implementation.
 * @date          20/02/26
 * @author        Yash Sunil Giramkar [YSG]
 * @copyright     Copyright(c) Yash Sunil Giramkar (YSG) as an unpublished work.
 */

/******************************************************************************/
/*                                  INCLUDES                                  */
/******************************************************************************/
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#include "FileSysManagerFSM.h"

/******************************************************************************/
/*                                  DEFINES                                   */
/******************************************************************************/
/**
 * @def           FSMGR_MAX_PATH_BUF
 * @brief         Defines the maximum buffer size for file paths in the File
 *                System Manager FSM. This is set to the maximum chunk size
 *                defined in TransferMsgTypes to ensure that file paths can
 *                be safely constructed from incoming messages without exceeding
 *                buffer limits.
 */
#define FSMGR_MAX_PATH_BUF                   FS_MAX_CHUNK_SIZE

/******************************************************************************/
/*                                   ENUMS                                    */
/******************************************************************************/
/**
 * @enum          e_FSMGR_FSM_states
 * @brief         Enum to maintain the various states of the File System Manager
 *                FSM.
 */
enum e_FSMGR_FSM_states
{
   STATE_IDLE,
   STATE_CREATE_DIR,
   STATE_CREATE_FILE,
   STATE_DELETE,
   STATE_WRITE_FILE,
   STATE_READ_FILE,
   STATE_CLOSE,
   STATE_FAILED,
};

/******************************************************************************/
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/******************************************************************************/

/* State functions associated with state machine starts*/
static void sv_IdleEntry(void *vptr);
static enum smf_state_result se_IdleRun(void *vptr);

static void sv_CreateDIREntry(void *vptr);
static enum smf_state_result se_CreateDIRRun(void *vptr);

static void sv_CreateFileEntry(void *vptr);
static enum smf_state_result se_CreateFileRun(void *vptr);

static void sv_DeleteEntry(void *vptr);
static enum smf_state_result se_DeleteRun(void *vptr);

static void sv_WriteFileEntry(void *vptr);
static enum smf_state_result se_WriteFileRun(void *vptr);

static void sv_ReadFileEntry(void *vptr);
static enum smf_state_result se_ReadFileRun(void *vptr);

static void sv_CloseFileEntry(void *vptr);
static enum smf_state_result se_CloseFileRun(void *vptr);

static void sv_OpFailedEntry(void *vptr);
static enum smf_state_result se_OpFailedRun(void *vptr);

/* State functions associated with state machine ends*/

static void sv_CloseIfOpen(FileSysManagerCTX_T *stpt_ctx);
static int si_BuildPathFromMsg(FileSysManagerCTX_T *stpt_ctx,
                               bool b_createDirectory,
                               char *cpt_builtPath,
                               size_t s_builtPathMaxSize);
static int si_FlushWriteCache(FileSysManagerCTX_T *stpt_ctx, bool b_syncFile);
static void sv_LogReadData(const uint8_t *u8pt_data, uint32_t u32_len);
static void sv_DebugListDrive(void);
static void sv_DebugListDirRecursive(const char *ccpt_rootPath);

/******************************************************************************/
/*                             PRIVATE VARIABLES                              */
/******************************************************************************/
/**
 * @struct        scst_FSMGR_stateTable
 * @brief         The state table for the File System Manager FSM.
 */
static const struct smf_state scst_FSMGR_stateTable[] = {
    [STATE_IDLE]        = SMF_CREATE_STATE(sv_IdleEntry, se_IdleRun, NULL, NULL, NULL),
    [STATE_CREATE_DIR]  = SMF_CREATE_STATE(sv_CreateDIREntry, se_CreateDIRRun, NULL, NULL, NULL),
    [STATE_CREATE_FILE] = SMF_CREATE_STATE(sv_CreateFileEntry, se_CreateFileRun, NULL, NULL, NULL),
    [STATE_DELETE]      = SMF_CREATE_STATE(sv_DeleteEntry, se_DeleteRun, NULL, NULL, NULL),
    [STATE_WRITE_FILE]  = SMF_CREATE_STATE(sv_WriteFileEntry, se_WriteFileRun, NULL, NULL, NULL),
    [STATE_READ_FILE]   = SMF_CREATE_STATE(sv_ReadFileEntry, se_ReadFileRun, NULL, NULL, NULL),
    [STATE_CLOSE]       = SMF_CREATE_STATE(sv_CloseFileEntry, se_CloseFileRun, NULL, NULL, NULL),
    [STATE_FAILED]      = SMF_CREATE_STATE(sv_OpFailedEntry, se_OpFailedRun, NULL, NULL, NULL),
};

/******************************************************************************/
/*                        PRIVATE FUNCTION DEFINITIONS                        */
/******************************************************************************/
/**
 * @private       sv_CloseIfOpen
 * @brief         Closes the file if it is open.
 * @param[in]     stpt_ctx - The context of the File System Manager.
 * @param[out]    None.
 * @param[inout]  None.
 * @return        None.
 */
static void sv_CloseIfOpen(FileSysManagerCTX_T *stpt_ctx)
{
   //  Check if the file is currently open
   if (stpt_ctx->b_fileOpenStatus)
   {
      // Close the file and update the open status
      (void)fs_close(&stpt_ctx->file);
      stpt_ctx->b_fileOpenStatus = false;
   }
}

/**
 * @private       sv_clearSavedContext
 * @brief         Closes the file if it is open.
 * @param[in]     stpt_ctx - The context of the File System Manager.
 * @param[out]    None.
 * @param[inout]  None.
 * @return        None.
 */
static void sv_clearSavedContext(FileSysManagerCTX_T *stpt_ctx)
{
   stpt_ctx->b_fileOpenStatus = false;
   stpt_ctx->u32_byteWritten = 0U;
   stpt_ctx->u32_totalExpectedBytes = 0U;
   stpt_ctx->as8_currentDir[0] = '\0';
   stpt_ctx->as8_activeFile[0] = '\0';
#ifdef CONFIG_FSMGR_BUFFERED_WRITE
   stpt_ctx->u32_writeCacheFill = 0U;
#endif
}


/**
 * @private       si_BuildPathFromMsg
 * @brief         Builds a file or directory path based on the incoming message
 *                and the current context.
 * @param[in]     stpt_ctx - The context of the File System Manager.
 * @param[in]     b_createDirectory - Whether we are creating a directory.
 * @param[out]    None
 * @param[inout]  cpt_builtPath - Pointer to output built path
 * @return        0 on success, negative error code on failure.
 */
static int si_BuildPathFromMsg(FileSysManagerCTX_T *stpt_ctx,
                               bool b_createDirectory,
                               char *cpt_builtPath,
                               size_t s_builtPathMaxSize)
{
   char c_tempCharString[FSMGR_MAX_PATH_BUF];
   const char *ccpt_nameStart;
   const char *ccpt_basePath;
   int i_retVal;
   uint32_t u32_length = stpt_ctx->st_currentMsg.u32_sizeOfData;
   bool b_isAbsoluteFsPath;

   // Ensure that the incoming data size does not exceed our temporary buffer
   if (u32_length >= sizeof(c_tempCharString))
   {
      // If it does, then truncate the length to fit in our buffer
      u32_length = sizeof(c_tempCharString) - 1U;
   }
   else
   {
      // Do nothing, the length is valid
   }

   // Copy the received data into temporary buffer
   if (u32_length > 0U)
   {
      memcpy(c_tempCharString, stpt_ctx->st_currentMsg.u8_data, u32_length);
   }
   else
   {
      return -EINVAL;
   }

   // Null-terminate the temporary string to ensure it is a valid C-string
   c_tempCharString[u32_length] = '\0';

   // Check if the path is an absolute path (starts with '/' and contains ':')
   b_isAbsoluteFsPath = ((c_tempCharString[0] == '/') && (strstr(c_tempCharString, ":") != NULL));
   if (b_isAbsoluteFsPath)
   {
      // If it is an absolute path, we can directly copy it to the output buffer
      i_retVal = snprintf(cpt_builtPath, s_builtPathMaxSize, "%s", c_tempCharString);
      return (i_retVal > 0 && (size_t)i_retVal < s_builtPathMaxSize) ? 0 : -ENAMETOOLONG;
   }

   // If it is not an absolute path, we need to determine the base path to prepend
   if (c_tempCharString[0] == '/')
   {
      // If the path starts with '/', we treat it as relative to the FAT mount point
      ccpt_nameStart = &c_tempCharString[1];
      ccpt_basePath = FAT_MOUNT_POINT;
   }
   else
   {
      // Otherwise, we treat it as relative to the current directory
      // (for file creation) or FAT mount point (for directory creation)
      ccpt_nameStart = c_tempCharString;
      ccpt_basePath = b_createDirectory ? FAT_MOUNT_POINT : stpt_ctx->as8_currentDir;
   }

   // Ensure that the name part of the path is not empty
   if (ccpt_nameStart[0] == '\0')
   {
      return -EINVAL;
   }

   // Build the final path by concatenating the base path and the name part
   i_retVal = snprintf(cpt_builtPath, s_builtPathMaxSize, "%s/%s", ccpt_basePath, ccpt_nameStart);

   // Check if the resulting path fits in the output buffer and return appropriate status
   return (i_retVal > 0 && i_retVal < s_builtPathMaxSize) ? 0 : -ENAMETOOLONG;
}

/**
 * @private       si_FlushWriteCache
 * @brief         Flushes buffered write data to file and optionally syncs.
 * @param[in]     stpt_ctx - FSM context.
 * @param[in]     b_syncFile - true to call fs_sync() after flushing data.
 * @return        0 on success, negative error code on failure.
 */
static int si_FlushWriteCache(FileSysManagerCTX_T *stpt_ctx, bool b_syncFile)
{
   int i_retVal;

#ifdef CONFIG_FSMGR_BUFFERED_WRITE
   if (stpt_ctx->u32_writeCacheFill > 0U)
   {
      i_retVal = fs_write(&stpt_ctx->file,
                          stpt_ctx->au8_writeCache,
                          stpt_ctx->u32_writeCacheFill);
      if ((i_retVal < 0) || ((uint32_t)i_retVal != stpt_ctx->u32_writeCacheFill))
      {
         return (i_retVal < 0) ? i_retVal : -EIO;
      }

      stpt_ctx->u32_byteWritten += stpt_ctx->u32_writeCacheFill;
      stpt_ctx->u32_writeCacheFill = 0U;
   }
#else
   ARG_UNUSED(stpt_ctx);
#endif

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
 * @private       sv_LogReadData
 * @brief         Helper function to print read data via logger module.
 * @param[in]     u8pt_data - Pointer to the data read.
 * @param[in]     u32_len - Length of the data read.
 * @param[out]    None
 * @param[inout]  None
 * @return        None
 */
static void sv_LogReadData(const uint8_t *u8pt_data, uint32_t u32_len)
{
   uint32_t u32_idx;

   if (u32_len > FS_MAX_CHUNK_SIZE)
   {
      u32_len = FS_MAX_CHUNK_SIZE;
   }

   LOG_INF("Read %u bytes : ", u32_len);
   LOG_HEXDUMP_INF(u8pt_data, u32_len, "Data content (hex):");
}

/**
 * @private       sv_DebugListDirRecursive
 * @brief         Non-recursive listing of root and one-level children for debug.
 * @param[in]     ccpt_rootPath - Absolute path of mount root to list.
 * @return        None
 */
static void sv_DebugListDirRecursive(const char *ccpt_rootPath)
{
   struct fs_dir_t st_rootDir;
   struct fs_dir_t st_childDir;
   struct fs_dirent st_rootEntry;
   struct fs_dirent st_childEntry;
   char as8_dirPath[FSMGR_MAX_PATH_BUF];
   int i_retVal;

   fs_dir_t_init(&st_rootDir);
   i_retVal = fs_opendir(&st_rootDir, ccpt_rootPath);
   if (i_retVal != 0)
   {
      LOG_ERR("[DBG][0] opendir failed (%d): %s", i_retVal, ccpt_rootPath);
      return;
   }

   while (1)
   {
      i_retVal = fs_readdir(&st_rootDir, &st_rootEntry);
      if (i_retVal != 0)
      {
         LOG_ERR("[DBG][0] readdir failed (%d): %s", i_retVal, ccpt_rootPath);
         break;
      }

      if (st_rootEntry.name[0] == '\0')
      {
         break;
      }

      if (snprintf(as8_dirPath, sizeof(as8_dirPath), "%s/%s", ccpt_rootPath, st_rootEntry.name) >= sizeof(as8_dirPath))
      {
         LOG_WRN("[DBG][0] path too long: %s/%s", ccpt_rootPath, st_rootEntry.name);
         continue;
      }

      if (st_rootEntry.type == FS_DIR_ENTRY_DIR)
      {
         LOG_INF("[DBG][0][DIR] %s", as8_dirPath);

         fs_dir_t_init(&st_childDir);
         i_retVal = fs_opendir(&st_childDir, as8_dirPath);
         if (i_retVal != 0)
         {
            LOG_ERR("[DBG][1] opendir failed (%d): %s", i_retVal, as8_dirPath);
            continue;
         }

         while (1)
         {
            i_retVal = fs_readdir(&st_childDir, &st_childEntry);
            if (i_retVal != 0)
            {
               LOG_ERR("[DBG][1] readdir failed (%d): %s", i_retVal, as8_dirPath);
               break;
            }

            if (st_childEntry.name[0] == '\0')
            {
               break;
            }

            if (st_childEntry.type == FS_DIR_ENTRY_DIR)
            {
               LOG_WRN("[DBG][1][DIR] %s/%s (ignored: depth>1 not supported)", as8_dirPath, st_childEntry.name);
            }
            else
            {
               LOG_INF("[DBG][1][FILE] %s/%s (%u bytes)", as8_dirPath, st_childEntry.name, (uint32_t)st_childEntry.size);
            }
         }

         (void)fs_closedir(&st_childDir);
      }
      else
      {
         LOG_INF("[DBG][0][FILE] %s (%u bytes)", as8_dirPath, (uint32_t)st_rootEntry.size);
      }
   }

   (void)fs_closedir(&st_rootDir);
}

/**
 * @private       sv_DebugListDrive
 * @brief         Debug helper to list all files and directories from FAT mount root.
 * @return        None
 */
static void sv_DebugListDrive(void)
{
   LOG_INF("[DBG] Listing drive from: %s", FAT_MOUNT_POINT);
   sv_DebugListDirRecursive(FAT_MOUNT_POINT);
}



/******************************************************************************/
/*                        PRIVATE FUNCTION FOR FSM                            */
/******************************************************************************/

/*************************Idel State Functions Begins**************************/
/**
 * @private       sv_IdleEntry
 * @brief         Entry function for the IDLE state of the FSM. This function is
 *                called when the FSM transitions into the IDLE state.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        None
 */
static void sv_IdleEntry(void *vptr)
{
   ARG_UNUSED(vptr);
}

/**
 * @private       se_IdleRun
 * @brief         Run function for the IDLE state of the FSM. This function is
 *                called when an event is processed while the FSM is in the
 *                IDLE state.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        SMF_EVENT_HANDLED always for now.
 */
static enum smf_state_result se_IdleRun(void *vptr)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vptr;

   switch (stpt_ctx->st_currentMsg.e_command)
   {
      case eFSC_OPEN_DIR:
      case eFSC_MAKE_DIR:
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_CREATE_DIR]);
      break;

      case eFSC_OPEN_FILE_READ:
      case eFSC_OPEN_FILE_WRITE:
         smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_CREATE_FILE]);
      break;

      case eFSC_READ_FILE:
         LOG_WRN("Open a file first, then use read");
      break;

      case eFSC_WRITE_DATA:
         LOG_WRN("Open a file first, then use write");
      break;

      case eFSC_DEBUG_LIST_DRIVE:
         sv_DebugListDrive();
      break;

      case eFSC_DELETE_FILE:
      case eFSC_DELETE_DIR:
         smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_DELETE]);
      break;

      case eFSC_CLOSE_FILE:
         sv_CloseIfOpen(stpt_ctx);
      break;
      case eFSC_ABORT:
         sv_CloseIfOpen(stpt_ctx);
         sv_clearSavedContext(stpt_ctx);
      break;


      default:
         LOG_ERR("Unexpected command %d in IDLE", stpt_ctx->st_currentMsg.e_command);
      break;
   }

   return SMF_EVENT_HANDLED;
}



/**
 * @private       si_CheckDIRAndUnlinkFiles
 * @brief         Helper function to check if a directory is empty and unlink
 *                files if not.
 * @param[in]     ccpt_dirPath - Path of the directory to check and clear.
 * @return        0 if the directory is empty or successfully cleared, negative
 *                error code if an error occurs. Note that if the directory
 *                contains subdirectories, this function will fail with -ENOTSUP
 *                as recursive deletion is not supported.
 */
static int si_CheckDIRAndUnlinkFiles(const char *ccpt_dirPath)
{
   struct fs_dir_t st_dir;
   struct fs_dirent st_entry;
   char as8_entryPath[FSMGR_MAX_PATH_BUF];
   int i_retVal;

   // Open the directory for reading
   fs_dir_t_init(&st_dir);
   i_retVal = fs_opendir(&st_dir, ccpt_dirPath);
   if (i_retVal != 0)
   {
      return i_retVal;
   }

   while (1)
   {
      // Read the next entry in the directory
      i_retVal = fs_readdir(&st_dir, &st_entry);
      if (i_retVal != 0)
      {
         // If there is an error reading the directory, close and return error
         fs_closedir(&st_dir);
         return i_retVal;
      }

      // If the entry name is empty, we have reached the end of the directory
      if (st_entry.name[0] == '\0')
      {
         break;
      }

      // If the entry is a directory, we do not support recursive deletion
      if (st_entry.type == FS_DIR_ENTRY_DIR)
      {
         // Subdirectories are not supported for deletion in this implementation
         fs_closedir(&st_dir);
         return -ENOTSUP;
      }

      // If the entry is a file, build the full path and unlink it
      if (snprintf(as8_entryPath, sizeof(as8_entryPath), "%s/%s", ccpt_dirPath,
                                          st_entry.name) >= sizeof(as8_entryPath))
      {
         // If the path is too long, log a warning and skip this entry
         fs_closedir(&st_dir);
         return -ENAMETOOLONG;
      }

      // Unlink the file and check for errors
      i_retVal = fs_unlink(as8_entryPath);
      if (i_retVal != 0)
      {
         // If there is an error unlinking the file, close and return error
         fs_closedir(&st_dir);
         return i_retVal;
      }
   }

   // Close the directory and return success
   fs_closedir(&st_dir);
   return 0;
}

/*************************Idel State Functions Ends****************************/


/*********************Create DIR State Functions Begins************************/

/**
 * @private       sv_CreateDIREntry
 * @brief         Entry function for the CREATE_DIR state of the FSM. This
 *                function is called when the FSM transitions into the
 *                CREATE_DIR state. It attempts to create a directory based on
 *                the current message and context, and then transitions to the
 *                appropriate next state based on the result.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        None
 */
static void sv_CreateDIREntry(void *vptr)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vptr;
   struct fs_dirent st_dirEntry;
   char as8_targetDir[FSMGR_MAX_PATH_BUF];
   int i_retVal;

   i_retVal = si_BuildPathFromMsg(stpt_ctx, true, as8_targetDir, sizeof(as8_targetDir));
   if (i_retVal != 0)
   {
      LOG_ERR("OPEN_DIR path build failed (%d)", i_retVal);
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
      return;
   }

   if (stpt_ctx->st_currentMsg.e_command == eFSC_MAKE_DIR)
   {
      i_retVal = fs_mkdir(as8_targetDir);
      if ((i_retVal != 0) && (i_retVal != -EEXIST))
      {
         LOG_ERR("Directory create failed (%d): %s", i_retVal, as8_targetDir);
         smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
         return;
      }
   }
   else if (stpt_ctx->st_currentMsg.e_command == eFSC_OPEN_DIR)
   {
      i_retVal = fs_stat(as8_targetDir, &st_dirEntry);
      if ((i_retVal != 0) || (st_dirEntry.type != FS_DIR_ENTRY_DIR))
      {
         LOG_ERR("Directory open failed (%d): %s", i_retVal, as8_targetDir);
         smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
         return;
      }
   }
   else
   {
      LOG_ERR("Invalid directory command: %d", stpt_ctx->st_currentMsg.e_command);
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
      return;
   }

   (void)strncpy(stpt_ctx->as8_currentDir, as8_targetDir, sizeof(stpt_ctx->as8_currentDir) - 1U);
   stpt_ctx->as8_currentDir[sizeof(stpt_ctx->as8_currentDir) - 1U] = '\0';
   LOG_INF("Directory active: %s", stpt_ctx->as8_currentDir);
   smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_IDLE]);
}

/**
 * @private       se_CreateDIRRun
 * @brief         Run function for the CREATE_DIR state of the FSM. This
 *                function is called when an event is processed while the FSM
 *                is in the CREATE_DIR state.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        SMF_EVENT_HANDLED always for now.
 */
static enum smf_state_result se_CreateDIRRun(void *vptr)
{
   ARG_UNUSED(vptr);
   return SMF_EVENT_HANDLED;
}

/*********************Create DIR State Functions Ends**************************/

/*******************Create File State Functions Begins*************************/

/**
 * @private       sv_CreateFileEntry
 * @brief         Entry function for the CREATE_FILE state of the FSM. This
 *                function is called when the FSM transitions into the
 *                CREATE_FILE state. It attempts to create a file based on
 *                the current message and context, and then transitions to the
 *                appropriate next state based on the result.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        None
 */
static void sv_CreateFileEntry(void *vptr)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vptr;
   int i_retVal;
   fs_mode_t fileOpenMode;
   enum e_FSMGR_FSM_states e_nextState;

   i_retVal = si_BuildPathFromMsg(stpt_ctx, false, stpt_ctx->as8_activeFile, sizeof(stpt_ctx->as8_activeFile));
   if (i_retVal != 0)
   {
      LOG_ERR("Unable to build path (%d): %s", i_retVal, stpt_ctx->as8_activeFile);
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
      return;
   }

   if (stpt_ctx->st_currentMsg.e_command == eFSC_OPEN_FILE_READ)
   {
      fileOpenMode = FS_O_READ;
      e_nextState = STATE_READ_FILE;
   }
   else if (stpt_ctx->st_currentMsg.e_command == eFSC_OPEN_FILE_WRITE)
   {
      fileOpenMode = FS_O_CREATE | FS_O_WRITE;
      e_nextState = STATE_WRITE_FILE;
   }
   else
   {
      LOG_ERR("Invalid open command: %d", stpt_ctx->st_currentMsg.e_command);
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
      return;
   }

   sv_CloseIfOpen(stpt_ctx);
   fs_file_t_init(&stpt_ctx->file);

   i_retVal = fs_open(&stpt_ctx->file, stpt_ctx->as8_activeFile, fileOpenMode);
   if (i_retVal == 0)
   {
      stpt_ctx->b_fileOpenStatus = true;
      stpt_ctx->u32_byteWritten = 0U;
#ifdef CONFIG_FSMGR_BUFFERED_WRITE
      stpt_ctx->u32_writeCacheFill = 0U;
#endif
      LOG_INF("File opened: %s", stpt_ctx->as8_activeFile);
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[e_nextState]);
   }
   else
   {
      LOG_ERR("File open failed (%d): %s", i_retVal, stpt_ctx->as8_activeFile);
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
   }
}

/**
 * @private       se_CreateFileRun
 * @brief         Run function for the CREATE_FILE state of the FSM. This
 *                function is called when an event is processed while the FSM
 *                is in the CREATE_FILE state.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        SMF_EVENT_HANDLED always for now.
 */
static enum smf_state_result se_CreateFileRun(void *vptr)
{
   ARG_UNUSED(vptr);
   return SMF_EVENT_HANDLED;
}

/********************Create File State Functions Ends**************************/
/**********************Delete State Functions Begins***************************/

/**
 * @private       sv_DeleteEntry
 * @brief         Entry function for DELETE state.
 * @param[in]     vptr - Pointer to FSM context.
 * @return        None
 */
static void sv_DeleteEntry(void *vptr)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vptr;
   struct fs_dirent st_entry;
   char as8_targetPath[FSMGR_MAX_PATH_BUF];
   int i_retVal;

   if(stpt_ctx->st_currentMsg.e_command == eFSC_DELETE_FILE)
   {
      i_retVal = si_BuildPathFromMsg(stpt_ctx, false, as8_targetPath, sizeof(as8_targetPath));
   }
   else
   {
      i_retVal = si_BuildPathFromMsg(stpt_ctx, true, as8_targetPath, sizeof(as8_targetPath));
   }
   if (i_retVal != 0)
   {
      LOG_ERR("Delete path build failed (%d)", i_retVal);
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
      return;
   }

   i_retVal = fs_stat(as8_targetPath, &st_entry);
   if (i_retVal != 0)
   {
      LOG_ERR("Delete target not found (%d): %s", i_retVal, as8_targetPath);
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
      return;
   }

   if ((stpt_ctx->st_currentMsg.e_command == eFSC_DELETE_FILE) &&
       (st_entry.type != FS_DIR_ENTRY_FILE))
   {
      LOG_ERR("Delete file requested for non-file: %s", as8_targetPath);
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
      return;
   }

   if (stpt_ctx->st_currentMsg.e_command == eFSC_DELETE_DIR)
   {
      if(st_entry.type != FS_DIR_ENTRY_DIR)
      {
         LOG_ERR("Delete dir requested for non-dir: %s", as8_targetPath);
         smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
         return;
      }
      else
      {
         // Check if the directory contains any files, if they exist, unlink all files first before deleting the directory
         i_retVal = si_CheckDIRAndUnlinkFiles(as8_targetPath);
         if (i_retVal != 0)
         {
            LOG_ERR("Failed to clear directory contents (%d): %s", i_retVal, as8_targetPath);
            smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
            return;
         }
         else
         {
            LOG_INF("Directory cleared successfully: %s", as8_targetPath);
         }
      }
   }


   i_retVal = fs_unlink(as8_targetPath);
   if (i_retVal != 0)
   {
      LOG_ERR("Delete failed (%d): %s", i_retVal, as8_targetPath);
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
      return;
   }

   LOG_INF("Delete successful: %s", as8_targetPath);
   smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_IDLE]);
}

/**
 * @private       se_DeleteRun
 * @brief         Run function for DELETE state.
 * @param[in]     vptr - Pointer to FSM context.
 * @return        SMF_EVENT_HANDLED
 */
static enum smf_state_result se_DeleteRun(void *vptr)
{
   ARG_UNUSED(vptr);
   return SMF_EVENT_HANDLED;
}

/***********************Delete State Functions Ends****************************/

/********************Write File State Functions Begins*************************/

/**
 * @private       sv_WriteFileEntry
 * @brief         Entry function for the WRITE_FILE state of the FSM. This
 *                function is called when the FSM transitions into the WRITE_FILE
 *                state.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        None
 */
static void sv_WriteFileEntry(void *vptr)
{
   ARG_UNUSED(vptr);
}

/**
 * @private       se_WriteFileRun
 * @brief         Run function for the WRITE_FILE state of the FSM. This
 *                function is called when an event is processed while the FSM
 *                is in the WRITE_FILE state. It handles writing data to the
 *                currently open file based on the incoming message, and
 *                transitions to the appropriate next state based on the result.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        SMF_EVENT_HANDLED always for now.
 */
static enum smf_state_result se_WriteFileRun(void *vptr)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vptr;
   int i_retVal;

   if (!stpt_ctx->b_fileOpenStatus)
   {
      LOG_ERR("WRITE state without open file");
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
      return SMF_EVENT_HANDLED;
   }

   switch (stpt_ctx->st_currentMsg.e_command)
   {
   case eFSC_WRITE_DATA:
      if ((stpt_ctx->st_currentMsg.u32_sizeOfData == 0U) ||
          (stpt_ctx->st_currentMsg.u32_sizeOfData > FS_MAX_CHUNK_SIZE))
      {
         LOG_ERR("Invalid write size: %u", stpt_ctx->st_currentMsg.u32_sizeOfData);
         smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
         break;
      }

#ifdef CONFIG_FSMGR_BUFFERED_WRITE
      {
         const uint8_t *cu8pt_data = stpt_ctx->st_currentMsg.u8_data;
         uint32_t u32_remaining = stpt_ctx->st_currentMsg.u32_sizeOfData;
         uint32_t u32_copyLen;
         uint32_t u32_space;
         i_retVal = 0;

         while (u32_remaining > 0U)
         {
            u32_space = CONFIG_FSMGR_WRITE_BUFFER_SIZE - stpt_ctx->u32_writeCacheFill;
            u32_copyLen = (u32_remaining < u32_space) ? u32_remaining : u32_space;

            memcpy(&stpt_ctx->au8_writeCache[stpt_ctx->u32_writeCacheFill], cu8pt_data, u32_copyLen);
            stpt_ctx->u32_writeCacheFill += u32_copyLen;
            cu8pt_data += u32_copyLen;
            u32_remaining -= u32_copyLen;

            if (stpt_ctx->u32_writeCacheFill == CONFIG_FSMGR_WRITE_BUFFER_SIZE)
            {
               i_retVal = si_FlushWriteCache(stpt_ctx, false);
               if (i_retVal < 0)
               {
                  LOG_ERR("Buffered file write flush failed (%d)", i_retVal);
                  smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
                  break;
               }
            }
         }

         if (i_retVal < 0)
         {
            break;
         }

         LOG_INF("Buffered %u bytes, flushed total %u",
            stpt_ctx->u32_writeCacheFill, stpt_ctx->u32_byteWritten);
      }
#else
      i_retVal = fs_write(&stpt_ctx->file,
                       stpt_ctx->st_currentMsg.u8_data,
                       stpt_ctx->st_currentMsg.u32_sizeOfData);

      if (i_retVal < 0)
      {
         LOG_ERR("File write failed (%d)", i_retVal);
         smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
         break;
      }

      stpt_ctx->u32_byteWritten += (uint32_t)i_retVal;
      LOG_INF("Written %d bytes, total %u", i_retVal, stpt_ctx->u32_byteWritten);
#endif
      break;

   case eFSC_CLOSE_FILE:
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_CLOSE]);
      break;

   case eFSC_ABORT:
      sv_CloseIfOpen(stpt_ctx);
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_IDLE]);
      break;

   case eFSC_READ_FILE:
   case eFSC_DEBUG_LIST_DRIVE:
   case eFSC_DELETE_FILE:
   case eFSC_DELETE_DIR:
   case eFSC_OPEN_FILE_READ:
   case eFSC_OPEN_FILE_WRITE:
   case eFSC_OPEN_DIR:
   case eFSC_MAKE_DIR:
   default:
      LOG_ERR("Unexpected command %d in WRITE", stpt_ctx->st_currentMsg.e_command);
      break;
   }

   return SMF_EVENT_HANDLED;
}


/********************Write File State Functions Ends***************************/
/********************Read File State Functions Begins**************************/

/**
 * @private       sv_ReadFileEntry
 * @brief         Entry function for the READ_FILE state of the FSM. This
 *                function is called when the FSM transitions into the READ_FILE
 *                state.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        None
 */
static void sv_ReadFileEntry(void *vptr)
{
   ARG_UNUSED(vptr);
}

/**
 * @private       se_ReadFileRun
 * @brief         Run function for the READ_FILE state of the FSM. This
 *                function is called when an event is processed while the FSM
 *                is in the READ_FILE state. It handles reading data from the
 *                currently open file based on the incoming message, and
 *                transitions to the appropriate next state based on the result.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        SMF_EVENT_HANDLED always for now.
 */
static enum smf_state_result se_ReadFileRun(void *vptr)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vptr;
   uint8_t au8_readBuf[FS_MAX_CHUNK_SIZE];
   uint32_t u32_lenToRead;
   int i_retVal;

   if (!stpt_ctx->b_fileOpenStatus)
   {
      LOG_ERR("READ state without open file");
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
      return SMF_EVENT_HANDLED;
   }

   switch (stpt_ctx->st_currentMsg.e_command)
   {
   case eFSC_READ_FILE:
      u32_lenToRead = stpt_ctx->st_currentMsg.u32_sizeOfData;
      if ((u32_lenToRead == 0U) || (u32_lenToRead > FS_MAX_CHUNK_SIZE))
      {
         u32_lenToRead = FS_MAX_CHUNK_SIZE;
      }

      i_retVal = fs_read(&stpt_ctx->file, au8_readBuf, u32_lenToRead);
      if (i_retVal < 0)
      {
         LOG_ERR("File read failed (%d)", i_retVal);
         smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
         break;
      }

      if (i_retVal == 0)
      {
         LOG_INF("End of file reached: %s", stpt_ctx->as8_activeFile);
      }
      else
      {
         sv_LogReadData(au8_readBuf, (uint32_t)i_retVal);
      }
      break;

   case eFSC_CLOSE_FILE:
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_CLOSE]);
      break;

   case eFSC_ABORT:
      sv_CloseIfOpen(stpt_ctx);
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_IDLE]);
      break;

   case eFSC_WRITE_DATA:
   case eFSC_DEBUG_LIST_DRIVE:
   case eFSC_DELETE_FILE:
   case eFSC_DELETE_DIR:
   case eFSC_OPEN_FILE_READ:
   case eFSC_OPEN_FILE_WRITE:
   case eFSC_OPEN_DIR:
   case eFSC_MAKE_DIR:
   default:
      LOG_ERR("Unexpected command %d in READ", stpt_ctx->st_currentMsg.e_command);
      break;
   }

   return SMF_EVENT_HANDLED;
}

/********************Read File State Functions Ends***************************/
/********************Close File State Functions Begins*************************/

/**
 * @private       sv_CloseFileEntry
 * @brief         Entry function for the CLOSE state of the FSM. This function is
 *                called when the FSM transitions into the CLOSE state. It
 *                attempts to close the currently open file and then transitions
 *                back to the IDLE state.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        None
 */
static void sv_CloseFileEntry(void *vptr)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vptr;
   int i_retVal;

   i_retVal = si_FlushWriteCache(stpt_ctx, true);
   if (i_retVal < 0)
   {
      LOG_ERR("File flush/sync failed (%d)", i_retVal);
      smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_FAILED]);
      return;
   }

   sv_CloseIfOpen(stpt_ctx);
   LOG_INF("File closed");
   smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_IDLE]);
}

/**
 * @private       se_CloseFileRun
 * @brief         Run function for the CLOSE state of the FSM. This function is
 *                called when an event is processed while the FSM is in the
 *                CLOSE state.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        SMF_EVENT_HANDLED always for now.
 */
static enum smf_state_result se_CloseFileRun(void *vptr)
{
   ARG_UNUSED(vptr);
   return SMF_EVENT_HANDLED;
}

/********************Close File State Functions Ends***************************/
/*****************Operation Failed State Functions Begins**********************/

/**
 * @private       sv_OpFailedEntry
 * @brief         Entry function for the FAILED state of the FSM. This function is
 *                called when the FSM transitions into the FAILED state. It
 *                performs necessary cleanup and then transitions back to the
 *                IDLE state.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        None
 */
static void sv_OpFailedEntry(void *vptr)
{
   FileSysManagerCTX_T *stpt_ctx = (FileSysManagerCTX_T *)vptr;

   sv_CloseIfOpen(stpt_ctx);
   LOG_ERR("FSM operation failed, returning to IDLE");
   smf_set_state(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_IDLE]);
}

/**
 * @private       se_OpFailedRun
 * @brief         Run function for the FAILED state of the FSM. This function is
 *                called when an event is processed while the FSM is in the
 *                FAILED state.
 * @param[in]     vptr - Pointer to the FSM context.
 * @param[out]    None
 * @param[inout]  None
 * @return        SMF_EVENT_HANDLED always for now.
 */
static enum smf_state_result se_OpFailedRun(void *vptr)
{
   ARG_UNUSED(vptr);
   return SMF_EVENT_HANDLED;
}

/******************************************************************************/
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/******************************************************************************/

/**
 * @public        gv_FileSysManagerFSMInit
 * @brief         Initializes the File System Manager FSM context and sets the
 *                initial state to IDLE.
 * @param[in]     stpt_ctx - Pointer to the File System Manager FSM context to initialize.
 * @param[out]    None
 * @param[inout]  None
 * @return        None
 */
void gv_FileSysManagerFSMInit(FileSysManagerCTX_T *stpt_ctx)
{
   stpt_ctx->u32_byteWritten = 0U;
   stpt_ctx->u32_totalExpectedBytes = 0U;
   stpt_ctx->b_fileOpenStatus = false;
   (void)strncpy(stpt_ctx->as8_currentDir, FAT_MOUNT_POINT, sizeof(stpt_ctx->as8_currentDir) - 1U);
   stpt_ctx->as8_currentDir[sizeof(stpt_ctx->as8_currentDir) - 1U] = '\0';
   stpt_ctx->as8_activeFile[0] = '\0';
#ifdef CONFIG_FSMGR_BUFFERED_WRITE
   stpt_ctx->u32_writeCacheFill = 0U;
#endif

   smf_set_initial(SMF_CTX(stpt_ctx), &scst_FSMGR_stateTable[STATE_IDLE]);
}

/**
 * @public        gv_FileSysManagerFSMRun
 * @brief         Runs the File System Manager FSM for the given context. This
 *                function should be called whenever there is a new message to
 *                process or when the FSM needs to be advanced.
 * @param[in]     stpt_ctx - Pointer to the File System Manager FSM context to run.
 * @param[out]    None
 * @param[inout]  None
 * @return        None
 */
void gv_FileSysManagerFSMRun(FileSysManagerCTX_T *stpt_ctx)
{
   smf_run_state(SMF_CTX(stpt_ctx));
}


/**
 * Copyright(c) Yash Sunil Giramkar (YSG) as an unpublished work.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION IS ALLOWED ONLY IN ACCORDANCE WITH
 * THE TERMS OF THE LICENSE
 *
 * @author:Yash Sunil Giramkar [YSG]
 */

