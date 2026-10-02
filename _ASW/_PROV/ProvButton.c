/**
 * @file          ProvButton.c
 * @brief         Source file containing the DK button that wipes the provisioning
 *                credentials. Pressing Button 0 starts a delayed work item;
 *                releasing it before CONFIG_PROV_WIPE_HOLD_MS cancels it. When it
 *                runs, it asks the provisioning thread for a wipe, which needs no
 *                connection and sends no RESULT. Kept apart from Prov.c so the
 *                host tests of Prov.c need no DK library.
 * @date          02/10/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "ProvButton.h"
#include <zephyr/kernel.h>
#include <dk_buttons_and_leds.h>
#include "Prov.h"
#include "AppLog.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           PROV_WIPE_BUTTON_MSK
 * @brief         Button that wipes the provisioning (Button 0 on the nRF54L15 DK).
 */
#define PROV_WIPE_BUTTON_MSK                 (DK_BTN1_MSK)

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
static void sv_WipeHeld(struct k_work *stpt_work);
static void sv_ButtonChanged(uint32_t u32_buttonState, uint32_t u32_hasChanged);

/******************************************************************************/
/*                                                                            */
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           sst_wipeWork
 * @brief         Runs once the button has been held for CONFIG_PROV_WIPE_HOLD_MS.
 */
static K_WORK_DELAYABLE_DEFINE(sst_wipeWork, sv_WipeHeld);

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
 * @private       sv_WipeHeld
 * @brief         The button was held long enough: request the wipe.
 * @param[in]     stpt_work Work item (unused).
 * @return        None.
 */
static void sv_WipeHeld(struct k_work *stpt_work)
{
   ARG_UNUSED(stpt_work);

   APP_LOG_WRN("button held %d ms: wiping provisioning", CONFIG_PROV_WIPE_HOLD_MS);
   gv_Prov_RequestWipe();
}

/**
 * @private       sv_ButtonChanged
 * @brief         DK library button handler: arm the wipe on press, cancel it on
 *                release.
 * @param[in]     u32_buttonState Current state of all buttons.
 * @param[in]     u32_hasChanged Buttons that changed.
 * @return        None.
 */
static void sv_ButtonChanged(uint32_t u32_buttonState, uint32_t u32_hasChanged)
{
   // Check if the wipe button changed
   if ((u32_hasChanged & PROV_WIPE_BUTTON_MSK) == 0U)
   {
      return;
   }

   // Check if it was pressed or released
   if ((u32_buttonState & PROV_WIPE_BUTTON_MSK) != 0U)
   {
      (void)k_work_schedule(&sst_wipeWork, K_MSEC(CONFIG_PROV_WIPE_HOLD_MS));
   }
   else
   {
      (void)k_work_cancel_delayable(&sst_wipeWork);
   }
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gi_ProvButton_Init
 * @brief         Start watching the wipe button. Call once from main(), after
 *                gi_Prov_Init().
 * @return        0 on success, otherwise the dk_buttons_init() error (the device
 *                then runs without the button; DEPROVISION still works).
 */
int gi_ProvButton_Init(void)
{
   int i_ret;

   i_ret = dk_buttons_init(sv_ButtonChanged);

   // Check if the buttons could be set up
   if (i_ret != 0)
   {
      APP_LOG_ERR("dk_buttons_init failed (%d)", i_ret);
   }

   return i_ret;
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
