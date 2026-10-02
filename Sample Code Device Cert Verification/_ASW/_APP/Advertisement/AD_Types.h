/**
 * @file          AD_Types.h
 * @brief         Header file containing different AD types in BLE.
 * @date          13/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _AD_TYPES_H
#define _AD_TYPES_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/

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
/**
 * @enum          ADTypes_E
 * @brief         Enumeration of different AD types given by BLE SIG.
 */
typedef enum
{
   eADT_FLAGS = 0x01,
   eADT_INCOMPLETE_LIST_OF_16_BIT_SERVICE_OR_SERVICE_CLASS_UUIDS = 0x02,
   eADT_COMPLETE_LIST_OF_16_BIT_SERVICE_OR_SERVICE_CLASS_UUIDS = 0x03,
   eADT_INCOMPLETE_LIST_OF_32_BIT_SERVICE_OR_SERVICE_CLASS_UUIDS = 0x04,
   eADT_COMPLETE_LIST_OF_32_BIT_SERVICE_OR_SERVICE_CLASS_UUIDS = 0x05,
   eADT_INCOMPLETE_LIST_OF_128_BIT_SERVICE_OR_SERVICE_CLASS_UUIDS = 0x06,
   eADT_COMPLETE_LIST_OF_128_BIT_SERVICE_OR_SERVICE_CLASS_UUIDS = 0x07,
   eADT_SHORTENED_LOCAL_NAME = 0x08,
   eADT_COMPLETE_LOCAL_NAME = 0x09,
   eADT_TX_POWER_LEVEL = 0x0A,
   eADT_CLASS_OF_DEVICE = 0x0D,
   eADT_SIMPLE_PAIRING_HASH_C_192 = 0x0E,
   eADT_SIMPLE_PAIRING_RANDOMIZER_R_192 = 0x0F,
   eADT_DEVICE_ID = 0x10,
   eADT_SECURITY_MANAGER_TK_VALUE_1 = 0x10,
   eADT_SECURITY_MANAGER_OUT_OF_BAND_FLAGS = 0x11,
   eADT_PERIPHERAL_CONNECTION_INTERVAL_RANGE = 0x12,
   eADT_LIST_OF_16_BIT_SERVICE_SOLICITATION_UUIDS = 0x14,
   eADT_LIST_OF_128_BIT_SERVICE_SOLICITATION_UUIDS = 0x15,
   eADT_SERVICE_DATA_16_BIT_UUID = 0x16,
   eADT_PUBLIC_TARGET_ADDRESS = 0x17,
   eADT_RANDOM_TARGET_ADDRESS = 0x18,
   eADT_APPEARANCE = 0x19,
   eADT_ADVERTISING_INTERVAL = 0x1A,
   eADT_LE_BLUETOOTH_DEVICE_ADDRESS = 0x1B,
   eADT_LE_ROLE = 0x1C,
   eADT_SIMPLE_PAIRING_HASH_C_256 = 0x1D,
   eADT_SIMPLE_PAIRING_RANDOMIZER_R_256 = 0x1E,
   eADT_LIST_OF_32_BIT_SERVICE_SOLICITATION_UUIDS = 0x1F,
   eADT_SERVICE_DATA_32_BIT_UUID = 0x20,
   eADT_SERVICE_DATA_128_BIT_UUID = 0x21,
   eADT_LE_SECURE_CONNECTIONS_CONFIRMATION_VALUE = 0x22,
   eADT_LE_SECURE_CONNECTIONS_RANDOM_VALUE = 0x23,
   eADT_URI = 0x24,
   eADT_INDOOR_POSITIONING = 0x25,
   eADT_TRANSPORT_DISCOVERY_DATA = 0x26,
   eADT_LE_SUPPORTED_FEATURES = 0x27,
   eADT_CHANNEL_MAP_UPDATE_INDICATION = 0x28,
   eADT_PB_ADV = 0x29,
   eADT_MESH_MESSAGE = 0x2A,
   eADT_MESH_BEACON = 0x2B,
   eADT_BIGINFO = 0x2C,
   eADT_BROADCAST_CODE = 0x2D,
   eADT_RESOLVABLE_SET_IDENTIFIER = 0x2E,
   eADT_ADVERTISING_INTERVAL_LONG = 0x2F,
   eADT_BROADCAST_NAME = 0x30,
   eADT_ENCRYPTED_ADVERTISING_DATA = 0x31,
   eADT_PERIODIC_ADVERTISING_RESPONSE_TIMING_INFORMATION = 0x32,
   eADT_ELECTRONIC_SHELF_LABEL = 0x34,
   eADT_3D_INFORMATION_DATA = 0x3D,
   eADT_MANUFACTURER_SPECIFIC_DATA = 0xFF
} ADTypes_E;

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

#endif //!_AD_TYPES_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
