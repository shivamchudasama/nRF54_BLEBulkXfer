/**
 * @file          MainFSM.c
 * @brief         Source file containing MainFSM.
 * @date          17/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "MainFSM.h"

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

// Declarations of all the structure variables
/**
 * @var           <Variable name>
 * @brief         <Variable details>.
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
 * @var           gst_mainFSMContext
 * @brief         Context for main FSM.
 */
FSMContext_T gst_mainFSMContext = {
    .vpt_customData = NULL
};

/**
 * @var           gst_mainFSMInstance
 * @brief         Instance for main FSM.
 */
FSMInstance_T gst_mainFSMInstance = {
   .stpt_FSMContext = &gst_mainFSMContext,
   .stpt_currentState = &gstar_mainFSMStateAttrs[0],
   .fpt_unhandledEventCb = NULL,
};

/**
 * @var           gst_mainFSMEvent
 * @brief         Event for main state.
 */
FSMEvent_T gst_mainFSMEvent = { 0 };

/**
 * @var           gstar_mainFSMStateAttrs
 * @brief         State attributes for main FSM.
 */
FSMStateAttr_T gstar_mainFSMStateAttrs[] = {
   [eMS_IDLE] = {
      .fpt_handler = gb_Idle_MainFSM_Handler,
      .u8_stateID = eMS_IDLE
   },
   [eMS_CSR_AVAILABLE] = {
      .fpt_handler = gb_CSRAvailable_MainFSM_Handler,
      .u8_stateID = eMS_CSR_AVAILABLE
   },
   [eMS_DEVICE_CERT_VERIFIED] = {
      .fpt_handler = gb_DeviceCertVerified_MainFSM_Handler,
      .u8_stateID = eMS_DEVICE_CERT_VERIFIED
   },
};

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           su8_advSetHandle
 * @brief         The advertising set handle allocated from Bluetooth stack.
 */
static uint8_t su8_advSetHandle = 0xFF;

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

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gb_IDLE_MainFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_Idle_MainFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{

}

