/**
 * @file          Sample_Format.c
 * @brief         Source file containing <Details>
 * @date          <Date of generating C file - DD/MM/YY>
 * @author        <Author of C file - Name [Initials]>
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "UartTestHarness.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           UART_LINE_BUF_SIZE
 * @brief         Defines the maximum buffer size for reading lines from UART.
 */
#define UART_LINE_BUF_SIZE                   255

/**
 * @def           UART_TEST_HARNESS_THREAD_STACK_SIZE
 * @brief         Defines the stack size for the UART Test Harness thread.
 */
#define UART_TEST_HARNESS_THREAD_STACK_SIZE  1024

/**
 * @def           UART_TEST_HARNESS_THREAD_PRIORITY
 * @brief         Defines the thread priority for the UART Test Harness thread.
 */
#define UART_TEST_HARNESS_THREAD_PRIORITY    4

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
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/*                                                                            */
/******************************************************************************/

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

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           stpt_consoleUART
 * @brief         Pointer to the UART device used for console input/output.
 *                This is initialized using the device tree to get the console
 *                UART specified for the Zephyr application.
 */
static const struct device *stpt_consoleUART = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

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
 * @private       sv_PrintHelp
 * @brief         Prints the list of supported UART commands.
 * @param[in]     None
 * @param[out]    None
 * @param[inout]  None
 * @return        None
 */
static void sv_PrintHelp(void)
{
   LOG_INF("FileSys UART Test Commands:");
   LOG_INF("  help");
   LOG_INF("  mkdir <path>");
   LOG_INF("  cd <path>");
   LOG_INF("  openr <path>");
   LOG_INF("  openw <path>");
   LOG_INF("  write <text payload>");
   LOG_INF("  read [bytes]");
   LOG_INF("  ls");
   LOG_INF("  delfile <path>");
   LOG_INF("  deldir <path>");
   LOG_INF("  close");
   LOG_INF("  abort");
}

/**
 * @private       si_UartReadLine
 * @brief         Reads one command line from UART console with basic echo and
 *                backspace handling.
 * @param[in]     cptr_rxLine - Destination buffer for received line.
 * @param[in]     u8_maxLen - Maximum buffer length.
 * @param[out]    None
 * @param[inout]  cptr_rxLine
 * @return        Number of bytes read (excluding null terminator).
 */
static int si_UartReadLine(char *cptr_rxLine, uint8_t u8_maxLen)
{
      uint8_t u8_idx = 0U;
      unsigned char uc_readChar;

   while (u8_idx < (u8_maxLen - 1U))
   {
      if (uart_poll_in(stpt_consoleUART, &uc_readChar) == 0)
      {
         if ((uc_readChar == '\r') || (uc_readChar == '\n')) {
               cptr_rxLine[u8_idx] = '\0';
               return (int)u8_idx;
         }

         if ((uc_readChar == '\b') || (uc_readChar == 127U))
         {
            if (u8_idx > 0U)
            {
               u8_idx--;
            }
            continue;
         }

         cptr_rxLine[u8_idx++] = (char)uc_readChar;
         uart_poll_out(stpt_consoleUART, uc_readChar);
      }
      else
      {
         k_msleep(10);
      }
   }

   cptr_rxLine[u8_maxLen - 1U] = '\0';
   return (int)(u8_maxLen - 1U);
}

/**
 * @private       sp_ArgStart
 * @brief         Splits command and argument, returns pointer to first argument.
 * @param[in]     cptr_rxLine - Input line containing command and optional args.
 * @param[out]    None
 * @param[inout]  cptr_rxLine
 * @return        Pointer to first arg, or NULL if arg is absent.
 */
static char *sp_ArgStart(char *cptr_rxLine)
{
   char *arg = strchr(cptr_rxLine, ' ');

   if (arg == NULL)
   {
      return NULL;
   }

   *arg = '\0';
   arg++;

   while (*arg == ' ')
   {
      arg++;
   }

   return (*arg == '\0') ? NULL : arg;
}

