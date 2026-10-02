/***************************************************************************//**
 * @file
 * @brief Application interface.
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
 * @file          app.h
 * @brief         Header file containing BLE application.
 * @date          11/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _APP_H
#define _APP_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdbool.h>
#include "FSM.h"
#include "MainFSM.h"
#include "CSRAvailableFSM.h"
#include "PairingFSM.h"
#include "Advertisement.h"
#include "CSR_Generator.h"
#include "DeviceCert.h"
#include "Pairing.h"
#include "Pairing_Config.h"
#include "gatt_db.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           MAX_CSR_LENGTH
 * @brief         Maximum length of CSR data.
 */
#define MAX_CSR_LENGTH                       (1024)

/**
 * @def           U32_ENDIAN_SWAP
 * @brief         Macro to swap endianness of a 32-bit unsigned integer.
 */
#define U32_ENDIAN_SWAP(val_u32)             (                                                                    \
                                                (uint32_t)((uint32_t)((uint32_t)(val_u32) & 0x000000FFU) << 24) | \
                                                (uint32_t)((uint32_t)((uint32_t)(val_u32) & 0x0000FF00U) << 8)  | \
                                                (uint32_t)((uint32_t)((uint32_t)(val_u32) & 0x00FF0000U) >> 8)  | \
                                                (uint32_t)((uint32_t)((uint32_t)(val_u32) & 0xFF000000U) >> 24)   \
                                             )

/**
 * @def           U16_ENDIAN_SWAP
 * @brief         Macro to swap endianness of a 16-bit unsigned integer.
 */
#define U16_ENDIAN_SWAP(val_u16)             (                                                                 \
                                                (uint16_t)((uint16_t)((uint16_t)(val_u16) & 0x00FFU) << 8)  |  \
                                                (uint16_t)((uint16_t)((uint16_t)(val_u16) & 0xFF00U) >> 8)     \
                                             )

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
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/
void app_proceed(void);
bool app_is_process_required(void);
bool app_mutex_acquire(void);
void app_mutex_release(void);
void app_init_bt(void);

#endif //!_APP_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
