/**
 * @file          CSR_Generator_Types.h
 * @brief         Header file containing types used in CSR generator.
 * @date          05/09/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _CSR_GENERATOR_TYPES_H
#define _CSR_GENERATOR_TYPES_H

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
 * @def           CSR_GENERATOR_DEVICE_PROVISIONING_RCB_NVM3_TAG
 * @brief         NVM3 tag (data control block) for production data.
 */
#define CSR_GENERATOR_DEVICE_PROVISIONING_RCB_NVM3_TAG \
                                             (0x0400)

/**
 * @def           NVM3_ID_CNT
 * @brief         Number of NVM3 IDs.
 */
#define NVM3_ID_CNT                          (4)

/**
 * @def           POS_DEVICE_CERTIFICATE
 * @brief         Device certificate position in NVM3.
 */
#define POS_DEVICE_CERTIFICATE               (0)

/**
 * @def           POS_DEVICE_EC_KEY
 * @brief         Device EC key position in NVM3.
 */
#define POS_DEVICE_EC_KEY                    (1)

/**
 * @def           POS_STATIC_AUTH_DATA
 * @brief         Static authentication data position in NVM3.
 */
#define POS_STATIC_AUTH_DATA                 (2)

/**
 * @def           POS_CSR
 * @brief         CSR position in NVM3.
 */
#define POS_CSR                              (3)

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
 * @struct        DeviceProvisioningRCB_T
 * @brief         Structure containing device provisioning record control block.
 */
typedef struct __attribute__((__packed__))
{
  uint32_t u32_bitmap;                       /**< Bitmap */
  uint8_t u8ar_positionNVM3[NVM3_ID_CNT];    /**< NVM3 positions array */
  uint16_t u16_maxLinkDataLen;               /**< Max link data length */
} DeviceProvisioningRCB_T;

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

#endif //!_CSR_GENERATOR_TYPES_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
