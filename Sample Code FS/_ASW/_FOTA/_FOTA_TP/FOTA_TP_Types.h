/**
 * @file          FOTA_TP_Types.h
 * @brief         Header file containing typedefines for FOTA transport protocol.
 * @date          13/02/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _FOTA_TP_TYPES_H
#define _FOTA_TP_TYPES_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           MAX_CP_BLOCK_SIZE
 * @brief         Maximum size of a Control Packet block in bytes.
 */
#define MAX_CP_BLOCK_SIZE                    (128U)

/**
 * @def           MAX_CP_BLOCKS
 * @brief         Maximum number of Control Packet blocks in one transport packet.
 */
#define MAX_CP_BLOCKS                        (10U)

/**
 * @def           MAX_MTU_SIZE_WITH_DLE
 * @brief         Maximum size of a MTU (Maximum Transmission Unit) with
 *                DLE (Data Length Extension) in bytes.
 */
#define MAX_MTU_SIZE_WITH_DLE                (244U)

/**
 * @def           MAX_GATT_DATA_SIZE_WITH_DLE
 * @brief         Maximum size of a GATT (MTU + Headers) with DLE in bytes.
 */
#define MAX_GATT_DATA_SIZE_WITH_DLE          (251U)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          CPTypes_E
 * @brief         Enumeration of different Control Packet types.
 */
typedef enum
{
   eCPT_FOTA_START = 0x01,                   /**< FOTA start packet */
   eCPT_MANIFEST,                            /**< Manifest packet */
   eCPT_TARGET_ECU,                          /**< Target ECU identifier packet */
   eCPT_IMAGE_SIZE,                          /**< Image size packet */
   eCPT_FW_VERSION,                          /**< Firmware version packet */
   eCPT_HW_VERSION,                          /**< Hardware version packet */
   eCPT_CRC,                                 /**< Checksum packet */
   eCPT_HASH,                                /**< Hash packet */
   eCPT_SIGNATURE,                           /**< Signature packet */
   eCPT_DEPENDENCY_INFO,                     /**< Dependency information packet */
   eCPT_METADATA,                            /**< Metadata packet */
   eCPT_SECTION_START_ADDR,                  /**< Section start address packet */
   eCPT_SECTION_SIZE,                        /**< Section size packet */
   eCPT_FOTA_DATA,                           /**< FOTA data packet */
   // More CP types can be added here as needed
   eCPT_UNKNOWN                              /**< Unknown CP type */
} CPTypes_E;

/**
 * @enum          TPError_E
 * @brief         Enums listing all the possible error codes for Transport Protocol
 *                APIs.
 */
typedef enum
{
   eTP_OK,                                   /**< No error. */
   eTP_INVALID_CP_BLOCK_LENGTH,              /**< Invalid Control Packet block length. */
   eTP_INVALID_CP_TYPE,                      /**< Invalid Control Packet type. */
   eTP_NO_SUCH_CP_BLOCK_AVAILABLE,           /**< No such Control Packet block available in
                                                   Control Packet list. */
} TPError_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        CPBlock_T
 * @brief         Structure containing information of different parameters of
 *                Control Packet data block.
 */
typedef struct
{
   CPTypes_E e_CPType;                       /**< Control Packet data type */
   uint8_t u8_CPBlockLength;                 /**< Length of Control Packet block */
   uint8_t u8ar_CPData[MAX_CP_BLOCK_SIZE];   /**< Actual Control Packet data */
} CPBlock_T;

/**
 * @struct        CPList_T
 * @brief         Structure containing list of all CP blocks parsed from received
 *                GATT characteristic data.
 */
typedef struct
{
   CPBlock_T star_CPBlocks[MAX_CP_BLOCKS];   /**< All the individual CP blocks */
   uint8_t u8_totCPBlock;                    /**< Count of total CP blocks */
} CPList_T;

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

#endif //!_FOTA_TP_TYPES_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
