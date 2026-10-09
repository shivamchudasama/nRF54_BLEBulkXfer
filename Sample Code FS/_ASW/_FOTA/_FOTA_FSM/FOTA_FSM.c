/**
 * @file          FOTA_FSM.c
 * @brief         Source file containing SMF implementation for FOTA service.
 * @date          13/02/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "FOTA_FSM.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           FOTAEventChannelSub
 * @brief         Subscribber for FOTAEventChannel with queue size of 4.
 */
ZBUS_SUBSCRIBER_DEFINE(FOTAEventChannelSub, 4);

/**
 * @def           FOTA_STATE_MACHINE_THREAD_STACK_SIZE
 * @brief         Stack size of the thread running the FOTA state machine.
 */
#define FOTA_STATE_MACHINE_THREAD_STACK_SIZE (1024 * 2)

/**
 * @def           FOTA_STATE_MACHINE_THREAD_PRIO
 * @brief         Priority of the thread running the FOTA state machine.
 */
#define FOTA_STATE_MACHINE_THREAD_PRIO       (3)

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
static void sv_eFS_IDLE_Entry(void *vpt_obj);
static enum smf_state_result se_eFS_IDLE_Run(void *vpt_obj);
static void sv_eFS_IDLE_Exit(void *vpt_obj);

static void sv_eFS_RECEIVING_METADATA_Entry(void *vpt_obj);
static enum smf_state_result se_eFS_RECEIVING_METADATA_Run(void *vpt_obj);
static void sv_eFS_RECEIVING_METADATA_Exit(void *vpt_obj);

static void sv_eFS_RECEIVING_MANIFEST_Entry(void *vpt_obj);
static enum smf_state_result se_eFS_RECEIVING_MANIFEST_Run(void *vpt_obj);
static void sv_eFS_RECEIVING_MANIFEST_Exit(void *vpt_obj);

static void sv_eFS_RECEIVING_DATA_Entry(void *vpt_obj);
static enum smf_state_result se_eFS_RECEIVING_DATA_Run(void *vpt_obj);
static void sv_eFS_RECEIVING_DATA_Exit(void *vpt_obj);

static void sv_eFS_VALIDATE_IMAGE_Entry(void *vpt_obj);
static enum smf_state_result se_eFS_VALIDATE_IMAGE_Run(void *vpt_obj);
static void sv_eFS_VALIDATE_IMAGE_Exit(void *vpt_obj);

static void sv_eFS_STAGE_IMAGE_Entry(void *vpt_obj);
static enum smf_state_result se_eFS_STAGE_IMAGE_Run(void *vpt_obj);
static void sv_eFS_STAGE_IMAGE_Exit(void *vpt_obj);

static void sv_eFS_COMPLETED_Entry(void *vpt_obj);
static enum smf_state_result se_eFS_COMPLETED_Run(void *vpt_obj);
static void sv_eFS_COMPLETED_Exit(void *vpt_obj);

static void sv_eFS_ABORT_Entry(void *vpt_obj);
static enum smf_state_result se_eFS_ABORT_Run(void *vpt_obj);
static void sv_eFS_ABORT_Exit(void *vpt_obj);

static void sv_FOTAStateMachineThread(void *vpt_entryParam1, void *vpt_entryParam2,
   void *vpt_entryParam3);

/******************************************************************************/
/*                                                                            */
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/
extern struct k_msgq FSMGR_MSG_Q;

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
 * @var           gst_FOTAStates
 * @brief         FOTA state machine states array.
 */
