/**
 * @file          CSRAvailableFSM.c
 * @brief         Source file containing FSM for CSR available state of MainFSM.
 * @date          17/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "CSRAvailableFSM.h"

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
static sl_status_t st_UpdateCSRDetailsOverGATT(void);

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
 * @var           gst_CSRAvailableFSMContext
 * @brief         Context for CSR available FSM.
 */
FSMContext_T gst_CSRAvailableFSMContext = {
   .vpt_customData = NULL
};

/**
 * @var           gst_CSRAvailableFSMInstance
 * @brief         Instance for CSR available FSM.
 */
FSMInstance_T gst_CSRAvailableFSMInstance = {
   .stpt_FSMContext = &gst_CSRAvailableFSMContext,
   .stpt_currentState = &gstar_CSRAvailableFSMStateAttrs[0],
   .fpt_unhandledEventCb = NULL,
};

/**
 * @var           gst_CSRAvailableFSMEvent
 * @brief         Event for CSR available state.
 */
FSMEvent_T gst_CSRAvailableFSMEvent = { 0 };

/**
 * @var           gstar_CSRAvailableFSMStateAttrs
 * @brief         State attributes for CSR available FSM.
 */
FSMStateAttr_T gstar_CSRAvailableFSMStateAttrs[] = {
   [eCAS_WAITING_FOR_SYSTEM_BOOT] = {
      .fpt_handler = gb_WaitingForSystemBoot_CSRAvailableFSM_Handler,
      .u8_stateID = eCAS_WAITING_FOR_SYSTEM_BOOT
   },
   [eCAS_WAITING_FOR_CONNECTION_OPEN] = {
      .fpt_handler = gb_WaitingForConnectionOpen_CSRAvailableFSM_Handler,
      .u8_stateID = eCAS_WAITING_FOR_CONNECTION_OPEN
   },
   [eCAS_WAITING_FOR_DEVICE_PROVISIONING_TO_COMPLETE] = {
      .fpt_handler = gb_WaitingForDeviceProvision2Complete_CSRAvailableFSM_Handler,
      .u8_stateID = eCAS_WAITING_FOR_DEVICE_PROVISIONING_TO_COMPLETE
   },
   [eCAS_DEVICE_PROVISIONING_COMPLETED] = {
      .fpt_handler = gb_DeviceProvisioningCompleted_CSRAvailableFSM_Handler,
      .u8_stateID = eCAS_DEVICE_PROVISIONING_COMPLETED
   },
   [eCAS_WAITING_FOR_CONNECTION_CLOSED] = {
      .fpt_handler = gb_WaitingForConnectionClosed_CSRAvailableFSM_Handler,
      .u8_stateID = eCAS_WAITING_FOR_CONNECTION_CLOSED
   }
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

/**
 * @var           su8_connHandle
 * @brief         The connection handle allocated from Bluetooth stack.
 */
static uint8_t su8_connHandle;

/**
 * @var           sstpt_CSROutput
 * @brief         Pointer to the CSROutput_T structure in RAM.
 */
static volatile CSROutput_T * const sstpt_CSROutput = &gst_CSROutput;

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
 * @private       st_UpdateCSRDetailsOverGATT
 * @brief         It updates the CSR details over GATT.
 * @return        SL_STATUS_OK upon successfully completing the operation,
 *                or any of the error codes from sl_status_t.
 */
static sl_status_t st_UpdateCSRDetailsOverGATT(void)
{
   sl_status_t t_retVal = SL_STATUS_FAIL;

   // Set the GATT database with CSR generated
   t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_CSR_BLOCK1, \
      0, gattdb_CSR_BLOCK1_len, &sstpt_CSROutput->u8ar_CSR[0]);
   app_assert_status(t_retVal);
   t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_CSR_BLOCK2, \
      0, gattdb_CSR_BLOCK2_len, &sstpt_CSROutput->u8ar_CSR[MTU_SIZE]);
   app_assert_status(t_retVal);
   t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_CSR_BLOCK3, \
      0, gattdb_CSR_BLOCK3_len, &sstpt_CSROutput->u8ar_CSR[(2*MTU_SIZE)]);
   app_assert_status(t_retVal);
   t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_CSR_BLOCK4, \
      0, gattdb_CSR_BLOCK4_len, &sstpt_CSROutput->u8ar_CSR[(3*MTU_SIZE)]);
   app_assert_status(t_retVal);
   t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_CSR_BLOCK5, \
      0, gattdb_CSR_BLOCK5_len, &sstpt_CSROutput->u8ar_CSR[(4*MTU_SIZE)]);
   app_assert_status(t_retVal);

   // Set the generated CSR length
   t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_CSR_LENGTH, \
      0, gattdb_CSR_LENGTH_len, &sstpt_CSROutput->u16_CSRLen);
   app_assert_status(t_retVal);

   // Set the CSR generation flag
   t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_IS_CSR_GENERATED, \
      0, gattdb_IS_CSR_GENERATED_len, &sstpt_CSROutput->u8_isCSRGenerated);
   app_assert_status(t_retVal);

   return t_retVal;
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gb_WaitingForSystemBoot_CSRAvailableFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForSystemBoot_CSRAvailableFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_status_t t_retVal;
   // uint8_t u8_smConfigFlags = (SL_BT_SM_CONFIGURATION_MITM_REQUIRED);
   // sl_bt_sm_io_capability_t e_ioCapability = sl_bt_sm_io_capability_keyboarddisplay;
   bd_addr st_address;
   uint8_t u8_addrType;
   uint16_t u16_maxMTU;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

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
      // Switch through all the allowed events
      switch (SL_BT_MSG_ID(stpt_evt->header))
      {
         // This event indicates the device has started and the radio is ready.
         case sl_bt_evt_system_boot_id:
         {
            // Set the max MTU as 192 bytes.
            t_retVal = sl_bt_gatt_server_set_max_mtu(MTU_SIZE, &u16_maxMTU);
            app_assert_status(t_retVal);

            // Check if the max MTU was set correctly with the desired value
            if ((SL_STATUS_OK == t_retVal) && (MTU_SIZE == u16_maxMTU))
            {
               // Update the CSR details over GATT
               t_retVal = st_UpdateCSRDetailsOverGATT();
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

            // Write the BLE address over GATT
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

            // Check if advertising started successfully
            if (SL_STATUS_OK == t_retVal)
            {
               // Transition to next state
               gv_FSM_Transition(&gst_CSRAvailableFSMInstance, \
                  &gstar_CSRAvailableFSMStateAttrs[eCAS_WAITING_FOR_CONNECTION_OPEN]);
            }

            // Mark the event as handled
            b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForSystemBoot_CSRAvailableFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }

   return b_retVal;
}

/**
 * @public        gb_WaitingForConnectionOpen_CSRAvailableFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForConnectionOpen_CSRAvailableFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

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
      // Switch through all the allowed events
      switch (SL_BT_MSG_ID(stpt_evt->header))
      {
         // This event indicates that a new connection was opened.
         case sl_bt_evt_connection_opened_id:
         {
            // Check if the connection handle is not assigned
            if (!su8_connHandle)
            {
               su8_connHandle = stpt_evt->data.evt_connection_opened.connection;
            }

            app_log_info("Connection Opened As: %s" , \
               ((sl_bt_connection_role_peripheral == stpt_evt->data.evt_connection_opened.role) \
                  ? ("Peripheral") : ("Central")));
            app_log_nl_info();

            app_log_info("Connected BLE device address: 0x");
            for (uint8_t u8_lpIdx = 0; u8_lpIdx < 6; u8_lpIdx++)
            {
               app_log_append_info("%02X ", stpt_evt->data.evt_connection_opened.address.addr[(sizeof(stpt_evt->data.evt_connection_opened.address.addr)-1)-u8_lpIdx]);
            }
            app_log_nl_info();

            // Transition to next state
            gv_FSM_Transition(&gst_CSRAvailableFSMInstance, \
               &gstar_CSRAvailableFSMStateAttrs[eCAS_WAITING_FOR_DEVICE_PROVISIONING_TO_COMPLETE]);

            // Mark the event as handled
            b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForConnectionOpen_CSRAvailableFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitingForDeviceProvision2Complete_CSRAvailableFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForDeviceProvision2Complete_CSRAvailableFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

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
      // Switch through all the allowed events
      switch (SL_BT_MSG_ID(stpt_evt->header))
      {
         // This event indicates a GATT server attribute value has changed.
         case sl_bt_evt_gatt_server_attribute_value_id:
         {
            gst_deviceProvisioningFSMEvent.vpt_eventParam = (sl_bt_msg_t *)stpt_evt;

            // Check if device provisioning FSM handled successfully
            if (true == gb_FSM_Dispatch(&gst_deviceProvisioningFSMInstance, &gst_deviceProvisioningFSMEvent))
            {
               // Check if the completion flag has been raised by the device provisioning FSM
               if (1 == gstar_deviceProvisioningFSMStateAttrs[eDPS_WAIT_FOR_WRITING_CERT].u32_flags)
               {
                  // Raise the flag indicating the device provisioning completed
                  gstar_CSRAvailableFSMStateAttrs[eCAS_WAITING_FOR_DEVICE_PROVISIONING_TO_COMPLETE].u32_flags = 1;

                  // Transition to next state
                  gv_FSM_Transition(&gst_CSRAvailableFSMInstance, \
                     &gstar_CSRAvailableFSMStateAttrs[eCAS_DEVICE_PROVISIONING_COMPLETED]);

                  // Mark the event as handled
                  b_retVal = true;
               }
            }
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForDeviceProvision2Complete_CSRAvailableFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_DeviceProvisioningCompleted_CSRAvailableFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_DeviceProvisioningCompleted_CSRAvailableFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

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
      // State event handling
   }
}

/**
 * @public        gb_WaitingForConnectionClosed_CSRAvailableFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForConnectionClosed_CSRAvailableFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

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
      // State event handling
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
