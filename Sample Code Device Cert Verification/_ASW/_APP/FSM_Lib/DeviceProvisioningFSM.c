/**
 * @file          DeviceProvisioningFSM.c
 * @brief         Source file containing device provisioning FSM.
 * @date          17/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "DeviceProvisioningFSM.h"

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
 * @var           gst_deviceProvisioningFSMContext
 * @brief         Context for device provisioning FSM.
 */
FSMContext_T gst_deviceProvisioningFSMContext = {
   .vpt_customData = NULL
};

/**
 * @var           gst_DeviceProvisioningFSM
 * @brief         Instance for device provisioning FSM.
 */
FSMInstance_T gst_deviceProvisioningFSMInstance = {
   .stpt_FSMContext = &gst_deviceProvisioningFSMContext,
   .stpt_currentState = &gstar_deviceProvisioningFSMStateAttrs[0],
   .fpt_unhandledEventCb = NULL,
};

/**
 * @var           gst_deviceProvisioningFSMEvent
 * @brief         Event for device provisioning FSM.
 */
FSMEvent_T gst_deviceProvisioningFSMEvent = { 0 };

/**
 * @var           gstar_deviceProvisioningFSMStateAttrs
 * @brief         State attributes for device provisioning FSM.
 */
FSMStateAttr_T gstar_deviceProvisioningFSMStateAttrs[] = {
   [eDPS_WAIT_FOR_CERT_GENERATION] = {
      .fpt_handler = gb_WaitForCertGeneration_DeviceProvisioningFSM_Handler,
      .u8_stateID = eDPS_WAIT_FOR_CERT_GENERATION
   },
   [eDPS_WAIT_FOR_CERT_LENGTH] = {
      .fpt_handler = gb_WaitForCertLength_DeviceProvisioningFSM_Handler,
      .u8_stateID = eDPS_WAIT_FOR_CERT_LENGTH
   },
   [eDPS_WAIT_FOR_CERT] = {
      .fpt_handler = gb_WaitForCert_DeviceProvisioningFSM_Handler,
      .u8_stateID = eDPS_WAIT_FOR_CERT
   },
   [eDPS_WAIT_FOR_WRITING_CERT] = {
      .fpt_handler = gb_WaitForWritingCert_DeviceProvisioningFSM_Handler,
      .u8_stateID = eDPS_WAIT_FOR_WRITING_CERT
   },
   [eDPS_DEVICE_CERT_WRITTEN] = {
      .fpt_handler = gb_DeviceCertWritten_DeviceProvisioningFSM_Handler,
      .u8_stateID = eDPS_DEVICE_CERT_WRITTEN
   }
};

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           su16_receivedCertDERLen
 * @brief         Length of the received device certificate.
 */
static uint16_t su16_receivedCertDERLen = 0;

/**
 * @var           su32_remainingDeviceCertDERLen
 * @brief         Stores the remaining length of the device certificate.
 */
static uint32_t su32_remainingDeviceCertDERLen = 0;

/**
 * @var           su8ar_receivedDeviceCertDER
 * @brief         Buffer to hold the received device certificate in DER format.
 */
static uint8_t su8ar_receivedDeviceCertDER[CHAIN_LINK_DATA_LEN * CHAIN_LINK_DATA_NUM] = { 0 };

/**
 * @var           su16_offset
 * @brief         Stores the offset of received device certificate.
 */