const struct smf_state gst_FOTAStates[eFS_STATE_MAX] =
{
   [eFS_IDLE] = SMF_CREATE_STATE(sv_eFS_IDLE_Entry, se_eFS_IDLE_Run, sv_eFS_IDLE_Exit, NULL, NULL),
   [eFS_RECEIVING_MANIFEST] = SMF_CREATE_STATE(sv_eFS_RECEIVING_MANIFEST_Entry, se_eFS_RECEIVING_MANIFEST_Run, sv_eFS_RECEIVING_MANIFEST_Exit, NULL, NULL),
   [eFS_RECEIVING_METADATA] = SMF_CREATE_STATE(sv_eFS_RECEIVING_METADATA_Entry, se_eFS_RECEIVING_METADATA_Run, sv_eFS_RECEIVING_METADATA_Exit, NULL, NULL),
   [eFS_RECEIVING_DATA] = SMF_CREATE_STATE(sv_eFS_RECEIVING_DATA_Entry, se_eFS_RECEIVING_DATA_Run, sv_eFS_RECEIVING_DATA_Exit, NULL, NULL),
   [eFS_VALIDATE_IMAGE] = SMF_CREATE_STATE(sv_eFS_VALIDATE_IMAGE_Entry, se_eFS_VALIDATE_IMAGE_Run, sv_eFS_VALIDATE_IMAGE_Exit, NULL, NULL),
   [eFS_STAGE_IMAGE] = SMF_CREATE_STATE(sv_eFS_STAGE_IMAGE_Entry, se_eFS_STAGE_IMAGE_Run, sv_eFS_STAGE_IMAGE_Exit, NULL, NULL),
   [eFS_COMPLETED] = SMF_CREATE_STATE(sv_eFS_COMPLETED_Entry, se_eFS_COMPLETED_Run, sv_eFS_COMPLETED_Exit, NULL, NULL),
   [eFS_ABORT] = SMF_CREATE_STATE(sv_eFS_ABORT_Entry, se_eFS_ABORT_Run, sv_eFS_ABORT_Exit, NULL, NULL),
};

/**
 * @var           sst_FOTAStateMachineCtx
 * @brief         FOTA SMF context structure.
 */
