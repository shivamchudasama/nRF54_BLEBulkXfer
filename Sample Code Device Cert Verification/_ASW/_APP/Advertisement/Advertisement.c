/**
 * @file          Advertisement.c
 * @brief         Source file containing APIs for parsing BLE advertising.
 * @date          13/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "Advertisement.h"

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
 * @private       <Function name>
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        ge_BLE_ParseLegacyAdvData
 * @brief         This function parses legacy advertising data.
 * @param[in]     u8pt_advData Pointer to advertising data.
 * @param[in]     u8_advDataLen Total length of advertising data.
 * @param[out]    stpt_adList Pointer to AD list structure. The function parses AD
 *                packet and populates this structure back.
 * @return        eBAE_OK if no error, otherwise appropriate error code from BLEAdvError_E.
 */
BLEAdvError_E ge_BLE_ParseLegacyAdvData(uint8_t *u8pt_advData, uint8_t u8_advDataLen, \
   ADList_T *stpt_adList)
{
   BLEAdvError_E e_retVal = eBAE_OK;
   uint8_t u8_idx = 0;
   uint8_t u8_length;
   uint8_t u8_adDataLength;
   ADTypes_E e_adType;
   ADField_T *stpt_adField;

   // Reset the count to AD list
   stpt_adList->u8_count = 0;

   while (u8_idx < u8_advDataLen)
   {
      // Take the length of the new AD field (First byte of each AD field is length)
      u8_length = u8pt_advData[u8_idx];

      // Check if there is any more AD data left in AD packet or the current index
      // and length together are more than total AD packet length
      if ((u8_length == 0) || (u8_idx + u8_length >= u8_advDataLen))
      {
         e_retVal = eBAE_INVALID_AD_FIELD_LENGTH;
      }

      // Check if there is no error in parameters
      if (eBAE_OK == e_retVal)
      {
         // Take the AD type of new AD field (Second byte of each AD field is AD type)
         e_adType = (ADTypes_E)u8pt_advData[u8_idx + 1];
         u8_adDataLength = u8_length - 1;

         // Check if the AD list is not full
         if (stpt_adList->u8_count < MAX_AD_FIELDS)
         {
            stpt_adField = &stpt_adList->star_adFields[stpt_adList->u8_count++];
            stpt_adField->e_adType = (ADTypes_E)e_adType;
            stpt_adField->u8_adLength = u8_adDataLength;
            memcpy(stpt_adField->u8ar_adData, &u8pt_advData[u8_idx + 2], u8_adDataLength);
         }

         // Move to next AD field
         u8_idx += u8_length + 1;
      }
   }

   return e_retVal;
}

/**
 * @public        ge_BLE_FindADByType
 * @brief         This function finds AD field from AD list by AD type.
 * @param[in]     stpt_adList Pointer to AD list structure.
 * @param[in]     e_adType Type of AD field which to be found from the list.
 * @param[out]    stptpt_adField Pointer pointer to requested AD field.
 * @return        Structure containing AD field of desired type.
 */
BLEAdvError_E ge_BLE_FindADByType(const ADList_T *stpt_adList, ADTypes_E e_adType, \
   ADField_T **stptpt_adField)
{
   BLEAdvError_E e_retVal = eBAE_NO_SUCH_AD_FIELD_AVAILABLE;
   uint8_t u8_idx;

   for (uint8_t u8_idx = 0; u8_idx < stpt_adList->u8_count; u8_idx++)
   {
      // Check if AD field type of any AD field is matching with the requested one
      if (stpt_adList->star_adFields[u8_idx].e_adType == e_adType)
      {
         *stptpt_adField = &stpt_adList->star_adFields[u8_idx];
         e_retVal = eBAE_OK;
         break;
      }
   }

   return e_retVal;
}

/**
 * @public        gv_BLE_GenerateLegacyAdvData
 * @brief         This function generates legacy advertisement data by combining
 *                multiple ADField_T entries into a single BLE legacy advertisement packet.
 * @param[in]     stptpt_adList Pointer to AD List structure.
 * @param[out]    u8pt_advData Pointer to generated advertisement packet.
 * @param[out]    u8pt_advDataLength Pointer to generated advertisement packet length.
 * @return        eBAE_OK if there is no error or else it will return appropriate
 *                error codes from BLEAdvError_E.
 */
