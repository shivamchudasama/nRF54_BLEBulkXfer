/**
 * @file          PairingFSM.c
 * @brief         Source file containing FSM for pairing state of MainFSM.
 * @date          17/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "PairingFSM.h"

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
static sl_status_t st_SignOOBData(uint8_t *u8pt_randomData, uint8_t *u8pt_confirmData, \
   uint8_t *u8pt_signedData, uint8_t *u8pt_signedDataLen);
static sl_status_t st_VerifyOOBData(uint8_t *u8pt_randomData, uint8_t *u8pt_confirmData, \
   uint8_t *u8pt_signature);

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
 * @var           gst_pairingFSMContext
 * @brief         Context for pairing FSM.
 */
FSMContext_T gst_pairingFSMContext = {
   .vpt_customData = NULL
};

/**
 * @var           gst_pairingFSMInstance
 * @brief         Instance for pairing FSM.
 */
FSMInstance_T gst_pairingFSMInstance = {
   .stpt_FSMContext = &gst_pairingFSMContext,
   .stpt_currentState = &gstar_pairingFSMStateAttrs[0],
   .fpt_unhandledEventCb = NULL,
};

/**
 * @var           gst_pairingFSMEvent
 * @brief         Event for pairing state.
 */
FSMEvent_T gst_pairingFSMEvent = { 0 };

/**
 * @var           gstar_pairingFSMStateAttrs
 * @brief         State attributes for pairing FSM.
 */
FSMStateAttr_T gstar_pairingFSMStateAttrs[] = {
   [ePS_WAITING_FOR_SYSTEM_BOOT] = {
      .fpt_handler = gb_WaitingForSystemBoot_PairingFSM_Handler,
      .u8_stateID = ePS_WAITING_FOR_SYSTEM_BOOT
   },
   [ePS_WAITING_FOR_CONNECTION_OPEN] = {
      .fpt_handler = gb_WaitingForConnectionOpen_PairingFSM_Handler,
      .u8_stateID = ePS_WAITING_FOR_CONNECTION_OPEN
   },
   [ePS_WAITING_FOR_TARGET_DEVICE_BLE_ADDR] = {
      .fpt_handler = gb_WaitingForTargetDeviceBLEAddr_PairingFSM_Handler,
      .u8_stateID = ePS_WAITING_FOR_TARGET_DEVICE_BLE_ADDR
   },
   [ePS_WAITING_FOR_CURRENT_DEVICE_BLE_ROLE] = {
      .fpt_handler = gb_WaitingForCurrentDeviceBLERole_PairingFSM_Handler,
      .u8_stateID = ePS_WAITING_FOR_CURRENT_DEVICE_BLE_ROLE
   },
   [ePS_WAITING_FOR_PAIRING_TRIGGER] = {
      .fpt_handler = gb_WaitingForPairingTrigger_PairingFSM_Handler,
      .u8_stateID = ePS_WAITING_FOR_PAIRING_TRIGGER
   },
   [ePS_WAITING_FOR_TARGET_DEVICE_DISCOVERY] = {
      .fpt_handler = gb_WaitingForTargetDeviceDiscovery_PairingFSM_Handler,
      .u8_stateID = ePS_WAITING_FOR_TARGET_DEVICE_DISCOVERY
   },
   [ePS_WAITING_FOR_CONNECTION_OPENED_FROM_TARGET_DEVICE] = {
      .fpt_handler = gb_WaitingForConnectionOpenedFromTargetDevice_PairingFSM_Handler,
      .u8_stateID = ePS_WAITING_FOR_CONNECTION_OPENED_FROM_TARGET_DEVICE
   },
   [ePS_READ_REMOTE_DEVICE_CERT] = {
      .fpt_handler = gb_ReadRemoteDeviceCert_PairingFSM_Handler,
      .u8_stateID = ePS_READ_REMOTE_DEVICE_CERT
   },
   [ePS_WAITING_FOR_PAIRING_SERVICE_DISCOVERY] = {
      .fpt_handler = gb_WaitingForPairingServiceDiscovery_PairingFSM_Handler,
      .u8_stateID = ePS_WAITING_FOR_PAIRING_SERVICE_DISCOVERY
   },
   [ePS_INQUIRE_FOR_OOB_DATA] = {
      .fpt_handler = gb_InquireForOOBData_PairingFSM_Handler,
      .u8_stateID = ePS_INQUIRE_FOR_OOB_DATA
   },
   [ePS_WAITING_FOR_OOB_DATA] = {
      .fpt_handler = gb_WaitingForOOBData_PairingFSM_Handler,
      .u8_stateID = ePS_WAITING_FOR_OOB_DATA
   },
   [ePS_WAITING_FOR_PAIRING_CONFIRMATION] = {
      .fpt_handler = gb_WaitingForPairingConfirmation_PairingFSM_Handler,
      .u8_stateID = ePS_WAITING_FOR_PAIRING_CONFIRMATION
   },
   [ePS_DEVICE_PAIRED] = {
      .fpt_handler = gb_DevicePaired_PairingFSM_Handler,
      .u8_stateID = ePS_DEVICE_PAIRED
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

/**
 * @var           su8_EOLToolConnHandle
 * @brief         The EOL tool connection handle allocated from Bluetooth stack.
 */
static uint8_t su8_EOLToolConnHandle;

/**
 * @var           su8_targetDeviceConnHandle
 * @brief         The target device connection handle allocated from Bluetooth stack.
 */
static uint8_t su8_targetDeviceConnHandle;

/**
 * @var           sst_targetDeviceAddr
 * @brief         Target device BLE address.
 */
static bd_addr sst_targetDeviceAddr = { 0 };

/**
 * @var           su8_targetDeviceAddrType
 * @brief         Target device BLE address type. Keeping it public (0) as of now.
 */
static uint8_t su8_targetDeviceAddrType = 0;

/**
 * @var           se_currentDeviceBLERole
 * @brief         Currnet device BLE role in pairing procedure.
 */
static BLERole_E se_currentDeviceBLERole = eBR_UNASSIGNED;

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
 * @var           su8ar_targetDeviceOOBData
 * @brief         Out-of-band data for the target device.
 */
static uint8_t su8ar_targetDeviceOOBData[OOB_SIGNED_DATA_LEN] = { 0 };

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
 * @private       st_UpdateDeviceCertOverGATT
 * @brief         It updates the device certificate details over GATT.
 * @return        SL_STATUS_OK upon successfully completing the operation,
 *                or any of the error codes from sl_status_t.
 */
static sl_status_t st_UpdateDeviceCertOverGATT(void)
{
   sl_status_t t_retVal = SL_STATUS_FAIL;
   uint8_t u8_isDeviceCertificateGenerated = 1;
   uint16_t u16_totalBytesUpdated = 0;
   uint8_t u8_currentLen = gattdb_DEVICE_CERT_BLOCK1_len;
   uint16_t u16_attr = gattdb_DEVICE_CERT_BLOCK1;
   uint8_t u8_lpIdx = 0;

   // Set GATT database with own device certificate
   while (u16_totalBytesUpdated != gu32_deviceCertDERLen)
   {
      t_retVal = sl_bt_gatt_server_write_attribute_value(u16_attr + u8_lpIdx, \
         0, u8_currentLen, &gu8ar_deviceCertDER[u16_totalBytesUpdated]);
      app_assert_status(t_retVal);

      // Increment the loop index by 2 (next attribute handle is incremented by 2)
      u8_lpIdx += 2;

      // Update the total number of bytes updated over local GATT
      u16_totalBytesUpdated += u8_currentLen;

      // Update the current length of bytes to be set over local GATT
      u8_currentLen = ((u16_totalBytesUpdated + u8_currentLen) > gu32_deviceCertDERLen) ? \
         (gu32_deviceCertDERLen - u16_totalBytesUpdated) : (u8_currentLen);
   }

   // t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_DEVICE_CERT_BLOCK1, \
   //    0, gattdb_DEVICE_CERT_BLOCK1_len, &gu8ar_deviceCertDER[0]);
   // app_assert_status(t_retVal);
   // t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_DEVICE_CERT_BLOCK2, \
   //    0, gattdb_DEVICE_CERT_BLOCK2_len, &gu8ar_deviceCertDER[MTU_SIZE]);
   // app_assert_status(t_retVal);
   // t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_DEVICE_CERT_BLOCK3, \
   //    0, gattdb_DEVICE_CERT_BLOCK3_len, &gu8ar_deviceCertDER[(2*MTU_SIZE)]);
   // app_assert_status(t_retVal);
   // t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_DEVICE_CERT_BLOCK4, \
   //    0, gattdb_DEVICE_CERT_BLOCK4_len, &gu8ar_deviceCertDER[(3*MTU_SIZE)]);
   // app_assert_status(t_retVal);

   // Set the generated device certificate length
   t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_DEVICE_CERT_LENGTH, \
      0, gattdb_DEVICE_CERT_LENGTH_len, &gu32_deviceCertDERLen);
   app_assert_status(t_retVal);

   // Set the device certificate generation flag
   t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_IS_DEVICE_CERT_GENERATED, \
      0, gattdb_IS_DEVICE_CERT_GENERATED_len, &u8_isDeviceCertificateGenerated);
   app_assert_status(t_retVal);

   return t_retVal;
}