static FOTAStateMachineCtx_T sst_FOTAStateMachineCtx = { 0 };

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
// eFS_IDLE state handlers
/**
 * @private       sv_eFS_IDLE_Entry
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
static void sv_eFS_IDLE_Entry(void *vpt_obj)
{
   LOG_INF("sv_eFS_IDLE entry");
}

/**
 * @private       sv_eFS_IDLE_Run
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static enum smf_state_result se_eFS_IDLE_Run(void *vpt_obj)
{
   FOTAStateMachineCtx_T *stpt_FOTAStateMachineCtx = (FOTAStateMachineCtx_T *)vpt_obj;
   enum smf_state_result e_retVal = SMF_EVENT_PROPAGATE;

   LOG_INF("sv_eFS_IDLE running");

   // Check if any event is pending to handle and that is FOTA START command
   if ((stpt_FOTAStateMachineCtx->b_isEventPending) &&
      (eFE_FOTA_START == stpt_FOTAStateMachineCtx->st_FOTAEvent.e_evt))
   {
      // Check if FOTA start signature has been received
      if (FOTA_START_SIGNATURE == *(uint32_t *)(&stpt_FOTAStateMachineCtx->st_FOTAEvent.u8ar_Payload[0]))
      {
         // Clear the event pending flag
         stpt_FOTAStateMachineCtx->b_isEventPending = false;
         LOG_INF("Start request received, transitioning to eFS_RECEIVING_MANIFEST state");
         smf_set_state(SMF_CTX(stpt_FOTAStateMachineCtx), &gst_FOTAStates[eFS_RECEIVING_MANIFEST]);
      }
      else
      {
         LOG_ERR("Invalid start signature.");
      }

      e_retVal = SMF_EVENT_HANDLED;
   }
   else
   {
      LOG_ERR("Invalid activity");
   }

   return e_retVal;
}

/**
 * @private       sv_eFS_IDLE_Exit
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_IDLE_Exit(void *vpt_obj)
{
   LOG_INF("sv_eFS_IDLE exit");
}

// eFS_RECEIVING_MANIFEST state handlers
/**
 * @private       sv_eFS_RECEIVING_MANIFEST_Entry
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_RECEIVING_MANIFEST_Entry(void *vpt_obj)
{
   LOG_INF("sv_eFS_RECEIVING_MANIFEST entry");
}

/**
 * @private       sv_eFS_RECEIVING_MANIFEST_Run
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static enum smf_state_result se_eFS_RECEIVING_MANIFEST_Run(void *vpt_obj)
{
   FOTAStateMachineCtx_T *stpt_FOTAStateMachineCtx = (FOTAStateMachineCtx_T *)vpt_obj;
   enum smf_state_result e_retVal = SMF_EVENT_PROPAGATE;
   CPList_T st_CPList = { 0 };
   TPError_E e_TPError = eTP_OK;
   FileSysMessage_T st_FilesysMsg = { 0 };
   uint8_t u8_lpIdx = 0;

   LOG_INF("sv_eFS_RECEIVING_MANIFEST running");

   // Check if any event is pending to handle and that is MANIFEST command
   if ((stpt_FOTAStateMachineCtx->b_isEventPending) &&
      (eFE_MANIFEST == stpt_FOTAStateMachineCtx->st_FOTAEvent.e_evt))
   {
      // Clear the event pending flag
      stpt_FOTAStateMachineCtx->b_isEventPending = false;
      // As there might be more than one manifest packets received. We need to parst it.
      e_TPError = ge_TP_ParseCPList(&stpt_FOTAStateMachineCtx->st_FOTAEvent.u8ar_Payload[0],
         stpt_FOTAStateMachineCtx->st_FOTAEvent.u16_payloadLength, &st_CPList);

      // Check if there is any error in parsing CP list
      if (eTP_OK != e_TPError)
      {
         LOG_ERR("Failed to parse CP list. Error code: %x", e_TPError);
         LOG_HEXDUMP_INF(&stpt_FOTAStateMachineCtx->st_FOTAEvent.u8ar_Payload[0],
            stpt_FOTAStateMachineCtx->st_FOTAEvent.u16_payloadLength,
            "Received FOTA Event payload: ");
      }

      // As this is a manifest, there are multiple CP blocks in it. After parsing
      // them, we need to drive the FS FSM.

      // Prepare FS command for creating a directory for the new firmware update
      st_FilesysMsg.e_command = eFSC_MAKE_DIR;
      st_FilesysMsg.u32_sizeOfData = strlen("VCU");
      strncpy((char *)st_FilesysMsg.u8_data, "VCU", strlen("VCU"));

      // Send the command to FS FSM through message queue
      k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);

      // Prepare FS command for creating a file for writing down the manifest data
      st_FilesysMsg.e_command = eFSC_OPEN_FILE_WRITE;
      st_FilesysMsg.u32_sizeOfData = strlen("Manifest.txt");
      strncpy((char *)st_FilesysMsg.u8_data, "Manifest.txt", strlen("Manifest.txt"));

      // Send the command to FS FSM through message queue
      k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);

      // Note: Here the first CP block was ECU name, which is already taken care of by creating a directory
      // Loop through the CP blocks in the manifest and send FS commands to write each of them
      for (u8_lpIdx = 1; u8_lpIdx < st_CPList.u8_totCPBlock; u8_lpIdx++)
      {
         // Prepare FS command for writing manifest data file
         st_FilesysMsg.e_command = eFSC_WRITE_DATA;
         st_FilesysMsg.u32_sizeOfData = st_CPList.star_CPBlocks[u8_lpIdx].u8_CPBlockLength + 2;
         st_FilesysMsg.u8_data[0] = st_CPList.star_CPBlocks[u8_lpIdx].u8_CPBlockLength + 1;
         st_FilesysMsg.u8_data[1] = st_CPList.star_CPBlocks[u8_lpIdx].e_CPType;
         memcpy(&st_FilesysMsg.u8_data[2], &st_CPList.star_CPBlocks[u8_lpIdx].u8ar_CPData[0], st_FilesysMsg.u32_sizeOfData);

         // Send the command to FS FSM through message queue
         k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);
      }

#if 0
      // Prepare FS command for writing manifest data file (Image size)
      st_FilesysMsg.e_command = eFSC_WRITE_DATA;
      st_FilesysMsg.u32_sizeOfData = st_CPList.star_CPBlocks[1].u8_CPBlockLength + 2;
      st_FilesysMsg.u8_data[0] = st_CPList.star_CPBlocks[1].u8_CPBlockLength + 1;
      st_FilesysMsg.u8_data[1] = st_CPList.star_CPBlocks[1].e_CPType;
      memcpy(&st_FilesysMsg.u8_data[2], &st_CPList.star_CPBlocks[1].u8ar_CPData[0], st_FilesysMsg.u32_sizeOfData);

      // Send the command to FS FSM through message queue
      k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);

      // Prepare FS command for writing manifest data file (FW version)
      st_FilesysMsg.e_command = eFSC_WRITE_DATA;
      st_FilesysMsg.u32_sizeOfData = st_CPList.star_CPBlocks[2].u8_CPBlockLength + 2;
      st_FilesysMsg.u8_data[0] = st_CPList.star_CPBlocks[2].u8_CPBlockLength + 1;
      st_FilesysMsg.u8_data[1] = st_CPList.star_CPBlocks[2].e_CPType;
      memcpy(&st_FilesysMsg.u8_data[2], &st_CPList.star_CPBlocks[2].u8ar_CPData[0], st_FilesysMsg.u32_sizeOfData);

      // Send the command to FS FSM through message queue
      k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);

      // Prepare FS command for writing manifest data file (Signature)
      st_FilesysMsg.e_command = eFSC_WRITE_DATA;
      st_FilesysMsg.u32_sizeOfData = st_CPList.star_CPBlocks[3].u8_CPBlockLength + 2;
      st_FilesysMsg.u8_data[0] = st_CPList.star_CPBlocks[3].u8_CPBlockLength + 1;
      st_FilesysMsg.u8_data[1] = st_CPList.star_CPBlocks[3].e_CPType;
      memcpy(&st_FilesysMsg.u8_data[2], &st_CPList.star_CPBlocks[3].u8ar_CPData[0], st_FilesysMsg.u32_sizeOfData);

      // Send the command to FS FSM through message queue
      k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);
#endif // 0

      // Prepare FS command for closing the manifest file after writing all the manifest data
      st_FilesysMsg.e_command = eFSC_CLOSE_FILE;

      // Send the command to FS FSM through message queue
      k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);

      LOG_INF("Manifest received, transitioning to eFS_RECEIVING_METADATA state");
      smf_set_state(SMF_CTX(stpt_FOTAStateMachineCtx), &gst_FOTAStates[eFS_RECEIVING_METADATA]);

      e_retVal = SMF_EVENT_HANDLED;
   }
   else
   {
      LOG_ERR("Invalid activity");
   }

   return e_retVal;
}

/**
 * @private       sv_eFS_RECEIVING_MANIFEST_Exit
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_RECEIVING_MANIFEST_Exit(void *vpt_obj)
{
   LOG_INF("sv_eFS_RECEIVING_MANIFEST exit");
}

// eFS_RECEIVING_METADATA state handlers
/**
 * @private       sv_eFS_RECEIVING_METADATA_Entry
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_RECEIVING_METADATA_Entry(void *vpt_obj)
{
   LOG_INF("sv_eFS_RECEIVING_METADATA entry");
}

/**
 * @private       sv_eFS_RECEIVING_METADATA_Run
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static enum smf_state_result se_eFS_RECEIVING_METADATA_Run(void *vpt_obj)
{
   FOTAStateMachineCtx_T *stpt_FOTAStateMachineCtx = (FOTAStateMachineCtx_T *)vpt_obj;
   enum smf_state_result e_retVal = SMF_EVENT_PROPAGATE;
   CPList_T st_CPList = { 0 };
   TPError_E e_TPError = eTP_OK;
   FileSysMessage_T st_FilesysMsg = { 0 };
   uint8_t u8_lpIdx = 0;

   LOG_INF("sv_eFS_RECEIVING_METADATA running");

   // Check if any event is pending to handle and that is METADATA command
   if ((stpt_FOTAStateMachineCtx->b_isEventPending) &&
      (eFE_METADATA == stpt_FOTAStateMachineCtx->st_FOTAEvent.e_evt))
   {
      // Clear the event pending flag
      stpt_FOTAStateMachineCtx->b_isEventPending = false;
      // As there might be more than one metadata packets received. We need to parst it.
      e_TPError = ge_TP_ParseCPList(&stpt_FOTAStateMachineCtx->st_FOTAEvent.u8ar_Payload[0],
         stpt_FOTAStateMachineCtx->st_FOTAEvent.u16_payloadLength, &st_CPList);

      // Check if there is any error in parsing CP list
      if (eTP_OK != e_TPError)
      {
         LOG_ERR("Failed to parse CP list. Error code: %x", e_TPError);
         LOG_HEXDUMP_INF(&stpt_FOTAStateMachineCtx->st_FOTAEvent.u8ar_Payload[0],
            stpt_FOTAStateMachineCtx->st_FOTAEvent.u16_payloadLength,
            "Received FOTA Event payload: ");
      }

      // As this is a metadata, there are multiple CP blocks in it. After parsing
      // them, we need to drive the FS FSM.

      // Prepare FS command for creating a file for writing down the metadata data
      st_FilesysMsg.e_command = eFSC_OPEN_FILE_WRITE;
      st_FilesysMsg.u32_sizeOfData = strlen("Metadata.txt");
      strncpy((char *)st_FilesysMsg.u8_data, "Metadata.txt", strlen("Metadata.txt"));

      // Send the command to FS FSM through message queue
      k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);

      // Loop through the CP blocks in the metadata and send FS commands to write each of them
      for (u8_lpIdx = 0; u8_lpIdx < st_CPList.u8_totCPBlock; u8_lpIdx++)
      {
         // Prepare FS command for writing metadata data file
         st_FilesysMsg.e_command = eFSC_WRITE_DATA;
         st_FilesysMsg.u32_sizeOfData = st_CPList.star_CPBlocks[u8_lpIdx].u8_CPBlockLength + 2;
         st_FilesysMsg.u8_data[0] = st_CPList.star_CPBlocks[u8_lpIdx].u8_CPBlockLength + 1;
         st_FilesysMsg.u8_data[1] = st_CPList.star_CPBlocks[u8_lpIdx].e_CPType;
         memcpy(&st_FilesysMsg.u8_data[2], &st_CPList.star_CPBlocks[u8_lpIdx].u8ar_CPData[0], st_FilesysMsg.u32_sizeOfData);

         // Send the command to FS FSM through message queue
         k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);
      }

      // Prepare FS command for closing the metadata file after writing all the metadata data
      st_FilesysMsg.e_command = eFSC_CLOSE_FILE;

      // Send the command to FS FSM through message queue
      k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);

      LOG_INF("Metadata received, transitioning to eFS_RECEIVING_DATA state");
      smf_set_state(SMF_CTX(stpt_FOTAStateMachineCtx), &gst_FOTAStates[eFS_RECEIVING_DATA]);

      e_retVal = SMF_EVENT_HANDLED;
   }
   else
   {
      LOG_ERR("Invalid activity");
   }

   return e_retVal;
}

/**
 * @private       sv_eFS_RECEIVING_METADATA_Exit
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_RECEIVING_METADATA_Exit(void *vpt_obj)
{
   LOG_INF("sv_eFS_RECEIVING_METADATA exit");
}

// eFS_RECEIVING_DATA state handlers
/**
 * @private       sv_eFS_RECEIVING_DATA_Entry
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_RECEIVING_DATA_Entry(void *vpt_obj)
{
   LOG_INF("sv_eFS_RECEIVING_DATA entry");
}

/**
 * @private       sv_eFS_RECEIVING_DATA_Run
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static enum smf_state_result se_eFS_RECEIVING_DATA_Run(void *vpt_obj)
{
   FOTAStateMachineCtx_T *stpt_FOTAStateMachineCtx = (FOTAStateMachineCtx_T *)vpt_obj;
   enum smf_state_result e_retVal = SMF_EVENT_PROPAGATE;
   static uint32_t slu32_totalDataReceived = 0;
   int64_t u64_delta = 0;
   static int64_t slu64_msgRef = 0;
   static bool slb_msgRefInit = false;
   FileSysMessage_T st_FilesysMsg = { 0 };
   static uint8_t slu8_maxConsumedBuff = 0;

   LOG_INF("sv_eFS_RECEIVING_DATA running");

   // Check if any event is pending to handle and that is FOTA DATA command
   if ((stpt_FOTAStateMachineCtx->b_isEventPending) &&
      (eFE_FOTA_DATA == stpt_FOTAStateMachineCtx->st_FOTAEvent.e_evt))
   {
      // Check if the message reference timestamp isn't initialized.
      if (!slb_msgRefInit)
      {
         slu64_msgRef = k_uptime_get();
         slb_msgRefInit = true;
         LOG_INF("First data received at %lld ms", (long long)slu64_msgRef);

         // Prepare FS command for creating a file for writing down the metadata data
         st_FilesysMsg.e_command = eFSC_OPEN_FILE_WRITE;
         st_FilesysMsg.u32_sizeOfData = strlen("1.txt");
         strncpy((char *)st_FilesysMsg.u8_data, "1.txt", strlen("1.txt"));

         // Send the command to FS FSM through message queue
         k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);
      }

      // LOG_INF("Data received, transitioning to eFS_VALIDATE_IMAGE state");
      slu32_totalDataReceived += stpt_FOTAStateMachineCtx->st_FOTAEvent.u16_payloadLength;

      // LOG_HEXDUMP_INF(&stpt_FOTAStateMachineCtx->st_FOTAEvent.u8pt_payload[0],
      //    stpt_FOTAStateMachineCtx->st_FOTAEvent.u16_payloadLength,
      //    "Received FOTA Data chunk: ");

      // // Prepare FS command for creating a file for writing down the metadata data
      // st_FilesysMsg.e_command = eFSC_OPEN_FILE_WRITE;
      // st_FilesysMsg.u32_sizeOfData = strlen("1.txt");
      // strncpy((char *)st_FilesysMsg.u8_data, "1.txt", strlen("1.txt"));

      // // Send the command to FS FSM through message queue
      // k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);

      if (gu8_consumedBuff > slu8_maxConsumedBuff)
      {
         slu8_maxConsumedBuff = gu8_consumedBuff;
      }
      LOG_INF("Currently consumed buffer: %u", gu8_consumedBuff);
      gu8_consumedBuff = 0;

      // Prepare FS command for writing metadata data file
      st_FilesysMsg.e_command = eFSC_WRITE_DATA;
      st_FilesysMsg.u32_sizeOfData = stpt_FOTAStateMachineCtx->st_FOTAEvent.u16_payloadLength;
      memcpy(&st_FilesysMsg.u8_data[0], stpt_FOTAStateMachineCtx->st_FOTAEvent.u8pt_payload, st_FilesysMsg.u32_sizeOfData);

      // Send the command to FS FSM through message queue
      k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);

      // // Prepare FS command for closing the metadata file after writing all the metadata data
      // st_FilesysMsg.e_command = eFSC_CLOSE_FILE;

      // // Send the command to FS FSM through message queue
      // k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);

      // Free the allocated memory for FOTA data chunk back to slab
      k_mem_slab_free(&gst_FOTASlab, stpt_FOTAStateMachineCtx->st_FOTAEvent.u8pt_payload);

      LOG_INF("Total data received: %d bytes", slu32_totalDataReceived);

      // Note: Drive the FS FSM on receiving each data packet.

      // Check if the all the data received
      if (403376 == slu32_totalDataReceived)
      {
         // Prepare FS command for closing the metadata file after writing all the metadata data
         st_FilesysMsg.e_command = eFSC_CLOSE_FILE;

         // Send the command to FS FSM through message queue
         k_msgq_put(&FSMGR_MSG_Q, &st_FilesysMsg, K_NO_WAIT);

         u64_delta = k_uptime_delta(&slu64_msgRef);
         LOG_INF("Total time to receive all data=%lld ms (now=%lld ms)", (long long)u64_delta, (long long)slu64_msgRef);
         LOG_INF("Maximum consumed buffer: %u", slu8_maxConsumedBuff);

         smf_set_state(SMF_CTX(stpt_FOTAStateMachineCtx), &gst_FOTAStates[eFS_VALIDATE_IMAGE]);
      }

      e_retVal = SMF_EVENT_HANDLED;
   }
   else
   {
      LOG_ERR("Invalid activity");
   }

   return e_retVal;
}

/**
 * @private       sv_eFS_RECEIVING_DATA_Exit
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_RECEIVING_DATA_Exit(void *vpt_obj)
{
   LOG_INF("sv_eFS_RECEIVING_DATA exit");
}

// eFS_VALIDATE_IMAGE state handlers
/**
 * @private       sv_eFS_VALIDATE_IMAGE_Entry
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_VALIDATE_IMAGE_Entry(void *vpt_obj)
{
   LOG_INF("sv_eFS_VALIDATE_IMAGE entry");
}

/**
 * @private       sv_eFS_VALIDATE_IMAGE_Run
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static enum smf_state_result se_eFS_VALIDATE_IMAGE_Run(void *vpt_obj)
{
   FOTAStateMachineCtx_T *stpt_FOTAStateMachineCtx = (FOTAStateMachineCtx_T *)vpt_obj;
   enum smf_state_result e_retVal = SMF_EVENT_PROPAGATE;

   LOG_INF("sv_eFS_VALIDATE_IMAGE running");

   // if (stpt_FOTAStateMachineCtx->b_verifyOk)
   // {
   //    LOG_INF("Verify OK received, transitioning to eFS_STAGE_IMAGE state");
   //    smf_set_state(SMF_CTX(stpt_FOTAStateMachineCtx), &gst_FOTAStates[eFS_STAGE_IMAGE]);
   //    e_retVal = SMF_EVENT_HANDLED;
   // }

   return e_retVal;
}

/**
 * @private       sv_eFS_VALIDATE_IMAGE_Exit
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_VALIDATE_IMAGE_Exit(void *vpt_obj)
{
   LOG_INF("sv_eFS_VALIDATE_IMAGE exit");
}

// eFS_STAGE_IMAGE state handlers
/**
 * @private       sv_eFS_STAGE_IMAGE_Entry
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_STAGE_IMAGE_Entry(void *vpt_obj)
{
   LOG_INF("sv_eFS_STAGE_IMAGE entry");
}

/**
 * @private       sv_eFS_STAGE_IMAGE_Run
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static enum smf_state_result se_eFS_STAGE_IMAGE_Run(void *vpt_obj)
{
   FOTAStateMachineCtx_T *stpt_FOTAStateMachineCtx = (FOTAStateMachineCtx_T *)vpt_obj;
   enum smf_state_result e_retVal = SMF_EVENT_PROPAGATE;

   LOG_INF("sv_eFS_STAGE_IMAGE running");

   // if (stpt_FOTAStateMachineCtx->b_verifyOk)
   // {
   //    LOG_INF("Verify OK received, transitioning to eFS_COMPLETED state");
   //    smf_set_state(SMF_CTX(stpt_FOTAStateMachineCtx), &gst_FOTAStates[eFS_COMPLETED]);
   //    e_retVal = SMF_EVENT_HANDLED;
   // }

   return e_retVal;
}

/**
 * @private       sv_eFS_STAGE_IMAGE_Exit
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_STAGE_IMAGE_Exit(void *vpt_obj)
{
   LOG_INF("sv_eFS_STAGE_IMAGE exit");
}

// eFS_COMPLETED state handlers
/**
 * @private       sv_eFS_COMPLETED_Entry
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_COMPLETED_Entry(void *vpt_obj)
{
   LOG_INF("sv_eFS_COMPLETED entry");
}

/**
 * @private       sv_eFS_COMPLETED_Run
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static enum smf_state_result se_eFS_COMPLETED_Run(void *vpt_obj)
{
   FOTAStateMachineCtx_T *stpt_FOTAStateMachineCtx = (FOTAStateMachineCtx_T *)vpt_obj;
   enum smf_state_result e_retVal = SMF_EVENT_PROPAGATE;

   LOG_INF("sv_eFS_COMPLETED running");

   // if (stpt_FOTAStateMachineCtx->b_verifyOk)
   // {
   //    LOG_INF("Verify OK received, transitioning to eFS_IDLE state");
   //    smf_set_state(SMF_CTX(stpt_FOTAStateMachineCtx), &gst_FOTAStates[eFS_IDLE]);
   //    e_retVal = SMF_EVENT_HANDLED;
   // }

   return e_retVal;
}

/**
 * @private       sv_eFS_COMPLETED_Exit
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_COMPLETED_Exit(void *vpt_obj)
{
   LOG_INF("sv_eFS_COMPLETED exit");
}

// eFS_ABORT state handlers
/**
 * @private       sv_eFS_ABORT_Entry
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_ABORT_Entry(void *vpt_obj)
{
   LOG_INF("sv_eFS_ABORT entry");
}

/**
 * @private       sv_eFS_ABORT_Run
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static enum smf_state_result se_eFS_ABORT_Run(void *vpt_obj)
{
   FOTAStateMachineCtx_T *stpt_FOTAStateMachineCtx = (FOTAStateMachineCtx_T *)vpt_obj;
   enum smf_state_result e_retVal = SMF_EVENT_PROPAGATE;

   LOG_INF("sv_eFS_ABORT running");

   // if (stpt_FOTAStateMachineCtx->b_startReq)
   // {
   //    LOG_INF("Start request received, transitioning to eFS_COMPLETED state");
   //    smf_set_state(SMF_CTX(stpt_FOTAStateMachineCtx), &gst_FOTAStates[eFS_COMPLETED]);
   //    e_retVal = SMF_EVENT_HANDLED;
   // }

   return e_retVal;
}

/**
 * @private       sv_eFS_ABORT_Exit
 * @brief         <Function details>.
 * @param[in]     vpt_obj Pointer to user object declared for FOTA state machine.
 * @return        <Return details>.
 */
