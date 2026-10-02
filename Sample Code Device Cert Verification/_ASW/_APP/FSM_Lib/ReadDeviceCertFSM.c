/**
 * @file          ReadDeviceCertFSM.c
 * @brief         Source file containing FSM for reading device certificate state of pairing FSM.
 * @date          17/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "ReadDeviceCertFSM.h"

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
static sl_status_t st_UpdateDeviceCertOverGATT(void);

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
 * @var           sst_ReadDeviceCertFSMCustomContext
 * @brief         Custom context for reading device certificate FSM.
 */
static ReadDeviceCertFSMContext_T sst_ReadDeviceCertFSMCustomContext = {
   .u8_connHandle = 0xFF
};

/**
 * @var           gst_readDeviceCertFSMContext
 * @brief         Context for reading device certificate FSM.
 */
FSMContext_T gst_readDeviceCertFSMContext = {
   .vpt_customData = &sst_ReadDeviceCertFSMCustomContext,
};

/**
 * @var           gst_readDeviceCertFSMInstance
 * @brief         Instance for reading device certificate FSM.
 */
FSMInstance_T gst_readDeviceCertFSMInstance = {
   .stpt_FSMContext = &gst_readDeviceCertFSMContext,
   .stpt_currentState = &gstar_readDeviceCertFSMStateAttrs[0],
   .fpt_unhandledEventCb = NULL,
};

/**
 * @var           gst_readDeviceCertFSMEvent
 * @brief         Event for reading device certificate state.
 */
FSMEvent_T gst_readDeviceCertFSMEvent = { 0 };

/**
 * @var           gstar_readDeviceCertFSMStateAttrs
 * @brief         State attributes for reading device certificate FSM.
 */
FSMStateAttr_T gstar_readDeviceCertFSMStateAttrs[] = {
   [eRDCS_WAITING_FOR_DEVICE_PROVISIONING_SERVICE_DISCOVERY] = {
      .fpt_handler = gb_WaitingForDeviceProvisioningServiceDiscovery_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_WAITING_FOR_DEVICE_PROVISIONING_SERVICE_DISCOVERY
   },
   [eRDCS_INQUIRE_FOR_DEVICE_CERT_AVAILABILITY] = {
      .fpt_handler = gb_InquireForDeviceCertAvailability_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_INQUIRE_FOR_DEVICE_CERT_AVAILABILITY
   },
   [eRDCS_WAITING_FOR_DEVICE_CERT_AVAILABILITY] = {
      .fpt_handler = gb_WaitingForDeviceCertAvailability_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_WAITING_FOR_DEVICE_CERT_AVAILABILITY
   },
   [eRDCS_INQUIRE_FOR_DEVICE_CERT_LENGTH] = {
      .fpt_handler = gb_InquireForDeviceCertLength_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_INQUIRE_FOR_DEVICE_CERT_LENGTH
   },
   [eRDCS_WAITING_FOR_DEVICE_CERT_LENGTH] = {
      .fpt_handler = gb_WaitingForDeviceCertLength_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_WAITING_FOR_DEVICE_CERT_LENGTH
   },
   [eRDCS_INQUIRE_FOR_BLOCK_SIZE] = {
      .fpt_handler = gb_InquireForBlockSize_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_INQUIRE_FOR_BLOCK_SIZE
   },
   [eRDCS_WAITING_FOR_BLOCK_SIZE] = {
      .fpt_handler = gb_WaitingForBlockSize_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_WAITING_FOR_BLOCK_SIZE
   },
   [eRDCS_INQUIRE_FOR_CERT_BLOCK_1] = {
      .fpt_handler = gb_InquireForCertBlock1_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_INQUIRE_FOR_CERT_BLOCK_1
   },
   [eRDCS_WAITING_FOR_CERT_BLOCK_1] = {
      .fpt_handler = gb_WaitingForCertBlock1_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_WAITING_FOR_CERT_BLOCK_1
   },
   [eRDCS_INQUIRE_FOR_CERT_BLOCK_2] = {
      .fpt_handler = gb_InquireForCertBlock2_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_INQUIRE_FOR_CERT_BLOCK_2
   },
   [eRDCS_WAITING_FOR_CERT_BLOCK_2] = {
      .fpt_handler = gb_WaitingForCertBlock2_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_WAITING_FOR_CERT_BLOCK_2
   },
   [eRDCS_INQUIRE_FOR_CERT_BLOCK_3] = {
      .fpt_handler = gb_InquireForCertBlock3_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_INQUIRE_FOR_CERT_BLOCK_3
   },
   [eRDCS_WAITING_FOR_CERT_BLOCK_3] = {
      .fpt_handler = gb_WaitingForCertBlock3_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_WAITING_FOR_CERT_BLOCK_3
   },
   [eRDCS_INQUIRE_FOR_CERT_BLOCK_4] = {
      .fpt_handler = gb_InquireForCertBlock4_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_INQUIRE_FOR_CERT_BLOCK_4
   },
   [eRDCS_WAITING_FOR_CERT_BLOCK_4] = {
      .fpt_handler = gb_WaitingForCertBlock4_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_WAITING_FOR_CERT_BLOCK_4
   },
   [eRDCS_DEVICE_CERT_READ_AND_VERIFIED] = {
      .fpt_handler = gb_DeviceCertReadAndVerified_ReadDeviceCertFSM_Handler,
      .u8_stateID = eRDCS_DEVICE_CERT_READ_AND_VERIFIED
   },
};