/**
 * @private       si_EnqueueMsg
 * @brief         Sends a parsed message to File System Manager message queue.
 * @param[in]     msg - Message to enqueue.
 * @param[out]    None
 * @param[inout]  None
 * @return        0 on success, negative error code on failure.
 */
static int si_EnqueueMsg(const FileSysMessage_T *msg)
{
   struct k_msgq *FSMGR_MsgQ = gstpt_FSMGR_GetMsgQ();

   return k_msgq_put(FSMGR_MsgQ, msg, K_MSEC(100));
}

/**
 * @private       sv_HandleLine
 * @brief         Parses one UART command line and converts it into
 *                FileSysMessage_T command for FSM processing.
 * @param[in]     cptr_rxLine - UART input line.
 * @param[out]    None
 * @param[inout]  cptr_rxLine
 * @return        None
 */
static void sv_HandleLine(char *cptr_rxLine)
{
   FileSysMessage_T msg;
   char *arg;
   char *end;
   unsigned long ul_readSize;
   int ret;

   while (isspace((unsigned char)*cptr_rxLine))
   {
      cptr_rxLine++;
   }

   if (*cptr_rxLine == '\0')
   {
      return;
   }

   arg = sp_ArgStart(cptr_rxLine);

   (void)memset(&msg, 0, sizeof(msg));

   if (strcmp(cptr_rxLine, "help") == 0)
   {
      sv_PrintHelp();
      return;
   }

   if (strcmp(cptr_rxLine, "mkdir") == 0)
   {
      if (arg == NULL)
      {
         LOG_ERR("mkdir requires a path");
         return;
      }

      msg.e_command = eFSC_MAKE_DIR;
      msg.u32_sizeOfData = MIN((uint32_t)strlen(arg), (uint32_t)FS_MAX_CHUNK_SIZE);
      (void)memcpy(msg.u8_data, arg, msg.u32_sizeOfData);
   }
   else if (strcmp(cptr_rxLine, "cd") == 0)
   {
      if (arg == NULL)
      {
         LOG_ERR("cd requires a path");
         return;
      }

      msg.e_command = eFSC_OPEN_DIR;
      msg.u32_sizeOfData = MIN((uint32_t)strlen(arg), (uint32_t)FS_MAX_CHUNK_SIZE);
      (void)memcpy(msg.u8_data, arg, msg.u32_sizeOfData);
   }
   else if (strcmp(cptr_rxLine, "openr") == 0)
   {
      if (arg == NULL)
      {
         LOG_ERR("openr requires a path");
         return;
      }

      msg.e_command = eFSC_OPEN_FILE_READ;
      msg.u32_sizeOfData = MIN((uint32_t)strlen(arg), (uint32_t)FS_MAX_CHUNK_SIZE);
      (void)memcpy(msg.u8_data, arg, msg.u32_sizeOfData);
   }
   else if (strcmp(cptr_rxLine, "openw") == 0)
   {
      if (arg == NULL)
      {
         LOG_ERR("openw requires a path");
         return;
      }

      msg.e_command = eFSC_OPEN_FILE_WRITE;
      msg.u32_sizeOfData = MIN((uint32_t)strlen(arg), (uint32_t)FS_MAX_CHUNK_SIZE);
      (void)memcpy(msg.u8_data, arg, msg.u32_sizeOfData);
   }
   else if (strcmp(cptr_rxLine, "write") == 0)
   {
      if (arg == NULL)
      {
         LOG_ERR("write requires payload");
         return;
      }

      msg.e_command = eFSC_WRITE_DATA;
      msg.u32_sizeOfData = MIN((uint32_t)strlen(arg), (uint32_t)FS_MAX_CHUNK_SIZE);
      (void)memcpy(msg.u8_data, arg, msg.u32_sizeOfData);
   }
   else if (strcmp(cptr_rxLine, "read") == 0)
   {
      msg.e_command = eFSC_READ_FILE;
      msg.u32_sizeOfData = FS_MAX_CHUNK_SIZE;

      if (arg != NULL)
      {
         ul_readSize = strtoul(arg, &end, 10);
         if ((*arg == '\0') || (*end != '\0') || (ul_readSize == 0UL))
         {
            LOG_ERR("read expects optional positive byte count");
            return;
         }

         msg.u32_sizeOfData = MIN((uint32_t)ul_readSize, (uint32_t)FS_MAX_CHUNK_SIZE);
      }
   }
   else if (strcmp(cptr_rxLine, "ls") == 0)
   {
      msg.e_command = eFSC_DEBUG_LIST_DRIVE;
   }
   else if (strcmp(cptr_rxLine, "delfile") == 0)
   {
      if (arg == NULL)
      {
         LOG_ERR("delfile requires a path");
         return;
      }

      msg.e_command = eFSC_DELETE_FILE;
      msg.u32_sizeOfData = MIN((uint32_t)strlen(arg), (uint32_t)FS_MAX_CHUNK_SIZE);
      (void)memcpy(msg.u8_data, arg, msg.u32_sizeOfData);
   }
   else if (strcmp(cptr_rxLine, "deldir") == 0)
   {
      if (arg == NULL)
      {
         LOG_ERR("deldir requires a path");
         return;
      }

      msg.e_command = eFSC_DELETE_DIR;
      msg.u32_sizeOfData = MIN((uint32_t)strlen(arg), (uint32_t)FS_MAX_CHUNK_SIZE);
      (void)memcpy(msg.u8_data, arg, msg.u32_sizeOfData);
   }
   else if (strcmp(cptr_rxLine, "close") == 0)
   {
      msg.e_command = eFSC_CLOSE_FILE;
   }
   else if (strcmp(cptr_rxLine, "abort") == 0)
   {
      msg.e_command = eFSC_ABORT;
   }
   else
   {
      LOG_ERR("Unknown command: %s", cptr_rxLine);
      sv_PrintHelp();
      return;
   }

   ret = si_EnqueueMsg(&msg);
   if (ret != 0)
   {
      LOG_ERR("Queue put failed (%d)", ret);
      return;
   }

   LOG_INF("Queued cmd=%d size=%u", msg.e_command, msg.u32_sizeOfData);
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @private       sv_UartTestHarness_Thread
 * @brief         The thread function for creating a UART test harness that
 *                reads commands from the console and sends them to the File
 *                System Manager FSM for testing.
 * @param[in]     vptr1 - Unused parameter for thread entry function.
 * @param[in]     vptr2 - Unused parameter for thread entry function.
 * @param[in]     vptr3 - Unused parameter for thread entry function.
 * @param[out]    None
 * @param[inout]  None
 * @return        0 on success, negative error code on failure.
 */
static void sv_UartTestHarness_Thread(void *vptr1, void *vptr2, void *vptr3)
{
   ARG_UNUSED(vptr1);
   ARG_UNUSED(vptr2);
   ARG_UNUSED(vptr3);

   char cptr_rxLine[UART_LINE_BUF_SIZE];

   if (!device_is_ready(stpt_consoleUART))
   {
      LOG_ERR("Console UART device is not ready");
      return;
   }

   LOG_INF("FileSysManager UART harness ready. Type 'help'.");

   while (1)
   {
      int len = si_UartReadLine(cptr_rxLine, sizeof(cptr_rxLine));

      if (len > 0)
      {
         sv_HandleLine(cptr_rxLine);
      }
   }
}

/**
 * @def           sv_UartTestHarness_Thread registeration
 * @brief         This macro registers the UART Test Harness thread with the
 *                Zephyr kernel.
 */
K_THREAD_DEFINE(uart_test_harness_tid, UART_TEST_HARNESS_THREAD_STACK_SIZE,
                sv_UartTestHarness_Thread, NULL, NULL, NULL,
                UART_TEST_HARNESS_THREAD_PRIORITY, 0, 0);


/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:<Author of C file - Name [Initials]>
 */
