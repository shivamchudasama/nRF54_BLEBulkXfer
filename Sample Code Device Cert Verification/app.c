/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

/**
 * @file          app.c
 * @brief         Source file containing BLE application.
 * @date          11/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "sl_bt_api.h"
#include "sl_main_init.h"
#include "app_assert.h"
#include "app.h"
#include "app_types.h"

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
// static sl_status_t st_CSRAvailableFSM(sl_bt_msg_t *stpt_evt);
static sl_status_t st_SMConfirmBonding(sl_bt_msg_t *stpt_evt);
static sl_status_t st_GATTServerAttributeValue_DEVICE_CERT_VERIFIED(sl_bt_msg_t *stpt_evt);
static sl_status_t st_GATTServiceID_DEVICE_CERT_VERIFIED(sl_bt_msg_t *stpt_evt);
static sl_status_t st_GATTCharacteristicID_DEVICE_CERT_VERIFIED(sl_bt_msg_t *stpt_evt);
static sl_status_t st_GATTCharacteristicValue_DEVICE_CERT_VERIFIED(sl_bt_msg_t *stpt_evt);
static sl_status_t st_ScannerLegacyAdvertisementReportHandler_DEVICE_CERT_VERIFIED(sl_bt_msg_t *stpt_evt);
static sl_status_t st_SMConfirmPasskey(sl_bt_msg_t *stpt_evt);
static sl_status_t st_ConnectionOpenedHandler_CSR_AVAILABLE(sl_bt_msg_t *stpt_evt);
static sl_status_t st_ConnectionOpenedHandler_DEVICE_CERT_VERIFIED(sl_bt_msg_t *stpt_evt);
static sl_status_t st_ConnectionClosedHandler(sl_bt_msg_t *stpt_evt);

/******************************************************************************/
/*                                                                            */
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/
extern sli_bt_gattdb_value_t gattdb_attribute_field_24;

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
 * @var           su8_isDeviceCertGenerated
 * @brief         Flag to indicate if the device certificate has been generated
 *                by remote client (CA).
 */
static uint8_t su8_isDeviceCertGenerated = 0;

/**
 * @var           se_appState
 * @brief         Application current state.
 */
static AppState_E se_appState = eAS_UNDEFINED;

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
 * @private       st_ConnectionOpenedHandler_CSR_AVAILABLE
 * @brief         sl_bt_evt_connection_opened_id event handler.
 * @param[in]     stpt_evt Pointer to event.
 * @return        Any of the error code from sl_status_t.
 */
static sl_status_t st_ConnectionOpenedHandler_CSR_AVAILABLE(sl_bt_msg_t *stpt_evt)
{
   sl_status_t t_retVal = SL_STATUS_OK;

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

   return t_retVal;
}

/**
 * @private       st_SMConfirmBonding
 * @brief         sl_bt_evt_sm_confirm_bonding_id event handler.
 * @param[in]     stpt_evt Pointer to event.
 * @return        Any of the error code from sl_status_t.
 */
static sl_status_t st_SMConfirmBonding(sl_bt_msg_t *stpt_evt)
{
   sl_status_t t_retVal = SL_STATUS_OK;

   // Check if the connection handle is not assigned
   if (!su8_connHandle)
   {
      su8_connHandle = stpt_evt->data.evt_sm_confirm_bonding.connection;
   }

   t_retVal = sl_bt_sm_bonding_confirm(su8_connHandle, true);
   app_assert_status(t_retVal);

   return t_retVal;
}

/**
 * @private       st_SMConfirmPasskey
 * @brief         sl_bt_evt_sm_confirm_passkey_id event handler.
 * @param[in]     stpt_evt Pointer to event.
 * @return        Any of the error code from sl_status_t.
 */
static sl_status_t st_SMConfirmPasskey(sl_bt_msg_t *stpt_evt)
{
   sl_status_t t_retVal = SL_STATUS_OK;

   // Check if the connection handle is not assigned
   if (!su8_connHandle)
   {
      su8_connHandle = stpt_evt->data.evt_sm_confirm_passkey.connection;
   }

   app_log_debug("Passkey: %d" APP_LOG_NL, stpt_evt->data.evt_sm_confirm_passkey.passkey);

   t_retVal = sl_bt_sm_passkey_confirm(su8_connHandle, true);
   app_assert_status(t_retVal);

   return t_retVal;
}

/**
 * @private       st_ConnectionOpenedHandler_DEVICE_CERT_VERIFIED
 * @brief         sl_bt_evt_connection_opened_id event handler.
 * @param[in]     stpt_evt Pointer to event.
 * @return        Any of the error code from sl_status_t.
 */
static sl_status_t st_ConnectionOpenedHandler_DEVICE_CERT_VERIFIED(sl_bt_msg_t *stpt_evt)
{
   return gt_PairingFSM(stpt_evt);
}

/**
 * @private       st_GATTServerAttributeValue_DEVICE_CERT_VERIFIED
 * @brief         sl_bt_evt_gatt_server_attribute_value_id event handler.
 * @param[in]     stpt_evt Pointer to event.
 * @return        Any of the error code from sl_status_t.
 */
static sl_status_t st_GATTServerAttributeValue_DEVICE_CERT_VERIFIED(sl_bt_msg_t *stpt_evt)
{
   return gt_PairingFSM(stpt_evt);
}

/**
 * @private       st_GATTServiceID_DEVICE_CERT_VERIFIED
 * @brief         sl_bt_evt_gatt_service_id event handler.
 * @param[in]     stpt_evt Pointer to event.
 * @return        Any of the error code from sl_status_t.
 */
