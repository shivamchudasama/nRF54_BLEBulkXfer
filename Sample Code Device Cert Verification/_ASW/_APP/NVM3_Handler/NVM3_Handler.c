/**
 * @file          NVM3_Handler.c
 * @brief         Source file containing wrapper functions for NVM3.
 * @date          05/09/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "NVM3_Handler.h"

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
static sl_status_t st_ErrCode2SLStatus(Ecode_t u32_errCode);

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
 * @var           <Variable name>
 * @brief         <Variable details>.
 */

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           <Variable name>
 * @brief         <Variable details>.
 */

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
 * @private       st_ErrCode2SLStatus
 * @brief         Convert error code from NVM3 to SL status.
 * @param[in]     u32_errCode Error code from NVM3.
 * @return        Converted sl_status_t.
 */
static sl_status_t st_ErrCode2SLStatus(Ecode_t u32_errCode)
{
   sl_status_t t_retVar = SL_STATUS_FAIL;

   switch (u32_errCode)
   {
      case ECODE_OK:
      {
         t_retVar = SL_STATUS_OK;
      }
      break;
      case ECODE_NVM3_ERR_PARAMETER:
      {
         t_retVar = SL_STATUS_INVALID_PARAMETER;
      }
      break;
      case ECODE_NVM3_ERR_KEY_INVALID:
      {
         t_retVar = SL_STATUS_INVALID_PARAMETER;
      }
      break;
      case ECODE_NVM3_ERR_KEY_NOT_FOUND:
      {
         t_retVar = SL_STATUS_BT_PS_KEY_NOT_FOUND;
      }
      break;
      case ECODE_NVM3_ERR_STORAGE_FULL:
      {
         t_retVar = SL_STATUS_BT_PS_STORE_FULL;
      }
      break;
      default:
      {
         t_retVar = \
            (sl_status_t)(SL_STATUS_BLUETOOTH_SPACE + 0x80 + (u32_errCode & (~ECODE_EMDRV_NVM3_BASE)));
      }
      break;
   }

   return t_retVar;
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gt_EraseAllNVM3
 * @brief         This is a wrapper function for erasing all data in NVM3.
 * @return        Converted (from Ecode_t to sl_status_t) status of the initialization.
 *                SL_STATUS_OK in case of no error.
 */
sl_status_t gt_EraseAllNVM3(void)
{
  return st_ErrCode2SLStatus(nvm3_eraseAll(CSR_NVM3_HANDLE));
}

/**
 * @public        gt_WriteRawNVM3
 * @brief         Write raw data from NVM3.
 * @param[in]     u32_region Region in NVM3.
 * @param[in]     u16_tag Tag in region of NVM3.
 * @param[out]    u8pt_buff Buffer to store the read data.
 * @param[out]    t_len Length of the buffer.
 * @return        Converted (from Ecode_t to sl_status_t) status of the initialization.
 *                SL_STATUS_OK in case of no error.
 */
sl_status_t gt_WriteRawNVM3(uint32_t u32_region, uint16_t u16_tag, \
   uint8_t *u8pt_buff, size_t t_len)
{
   return st_ErrCode2SLStatus(nvm3_writeData(
      CSR_NVM3_HANDLE,                       // NVM3 handle
      (u32_region | u16_tag),                // Object identifier (key)
      u8pt_buff,                             // Buffer to store the data to write
      t_len                                  // Length of the data to be written
   ));
}

/**
 * @public        gt_ReadRawNVM3
 * @brief         This is a wrapper function for reading any data from NVM3.
 *                It first reads the requested object info from NVM3 and reads
 *                the requested bytes of data (if possible, if object size is less
 *                than the requested bytes, than it reads actual size only).
 * @param[in]     u32_region Region in NVM3.
 * @param[in]     u16_tag Tag in region of NVM3.
 * @param[out]    u8pt_buff Buffer to store the read data.
 * @param[inout]  tpt_len Application shall fill the required length of the
 *                data to be read. After performing an NVM3 read operation, this
 *                wrapper shall return the actual length of the data read.
 * @return        Converted (from Ecode_t to sl_status_t) status of the initialization.
 *                SL_STATUS_OK in case of no error.
 */
sl_status_t gt_ReadRawNVM3(uint32_t u32_region, uint16_t u16_tag,
   uint8_t *u8pt_buff, size_t *tpt_len)
{
   uint32_t u32_objType;
   size_t t_objLen;
   size_t t_readLen;
   sl_status_t t_retVal = SL_STATUS_OK;

   // Get the type and size of the object from NVM3
   t_retVal = st_ErrCode2SLStatus(nvm3_getObjectInfo(
      CSR_NVM3_HANDLE,                       // NVM3 handle
      (u32_region | u16_tag),                // Object identifier (key)
      &u32_objType,                          // Object type (data type or counter type)
      &t_objLen                              // Object size
   ));

   // Check if the object was found
   if (SL_STATUS_OK == t_retVal)
   {
      // If requested length of the data to be read is more than available length,
      // limit the read length.
      t_readLen = (t_objLen > *tpt_len) ? *tpt_len : t_objLen;

      // Read the data from NVM3
      t_retVal = st_ErrCode2SLStatus(nvm3_readData(
         CSR_NVM3_HANDLE,                    // NVM3 handle
         (u32_region | u16_tag),             // Object identifier (key)
         u8pt_buff,                          // Buffer to store the read data
         t_readLen                           // Length of the data to be read
      ));

      // Check if the read operation was successful
      if (SL_STATUS_OK == t_retVal)
      {
         // Update the actual length of the data read
         *tpt_len = t_readLen;
      }
   }

   return t_retVal;
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
