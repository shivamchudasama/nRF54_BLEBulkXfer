/**
 * @file          FOTAEvent_Types.h
 * @brief         Header file containing type definitions of FOTA Event.
 * @date          15/02/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _FOTA_EVENT_TYPES_H
#define _FOTA_EVENT_TYPES_H

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
 * @def           MAX_SECTIONS_IN_IMAGE
 * @brief         This is part of image metadata. It will indicate the maximum
 *                sections possible for a single HEX file.
 */
#define MAX_SECTIONS_IN_IMAGE                (10)

/**
 * @def           MAX_DATA_PACKET_IN_ONE_MTU
 * @brief         This is part of data packet. It will indicate the maximum
 *                data available in one MTU.
 */
#define MAX_DATA_PACKET_IN_ONE_MTU           (242)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          FOTAEvent_E
 * @brief         Enumeration of different FOTA Events.
 */
typedef enum
{
   eFE_FOTA_START = 0x01,                    /**< FOTA start packet */
   eFE_MANIFEST,                             /**< Manifest packet */
   eFE_METADATA,                             /**< Metadata packet */
   eFE_FOTA_DATA,                            /**< FOTA data packet */
   // More FOTA Events can be added here as needed
} FOTAEvent_E;

/**
 * @enum          TargetECU_E
 * @brief         Enumeration of different ECUs.
 */
typedef enum
{
   eTE_UNKNOWN = 0x00,                       /**< Unknown ECU */
   eTE_VCU,                                  /**< VCU */
   eTE_MCU,                                  /**< MCU */
   eTE_TEL,                                  /**< TEL */
   eTE_BMS,                                  /**< BMS */
   // More ECUs can be added here as needed
} TargetECU_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        FOTAStart_T
 * @brief         Structure containing FOTA Start Event details.
 */
typedef struct
{
   uint32_t u32_FOTAStartSignal;             /**< FOTA Start Signal */
} FOTAStart_T;

/**
 * @struct        Version_U
 * @brief         Union containing versioning information.
 */
typedef union
{
   struct Version_T
   {
      uint8_t u8_majorVer;                   /**< Major version */
      uint8_t u8_minorVer;                   /**< Minor version */
      uint8_t u8_revisionVer;                /**< Revision version */
      uint8_t u8_reserved;                   /**< Reserved for future use */
   } st_version;                             /**< Structure containing version info */
   uint32_t u32_version;                     /**< 32-bit version info */
} Version_U;

/**
 * @struct        Manifest_T
 * @brief         Structure containing Manifest details.
 */
typedef struct
{
   TargetECU_E e_targetECU;                  /**< Target ECU */
   uint32_t u32_imageSize;                   /**< Image Size */
   Version_U u_appVersion;                   /**< Application version */
   uint32_t u32_imageSign;                   /**< Image signature */
} Manifest_T;

/**
 * @struct        Metadata_T
 * @brief         Structure containing Metadata details.
 */
typedef struct
{
   uint32_t u32_startAddr;                   /**< Start address */
   uint32_t u32_sectionSize;                 /**< Size of the section */
} Metadata_T;

/**
 * @struct        FOTAAbort_T
 * @brief         Structure containing FOTA Abort details.
 */
typedef struct
{
   // As of now, this is written as an array of 128 bytes, but it can be modified
   // as per the actual abort details to be stored.
   uint8_t u8ar_abortPkt[128];               /**< FOTA Abort packet */
} FOTAAbort_T;

/**
 * @struct        FOTAEvent_T
 * @brief         Structure containing FOTA Event details.
 */
typedef struct
{
   FOTAEvent_E e_evt;                        /**< FOTA Event type */
   uint16_t u16_payloadLength;               /**< Length of event payload */
   uint8_t *u8pt_payload;                    /**< Pointer to payload data */
   uint8_t u8ar_Payload[MAX_DATA_PACKET_IN_ONE_MTU];
                                             /**< Payload for the event */
   // union FOTAEventPayload_U
   // {
   //    FOTAStart_T st_FOTAStart;              /**< FOTA Start Event payload */
   //    Manifest_T st_manifest;                /**< Manifest payload */
   //    Metadata_T star_metadata[MAX_SECTIONS_IN_IMAGE];
   //                                           /**< Metadata payload */
   //    uint8_t u8ar_data[MAX_DATA_PACKET_IN_ONE_MTU];
   //                                           /**< FOTA Data payload */
   //    FOTAAbort_T st_FOTAAbort;              /**< FOTA Abort payload */
   //    // More FOTA Event details can be added here as needed
   // } u_FOTAEventsPayload;                    /**< FOTA events payload */
} FOTAEvent_T;

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

#endif //!_FOTA_EVENT_TYPES_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
