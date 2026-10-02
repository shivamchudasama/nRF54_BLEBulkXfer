/**
 * @file          Advertisement.h
 * @brief         Header file containing APIs for BLE advertising.
 * @date          13/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _ADVERTISEMENT_H
#define _ADVERTISEMENT_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include <string.h>
#include "AD_Types.h"
#include "Advertisement_Types.h"
#include "AdvConfig.h"

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
/**
 * @enum          <Enum name>
 * @brief         <Enum details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        <Structure name>
 * @brief         <Structure details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                   UNIONS                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @union         <Union name>
 * @brief         <Union details>.
 */

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
extern BLEAdvError_E ge_BLE_ParseLegacyAdvData(uint8_t *u8pt_advData, uint8_t u8_advDataLen, \
   ADList_T *stpt_adList);
extern BLEAdvError_E ge_BLE_FindADByType(const ADList_T *stpt_adList, ADTypes_E e_adType, \
   ADField_T **stptpt_adField);
extern BLEAdvError_E ge_BLE_GenerateLegacyAdvData(const ADList_T *stptpt_adList, \
   uint8_t *u8pt_advData, uint8_t *u8pt_advDataLength);

#endif //!_ADVERTISEMENT_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
