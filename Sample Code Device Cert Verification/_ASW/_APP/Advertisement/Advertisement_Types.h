/**
 * @file          Advertisement_Types.h
 * @brief         Header file containing structure definitions required for
 *                parsing BLE advertisement.
 * @date          13/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _ADVERTISEMENT_TYPES_H
#define _ADVERTISEMENT_TYPES_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include "AD_Types.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           MAX_LEG_ADV_DATA_PKT_LEN
 * @brief         Maximum size of legacy advertising data.
 */
#define MAX_LEG_ADV_DATA_PKT_LEN             (31)

/**
 * @def           SIZE_OF_LEN_FIELD
 * @brief         Size of length field in legacy AD packet.
 */
#define SIZE_OF_LEN_FIELD                    (1)

/**
 * @def           SIZE_OF_AD_TYPE_FIELD
 * @brief         Size of AD Type field in legacy AD packet.
 */
#define SIZE_OF_AD_TYPE_FIELD                (1)

/**
 * @def           MAX_SIZE_OF_AD_DATA_FIELD
 * @brief         Maximum size of AD Data field in legacy AD packet.
 */
#define MAX_SIZE_OF_AD_DATA_FIELD            (MAX_LEG_ADV_DATA_PKT_LEN - SIZE_OF_LEN_FIELD - SIZE_OF_AD_TYPE_FIELD)

/**
 * @def           MAX_EXT_ADV_DATA_PKT_LEN
 * @brief         Maximum size of extended advertising data.
 */
#define MAX_EXT_ADV_DATA_PKT_LEN             (1650)

/**
 * @def           MAX_AD_FIELDS
 * @brief         Maximum number of AD fields in single AD packet.
 */
#define MAX_AD_FIELDS                        (10)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          BLEAdvError_E
 * @brief         Enums listing all the possible error codes for BLE Advertising
 *                APIs.
 */
typedef enum
{
   eBAE_OK,                                  /**< No error. */
   eBAE_INVALID_AD_FIELD_LENGTH,             /**< Invalid AD field length. */
   eBAE_NO_SUCH_AD_FIELD_AVAILABLE,          /**< No such AD field available in
                                                   AD list. */
} BLEAdvError_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        ADTypeFlags_T
 * @brief         Structure containing information different flags used for
 *                flags type AD field.
 */
typedef struct
{
   uint8_t u8_limitedDiscoverableMode:1;     /**< LE Limited Discoverable Mode. */
   uint8_t u8_generalDiscoverableMode:1;     /**< LE LE General Discoverable Mode. */
   uint8_t u8_BREDRNotSupported:1;           /**< BR/ EDR Not Supported. */
   uint8_t u8_simultaneousLEBREDR:1;         /**< Simultaneous LE and BR/EDR to Same
                                                   Device Capable. */
   uint8_t u8_previouslyUsed:1;              /**< Previously used. */
   uint8_t u8_reserved:3;                    /**< Reserved for future use. */
} ADTypeFlags_T;

/**
 * @struct        ADField_T
 * @brief         Structure containing information of different parameters of
 *                advertisement data field.
 */
typedef struct
{
   ADTypes_E e_adType;                       /**< Advertisement data type */
   uint8_t u8_adLength;                      /**< Length of AD data */
   uint8_t u8ar_adData[MAX_SIZE_OF_AD_DATA_FIELD];
                                             /**< Actual AD data */
} ADField_T;

/**
 * @struct        ADList_T
 * @brief         Structure containing list of all AD fields parsed from AD packets.
 */
typedef struct
{
   ADField_T star_adFields[MAX_AD_FIELDS];   /**< All the individual AD fields */
   uint8_t u8_count;                         /**< Count of total AD fields */
} ADList_T;

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

#endif //!_ADVERTISEMENT_TYPES_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