/**
 * @private       st_SignOOBData
 * @brief         It signs the OOB data using private key.
 * @param[in]     u8pt_randomData Pointer to random data.
 * @param[in]     u8pt_confirmData Pointer to confirm data.
 * @param[out]    u8pt_signedData Pointer to signed data.
 * @param[out]    u8pt_signedDataLen Pointer to length of signed data.
 * @return        SL_STATUS_OK upon successfully completing the operation,
 *                or any of the error codes from sl_status_t.
 */
static sl_status_t st_SignOOBData(uint8_t *u8pt_randomData, uint8_t *u8pt_confirmData, \
   uint8_t *u8pt_signedData, uint8_t *u8pt_signedDataLen)
{
   sl_status_t t_retVal;
   mbedtls_svc_key_id_t t_keyID = DEVICE_EC_KEY_ID;
   uint8_t u8ar_inputData[OOB_DATA_LEN] = { 0 };
   uint8_t u8ar_signature[OOB_SIGNATURE_LEN] = { 0 };
   size_t t_signatureLen = 0;

   // Check if any pointer is NULL
   if (u8pt_randomData == NULL || u8pt_confirmData == NULL || u8pt_signedData == NULL || u8pt_signedDataLen == NULL)
   {
      u8pt_randomData = SL_STATUS_NULL_POINTER;
   }
   else
   {
      memcpy(&u8ar_inputData[0], u8pt_randomData, OOB_RANDOM_DATA_LEN);
      memcpy(&u8ar_inputData[OOB_RANDOM_DATA_LEN], u8pt_confirmData, OOB_CONFIRM_DATA_LEN);

      // Initialize the cryptographic library
      t_retVal = gt_PSAStatus2SLStatus(psa_crypto_init());

      // Check if the initialization was successful
      if (SL_STATUS_OK == t_retVal)
      {
         // Sign the OOB data
         t_retVal = gt_PSAStatus2SLStatus(psa_sign_message(
            t_keyID,                         // Key ID
            PSA_ALG_ECDSA(PSA_ALG_SHA_256),  // Algorithm
            u8ar_inputData,                  // Input data
            OOB_DATA_LEN,                    // Input data length
            u8ar_signature,                  // Signature
            OOB_SIGNATURE_LEN,               // Signature size
            &t_signatureLen                  // Signature length
         ));

         // Check if the signing was successful
         if (SL_STATUS_OK == t_retVal)
         {
            memcpy(u8pt_signedData, u8ar_inputData, OOB_DATA_LEN);
            memcpy(&u8pt_signedData[OOB_DATA_LEN], u8ar_signature, t_signatureLen);

            *u8pt_signedDataLen = OOB_SIGNED_DATA_LEN;
         }
      }
   }

   return t_retVal;
}

