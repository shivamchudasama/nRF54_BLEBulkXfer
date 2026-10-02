/**
 * @file          NVM3_Handler.h
 * @brief         Header file containing NVM3 handler function declarations.
 * @date          05/09/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _NVM3_HANDLER_H
#define _NVM3_HANDLER_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "sl_status.h"
#include "ecode.h"
#include "nvm3.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           CSR_NVM3_HANDLE
 * @brief         NVM3 handle for CSR data.
 */
#define CSR_NVM3_HANDLE                      (nvm3_defaultHandle)

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
extern sl_status_t gt_EraseAllNVM3(void);
extern sl_status_t gt_WriteRawNVM3(uint32_t u32_region, uint16_t u16_tag, \
   uint8_t *u8pt_buff, size_t t_len);
extern sl_status_t gt_ReadRawNVM3(uint32_t u32_region, uint16_t u16_tag, uint8_t *u8pt_buff,
   size_t *tpt_len);


#endif //!_NVM3_HANDLER_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
