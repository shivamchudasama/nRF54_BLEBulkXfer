/**
 * @file          Pairing.c
 * @brief         Source file containing BLE device pairing related APIs.
 * @date          10/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "Pairing.h"
#include "Pairing_Config.h"

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
static sl_status_t st_readDeviceCertOverGATT(ReadDeviceCert_T st_readDeviceCert);

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

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           se_currentState
 * @brief         Current state of device provisioning FSM.
 */
static PairingState_E se_currentState = ePS_WAIT_FOR_TARGET_DEVICE_ADDR;

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
 * @var           sb_isPairingTriggered
 * @brief         Flag to indicate if the pairing triggered by EOL tool.
 */
static bool sb_isPairingTriggered = false;

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
 * @var           su8ar_targetDeviceCertDER
 * @brief         Buffer to hold the terget device certificate in DER format.
 */
static uint8_t su8ar_targetDeviceCertDER[CHAIN_LINK_DATA_LEN * CHAIN_LINK_DATA_NUM] = { 0 };

/**
 * @var           su32_targetDeviceCertDERLen
 * @brief         Length of the terget device certificate.
 */
static uint32_t su32_targetDeviceCertDERLen = 0;

/**
 * @var           se_readTargetDeviceCertCurrentState
 * @brief         Current state for reading target device's device certificate FSM.
 */
static ReadTargetDeviceCertState_E se_readTargetDeviceCertCurrentState = eRTDC_DISCOVER_BATL_DEVICE_PROVISIONING_SERVICE;

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
 * @private       st_readDeviceCertOverGATT
 * @brief         It reads the device certificate details over GATT.
 * @param[in]     u8_connHandle Connection handle with a BLE device from which the
 *                device certificate is to be read.
 * @param[in]     u32_serviceHandle Service handle with a BLE device from which the
 *                device certificate is to be read.
 * @param[in]     t_deviceCertLen Length of device certificate to be read.
 * @return        SL_STATUS_OK upon successfully completing the operation,
 *                or any of the error codes from sl_status_t.
 */