BLEAdvError_E ge_BLE_GenerateLegacyAdvData(const ADList_T *stptpt_adList, \
   uint8_t *u8pt_advData, uint8_t *u8pt_advDataLength)
{
   BLEAdvError_E e_retVal = eBAE_OK;
   uint8_t u8_idx = 0;
   uint8_t u8_lpIdx = 0;
   uint8_t u8_adLength = 0;
   uint8_t u8_adSize = 0;
   uint8_t u8_reqBytes = 0;
   ADField_T *stpt_adField;

   // Extract the AD fields from given AD list one by one
   for (uint8_t u8_lpIdx = 0; u8_lpIdx < stptpt_adList->u8_count; u8_lpIdx++)
   {
      stpt_adField = &stptpt_adList->star_adFields[u8_lpIdx];
      u8_adLength = stpt_adField->u8_adLength;
      u8_adSize = u8_adLength + 1;
      u8_reqBytes = u8_adSize + 1;

      // Check if space remaining
      if ((u8_idx + u8_reqBytes) > MAX_LEG_ADV_DATA_PKT_LEN)
      {
         e_retVal = eBAE_INVALID_AD_FIELD_LENGTH;
         break;
      }

      // Check if there is any error in earlier steps
      if (eBAE_OK == e_retVal)
      {
         // Add Length byte (Type + Data)
         u8pt_advData[u8_idx++] = u8_adSize;

         // Add Type (cast enum to byte for output buffer)
         u8pt_advData[u8_idx++] = (uint8_t)stpt_adField->e_adType;

         // Add Data
         memcpy(&u8pt_advData[u8_idx], stpt_adField->u8ar_adData, u8_adLength);

         // Increase the index
         u8_idx += u8_adLength;
      }
   }

   // Check if there is any error in earlier steps
   if (eBAE_OK == e_retVal)
   {
      *u8pt_advDataLength = u8_idx;
   }

   return e_retVal;
}

#if 0
int main()
{
#if 0
   uint8_t u8ar_adData[] = {
      2, eADT_FLAGS, 0x06,                   // Flags
      3, eADT_COMPLETE_LIST_OF_16_BIT_SERVICE_OR_SERVICE_CLASS_UUIDS, 0x0A, 0x18,
                                             // UUIDs
      5, eADT_COMPLETE_LOCAL_NAME, 'T', 'e', 's', 't'
                                             // Local Name: "Test"
   };
#else
   uint8_t u8ar_adData[MAX_AD_FIELDS * MAX_LEG_ADV_DATA_PKT_LEN];
#endif // 0

#if 0
   uint8_t u8_advDataLen = sizeof(u8ar_adData);
   ADList_T st_adList;
   ADField_T *stpt_adField;
#endif // 0

#if 1
   ADList_T st_adList = {
      .star_adFields[0] = {
         .u8_adLength = 2,
         .e_adType = eADT_FLAGS,
         .u8ar_adData = {
            0x06
         }
      },
      .star_adFields[1] = {
         .u8_adLength = 3,
         .e_adType = eADT_COMPLETE_LIST_OF_16_BIT_SERVICE_OR_SERVICE_CLASS_UUIDS,
         .u8ar_adData = {
            0x0A, 0x18
         }
      },
      .star_adFields[2] = {
         .u8_adLength = 5,
         .e_adType = eADT_COMPLETE_LOCAL_NAME,
         .u8ar_adData = {
            'T', 'e', 's', 't'
         }
      },
      .u8_count = 3
   };
#endif // 0

#if 0
   ge_BLE_ParseLegacyAdvData(u8ar_adData, u8_advDataLen, &st_adList);

   ge_BLE_FindADByType(&st_adList, \
      eADT_COMPLETE_LIST_OF_16_BIT_SERVICE_OR_SERVICE_CLASS_UUIDS, &stpt_adField);
#endif // 0

   ge_BLE_GenerateLegacyAdvData(&st_adList, u8ar_adData);
   return 0;
}
#endif // 0

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