static sl_status_t st_GATTServiceID_DEVICE_CERT_VERIFIED(sl_bt_msg_t *stpt_evt)
{
   return gt_PairingFSM(stpt_evt);
}

/**
 * @private       st_GATTCharacteristicID_DEVICE_CERT_VERIFIED
 * @brief         sl_bt_evt_gatt_characteristic_id event handler.
 * @param[in]     stpt_evt Pointer to event.
 * @return        Any of the error code from sl_status_t.
 */
static sl_status_t st_GATTCharacteristicID_DEVICE_CERT_VERIFIED(sl_bt_msg_t *stpt_evt)
{
   return gt_PairingFSM(stpt_evt);
}

/**
 * @private       st_GATTCharacteristicValue_DEVICE_CERT_VERIFIED
 * @brief         sl_bt_evt_gatt_characteristic_value_id event handler.
 * @param[in]     stpt_evt Pointer to event.
 * @return        Any of the error code from sl_status_t.
 */
static sl_status_t st_GATTCharacteristicValue_DEVICE_CERT_VERIFIED(sl_bt_msg_t *stpt_evt)
{
   return gt_PairingFSM(stpt_evt);
}

/**
 * @private       st_ScannerLegacyAdvertisementReportHandler_DEVICE_CERT_VERIFIED
 * @brief         sl_bt_evt_scanner_legacy_advertisement_report_id event handler.
 * @param[in]     stpt_evt Pointer to event.
 * @return        Any of the error code from sl_status_t.
 */
static sl_status_t st_ScannerLegacyAdvertisementReportHandler_DEVICE_CERT_VERIFIED(sl_bt_msg_t *stpt_evt)
{
   return gt_PairingFSM(stpt_evt);
}

/**
 * @private       st_ConnectionClosedHandler
 * @brief         sl_bt_evt_connection_closed_id event handler.
 * @param[in]     stpt_evt Pointer to event.
 * @return        Any of the error code from sl_status_t.
 */
static sl_status_t st_ConnectionClosedHandler(sl_bt_msg_t *stpt_evt)
{
   sl_status_t t_retVal;

   // Reset the connection handle
   su8_connHandle = 0;

   app_log_info("Connection is closed" APP_LOG_NL);

   app_log_info("Reason code: 0x%X" APP_LOG_NL, stpt_evt->data.evt_connection_closed.reason);

   // // Generate data for advertising
   // t_retVal = sl_bt_legacy_advertiser_generate_data(su8_advSetHandle, \
   //    sl_bt_advertiser_general_discoverable);
   // app_assert_status(t_retVal);

   // Restart advertising after client has disconnected.
   t_retVal = sl_bt_legacy_advertiser_start(su8_advSetHandle, \
      sl_bt_legacy_advertiser_connectable);
   app_assert_status(t_retVal);

   // // Delete all bondings.
   // t_retVal = sl_bt_sm_delete_bondings();
   // app_assert_status(t_retVal);

   return t_retVal;
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gv_AppInit
 * @brief         Application Init.
 * @return        None.
 */
void gv_AppInit(void)
{
   sl_status_t t_retVal = SL_STATUS_OK;

   app_log_info("Starting keyfob BLE project in SoC mode...." APP_LOG_NL);

   // Generate CSR/ load device certificate
   gv_GenerateCSR();

   // Check if the device certificate found in NVM3
   if (gb_isDeviceCertAvailable)
   {
      // Verify the device certificate
      t_retVal = gt_VerifyOwnDeviceCertificate();

      // Check if the device certificate verified successfully
      if ((SL_STATUS_OK == t_retVal) && (gb_isDeviceCertVerified))
      {
         // Update the application state
         se_appState = eAS_DEVICE_CERT_VERIFIED;
         gv_FSM_Init(&gst_mainFSMInstance, &gst_mainFSMContext, \
            &gstar_mainFSMStateAttrs[eMS_DEVICE_CERT_VERIFIED]);
      }
   }
   // Check if the CSR is available
   else if (gb_isCSRAvailable)
   {
      // Update the application state
      se_appState = eAS_CSR_AVAILABLE;
      gv_FSM_Init(&gst_mainFSMInstance, &gst_mainFSMContext, \
         &gstar_mainFSMStateAttrs[eMS_CSR_AVAILABLE]);
   }

   /////////////////////////////////////////////////////////////////////////////
   // Put your additional application init code here!                         //
   // This is called once during start-up.                                    //
   /////////////////////////////////////////////////////////////////////////////
}

/**
 * @public        gv_AppProcessAction
 * @brief         Application Process Action.
 * @return        None.
 */
void gv_AppProcessAction(void)
{
   // Check if application processing is required
   if (app_is_process_required())
   {
      /////////////////////////////////////////////////////////////////////////////
      // Put your additional application code here!                              //
      // This is will run each time app_proceed() is called.                     //
      // Do not call blocking functions from here!                               //
      /////////////////////////////////////////////////////////////////////////////
   }
}

/**
 * @public        sl_bt_on_event
 * @brief         Bluetooth stack event handler.
 * @param[in]     stpt_evt Event coming from the Bluetooth stack.
 * @return        None.
 */
void sl_bt_on_event(sl_bt_msg_t *stpt_evt)
{
   sl_status_t t_retVal;

   gst_mainFSMEvent.vpt_eventParam = (sl_bt_msg_t *)stpt_evt;

   // Dispatch all the events to MainFSM
   gb_FSM_Dispatch(&gst_mainFSMInstance, &gst_mainFSMEvent);
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