/**
 * @private       st_VerifyOOBData
 * @brief         It verifies the OOB data signature using public key.
 * @param[in]     u8pt_randomData Pointer to random data.
 * @param[in]     u8pt_confirmData Pointer to confirm data.
 * @param[out]    u8pt_signature Pointer to signature data.
 * @return        SL_STATUS_OK upon successfully completing the operation,
 *                or any of the error codes from sl_status_t.
 */
static sl_status_t st_VerifyOOBData(uint8_t *u8pt_randomData, uint8_t *u8pt_confirmData, \
   uint8_t *u8pt_signature)
{
   sl_status_t t_retVal;
   uint8_t u8ar_inputData[OOB_DATA_LEN] = { 0 };

   // Check if any pointer is NULL
   if (u8pt_randomData == NULL || u8pt_confirmData == NULL || u8pt_signature == NULL)
   {
      t_retVal = SL_STATUS_NULL_POINTER;
   }
   else
   {
      memcpy(&u8ar_inputData[0], u8pt_randomData, OOB_RANDOM_DATA_LEN);
      memcpy(&u8ar_inputData[OOB_RANDOM_DATA_LEN], u8pt_confirmData, OOB_CONFIRM_DATA_LEN);

      // Initialize the cryptographic library
      t_retVal = gt_PSAStatus2SLStatus(psa_crypto_init());

      // Check if the initialization was successful
      if (SL_STATUS_OK == t_retVal)
      {
         // Sign the OOB data
         t_retVal = gt_PSAStatus2SLStatus(psa_verify_message(
            gt_remotePubKeyID,               // Key ID
            PSA_ALG_ECDSA(PSA_ALG_SHA_256),  // Algorithm
            u8ar_inputData,                  // Input data
            OOB_DATA_LEN,                    // Input data length
            u8pt_signature,                  // Signature
            OOB_SIGNATURE_LEN                // Signature size
         ));
      }
   }

   return t_retVal;
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gt_UpdatePairingReadinessOverGATT
 * @brief         It updates pairing readiness over GATT.
 * @return        SL_STATUS_OK upon successfully completing the operation,
 *                or any of the error codes from sl_status_t.
 */
sl_status_t gt_UpdatePairingReadinessOverGATT(void)
{
   sl_status_t t_retVal = SL_STATUS_FAIL;
   uint8_t u8_isReady2Pair = (uint8_t)true;
   uint8_t u8_isDeviceCertGenerated = (uint8_t)true;
   ECUType_E e_ECUType = CURRENT_ECU_TYPE;

   // Set the GATT database with CSR generated
   t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_IS_READY_2_PAIR, \
      0, gattdb_IS_READY_2_PAIR_len, &u8_isReady2Pair);
   app_assert_status(t_retVal);

   // Set the GATT database with device certificate generated
   t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_IS_DEVICE_CERT_GENERATED, \
      0, gattdb_IS_DEVICE_CERT_GENERATED_len, &u8_isDeviceCertGenerated);
   app_assert_status(t_retVal);

   // As of now during POC, let the EOL tool device decice which BLE is
   // VCU and keyfob. While production, this needs to be autogenerated
   // as per the configuration.
#if 0
   // Set the GATT database with CSR generated
   t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_ECU_TYPE, \
      0, gattdb_ECU_TYPE_len, (uint8_t *)&e_ECUType);
   app_assert_status(t_retVal);
#endif // 0

   return t_retVal;
}