static void sv_eFS_ABORT_Exit(void *vpt_obj)
{
   LOG_INF("sv_eFS_ABORT exit");
}

/**
 * @private       sv_FOTAStateMachineThread
 * @brief         FOTA thread function. This function is called when the FOTA
 *                thread is running. It waits for publisher to publish events on
 *                FOTAEventChannelSub and reads it if published. It also runs
 *                FOTA state machine.
 * @return        Number of bytes written.
 */
static void sv_FOTAStateMachineThread(void *vpt_entryParam1, void *vpt_entryParam2,
   void *vpt_entryParam3)
{
   ARG_UNUSED(vpt_entryParam1);
   ARG_UNUSED(vpt_entryParam2);
   ARG_UNUSED(vpt_entryParam3);

   const struct zbus_channel *stpt_channel;

   smf_set_initial(SMF_CTX(&sst_FOTAStateMachineCtx), &gst_FOTAStates[eFS_IDLE]);

   while (1)
   {
      // Wait for FOTA events from the FOTAEventChannel and process them
      if (0 == zbus_sub_wait(&FOTAEventChannelSub, &stpt_channel, K_FOREVER))
      {
         // Check if the event is from FOTAEventChannel
         if (&FOTAEventChannel == stpt_channel)
         {
            // Read the latest message from the channel (populate the FOTA state
            // machine with the latest event).
            zbus_chan_read(&FOTAEventChannel, &sst_FOTAStateMachineCtx.st_FOTAEvent, K_FOREVER);

            // Mark the pending event flag
            sst_FOTAStateMachineCtx.b_isEventPending = true;

            // LOG_HEXDUMP_INF(&sst_FOTAStateMachineCtx.st_FOTAEvent.u8ar_Payload[0],
            //    sst_FOTAStateMachineCtx.st_FOTAEvent.u16_payloadLength,
            //    "Received FOTA Event payload: ");

            // Execute the FOTA state machine
            smf_run_state(SMF_CTX(&sst_FOTAStateMachineCtx));
         }
      }
      else
      {
         LOG_ERR("No FOTA Event received from ZBUS channel");
      }
   }
}

/**
 * @def           sv_FOTAStateMachineThread registration
 * @brief         This macro registers the FOTA state machine thread with the
 *                Zephyr kernel.
 */
K_THREAD_DEFINE(FOTAThread, FOTA_STATE_MACHINE_THREAD_STACK_SIZE,
                sv_FOTAStateMachineThread, NULL, NULL, NULL,
                FOTA_STATE_MACHINE_THREAD_PRIO, 0, 0);

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