/**
 * @public        gb_CSRAvailable_MainFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_CSRAvailable_MainFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_status_t t_retVal;
   // uint8_t u8_smConfigFlags = (SL_BT_SM_CONFIGURATION_MITM_REQUIRED);
   // sl_bt_sm_io_capability_t e_ioCapability = sl_bt_sm_io_capability_keyboarddisplay;
   bd_addr st_address;
   uint8_t u8_addrType;
   uint16_t u16_maxMTU;

   // Check if the current event is entry event
   if (FSM_EVENT_ENTRY == stpt_event->u32_eventID)
   {
      // Entry event handling
   }
   // Check if the current event is exit event
   else if (FSM_EVENT_EXIT == stpt_event->u32_eventID)
   {
      // Set the max MTU as 200 bytes.
      t_retVal = sl_bt_gatt_server_set_max_mtu(MTU_SIZE, &u16_maxMTU);
      app_assert_status(t_retVal);

      // Check if the max MTU was set correctly with the desired value
      if ((SL_STATUS_OK == t_retVal) && (MTU_SIZE == u16_maxMTU))
      {
         // Update the pairing readiness over GATT
         t_retVal = gt_UpdatePairingReadinessOverGATT();
         app_assert_status(t_retVal);
      }

      // Extract own address
      t_retVal = sl_bt_system_get_identity_address(&st_address, &u8_addrType);
      app_assert_status(t_retVal);

      app_log_info("BLE Address: 0x");
      for (uint8_t u8_lpIdx = 0; u8_lpIdx < 6; u8_lpIdx++)
      {
         app_log_append_info("%02X ", st_address.addr[(sizeof(st_address.addr)-1)-u8_lpIdx]);
      }
      app_log_nl_info();

      // Set the BLE address
      t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_BLE_MAC_ID, \
         0, gattdb_BLE_MAC_ID_len, &st_address.addr[0]);
      app_assert_status(t_retVal);

      // // Configure SM
      // t_retVal = sl_bt_sm_configure(u8_smConfigFlags, e_ioCapability);
      // app_assert_status(t_retVal);

      // Create an advertising set.
      t_retVal = sl_bt_advertiser_create_set(&su8_advSetHandle);
      app_assert_status(t_retVal);

      // Generate data for advertising (stack-generated)
      t_retVal = sl_bt_legacy_advertiser_generate_data(su8_advSetHandle, \
         sl_bt_advertiser_general_discoverable);
      app_assert_status(t_retVal);

      // Set advertising interval to 100ms.
      t_retVal = sl_bt_advertiser_set_timing(
         su8_advSetHandle, // Advertising set handle
         160,              // min. adv. interval (milliseconds * 1.6)
         160,              // max. adv. interval (milliseconds * 1.6)
         0,                // adv. duration
         0);               // max. num. adv. events
      app_assert_status(t_retVal);

      app_log_info("Advertising with BATL Device Provisioning service...." APP_LOG_NL);

      // Start advertising and enable connections.
      t_retVal = sl_bt_legacy_advertiser_start(su8_advSetHandle, \
         sl_bt_legacy_advertiser_connectable);
      app_assert_status(t_retVal);

      // Check if the advertisement started successfully
      if (SL_STATUS_OK == t_retVal)
      {
         // Mark the event as handled
         b_retVal = true;
      }
   }
   else
   {
      // Populate the event for CSR available FSM
      gst_CSRAvailableFSMEvent.vpt_eventParam = ((sl_bt_msg_t *)stpt_event->vpt_eventParam);

      // Check if the CSR available FSM handled successfully
      if (true == gb_FSM_Dispatch(&gst_CSRAvailableFSMInstance, &gst_CSRAvailableFSMEvent))
      {
         // Check if the completion flag has been raised by the device provisioning FSM
         if (1 == gstar_CSRAvailableFSMStateAttrs[eCAS_WAITING_FOR_DEVICE_PROVISIONING_TO_COMPLETE].u32_flags)
         {
            // Raise the flag indicating the device provisioning completed
            gstar_mainFSMStateAttrs[eMS_CSR_AVAILABLE].u32_flags = 1;

            // Transition to next state
            gv_FSM_Transition(&gst_mainFSMInstance, \
               &gstar_mainFSMStateAttrs[eMS_DEVICE_CERT_VERIFIED]);

            // Mark the event as handled
            b_retVal = true;
         }
      }
   }
}

/**
 * @public        gb_DeviceCertVerified_MainFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_DeviceCertVerified_MainFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;

   // Check if the current event is entry event
   if (FSM_EVENT_ENTRY == stpt_event->u32_eventID)
   {
      // Entry event handling
   }
   // Check if the current event is exit event
   else if (FSM_EVENT_EXIT == stpt_event->u32_eventID)
   {
      // Exit event handling
   }
   else
   {
      // Populate the event for pairing FSM
      gst_pairingFSMEvent.vpt_eventParam = ((sl_bt_msg_t *)stpt_event->vpt_eventParam);

      // Check if the pairing FSM handled successfully
      if (true == gb_FSM_Dispatch(&gst_pairingFSMInstance, &gst_pairingFSMEvent))
      {
         // Check if the completion flag has been raised by the pairing FSM
         if (1 == gstar_pairingFSMStateAttrs[ePS_DEVICE_PAIRED].u32_flags)
         {
            // // Raise the flag indicating the pairing completed
            // gstar_mainFSMStateAttrs[eMS_CSR_AVAILABLE].u32_flags = 1;

            // // Transition to next state
            // gv_FSM_Transition(&gst_mainFSMInstance, \
            //    &gstar_mainFSMStateAttrs[eMS_DEVICE_CERT_VERIFIED]);

            // // Mark the event as handled
            // b_retVal = true;
         }
      }
   }
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