/**
 * @public        gb_WaitingForSystemBoot_PairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForSystemBoot_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal;

   // // Set the flags and compatibility for pairing as required by OOB
   // uint8_t u8_smConfigFlags = (SL_BT_SM_CONFIGURATION_OOB_FROM_BOTH_DEVICES_REQUIRED);
   // sl_bt_sm_io_capability_t e_ioCapability = sl_bt_sm_io_capability_noinputnooutput;

   bd_addr st_address;
   uint8_t u8_addrType;
   uint16_t u16_maxMTU;

   // aes_key_128 st_randomOOB;
   // aes_key_128 st_confirmOOB;

   // uint8_t u8ar_signedOOBData[OOB_SIGNED_DATA_LEN];
   // size_t t_signedOOBDattaLen = 0;

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
               // Transition to next state
               gv_FSM_Transition(&gst_pairingFSMInstance, \
                  &gstar_pairingFSMStateAttrs[ePS_WAITING_FOR_CONNECTION_OPEN]);

               // Mark the event as handled
               b_retVal = true;
            }
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForSystemBoot_PairingFSM_Handler'." APP_LOG_NL);
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
 * @public        gb_WaitingForConnectionOpen_PairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForConnectionOpen_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   bd_addr st_scannedDeviceAddr;
   uint8_t u8_role;

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
            // Get different parameters from the event
            su8_EOLToolConnHandle = stpt_evt->data.evt_connection_opened.connection;
            u8_role = stpt_evt->data.evt_connection_opened.role;
            st_scannedDeviceAddr = stpt_evt->data.evt_connection_opened.address;

            app_log_info("Connection Opened As: %s" , \
               ((sl_bt_connection_role_peripheral == u8_role) \
                  ? ("Peripheral") : ("Central")));
            app_log_nl_info();

            app_log_info("Connected BLE device address: 0x");
            for (uint8_t u8_lpIdx = 0; u8_lpIdx < 6; u8_lpIdx++)
            {
               app_log_append_info("%02X ", st_scannedDeviceAddr.addr[(sizeof(st_scannedDeviceAddr.addr)-1)-u8_lpIdx]);
            }
            app_log_nl_info();

            // Transition to next state
            gv_FSM_Transition(&gst_pairingFSMInstance, \
               &gstar_pairingFSMStateAttrs[ePS_WAITING_FOR_TARGET_DEVICE_BLE_ADDR]);

            // Mark the event as handled
            b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForConnectionOpen_PairingFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitingForTargetDeviceBLEAddr_PairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForTargetDeviceBLEAddr_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   uint16_t u16_attrHandle;
   uint16_t u16_offset;
   uint16_t u16_valueLen;
   uint8_t *u8pt_value;
   bd_addr st_tempAddr;

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
            // Get different parameters from the event
            u16_attrHandle = stpt_evt->data.evt_gatt_server_attribute_value.attribute;
            u16_offset = stpt_evt->data.evt_gatt_server_attribute_value.offset;
            u16_valueLen = stpt_evt->data.evt_gatt_server_attribute_value.value.len;
            u8pt_value = &stpt_evt->data.evt_gatt_server_attribute_value.value.data;

            // Check if the remote BLE device has updated target device's BLE address
            if (u16_attrHandle == gattdb_TARGET_DEVICE_2_PAIR)
            {
               // Check if the target device BLE address received successfully
               if ((sizeof(st_tempAddr) == u16_valueLen))
               {
                  app_log_info("Target device's BLE address received." APP_LOG_NL);
                  app_log_info("Target device's BLE address is: 0x ");

                  // Store & print target device's BLE address
                  for (uint8_t u8_lpIdx = 0; u8_lpIdx < sizeof(st_tempAddr); u8_lpIdx++)
                  {
                     st_tempAddr.addr[u8_lpIdx] = u8pt_value[u8_lpIdx];
                     app_log("%02X ", st_tempAddr.addr[u8_lpIdx]);
                  }
                  app_log_nl_info();

                  // Modify the endianness of the target device for directed advertising
                  for (uint8_t u8_lpIdx = 0; u8_lpIdx < sizeof(st_tempAddr); u8_lpIdx++)
                  {
                     sst_targetDeviceAddr.addr[(sizeof(st_tempAddr) -1) - u8_lpIdx] = \
                        st_tempAddr.addr[u8_lpIdx];
                  }

                  // Transition to next state
                  gv_FSM_Transition(&gst_pairingFSMInstance, \
                     &gstar_pairingFSMStateAttrs[ePS_WAITING_FOR_CURRENT_DEVICE_BLE_ROLE]);

                  // Mark the event as handled
                  b_retVal = true;
               }
               else
               {
                  app_log_info("Received target BLE device address is of inappropriate length." APP_LOG_NL);
               }
            }
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForTargetDeviceBLEAddr_PairingFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitingForCurrentDeviceBLERole_PairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForCurrentDeviceBLERole_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

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
            // Get different parameters from the event
            u16_attrHandle = stpt_evt->data.evt_gatt_server_attribute_value.attribute;
            u16_offset = stpt_evt->data.evt_gatt_server_attribute_value.offset;
            u16_valueLen = stpt_evt->data.evt_gatt_server_attribute_value.value.len;
            u8pt_value = &stpt_evt->data.evt_gatt_server_attribute_value.value.data;

            // Check if the remote BLE device has updated current device's BLE role
            if (u16_attrHandle == gattdb_ECU_ROLE)
            {
               // Check if the target device BLE role received successfully
               if (eBR_UNASSIGNED != (BLERole_E)*u8pt_value)
               {
                  app_log_info("Currnet device's BLE role received." APP_LOG_NL);

                  se_currentDeviceBLERole = (BLERole_E)*u8pt_value;

                  app_log_info("Current device's BLE role is: %s" , \
                     ((eBR_CENTRAL == se_currentDeviceBLERole) \
                        ? ("Central") : ("Peripheral")));
                  app_log_nl_info();

                  // Transition to next state
                  gv_FSM_Transition(&gst_pairingFSMInstance, \
                     &gstar_pairingFSMStateAttrs[ePS_WAITING_FOR_PAIRING_TRIGGER]);

                  // Mark the event as handled
                  b_retVal = true;
               }
            }
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForCurrentDeviceBLERole_PairingFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitingForPairingTrigger_PairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForPairingTrigger_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   // Set the flags and compatibility for pairing as required by OOB
   uint8_t u8_smConfigFlags = (SL_BT_SM_CONFIGURATION_OOB_FROM_BOTH_DEVICES_REQUIRED);
   sl_bt_sm_io_capability_t e_ioCapability = sl_bt_sm_io_capability_noinputnooutput;

   // aes_key_128 st_randomOOB;
   // aes_key_128 st_confirmOOB;

   // uint8_t u8ar_signedOOBData[OOB_SIGNED_DATA_LEN];
   // size_t t_signedOOBDattaLen = 0;

   uint16_t u16_attrHandle;
   uint16_t u16_offset;
   uint16_t u16_valueLen;
   uint8_t *u8pt_value;

   sl_status_t t_retVal = SL_STATUS_FAIL;

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
            // Get different parameters from the event
            u16_attrHandle = stpt_evt->data.evt_gatt_server_attribute_value.attribute;
            u16_offset = stpt_evt->data.evt_gatt_server_attribute_value.offset;
            u16_valueLen = stpt_evt->data.evt_gatt_server_attribute_value.value.len;
            u8pt_value = &stpt_evt->data.evt_gatt_server_attribute_value.value.data;

            // Check if the remote BLE device has triggered pairing
            if (u16_attrHandle == gattdb_IS_PAIRING_REQUESTED)
            {
               // Check if the pairing triggered by EOl device
               if (0 != *u8pt_value)
               {
                  app_log_info("Pairing is triggered by EOL tool." APP_LOG_NL);

                  // Update the current device's device certificate over GATT for the
                  // other device to consume and verify
                  t_retVal = st_UpdateDeviceCertOverGATT();
                  app_assert_status(t_retVal);

                  // Check if the current device is central
                  if (eBR_CENTRAL == se_currentDeviceBLERole)
                  {
                     // Configure SM
                     t_retVal = sl_bt_sm_configure(u8_smConfigFlags, e_ioCapability);
                     app_assert_status(t_retVal);

                     // Configure scanner
                     t_retVal = sl_bt_scanner_set_parameters(
                        sl_bt_scanner_scan_mode_passive, // Passive scanning mode
                        160,                             // min. adv. interval (milliseconds * 1.6)
                        160);                            // window (milliseconds * 1.6)
                     app_assert_status(t_retVal);

                     // Start scanning
                     t_retVal = sl_bt_scanner_start(
                        sl_bt_scanner_scan_phy_1m,       // Scanning on 1M PHY
                        sl_bt_scanner_discover_generic); // Discover limited and general discoverable devices
                     app_assert_status(t_retVal);

                     app_log_info("Scanning for target devices...." APP_LOG_NL);

                     // Check if the scanning started
                     if (SL_STATUS_OK == t_retVal)
                     {
                        // Transition to next state
                        gv_FSM_Transition(&gst_pairingFSMInstance, \
                           &gstar_pairingFSMStateAttrs[ePS_WAITING_FOR_TARGET_DEVICE_DISCOVERY]);
                     }
                  }
                  // Check if the current device is peripheral
                  else if (eBR_PERIPHERAL == se_currentDeviceBLERole)
                  {
                     // Configure SM
                     t_retVal = sl_bt_sm_configure(u8_smConfigFlags, e_ioCapability);
                     app_assert_status(t_retVal);

                     // Create an advertising set.
                     t_retVal = sl_bt_advertiser_create_set(&su8_advSetHandle);
                     app_assert_status(t_retVal);

                     // Generate data for advertising (stack-generated)
                     t_retVal = sl_bt_legacy_advertiser_generate_data(su8_advSetHandle, \
                        sl_bt_advertiser_general_discoverable);
                     app_assert_status(t_retVal);

                     // Set advertising interval to 100ms.
                     t_retVal = sl_bt_advertiser_set_timing(
                        su8_advSetHandle,                // Advertising set handle
                        160,                             // min. adv. interval (milliseconds * 1.6)
                        160,                             // max. adv. interval (milliseconds * 1.6)
                        0,                               // adv. duration
                        0);                              // max. num. adv. events
                     app_assert_status(t_retVal);

                     app_log_info("Directed advertising for target device...." APP_LOG_NL);

                     // Start directed advertising and enable connections.
                     t_retVal = sl_bt_legacy_advertiser_start_directed(su8_advSetHandle, \
                        sl_bt_legacy_advertiser_low_duty_directed_connectable, \
                        sst_targetDeviceAddr, su8_targetDeviceAddrType);
                     app_assert_status(t_retVal);

                     // Check if the directed advertising started
                     if (SL_STATUS_OK == t_retVal)
                     {
                        // Transition to next state
                        gv_FSM_Transition(&gst_pairingFSMInstance, \
                           &gstar_pairingFSMStateAttrs[ePS_WAITING_FOR_CONNECTION_OPENED_FROM_TARGET_DEVICE]);
                     }
                  }

                  // Mark the event as handled
                  b_retVal = true;
               }
            }
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForPairingTrigger_PairingFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitingForTargetDeviceDiscovery_PairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForTargetDeviceDiscovery_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   bd_addr st_scannedDeviceAddr;
   sl_status_t t_retVal = SL_STATUS_FAIL;

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
         // This event reports an advertising data or scan response packet from an
         // advertising device that uses legacy advertising PDUs.
         case sl_bt_evt_scanner_legacy_advertisement_report_id:
         {
            // Get different parameters from the event
            st_scannedDeviceAddr = stpt_evt->data.evt_scanner_legacy_advertisement_report.address;

            // Check if the target address is same as the received address
            if (0 == memcmp(st_scannedDeviceAddr.addr, sst_targetDeviceAddr.addr, sizeof(sst_targetDeviceAddr)))
            {
               // Stop scanning
               t_retVal = sl_bt_scanner_stop();
               app_assert_status(t_retVal);

               // app_log_info("Scanning stopped as target device found." APP_LOG_NL);
               app_log_info("Opening connection with the target device...." APP_LOG_NL);

               // Open connection with this device
               t_retVal = sl_bt_connection_open(
                  stpt_evt->data.evt_scanner_legacy_advertisement_report.address,
                                                // Bluetooth device address
                  stpt_evt->data.evt_scanner_legacy_advertisement_report.address_type,
                                                // Bluetooth device address type
                  sl_bt_scanner_scan_phy_1m,    // PHY to use (same as the one which was used for scanning)
                  &su8_targetDeviceConnHandle);             // Connection handle
               app_assert_status(t_retVal);

               // Check if the connection opened successfully
               if (SL_STATUS_OK == t_retVal)
               {
                  // Transition to next state
                  gv_FSM_Transition(&gst_pairingFSMInstance, \
                     &gstar_pairingFSMStateAttrs[ePS_WAITING_FOR_CONNECTION_OPENED_FROM_TARGET_DEVICE]);
               }
            }
            else
            {
               app_log_info("Scanned BLE device address: 0x");
               for (uint8_t u8_lpIdx = 0; u8_lpIdx < 6; u8_lpIdx++)
               {
                  app_log("%02X ", st_scannedDeviceAddr.addr[(sizeof(st_scannedDeviceAddr.addr)-1)-u8_lpIdx]);
               }
               app_log_nl_info();
               app_log_info("Continue scanning for the target device." APP_LOG_NL);
            }

            // Mark the event as handled
            b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForTargetDeviceDiscovery_PairingFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitingForConnectionOpenedFromTargetDevice_PairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForConnectionOpenedFromTargetDevice_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   bd_addr st_scannedDeviceAddr;
   uint8_t u8_role;

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
            // Get different parameters from the event
            su8_targetDeviceConnHandle = stpt_evt->data.evt_connection_opened.connection;
            u8_role = stpt_evt->data.evt_connection_opened.role;
            st_scannedDeviceAddr = stpt_evt->data.evt_connection_opened.address;

            app_log_info("Connection Opened As: %s" , \
               ((sl_bt_connection_role_peripheral == u8_role) \
                  ? ("Peripheral") : ("Central")));
            app_log_nl_info();

            app_log_info("Connected BLE device address: 0x");
            for (uint8_t u8_lpIdx = 0; u8_lpIdx < 6; u8_lpIdx++)
            {
               app_log_append_info("%02X ", st_scannedDeviceAddr.addr[(sizeof(st_scannedDeviceAddr.addr)-1)-u8_lpIdx]);
            }
            app_log_nl_info();

            // Transition to next state
            gv_FSM_Transition(&gst_pairingFSMInstance, \
               &gstar_pairingFSMStateAttrs[ePS_READ_REMOTE_DEVICE_CERT]);

            // Mark the event as handled
            b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForConnectionOpenedFromTargetDevice_PairingFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_ReadRemoteDeviceCert_PairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_ReadRemoteDeviceCert_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_status_t t_retVal = SL_STATUS_FAIL;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   aes_key_128 st_randomOOB;
   aes_key_128 st_confirmOOB;

   uint8_t u8ar_signedOOBData[OOB_SIGNED_DATA_LEN];
   size_t t_signedOOBDattaLen = 0;

   // Check if the current event is entry event
   if (FSM_EVENT_ENTRY == stpt_event->u32_eventID)
   {
      app_log_info("Reading remote device's device certificate...." APP_LOG_NL);
      app_log_info("Discovering BATL_DEVICE_PROVISIONING service from the remote GATT server." APP_LOG_NL);

      // Load the connection handle in the reading device certificate FSM context
      ((ReadDeviceCertFSMContext_T *)(gst_readDeviceCertFSMContext.vpt_customData))->u8_connHandle = \
         su8_targetDeviceConnHandle;

      // Load the current device's BLE role in the reading device certificate FSM context
      ((ReadDeviceCertFSMContext_T *)(gst_readDeviceCertFSMContext.vpt_customData))->e_currentDeviceBLERole = \
         se_currentDeviceBLERole;

      // Discover BATL_DEVICE_PROVISIONING service
      t_retVal = sl_bt_gatt_discover_primary_services_by_uuid(
         ((ReadDeviceCertFSMContext_T *)(gst_readDeviceCertFSMContext.vpt_customData))->u8_connHandle, // Connection handle
         gattdb.attributes[gattdb_BATL_DEVICE_PROVISIONING - 1].constdata->len,
         gattdb.attributes[gattdb_BATL_DEVICE_PROVISIONING - 1].constdata->data
      );
      app_assert_status(t_retVal);

      // // Mark the event as handled
      // b_retVal = true;
   }
   // Check if the current event is exit event
   else if (FSM_EVENT_EXIT == stpt_event->u32_eventID)
   {
      // Generate device oob data and send over GATT
      t_retVal = sl_bt_sm_set_oob(1, &st_randomOOB, &st_confirmOOB);
      app_assert_status(t_retVal);

      // Generate and sign OOB data using device private key. Also update
      // the local GATT database with signed OOB data.
      app_log_info("Device OOB data generated." APP_LOG_NL);
      app_log_info("Random data: 0x");
      app_log_hexdump_info(&st_randomOOB.data[0], sizeof(aes_key_128));
      app_log_nl_info();
      app_log_info("Confirm data: 0x");
      app_log_hexdump_info(&st_confirmOOB.data[0], sizeof(aes_key_128));
      app_log_nl_info();

      // Sign the OOB data using private key
      t_retVal = st_SignOOBData(&st_randomOOB.data[0], &st_confirmOOB.data[0], \
         u8ar_signedOOBData, &t_signedOOBDattaLen);
      app_assert_status(t_retVal);

      app_log_info("Signature of OOB data is: 0x");
      app_log_hexdump_info(&u8ar_signedOOBData[OOB_DATA_LEN], OOB_SIGNATURE_LEN);
      app_log_nl_info();

      // Update the GATT database with signed OOB data
      t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_OOB_DATA, \
         0, gattdb_OOB_DATA_len, u8ar_signedOOBData);
      app_assert_status(t_retVal);
   }
   else
   {
      // Populate the event for reading device certificate FSM
      gst_readDeviceCertFSMEvent.vpt_eventParam = ((sl_bt_msg_t *)stpt_event->vpt_eventParam);

      // Check if the reading device certificate FSM handled successfully
      if (true == gb_FSM_Dispatch(&gst_readDeviceCertFSMInstance, &gst_readDeviceCertFSMEvent))
      {
         // Check if the completion flag has been raised by the read device certificate FSM
         if (1 == gstar_readDeviceCertFSMStateAttrs[eRDCS_WAITING_FOR_CERT_BLOCK_4].u32_flags)
         {
            app_log_info("Proceeding further for OOB pairing...." APP_LOG_NL);

            // Transition to next state
            gv_FSM_Transition(&gst_pairingFSMInstance, \
               &gstar_pairingFSMStateAttrs[ePS_WAITING_FOR_PAIRING_SERVICE_DISCOVERY]);

            // // Mark the event as handled
            // b_retVal = true;
         }
      }
   }
}

/**
 * @public        gb_WaitingForPairingServiceDiscovery_PairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForPairingServiceDiscovery_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_status_t t_retVal = SL_STATUS_FAIL;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   uint32_t u32_serviceHandle;
   uint8array st_serviceUUID;
   uint16_t u16_result;

   uint8_t u8ar_OOB_DATA_UUID[] = {
      0x4a, 0xa2, 0x36, 0x59, 0xfb, 0x86, 0x53, 0x99, 0x75, 0x4e, 0xa2, 0x47, 0x42, 0xe7, 0xd4, 0x83,
   };

   // Check if the current event is entry event
   if (FSM_EVENT_ENTRY == stpt_event->u32_eventID)
   {
      app_log_info("Reading remote device's OOB data...." APP_LOG_NL);
      app_log_info("Discovering BATL_PAIRING service from the remote GATT server." APP_LOG_NL);

      // Load the connection handle in the reading device certificate FSM context
      ((ReadDeviceCertFSMContext_T *)(gst_readDeviceCertFSMContext.vpt_customData))->u8_connHandle = \
         su8_targetDeviceConnHandle;

      // Load the current device's BLE role in the reading device certificate FSM context
      ((ReadDeviceCertFSMContext_T *)(gst_readDeviceCertFSMContext.vpt_customData))->e_currentDeviceBLERole = \
         se_currentDeviceBLERole;

      // Discover BATL_DEVICE_PROVISIONING service
      t_retVal = sl_bt_gatt_discover_primary_services_by_uuid(
         ((ReadDeviceCertFSMContext_T *)(gst_readDeviceCertFSMContext.vpt_customData))->u8_connHandle, // Connection handle
         gattdb.attributes[gattdb_BATL_PAIRING - 1].constdata->len,
         gattdb.attributes[gattdb_BATL_PAIRING - 1].constdata->data
      );
      app_assert_status(t_retVal);

      // // Mark the event as handled
      // b_retVal = true;
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

            // Check if the discovered service is BATL_PAIRING service
            if (0 == (memcmp(st_serviceUUID.data, \
               gattdb.attributes[gattdb_BATL_PAIRING - 1].constdata->data, \
               st_serviceUUID.len)))
            {
               app_log_info("Discovered BATL_PAIRING service from the remote GATT server." APP_LOG_NL);

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
               app_log_info("Discovering OOB_DATA characteristic from the remote GATT server...." APP_LOG_NL);

               // Discover OOB_DATA characteristic
               t_retVal = sl_bt_gatt_discover_characteristics_by_uuid(
                  su8_targetDeviceConnHandle,
                  su32_serviceHandle,
                  sizeof(u8ar_OOB_DATA_UUID),
                  u8ar_OOB_DATA_UUID
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_pairingFSMInstance, \
                  &gstar_pairingFSMStateAttrs[ePS_INQUIRE_FOR_OOB_DATA]);
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
            app_log_debug("Event is not handled at 'gb_WaitingForPairingServiceDiscovery_PairingFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_InquireForOOBData_PairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_InquireForOOBData_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);
   sl_status_t t_retVal = SL_STATUS_FAIL;

   uint16_t u16_result;

   uint16_t u16_charHandle;
   uint8array st_charUUID;

   uint8_t u8ar_OOB_DATA_UUID[] = {
      0x4a, 0xa2, 0x36, 0x59, 0xfb, 0x86, 0x53, 0x99, 0x75, 0x4e, 0xa2, 0x47, 0x42, 0xe7, 0xd4, 0x83,
   };

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

            // Check if the discovered characteristic is OOB_DATA characteristic
            if (0 == (memcmp(st_charUUID.data, \
               u8ar_OOB_DATA_UUID, \
               st_charUUID.len)))
            {
               app_log_info("Discovered OOB_DATA characteristic from the remote GATT server." APP_LOG_NL);

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
               app_log_info("Reading OOB_DATA characteristic from the remote GATT server...." APP_LOG_NL);

               // Read OOB_DATA characteristic
               t_retVal = sl_bt_gatt_read_characteristic_value(
                  su8_targetDeviceConnHandle,
                  su16_charHandle
               );
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_pairingFSMInstance, \
                  &gstar_pairingFSMStateAttrs[ePS_WAITING_FOR_OOB_DATA]);
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
            app_log_debug("Event is not handled at 'gb_InquireForOOBData_PairingFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitingForOOBData_PairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForOOBData_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   sl_status_t t_retVal = SL_STATUS_FAIL;
   uint16_t u16_result;

   uint8_t u8ar_remoteOOBData[OOB_SIGNED_DATA_LEN] = { 0 };
   uint8_t u8_remoteOOBDataLen = 0;

   aes_key_128 st_remoteRandomOOB;
   aes_key_128 st_remoteConfirmOOB;
   uint8_t u8ar_remoteOOBSignature[OOB_SIGNATURE_LEN] = { 0 };

   // Set the flags and compatibility for pairing as required by OOB
   uint8_t u8_smConfigFlags = (SL_BT_SM_CONFIGURATION_OOB_FROM_BOTH_DEVICES_REQUIRED);
   sl_bt_sm_io_capability_t e_ioCapability = sl_bt_sm_io_capability_noinputnooutput;

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
            u8_remoteOOBDataLen = stpt_evt->data.evt_gatt_characteristic_value.value.len;
            memcpy(&u8ar_remoteOOBData[0], &stpt_evt->data.evt_gatt_characteristic_value.value.data[0], \
               u8_remoteOOBDataLen);

            // Check if the read data length is as expected
            if (OOB_SIGNED_DATA_LEN == u8_remoteOOBDataLen)
            {
               memcpy(&st_remoteRandomOOB.data[0], &u8ar_remoteOOBData[0], OOB_RANDOM_DATA_LEN);
               memcpy(&st_remoteConfirmOOB.data[0], &u8ar_remoteOOBData[OOB_RANDOM_DATA_LEN], OOB_CONFIRM_DATA_LEN);
               memcpy(&u8ar_remoteOOBSignature[0], &u8ar_remoteOOBData[OOB_DATA_LEN], OOB_SIGNATURE_LEN);

               app_log_info("Received random OOB data is: 0x");
               app_log_hexdump_info(&st_remoteRandomOOB.data[0], OOB_RANDOM_DATA_LEN);
               app_log_nl_info();

               app_log_info("Received confirm OOB data is: 0x");
               app_log_hexdump_info(&st_remoteConfirmOOB.data[0], OOB_CONFIRM_DATA_LEN);
               app_log_nl_info();

               app_log_info("Received OOB data signature is: 0x");
               app_log_hexdump_info(&u8ar_remoteOOBSignature[0], OOB_SIGNATURE_LEN);
               app_log_nl_info();

               // Verify remote OOB data
               t_retVal = st_VerifyOOBData(&st_remoteRandomOOB.data[0], &st_remoteConfirmOOB.data[0], \
                  u8ar_remoteOOBSignature);
               app_assert_status(t_retVal);

               // Check if the remote OOB data is verified successfully
               if (SL_STATUS_OK == t_retVal)
               {
                  app_log_info("Target device's OOB data verified successfully." APP_LOG_NL);

                  // Set the remote OOB data for pairing
                  t_retVal = sl_bt_sm_set_remote_oob(1, st_remoteRandomOOB, st_remoteConfirmOOB);
                  app_assert_status(t_retVal);

                  // Check if the remote OOB data is set successfully
                  if (SL_STATUS_OK == t_retVal)
                  {
                     app_log_info("Remote OOB data set successfully."  APP_LOG_NL);

                     // Check if the current device is central
                     if (eBR_CENTRAL == se_currentDeviceBLERole)
                     {
                        // Increase security using OOB data (initiate pairing)
                        t_retVal = sl_bt_sm_increase_security(su8_targetDeviceConnHandle);
                        app_assert_status(t_retVal);

                        // Check if the pairing initiated successfully
                        if (SL_STATUS_OK == t_retVal)
                        {
                           app_log_info("OOB pairing initiated successfully."  APP_LOG_NL);
                        }
                     }

                     // Transition to next state
                     gv_FSM_Transition(&gst_pairingFSMInstance, \
                        &gstar_pairingFSMStateAttrs[ePS_WAITING_FOR_PAIRING_CONFIRMATION]);
                  }
               }
               else
               {
                  app_log_info("Target device's OOB data verification failed." APP_LOG_NL);
               }
            }

            // // Mark the event as handled
            // b_retVal = true;
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForOOBData_PairingFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_WaitingForPairingConfirmation_PairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForPairingConfirmation_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_status_t t_retVal = SL_STATUS_FAIL;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   uint8_t u8_securityMode;
   uint8_t u8_isPairingSuccessful;

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
         // Triggered after increasing security has been completed successfully
         // and indicates the latest security mode of the connection.
         case sl_bt_evt_connection_parameters_id:
         {
            // Get different parameters from the event
            u8_securityMode = stpt_evt->data.evt_connection_parameters.security_mode;

            // Check if the current security mode is as expected
            if (sl_bt_connection_mode1_level4 == u8_securityMode)
            {
               u8_isPairingSuccessful = 1;
               app_log_info("OOB pairing completed successfully with OOB data." APP_LOG_NL);

               // Update the GATT database with signed OOB data
               t_retVal = sl_bt_gatt_server_write_attribute_value(gattdb_IS_PAIRING_SUCCESSFUL, \
                  0, gattdb_IS_PAIRING_SUCCESSFUL_len, &u8_isPairingSuccessful);
               app_assert_status(t_retVal);

               // Transition to next state
               gv_FSM_Transition(&gst_pairingFSMInstance, \
                  &gstar_pairingFSMStateAttrs[ePS_DEVICE_PAIRED]);
            }
            else
            {
               app_log_info("OOB pairing failed or was not completed with OOB data." APP_LOG_NL);
               app_log_info("Current security mode is: %X." APP_LOG_NL, u8_securityMode);
            }
         }
         break;

         default:
         {
            app_log_debug("Event is not handled at 'gb_WaitingForPairingConfirmation_PairingFSM_Handler'." APP_LOG_NL);
            app_log_debug("Unhandled event is: 0x");
            app_log_append_debug("%02X", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_debug();
         }
         break;
      }
   }
}

/**
 * @public        gb_DevicePaired_PairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_DevicePaired_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_status_t t_retVal = SL_STATUS_FAIL;
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