static uint16_t su16_offset = 0;

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
 * @private       <Function name>
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gb_WaitForCertGeneration_DeviceProvisioningFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitForCertGeneration_DeviceProvisioningFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);
   // Get different parameters from the GATT update event
   uint16_t u16_attrHandle;
   uint8_t *u8pt_value;

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
            u16_attrHandle = stpt_evt->data.evt_gatt_server_attribute_value.attribute;
            u8pt_value = stpt_evt->data.evt_gatt_server_attribute_value.value.data;

            // Check if the current event is for certificate generation
            if (gattdb_IS_DEVICE_CERT_GENERATED == u16_attrHandle)
            {
               // Check if the device certificate has been generated successfully
               if ((true == *u8pt_value))
               {
                  app_log_info("Device certificate has been successfully generated." APP_LOG_NL);

                  // Transition to next state
                  gv_FSM_Transition(&gst_deviceProvisioningFSMInstance, \
                     &gstar_deviceProvisioningFSMStateAttrs[eDPS_WAIT_FOR_CERT_LENGTH]);

                  // Mark the event as handled
                  b_retVal = true;
               }
            }
            else
            {
               app_log_info("Received attribute is not handled at current state of device provisioning state machine." APP_LOG_NL);
            }
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitForCertGeneration_DeviceProvisioningFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_info(SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitForCertLength_DeviceProvisioningFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitForCertLength_DeviceProvisioningFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);
   // Get different parameters from the GATT update event
   uint16_t u16_attrHandle;
   uint8_t *u8pt_value;

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
            u16_attrHandle = stpt_evt->data.evt_gatt_server_attribute_value.attribute;
            u8pt_value = stpt_evt->data.evt_gatt_server_attribute_value.value.data;

            // Check if the current event is for certificate length
            if (u16_attrHandle == gattdb_DEVICE_CERT_LENGTH)
            {
               app_log_info("Device certificate length has been successfully received." APP_LOG_NL);
               app_log_info("Device certificate length: %d" APP_LOG_NL, (*((uint16_t*)(u8pt_value))));
               su16_receivedCertDERLen = (*((uint16_t*)(u8pt_value)));

               // Check if the device certificate length is non-zero
               if (su16_receivedCertDERLen)
               {
                  // Update the remaining bytes for device certificate length
                  su32_remainingDeviceCertDERLen = su16_receivedCertDERLen;

                  // Transition to next state
                  gv_FSM_Transition(&gst_deviceProvisioningFSMInstance, \
                     &gstar_deviceProvisioningFSMStateAttrs[eDPS_WAIT_FOR_CERT]);

                  // Mark the event as handled
                  b_retVal = true;
               }
               else
               {
                  app_log_info("Device certificate length is zero." APP_LOG_NL);
               }
            }
            else
            {
               app_log_info("Received attribute is not handled at current state of device provisioning state machine." APP_LOG_NL);
            }
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitForCertLength_DeviceProvisioningFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_info(SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitForCert_DeviceProvisioningFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitForCert_DeviceProvisioningFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);
   // Get different parameters from the GATT update event
   uint16_t u16_attrHandle;
   uint16_t u16_offset;
   uint16_t u16_valueLen;
   uint8_t *u8pt_value;

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
            u16_attrHandle = stpt_evt->data.evt_gatt_server_attribute_value.attribute;
            u16_offset = stpt_evt->data.evt_gatt_server_attribute_value.offset;
            u16_valueLen = stpt_evt->data.evt_gatt_server_attribute_value.value.len;
            u8pt_value = stpt_evt->data.evt_gatt_server_attribute_value.value.data;

            // Check if the current event is for certificate block (any block from 1 to 8)
            // and there are still few bytes remaining to receive
            if (((u16_attrHandle == gattdb_DEVICE_CERT_BLOCK1) || \
               (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK2) || \
               (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK3) || \
               (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK4) || \
               (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK5) || \
               (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK6) || \
               (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK7) || \
               (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK8)) && \
               (su32_remainingDeviceCertDERLen))
            {
               app_log_info("Device certificate: " APP_LOG_NL);
               for (uint8_t u8_lpIdx = 0; u8_lpIdx < u16_valueLen; u8_lpIdx++)
               {
                  app_log_append_info("%02X ", u8pt_value[u8_lpIdx]);

                  // Accumulate the received device certificate
                  su8ar_receivedDeviceCertDER[su16_offset + u8_lpIdx] = u8pt_value[u8_lpIdx];
               }
               app_log_nl_info();

               // Increment the offset by received device certificate length
               su16_offset += u16_valueLen;

               // Decrement the remaining length by received device certificate length
               su32_remainingDeviceCertDERLen -= u16_valueLen;

               // Check if entire device certificate has been received
               if (0 == su32_remainingDeviceCertDERLen)
               {
                  app_log_info("Entire certificate has been received." APP_LOG_NL);

                  // Transition to next state
                  gv_FSM_Transition(&gst_deviceProvisioningFSMInstance, \
                     &gstar_deviceProvisioningFSMStateAttrs[eDPS_WAIT_FOR_WRITING_CERT]);
               }

               // Mark the event as handled
               b_retVal = true;
            }
            else
            {
               app_log_info("Received attribute is not handled at current state of device provisioning state machine." APP_LOG_NL);
            }
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitForCert_DeviceProvisioningFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_info(SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitForWritingCert_DeviceProvisioningFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitForWritingCert_DeviceProvisioningFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_status_t t_retVal = SL_STATUS_FAIL;
   // Device provisioning record control block structure
   DeviceProvisioningRCB_T st_deviceProvisioningRCB = {
      .u32_bitmap = 0,
      .u8ar_positionNVM3 = {
         [POS_DEVICE_CERTIFICATE] = DEVICE_CERTIFICATE_NVM3_START_KEY,
         [POS_DEVICE_EC_KEY] = 0,
         [POS_STATIC_AUTH_DATA] = 0,
         [POS_CSR] = DEVICE_CSR_NVM3_START_KEY,
      },
      .u16_maxLinkDataLen = CHAIN_LINK_DATA_LEN,
   };
   uint16_t u16_certTagNVM3;

   // Check if the current event is entry event
   if (FSM_EVENT_ENTRY == stpt_event->u32_eventID)
   {
      // Read device provisioning record control block from NVM3
      t_retVal = gt_ReadDeviceProvisioningRCB(&st_deviceProvisioningRCB);

      // Check if reading device provisioning RCB was successful
      if (SL_STATUS_OK == t_retVal)
      {
         // Device certificate tag for NVM3
         u16_certTagNVM3 = \
            (st_deviceProvisioningRCB.u8ar_positionNVM3[POS_DEVICE_CERTIFICATE] | \
               CSR_GENERATOR_DEVICE_PROVISIONING_RCB_NVM3_TAG);

         // Store the device certificate
         t_retVal = gt_StoreCertificate(su8ar_receivedDeviceCertDER, su16_receivedCertDERLen, \
            CSR_GENERATOR_NVM3_REGION, u16_certTagNVM3);

         // Check if the device certificate has been written successfully
         if (SL_STATUS_OK == t_retVal)
         {
            app_log_info("Device certificate written successfully." APP_LOG_NL);

            // Update the device provisioning RCB
            gv_UpdateDeviceProvisioningRCB(POS_DEVICE_CERTIFICATE, true, \
               gst_CSRConfig.u8_certPositionOnNVM3);

            // Store the provisioning record control block
            t_retVal = st_WriteDeviceProvisioningRCB(&gst_deviceProvisioningRCB);
            app_assert((t_retVal == SL_STATUS_OK), \
               "Could not write the device provisioning record control block." APP_LOG_NL);

            // Check if the device provisioning RCB has been written successfully
            if (SL_STATUS_OK == t_retVal)
            {
               app_log_info("Device provisioning has been completed successfully." APP_LOG_NL);

               // Raise the flag indicating the device provisioning completed
               gstar_deviceProvisioningFSMStateAttrs[eDPS_WAIT_FOR_WRITING_CERT].u32_flags = 1;

               // Transition to next state
               gv_FSM_Transition(&gst_deviceProvisioningFSMInstance, \
                  &gstar_deviceProvisioningFSMStateAttrs[eDPS_DEVICE_CERT_WRITTEN]);

               // Mark the event as handled
               b_retVal = true;
            }
         }
      }
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
 * @public        gb_DeviceCertWritten_DeviceProvisioningFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_DeviceCertWritten_DeviceProvisioningFSM_Handler(FSMContext_T *stpt_FSMContext, \
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
