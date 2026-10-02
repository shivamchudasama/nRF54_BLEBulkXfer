/**
 * @file          DeviceCert_Types.h
 * @brief         Header file containing device certificate types.
 * @date          05/09/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _DEVICE_CERT_TYPES_H
#define _DEVICE_CERT_TYPES_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include <stddef.h>

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           CHAIN_LINK_DATA_LEN
 * @brief         Length of each data chunk in chain linked data.
 */
#define CHAIN_LINK_DATA_LEN                  (192)

/**
 * @def           CHAIN_LINK_DATA_NUM
 * @brief         Number of how many chain linked data required for certificate.
 */
#define CHAIN_LINK_DATA_NUM                  (8)

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
 * @struct        CertificateChainLink_T
 * @brief         Structure contains chain link of any certificate stored on the device.
 */
typedef struct
{
   uint16_t u16_header;                      /**< Header of the certificate CL */
   uint16_t u16_nextID;                      /**< Next ID in the CL */
   uint16_t u16_dataLength;                  /**< Length of the current chunk in CL */
   uint8_t u8ar_data[CHAIN_LINK_DATA_LEN];   /**< Data of the current chunk in CL */
} CertificateChainLink_T;

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

#endif //!_DEVICE_CERT_TYPES_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