static sl_status_t st_readDeviceCertOverGATT(ReadDeviceCert_T st_readDeviceCert)
{
   sl_status_t t_retVal = SL_STATUS_FAIL;
   static uint32_t su32_targetDeviceCertDERLenRemaining = 0;
   size_t t_readValue = 0;

   uint8_t u8ar_IS_DEVICE_CERT_GENERATED_UUID[] = {
      0x7a, 0x75, 0xbe, 0x4d, 0x70, 0x8d, 0xbc, 0xb2, 0x36, 0x4f, 0xcc, 0x8c, 0xf9, 0x98, 0x96, 0x3a,
   };

   uint8_t u8ar_DEVICE_CERT_LENGTH_UUID[] = {
      0x45, 0x74, 0xd7, 0xf1, 0x41, 0x77, 0x42, 0xba, 0xed, 0x4a, 0xff, 0xa8, 0x01, 0x32, 0x8e, 0xf3,
   };

   switch (se_readTargetDeviceCertCurrentState)
   {
      // Discover the desired service
      case eRTDC_DISCOVER_BATL_DEVICE_PROVISIONING_SERVICE:
      {
         app_log_info("Requesting to discover BATL_DEVICE_PROVISIONING service from the remote GATT server." APP_LOG_NL);

         // Discover BATL_DEVICE_PROVISIONING service
         t_retVal = sl_bt_gatt_discover_primary_services_by_uuid(
            st_readDeviceCert.u8_connHandle,
            gattdb.attributes[gattdb_BATL_DEVICE_PROVISIONING - 1].constdata->len,
            gattdb.attributes[gattdb_BATL_DEVICE_PROVISIONING - 1].constdata->data
         );
         app_assert_status(t_retVal);

         if (SL_STATUS_OK == t_retVal)
         {
            // Transit to next state
            se_readTargetDeviceCertCurrentState = eRTDC_WAIT_FOR_DEVICE_PROVISIONING_SERVICE_DISCOVERY;
         }
      }
      break;

      // Discovered the desired service, discover the desired characteristic
      case eRTDC_WAIT_FOR_DEVICE_PROVISIONING_SERVICE_DISCOVERY:
      {
         app_log_info("Requesting to discover IS_DEVICE_CERT_GENERATED characteristic from the remote GATT server." APP_LOG_NL);

         // Discover IS_DEVICE_CERT_GENERATED characteristic
         t_retVal = sl_bt_gatt_discover_characteristics_by_uuid(
            st_readDeviceCert.u8_connHandle,
            st_readDeviceCert.u32_serviceHandle,
            gattdb_IS_DEVICE_CERT_GENERATED_len,
            u8ar_IS_DEVICE_CERT_GENERATED_UUID
         );
         app_assert_status(t_retVal);

         if (SL_STATUS_OK == t_retVal)
         {
            // Transit to next state
            se_readTargetDeviceCertCurrentState = eRTDC_INQUIRE_FOR_DEVICE_CERT_AVAILABILITY;
         }
      }
      break;

      // Inquire for the target device's device certificate availability
      case eRTDC_INQUIRE_FOR_DEVICE_CERT_AVAILABILITY:
      {
         app_log_info("Requesting to read value of IS_DEVICE_CERT_GENERATED characteristic from the remote GATT server." APP_LOG_NL);

         // Read the device certificate characteristic value
         t_retVal = sl_bt_gatt_read_characteristic_value(
            st_readDeviceCert.u8_connHandle,
            st_readDeviceCert.u16_charHandle
         );
         app_assert_status(t_retVal);

         if (SL_STATUS_OK == t_retVal)
         {
            // Transit to next state
            se_readTargetDeviceCertCurrentState = eRTDC_WAIT_FOR_DEVICE_CERT_AVAILABILITY;
         }
      }
      break;

      // Wait for device certificate availability
      case eRTDC_WAIT_FOR_DEVICE_CERT_AVAILABILITY:
      {
         // Check if hte device certificate is available at target device
         if (st_readDeviceCert.t_value.data[0])
         {
            app_log_info("Target device's device certificate is available to read." APP_LOG_NL);

            // Discover DEVICE_CERT_LENGTH characteristic
            t_retVal = sl_bt_gatt_discover_characteristics_by_uuid(
               st_readDeviceCert.u8_connHandle,
               st_readDeviceCert.u32_serviceHandle,
               gattdb_DEVICE_CERT_LENGTH_len,
               u8ar_DEVICE_CERT_LENGTH_UUID
            );
            app_assert_status(t_retVal);

            if (SL_STATUS_OK == t_retVal)
            {
               // Transit to next state
               se_readTargetDeviceCertCurrentState = eRTDC_INQUIRE_FOR_DEVICE_CERT_LENGTH;
            }
         }
      }
      break;

      // Inquire for device certificate length
      case eRTDC_INQUIRE_FOR_DEVICE_CERT_LENGTH:
      {
         // (*((uint16_t*)(u8pt_value)));
         su32_targetDeviceCertDERLenRemaining = (uint16_t*)st_readDeviceCert.t_value.data;

         app_log_info("Target device's device certificate length has been successfully received." APP_LOG_NL);
         app_log_info("Device certificate length: %d" APP_LOG_NL, su32_targetDeviceCertDERLenRemaining);
      }
      break;

      default:
      {

      }
      break;
   }

   // // Check if the device certificatee value is yet to read
   // if (0 == su32_targetDeviceCertDERLenRemaining)
   // {
   //    // Read remote device's GATT database
   //    t_retVal = sl_bt_gatt_read_multiple_characteristic_values(u8_connHandle, t_deviceCertLen, gattdb_DEVICE_CERT_LENGTH, \
   //       0, gattdb_DEVICE_CERT_LENGTH_len, &t_readValue, &su32_targetDeviceCertDERLenRemaining);
   //    app_assert_status(t_retVal);
   // }

   return t_retVal;
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gt_PairingFSM
 * @brief         This function implements the pairing state machine.
 * @param[in]     stpt_evt Pointer to the Bluetooth event.
 * @return        SL_STATUS_OK if successful, error code otherwise.
 */
sl_status_t gt_PairingFSM(sl_bt_msg_t *stpt_evt)
{
#if 0
   sl_status_t t_retVal = SL_STATUS_FAIL;

   uint16_t u16_attrHandle;
   uint16_t u16_offset;
   uint16_t u16_valueLen;
   uint8_t *u8pt_value;
   bd_addr st_tempAddr;
   bd_addr st_scannedDeviceAddr;
   static uint8_t su8_connHandle;
   uint8_t u8_role;
   static ReadDeviceCert_T sst_readDeviceCert = { 0 };

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
      }
      break;

      // This event reports an advertising data or scan response packet from an
      // advertising device that uses legacy advertising PDUs.
      case sl_bt_evt_scanner_legacy_advertisement_report_id:
      {
         // Get different parameters from the event
         st_scannedDeviceAddr = stpt_evt->data.evt_scanner_legacy_advertisement_report.address;
      }
      break;

      // This event indicates that a new connection was opened.
      case sl_bt_evt_connection_opened_id:
      {
         // Get different parameters from the event
         su8_connHandle = stpt_evt->data.evt_connection_opened.connection;
         u8_role = stpt_evt->data.evt_connection_opened.role;
         st_scannedDeviceAddr = stpt_evt->data.evt_connection_opened.address;
      }
      break;

      // This event indicates that we've discovered a service from remote GATT
      case sl_bt_evt_gatt_service_id:
      {
         // Get different parameters from the event
         sst_readDeviceCert.u32_serviceHandle = stpt_evt->data.evt_gatt_service.service;
      }

      // This event indicated that we've discovered a characteristic from remote GATT
      case sl_bt_evt_gatt_characteristic_id:
      {
         // Get different parameters from the event
         sst_readDeviceCert.u16_charHandle = stpt_evt->data.evt_gatt_characteristic.characteristic;
      }
      break;

      // This event contains the data of a characteristic sent by GATT server
      case sl_bt_evt_gatt_characteristic_value_id:
      {
         // Get different parameters from the event
         sst_readDeviceCert.u8_attrOpCode = stpt_evt->data.evt_gatt_characteristic_value.att_opcode;
         sst_readDeviceCert.u16_offset = stpt_evt->data.evt_gatt_characteristic_value.offset;
         sst_readDeviceCert.t_value = stpt_evt->data.evt_gatt_characteristic_value.value;
      }
      break;

      default:
      {
         // None.
      }
      break;
   }

   // Run the FSM as per the current state, each state is capable of handling
   // certain events in that state.
   switch (se_currentState)
   {
      // Waiting for EOL tool to send the target device's BLE address
      case ePS_WAIT_FOR_TARGET_DEVICE_ADDR:
      {
         // Check if the current event is generated because remote device has modified
         // current device's GATT value which is BLE target device's address.
         if ((sl_bt_evt_gatt_server_attribute_value_id == SL_BT_MSG_ID(stpt_evt->header)) \
            && (u16_attrHandle == gattdb_TARGET_DEVICE_2_PAIR))
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

               // Update the current state
               se_currentState = ePS_WAIT_FOR_BLE_ROLE;
            }
            else
            {
               app_log_info("Received target BLE device address is of inappropriate length." APP_LOG_NL);
            }
         }
         // Check if the connection is opened with another BLE device (mostly for POC,
         // the EOL tool keep on opening and closing its connection with the same device).
         // This is to handle that scenario.
         else if (sl_bt_evt_connection_opened_id == SL_BT_MSG_ID(stpt_evt->header))
         {
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
         }
         else
         {
            app_log_info("Received event is: %x", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_info();
            app_log_info("Received attribute handle is: %x", u16_attrHandle);
            app_log_nl_info();
            app_log_info("Received events/ attribute is not handled at current state of pairing state machine." APP_LOG_NL);
         }
      }
      break;

      // Waiting for host device's BLE role in an ongoing priring procedure
      case ePS_WAIT_FOR_BLE_ROLE:
      {
         // Check if the current event is generated because remote device has modified
         // current device's GATT value which is BLE target device's address
         if ((sl_bt_evt_gatt_server_attribute_value_id == SL_BT_MSG_ID(stpt_evt->header)) && \
            (u16_attrHandle == gattdb_ECU_ROLE))
         {
            // Check if the target device BLE role received successfully
            if (eBR_UNASSIGNED != (BLERole_E)*u8pt_value)
            {
               app_log_info("Currnet device's BLE role received." APP_LOG_NL);

               se_deviceBLERole = (BLERole_E)*u8pt_value;

               app_log_info("Current device's BLE role is: %s" , \
                  ((eBR_CENTRAL == se_deviceBLERole) \
                     ? ("Central") : ("Peripheral")));
               app_log_nl_info();

               // Update the current state
               se_currentState = ePS_WAIT_FOR_PAIRING_TRIGGER;
            }
         }
         // Check if the connection is opened with another BLE device (mostly for POC,
         // the EOL tool keep on opening and closing its connection with the same device).
         // This is to handle that scenario.
         else if (sl_bt_evt_connection_opened_id == SL_BT_MSG_ID(stpt_evt->header))
         {
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
         }
         else
         {
            app_log_info("Received event is: %x", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_info();
            app_log_info("Received attribute handle is: %x", u16_attrHandle);
            app_log_nl_info();
            app_log_info("Received events/ attribute is not handled at current state of pairing state machine." APP_LOG_NL);
         }
      }
      break;

      // Waiting EOL tool to trigger for pairing with target device
      case ePS_WAIT_FOR_PAIRING_TRIGGER:
      {
         // Check if the current event is generated because remote device has modified
         // current device's GATT value which is start pairing
         if ((sl_bt_evt_gatt_server_attribute_value_id == SL_BT_MSG_ID(stpt_evt->header)) && \
            (u16_attrHandle == gattdb_IS_PAIRING_REQUESTED))
         {
            // Check if the pairing triggered by EOl device
            if (0 != *u8pt_value)
            {
               app_log_info("Pairing is triggered by EOL tool." APP_LOG_NL);

               sb_isPairingTriggered = true;

               // Update the current device's device certificate over GATT for the
               // other device to consume and verify
               t_retVal = st_UpdateDeviceCertOverGATT();
               app_assert_status(t_retVal);

               // Check if the current device is central
               if (eBR_CENTRAL == se_deviceBLERole)
               {
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
                     // Update the current state
                     se_currentState = ePS_WAIT_FOR_TARGET_DEVICE_DISCOVERY;
                  }
               }
               // Check if the current device is peripheral
               else if (eBR_PERIPHERAL == se_deviceBLERole)
               {
                  // Create an advertising set.
                  t_retVal = sl_bt_advertiser_create_set(&su8_advSetHandle);
                  app_assert_status(t_retVal);

                  // // Generate data for advertising (stack-generated)
                  // t_retVal = sl_bt_legacy_advertiser_generate_data(su8_advSetHandle, \
                  //    sl_bt_advertiser_general_discoverable);
                  // app_assert_status(t_retVal);

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
                     // Update the current state
                     se_currentState = ePS_WAIT_FOR_CONNECTION_OPENED_FROM_TARGET_DEVICE;
                  }
               }
            }
         }
         // Check if the connection is opened with another BLE device (mostly for POC,
         // the EOL tool keep on opening and closing its connection with the same device).
         // This is to handle that scenario.
         else if (sl_bt_evt_connection_opened_id == SL_BT_MSG_ID(stpt_evt->header))
         {
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
         }
         else
         {
            app_log_info("Received event is: %x", SL_BT_MSG_ID(stpt_evt->header));
            app_log_nl_info();
            app_log_info("Received attribute handle is: %x", u16_attrHandle);
            app_log_nl_info();
            app_log_info("Received events/ attribute is not handled at current state of pairing state machine." APP_LOG_NL);
         }
      }
      break;

      // Waiting for target device discovery
      case ePS_WAIT_FOR_TARGET_DEVICE_DISCOVERY:
      {
         // Check if the target address is same as the received address
         if (0 == memcmp(st_scannedDeviceAddr.addr, sst_targetDeviceAddr.addr, sizeof(sst_targetDeviceAddr)))
         {
            // // Stop scanning
            // t_retVal = sl_bt_scanner_stop();
            // app_assert_status(t_retVal);

            // app_log_info("Scanning stopped as target device found." APP_LOG_NL);
            app_log_info("Opening connection with the target device...." APP_LOG_NL);

            // Open connection with this device
            t_retVal = sl_bt_connection_open(
               stpt_evt->data.evt_scanner_legacy_advertisement_report.address,
                                             // Bluetooth device address
               stpt_evt->data.evt_scanner_legacy_advertisement_report.address_type,
                                             // Bluetooth device address type
               sl_bt_scanner_scan_phy_1m,    // PHY to use (same as the one which was used for scanning)
               &su8_connHandle);             // Connection handle
            app_assert_status(t_retVal);

            // Update the current state
            se_currentState = ePS_WAIT_FOR_CONNECTION_OPENED_FROM_TARGET_DEVICE;
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
      }
      break;

      // Waiting for connection to be opened from the target device
      case ePS_WAIT_FOR_CONNECTION_OPENED_FROM_TARGET_DEVICE:
      {
         // Store the connection handle
         su8_connHandle = su8_connHandle;

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

         // Update the current state
         se_currentState = ePS_READ_REMOTE_DEVICE_CERT;
      }

      // Note: It is fall through state for the above state ().
      // Read the remote device's device certificate
      case ePS_READ_REMOTE_DEVICE_CERT:
      {
         app_log_info("Reading remote device's device certificate...." APP_LOG_NL);

         sst_readDeviceCert.u8_connHandle = su8_connHandle;
         st_readDeviceCertOverGATT(sst_readDeviceCert);
      }
      break;

      default:
      {
         app_log_info("Unhandled state." APP_LOG_NL);
      }
      break;
   }

   return t_retVal;
#endif
}
/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
