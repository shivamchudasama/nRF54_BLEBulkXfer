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
#include "ConnectionHandling.h"
#include "DataStore.h"
#include "Prov.h"
#include "ProvButton.h"
#include "BulkRouter.h"

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
 * @brief         Initializes the BulkXfer users (hex upload, provisioning), the BulkXfer
 *                Server, the BLE stack and starts advertising.
 * @return        0 upon successful execution.
 */
int main(void)
{
   // Register the BulkXfer appType ranges: hex upload, then provisioning. The
   // provisioning init also restores the stored certificates, or generates (first
   // boot) or loads the device key and CSR.
   (void)gi_DataStore_Init();
   (void)gi_Prov_Init();

   // Holding DK Button 0 wipes the provisioning credentials
   (void)gi_ProvButton_Init();

   // Start the BulkXfer Server before advertising, so it is ready for the first
   // connection. It needs no Bluetooth stack yet.
   (void)gi_BulkRouter_Start();

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
