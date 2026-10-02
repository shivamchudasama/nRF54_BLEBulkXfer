/***************************************************************************//**
 * @file
 * @brief Baremetal compatibility layer.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
 * @file          app_bm.c
 * @brief         Source file containing baremetal compatibility layer.
 * @date          11/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include "sl_core.h"
#include "sl_main_init.h"
#include "app.h"

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
 * @var           su16_proceedReq
 * @brief         "Semaphore" indicating that it is required to execute
 *                application process action.
 */
static uint16_t su16_proceedReq;

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
 * @public        app_init_bt
 * @brief         Application Runtime Init.
 * @return        None.
 */
void app_init_bt(void)
{
  su16_proceedReq = 0;
}

/**
 * @public        app_proceed
 * @brief         Proceed with execution.
 * @return        None.
 */
void app_proceed(void)
{
  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_CRITICAL();
  if (su16_proceedReq < UINT16_MAX)
  {
    su16_proceedReq++;
  }
  CORE_EXIT_CRITICAL();
}

/**
 * @public        app_is_process_required
 * @brief         Check if it is required to process with execution.
 * @return        None.
 */
bool app_is_process_required(void)
{
  bool ret = false;
  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_CRITICAL();
  if (su16_proceedReq > 0)
  {
    su16_proceedReq--;
    ret = true;
  }
  CORE_EXIT_CRITICAL();
  return ret;
}

/**
 * @public        app_mutex_acquire
 * @brief         Acquire access to protected variables.
 * @return        None.
 */
bool app_mutex_acquire(void)
{
  // There are no tasks to protect shared resources from.
  return true;
}

/**
 * @public        app_mutex_release
 * @brief         Finish access to protected variables.
 * @return        None.
 */
void app_mutex_release(void)
{
  // There are no tasks to protect shared resources from.
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
