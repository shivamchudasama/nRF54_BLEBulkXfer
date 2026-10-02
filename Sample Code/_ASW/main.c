/**
 * @file          main.c
 * @brief         Source file containing the main function which performs all initialization
 *                activity.
 * @date          16/02/26
 * @author        Yash Sunil Giramkar [YSG], Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "CSR_Generator.h"
#include "DeviceCert.h"
#include "ConnectionHandling.h"
#include "GATT_GenericCallbacks.h"
#include "DeviceProvisioning.h"
#include "OOBPairing.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                       PRIVATE FUNCTION DEFINITIONS                          */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        main
 * @brief         Initializes the BLE stack and starts advertising.
 * @return        0 upon successful execution.
 */
int main(void)
{
   uint8_t u8_isReadyToPair = 0;

   // Check if loading the device certificate from ITS completed successfully
   if (PSA_SUCCESS == gt_LoadStoredDeviceCert())
   {
      // As the device certificate is already stored, directly verify it against
      // the CA certificate.

      // TODO: Verify the stored device certificate against the CA certificate
      
      // Update the pairing readiness over the local GATT database
      gv_GATT_LocalWrite(&gst_isReadyToPairDesc, (uint8_t *)&u8_isReadyToPair, 
         sizeof(u8_isReadyToPair));      
   }
   else
   {
      APP_LOG_INF("Generating or loading CSR as there is no valid device certificate available.");
      // Generate/ Load CSR
      gv_GenerateOrLoadCSR();

      // Update local GATT database with CSR and its availability
      gv_GATT_LocalWrite(&gst_CSRDataDesc, (uint8_t *)&gst_CSRData.u8ar_CSR[0], 
         gst_CSRData.u16_CSRLen);
      gv_GATT_LocalWrite(&gst_CSRGenerationStatusDesc, 
         (uint8_t *)&gst_CSRData.u8_isCSRGenerated, sizeof(gst_CSRData.u8_isCSRGenerated));
   }

   // Init and start BLE advertising
   gv_BLEInitStartAdv();

   return 0;
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Yash Sunil Giramkar [YSG], Shivam Chudasama [SC]
 */