/**
 * @var           gu16_remoteDeviceCertDERLen
 * @brief         Length of the received device's device certificate.
 */
uint16_t gu16_remoteDeviceCertDERLen = 0;

/**
 * @var           gu8ar_remoteDeviceCertDER
 * @brief         Buffer to hold the remote device's device certificate in DER format.
 */
uint8_t gu8ar_remoteDeviceCertDER[CHAIN_LINK_DATA_LEN * CHAIN_LINK_DATA_NUM] = { 0 };

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           su32_serviceHandle
 * @brief         Service handle.
 */
static uint32_t su32_serviceHandle;

/**
 * @var           su16_charHandle
 * @brief         Characteristic handle.
 */
static uint16_t su16_charHandle;

/**
 * @var           sstpt_readDeviceCertFSMContext
 * @brief         Pointer to reading device certificate FSM context.
 */
static ReadDeviceCertFSMContext_T *sstpt_readDeviceCertFSMContext;

/**
 * @var           su16_remoteDeviceBlockSize
 * @brief         Block size of the remote device's device certificate.
 */
static uint16_t su16_remoteDeviceBlockSize = 0;

/**
 * @var           su16_certOffset
 * @brief         Offset for the remote device's device certificate.
 */
static uint16_t su16_certOffset = 0;

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
 * @public        gb_WaitingForDeviceProvisioningServiceDiscovery_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForDeviceProvisioningServiceDiscovery_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   uint32_t u32_serviceHandle;
   uint8array st_serviceUUID;

   sl_status_t t_retVal = SL_STATUS_FAIL;
   uint8_t u8ar_IS_DEVICE_CERT_GENERATED_UUID[] = {
      0x7a, 0x75, 0xbe, 0x4d, 0x70, 0x8d, 0xbc, 0xb2, 0x36, 0x4f, 0xcc, 0x8c, 0xf9, 0x98, 0x96, 0x3a,
   };

   uint16_t u16_result;

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Discovered service from remote GATT database
         case sl_bt_evt_gatt_service_id:
         {
            // Get different parameters from the event
            u32_serviceHandle = stpt_evt->data.evt_gatt_service.service;
            st_serviceUUID.len = stpt_evt->data.evt_gatt_service.uuid.len;
            memcpy(st_serviceUUID.data, stpt_evt->data.evt_gatt_service.uuid.data, \
               st_serviceUUID.len);

            // Check if the discovered service is BATL_DEVICE_PROVISIONING service
            if (0 == (memcmp(st_serviceUUID.data, \
               gattdb.attributes[gattdb_BATL_DEVICE_PROVISIONING - 1].constdata->data, \
               st_serviceUUID.len)))
            {
               app_log_info("Discovered BATL_DEVICE_PROVISIONING service from the remote GATT server." APP_LOG_NL);

               // Store the service handle
               su32_serviceHandle = u32_serviceHandle;
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Discovering IS_DEVICE_CERT_GENERATED characteristic from the remote GATT server...." APP_LOG_NL);

               // Discover IS_DEVICE_CERT_GENERATED characteristic
               t_retVal = sl_bt_gatt_discover_characteristics_by_uuid(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su32_serviceHandle,
                  sizeof(u8ar_IS_DEVICE_CERT_GENERATED_UUID),
                  u8ar_IS_DEVICE_CERT_GENERATED_UUID
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_INQUIRE_FOR_DEVICE_CERT_AVAILABILITY]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForDeviceProvisioningServiceDiscovery_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
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
 * @public        gb_InquireForDeviceCertAvailability_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_InquireForDeviceCertAvailability_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   uint16_t u16_charHandle;
   uint8array st_charUUID;

   sl_status_t t_retVal = SL_STATUS_FAIL;

   uint16_t u16_result;

   uint8_t u8ar_IS_DEVICE_CERT_GENERATED_UUID[] = {
      0x7a, 0x75, 0xbe, 0x4d, 0x70, 0x8d, 0xbc, 0xb2, 0x36, 0x4f, 0xcc, 0x8c, 0xf9, 0x98, 0x96, 0x3a,
   };

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Discovered characteristic from remote GATT database
         case sl_bt_evt_gatt_characteristic_id:
         {
            // Get different parameters from the event
            u16_charHandle = stpt_evt->data.evt_gatt_characteristic.characteristic;
            st_charUUID.len = stpt_evt->data.evt_gatt_characteristic.uuid.len;
            memcpy(st_charUUID.data, stpt_evt->data.evt_gatt_characteristic.uuid.data, \
               st_charUUID.len);

            // Check if the discovered characteristic is IS_DEVICE_CERT_GENERATED characteristic
            if (0 == (memcmp(st_charUUID.data, \
               u8ar_IS_DEVICE_CERT_GENERATED_UUID, \
               st_charUUID.len)))
            {
               app_log_info("Discovered IS_DEVICE_CERT_GENERATED characteristic from the remote GATT server." APP_LOG_NL);

               // Store the characteristic handle
               su16_charHandle = u16_charHandle;
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Reading IS_DEVICE_CERT_GENERATED characteristic from the remote GATT server...." APP_LOG_NL);

               // Read IS_DEVICE_CERT_GENERATED characteristic
               t_retVal = sl_bt_gatt_read_characteristic_value(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su16_charHandle
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_WAITING_FOR_DEVICE_CERT_AVAILABILITY]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_InquireForDeviceCertAvailability_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitingForDeviceCertAvailability_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForDeviceCertAvailability_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;

   byte_array st_data;

   uint8_t u8ar_DEVICE_CERT_LENGTH_UUID[] = {
      0x45, 0x74, 0xd7, 0xf1, 0x41, 0x77, 0x42, 0xba, 0xed, 0x4a, 0xff, 0xa8, 0x01, 0x32, 0x8e, 0xf3,
   };

   uint16_t u16_result;

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Contains the data of a characteristic sent by the GATT Server.
         case sl_bt_evt_gatt_characteristic_value_id:
         {
            // Get different parameters from the event
            st_data.len = stpt_evt->data.evt_gatt_characteristic_value.value.len;
            memcpy(st_data.data, stpt_evt->data.evt_gatt_characteristic_value.value.data, \
               st_data.len);

            // Check if the device certificate is available at target device
            if (1 == st_data.data[0])
            {
               app_log_info("Target device's device certificate is available to read." APP_LOG_NL);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Discovering DEVICE_CERT_LENGTH characteristic from the remote GATT server...." APP_LOG_NL);

               // Discover DEVICE_CERT_LENGTH characteristic
               t_retVal = sl_bt_gatt_discover_characteristics_by_uuid(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su32_serviceHandle,
                  sizeof(u8ar_DEVICE_CERT_LENGTH_UUID),
                  u8ar_DEVICE_CERT_LENGTH_UUID
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_INQUIRE_FOR_DEVICE_CERT_LENGTH]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForDeviceCertAvailability_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_InquireForDeviceCertLength_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_InquireForDeviceCertLength_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;

   uint16_t u16_charHandle;
   uint8array st_charUUID;

   uint8_t u8ar_DEVICE_CERT_LENGTH_UUID[] = {
      0x45, 0x74, 0xd7, 0xf1, 0x41, 0x77, 0x42, 0xba, 0xed, 0x4a, 0xff, 0xa8, 0x01, 0x32, 0x8e, 0xf3,
   };

   uint16_t u16_result;

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Discovered characteristic from remote GATT database
         case sl_bt_evt_gatt_characteristic_id:
         {
            // Get different parameters from the event
            u16_charHandle = stpt_evt->data.evt_gatt_characteristic.characteristic;
            st_charUUID.len = stpt_evt->data.evt_gatt_characteristic.uuid.len;
            memcpy(st_charUUID.data, stpt_evt->data.evt_gatt_characteristic.uuid.data, \
               st_charUUID.len);

            // Check if the discovered characteristic is DEVICE_CERT_LENGTH characteristic
            if (0 == (memcmp(st_charUUID.data, \
               u8ar_DEVICE_CERT_LENGTH_UUID, \
               st_charUUID.len)))
            {
               app_log_info("Discovered DEVICE_CERT_LENGTH characteristic from the remote GATT server." APP_LOG_NL);

               // Store the characteristic handle
               su16_charHandle = u16_charHandle;
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Reading value of DEVICE_CERT_LENGTH characteristic from the remote GATT server...." APP_LOG_NL);

               // Read the device certificate length value
               t_retVal = sl_bt_gatt_read_characteristic_value(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su16_charHandle
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_WAITING_FOR_DEVICE_CERT_LENGTH]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_InquireForDeviceCertLength_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitingForDeviceCertLength_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForDeviceCertLength_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;

   byte_array st_data;
   uint16_t u16_result;

   uint8_t u8ar_BLOCK_SIZE_UUID[] = {
      0x51, 0xc1, 0x2d, 0x3f, 0xe6, 0x7e, 0x91, 0x9b, 0x8b, 0x4b, 0x8c, 0xb8, 0xad, 0x48, 0xfb, 0x5f,
   };

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Contains the data of a characteristic sent by the GATT Server.
         case sl_bt_evt_gatt_characteristic_value_id:
         {
            // Get different parameters from the event
            st_data.len = stpt_evt->data.evt_gatt_characteristic_value.value.len;
            memcpy(st_data.data, stpt_evt->data.evt_gatt_characteristic_value.value.data, \
               st_data.len);

            gu16_remoteDeviceCertDERLen = (uint16_t)((uint16_t)(st_data.data[1] << 8) | (uint16_t)(st_data.data[0]));
            app_log_info("Target device's device certificate length is: %d." APP_LOG_NL, \
               gu16_remoteDeviceCertDERLen);

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Discovering BLOCK_SIZE characteristic from the remote GATT server...." APP_LOG_NL);

               // Discover BLOCK_SIZE characteristic
               t_retVal = sl_bt_gatt_discover_characteristics_by_uuid(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su32_serviceHandle,
                  sizeof(u8ar_BLOCK_SIZE_UUID),
                  u8ar_BLOCK_SIZE_UUID
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_INQUIRE_FOR_BLOCK_SIZE]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForDeviceCertLength_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_InquireForBlockSize_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_InquireForBlockSize_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;

   uint16_t u16_charHandle;
   uint8array st_charUUID;

   uint8_t u8ar_BLOCK_SIZE_UUID[] = {
      0x51, 0xc1, 0x2d, 0x3f, 0xe6, 0x7e, 0x91, 0x9b, 0x8b, 0x4b, 0x8c, 0xb8, 0xad, 0x48, 0xfb, 0x5f,
   };

   uint16_t u16_result;

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Discovered characteristic from remote GATT database
         case sl_bt_evt_gatt_characteristic_id:
         {
            // Get different parameters from the event
            u16_charHandle = stpt_evt->data.evt_gatt_characteristic.characteristic;
            st_charUUID.len = stpt_evt->data.evt_gatt_characteristic.uuid.len;
            memcpy(st_charUUID.data, stpt_evt->data.evt_gatt_characteristic.uuid.data, \
               st_charUUID.len);

            // Check if the discovered characteristic is BLOCK_SIZE characteristic
            if (0 == (memcmp(st_charUUID.data, \
               u8ar_BLOCK_SIZE_UUID, \
               st_charUUID.len)))
            {
               app_log_info("Discovered BLOCK_SIZE characteristic from the remote GATT server." APP_LOG_NL);

               // Store the characteristic handle
               su16_charHandle = u16_charHandle;
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Reading value of BLOCK_SIZE characteristic from the remote GATT server...." APP_LOG_NL);

               // Read the device certificate length value
               t_retVal = sl_bt_gatt_read_characteristic_value(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su16_charHandle
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_WAITING_FOR_BLOCK_SIZE]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_InquireForBlockSize_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitingForBlockSize_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForBlockSize_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;

   byte_array st_data;
   uint16_t u16_result;

   uint8_t u8ar_DEVICE_CERT_BLOCK1_UUID[] = {
      0x91, 0x61, 0x8e, 0xd5, 0xae, 0xf0, 0x97, 0x89, 0xed, 0x4b, 0xc8, 0xd1, 0xb6, 0x40, 0xf7, 0xdd,
   };

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Contains the data of a characteristic sent by the GATT Server.
         case sl_bt_evt_gatt_characteristic_value_id:
         {
            // Get different parameters from the event
            st_data.len = stpt_evt->data.evt_gatt_characteristic_value.value.len;
            memcpy(st_data.data, stpt_evt->data.evt_gatt_characteristic_value.value.data, \
               st_data.len);

            su16_remoteDeviceBlockSize = (uint16_t)((uint16_t)(st_data.data[1] << 8) | (uint16_t)(st_data.data[0]));
            app_log_info("Target device's block size is: %d." APP_LOG_NL, \
               su16_remoteDeviceBlockSize);

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Discovering DEVICE_CERT_BLOCK1 characteristic from the remote GATT server...." APP_LOG_NL);

               // Discover DEVICE_CERT_BLOCK1 characteristic
               t_retVal = sl_bt_gatt_discover_characteristics_by_uuid(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su32_serviceHandle,
                  sizeof(u8ar_DEVICE_CERT_BLOCK1_UUID),
                  u8ar_DEVICE_CERT_BLOCK1_UUID
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_INQUIRE_FOR_CERT_BLOCK_1]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForBlockSize_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_InquireForCertBlock1_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_InquireForCertBlock1_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;

   uint16_t u16_charHandle;
   uint8array st_charUUID;

   uint8_t u8ar_DEVICE_CERT_BLOCK1_UUID[] = {
      0x91, 0x61, 0x8e, 0xd5, 0xae, 0xf0, 0x97, 0x89, 0xed, 0x4b, 0xc8, 0xd1, 0xb6, 0x40, 0xf7, 0xdd,
   };

   uint16_t u16_result;

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Discovered characteristic from remote GATT database
         case sl_bt_evt_gatt_characteristic_id:
         {
            // Get different parameters from the event
            u16_charHandle = stpt_evt->data.evt_gatt_characteristic.characteristic;
            st_charUUID.len = stpt_evt->data.evt_gatt_characteristic.uuid.len;
            memcpy(st_charUUID.data, stpt_evt->data.evt_gatt_characteristic.uuid.data, \
               st_charUUID.len);

            // Check if the discovered characteristic is DEVICE_CERT_BLOCK1 characteristic
            if (0 == (memcmp(st_charUUID.data, \
               u8ar_DEVICE_CERT_BLOCK1_UUID, \
               st_charUUID.len)))
            {
               app_log_info("Discovered DEVICE_CERT_BLOCK1 characteristic from the remote GATT server." APP_LOG_NL);

               // Store the characteristic handle
               su16_charHandle = u16_charHandle;
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Reading value of DEVICE_CERT_BLOCK1 characteristic from the remote GATT server...." APP_LOG_NL);

               // Read the device certificate length value
               t_retVal = sl_bt_gatt_read_characteristic_value(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su16_charHandle
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_WAITING_FOR_CERT_BLOCK_1]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_InquireForCertBlock1_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
      }
   }
}

/**
 * @public        gb_WaitingForCertBlock1_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForCertBlock1_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;
   uint8_t u8_len = 0;
   uint16_t u16_result;

   uint8_t u8ar_DEVICE_CERT_BLOCK2_UUID[] = {
      0x87, 0x79, 0x1c, 0xc2, 0x4c, 0xd7, 0x0b, 0x8f, 0x02, 0x40, 0x56, 0x50, 0x9a, 0x28, 0x74, 0xf9,
   };

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Contains the data of a characteristic sent by the GATT Server.
         case sl_bt_evt_gatt_characteristic_value_id:
         {
            // Get different parameters from the event
            u8_len = stpt_evt->data.evt_gatt_characteristic_value.value.len;
            memcpy(&gu8ar_remoteDeviceCertDER[su16_certOffset], \
               stpt_evt->data.evt_gatt_characteristic_value.value.data, \
               u8_len);
            app_log_info("Received %d bytes of remote device's device certificate data." APP_LOG_NL, u8_len);

            // Increase the offset value
            su16_certOffset += u8_len;
            app_log_info("Total %d bytes received." APP_LOG_NL, su16_certOffset);

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Discovering DEVICE_CERT_BLOCK2 characteristic from the remote GATT server...." APP_LOG_NL);

               // Discover DEVICE_CERT_BLOCK2 characteristic
               t_retVal = sl_bt_gatt_discover_characteristics_by_uuid(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su32_serviceHandle,
                  sizeof(u8ar_DEVICE_CERT_BLOCK2_UUID),
                  u8ar_DEVICE_CERT_BLOCK2_UUID
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_INQUIRE_FOR_CERT_BLOCK_2]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForCertBlock1_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
      }
   }
}

/**
 * @public        gb_InquireForCertBlock2_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_InquireForCertBlock2_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;

   uint16_t u16_charHandle;
   uint8array st_charUUID;

   uint8_t u8ar_DEVICE_CERT_BLOCK2_UUID[] = {
      0x87, 0x79, 0x1c, 0xc2, 0x4c, 0xd7, 0x0b, 0x8f, 0x02, 0x40, 0x56, 0x50, 0x9a, 0x28, 0x74, 0xf9,
   };

   uint16_t u16_result;

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Discovered characteristic from remote GATT database
         case sl_bt_evt_gatt_characteristic_id:
         {
            // Get different parameters from the event
            u16_charHandle = stpt_evt->data.evt_gatt_characteristic.characteristic;
            st_charUUID.len = stpt_evt->data.evt_gatt_characteristic.uuid.len;
            memcpy(st_charUUID.data, stpt_evt->data.evt_gatt_characteristic.uuid.data, \
               st_charUUID.len);

            // Check if the discovered characteristic is DEVICE_CERT_BLOCK2 characteristic
            if (0 == (memcmp(st_charUUID.data, \
               u8ar_DEVICE_CERT_BLOCK2_UUID, \
               st_charUUID.len)))
            {
               app_log_info("Discovered DEVICE_CERT_BLOCK2 characteristic from the remote GATT server." APP_LOG_NL);

               // Store the characteristic handle
               su16_charHandle = u16_charHandle;
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Reading value of DEVICE_CERT_BLOCK2 characteristic from the remote GATT server...." APP_LOG_NL);

               // Read the device certificate length value
               t_retVal = sl_bt_gatt_read_characteristic_value(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su16_charHandle
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_WAITING_FOR_CERT_BLOCK_2]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_InquireForCertBlock2_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
      }
   }
}

/**
 * @public        gb_WaitingForCertBlock2_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForCertBlock2_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;
   uint8_t u8_len = 0;
   uint16_t u16_result;

   uint8_t u8ar_DEVICE_CERT_BLOCK3_UUID[] = {
      0xdd, 0xd8, 0x6a, 0xce, 0xa7, 0x4f, 0x72, 0xb6, 0x56, 0x4c, 0x92, 0xfb, 0xc7, 0xa4, 0x79, 0xd5,
   };

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Contains the data of a characteristic sent by the GATT Server.
         case sl_bt_evt_gatt_characteristic_value_id:
         {
            // Get different parameters from the event
            u8_len = stpt_evt->data.evt_gatt_characteristic_value.value.len;
            memcpy(&gu8ar_remoteDeviceCertDER[su16_certOffset], \
               stpt_evt->data.evt_gatt_characteristic_value.value.data, \
               u8_len);
            app_log_info("Received %d bytes of remote device's device certificate data." APP_LOG_NL, u8_len);

            // Increase the offset value
            su16_certOffset += u8_len;
            app_log_info("Total %d bytes received." APP_LOG_NL, su16_certOffset);

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Discovering DEVICE_CERT_BLOCK3 characteristic from the remote GATT server...." APP_LOG_NL);

               // Discover DEVICE_CERT_BLOCK3 characteristic
               t_retVal = sl_bt_gatt_discover_characteristics_by_uuid(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su32_serviceHandle,
                  sizeof(u8ar_DEVICE_CERT_BLOCK3_UUID),
                  u8ar_DEVICE_CERT_BLOCK3_UUID
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_INQUIRE_FOR_CERT_BLOCK_3]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForCertBlock2_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
      }
   }
}

/**
 * @public        gb_InquireForCertBlock3_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_InquireForCertBlock3_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;

   uint16_t u16_charHandle;
   uint8array st_charUUID;

   uint8_t u8ar_DEVICE_CERT_BLOCK3_UUID[] = {
      0xdd, 0xd8, 0x6a, 0xce, 0xa7, 0x4f, 0x72, 0xb6, 0x56, 0x4c, 0x92, 0xfb, 0xc7, 0xa4, 0x79, 0xd5,
   };

   uint16_t u16_result;

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Discovered characteristic from remote GATT database
         case sl_bt_evt_gatt_characteristic_id:
         {
            // Get different parameters from the event
            u16_charHandle = stpt_evt->data.evt_gatt_characteristic.characteristic;
            st_charUUID.len = stpt_evt->data.evt_gatt_characteristic.uuid.len;
            memcpy(st_charUUID.data, stpt_evt->data.evt_gatt_characteristic.uuid.data, \
               st_charUUID.len);

            // Check if the discovered characteristic is DEVICE_CERT_BLOCK3 characteristic
            if (0 == (memcmp(st_charUUID.data, \
               u8ar_DEVICE_CERT_BLOCK3_UUID, \
               st_charUUID.len)))
            {
               app_log_info("Discovered DEVICE_CERT_BLOCK3 characteristic from the remote GATT server." APP_LOG_NL);

               // Store the characteristic handle
               su16_charHandle = u16_charHandle;
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Reading value of DEVICE_CERT_BLOCK3 characteristic from the remote GATT server...." APP_LOG_NL);

               // Read the device certificate length value
               t_retVal = sl_bt_gatt_read_characteristic_value(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su16_charHandle
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_WAITING_FOR_CERT_BLOCK_3]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_InquireForCertBlock3_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
      }
   }
}

/**
 * @public        gb_WaitingForCertBlock3_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForCertBlock3_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;
   uint8_t u8_len = 0;
   uint16_t u16_result;

   uint8_t u8ar_DEVICE_CERT_BLOCK4_UUID[] = {
      0xf3, 0xb3, 0xc7, 0x7e, 0x99, 0xd4, 0x13, 0x94, 0x13, 0x49, 0xd0, 0x9b, 0x32, 0x2f, 0xf3, 0xaf,
   };

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Contains the data of a characteristic sent by the GATT Server.
         case sl_bt_evt_gatt_characteristic_value_id:
         {
            // Get different parameters from the event
            u8_len = stpt_evt->data.evt_gatt_characteristic_value.value.len;
            memcpy(&gu8ar_remoteDeviceCertDER[su16_certOffset], \
               stpt_evt->data.evt_gatt_characteristic_value.value.data, \
               u8_len);
            app_log_info("Received %d bytes of remote device's device certificate data." APP_LOG_NL, u8_len);

            // Increase the offset value
            su16_certOffset += u8_len;
            app_log_info("Total %d bytes received." APP_LOG_NL, su16_certOffset);

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Discovering DEVICE_CERT_BLOCK4 characteristic from the remote GATT server...." APP_LOG_NL);

               // Discover DEVICE_CERT_BLOCK4 characteristic
               t_retVal = sl_bt_gatt_discover_characteristics_by_uuid(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su32_serviceHandle,
                  sizeof(u8ar_DEVICE_CERT_BLOCK4_UUID),
                  u8ar_DEVICE_CERT_BLOCK4_UUID
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_INQUIRE_FOR_CERT_BLOCK_4]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForCertBlock3_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
      }
   }
}

/**
 * @public        gb_InquireForCertBlock4_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_InquireForCertBlock4_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;

   uint16_t u16_charHandle;
   uint8array st_charUUID;

   uint8_t u8ar_DEVICE_CERT_BLOCK4_UUID[] = {
      0xf3, 0xb3, 0xc7, 0x7e, 0x99, 0xd4, 0x13, 0x94, 0x13, 0x49, 0xd0, 0x9b, 0x32, 0x2f, 0xf3, 0xaf,
   };

   uint16_t u16_result;

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Discovered characteristic from remote GATT database
         case sl_bt_evt_gatt_characteristic_id:
         {
            // Get different parameters from the event
            u16_charHandle = stpt_evt->data.evt_gatt_characteristic.characteristic;
            st_charUUID.len = stpt_evt->data.evt_gatt_characteristic.uuid.len;
            memcpy(st_charUUID.data, stpt_evt->data.evt_gatt_characteristic.uuid.data, \
               st_charUUID.len);

            // Check if the discovered characteristic is DEVICE_CERT_BLOCK4 characteristic
            if (0 == (memcmp(st_charUUID.data, \
               u8ar_DEVICE_CERT_BLOCK4_UUID, \
               st_charUUID.len)))
            {
               app_log_info("Discovered DEVICE_CERT_BLOCK4 characteristic from the remote GATT server." APP_LOG_NL);

               // Store the characteristic handle
               su16_charHandle = u16_charHandle;
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               app_log_info("Reading value of DEVICE_CERT_BLOCK4 characteristic from the remote GATT server...." APP_LOG_NL);

               // Read the device certificate length value
               t_retVal = sl_bt_gatt_read_characteristic_value(
                  sstpt_readDeviceCertFSMContext->u8_connHandle,
                  su16_charHandle
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                  &gstar_readDeviceCertFSMStateAttrs[eRDCS_WAITING_FOR_CERT_BLOCK_4]);
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_InquireForCertBlock4_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
      }
   }
}

/**
 * @public        gb_WaitingForCertBlock4_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForCertBlock4_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;
   uint8_t u8_len = 0;
   uint16_t u16_result;

   sstpt_readDeviceCertFSMContext = (ReadDeviceCertFSMContext_T *)(stpt_FSMContext->vpt_customData);

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
         // Contains the data of a characteristic sent by the GATT Server.
         case sl_bt_evt_gatt_characteristic_value_id:
         {
            // Get different parameters from the event
            u8_len = stpt_evt->data.evt_gatt_characteristic_value.value.len;
            memcpy(&gu8ar_remoteDeviceCertDER[su16_certOffset], \
               stpt_evt->data.evt_gatt_characteristic_value.value.data, \
               u8_len);
            app_log_info("Received %d bytes of remote device's device certificate data." APP_LOG_NL, u8_len);

            // Increase the offset value
            su16_certOffset += u8_len;
            app_log_info("Total %d bytes received." APP_LOG_NL, su16_certOffset);

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         // Procedure was successfully completed or failed with an error.
         case sl_bt_evt_gatt_procedure_completed_id:
         {
            // Get different parameters from the event
            u16_result = stpt_evt->data.evt_gatt_procedure_completed.result;

            // Check if the GATT procedure completed successfully
            if (SL_STATUS_OK == u16_result)
            {
               t_retVal = gt_VerifyRemoteDeviceCertificate();

               // Check if the remote device's device certificate is verified successfully
               if (SL_STATUS_OK == t_retVal)
               {
                  // Raise the flag indicating the remote device's device certificate is
                  // read and verified.
                  gstar_readDeviceCertFSMStateAttrs[eRDCS_WAITING_FOR_CERT_BLOCK_4].u32_flags = 1;

                  // Transition to next state
                  gv_FSM_Transition(&gst_readDeviceCertFSMInstance, \
                     &gstar_readDeviceCertFSMStateAttrs[eRDCS_DEVICE_CERT_READ_AND_VERIFIED]);
               }
            }
            else
            {
               app_log_debug("GATT procedure result is: %X" APP_LOG_NL, u16_result);
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForCertBlock4_ReadDeviceCertFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
      }
   }
}

/**
 * @public        gb_DeviceCertReadAndVerified_ReadDeviceCertFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_DeviceCertReadAndVerified_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
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
