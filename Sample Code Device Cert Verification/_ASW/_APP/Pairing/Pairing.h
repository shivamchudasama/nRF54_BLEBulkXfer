/**
 * @file          Pairing.h
 * @brief         Header file containing BLE device pairing related APIs.
 * @date          10/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _PAIRING_H
#define _PAIRING_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include "sl_status.h"
#include "sl_bt_root_cert.h"
#include "sl_bt_api.h"
#include "gatt_db.h"
#include "app_assert.h"
#include "app_log.h"
#include "sl_bt_api.h"
#include "DeviceCert.h"
#include "app_types.h"

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
 * @enum          PairingState_E
 * @brief         Enums of different states for pairing.
 */
typedef enum
{
   ePS_WAIT_FOR_TARGET_DEVICE_ADDR = 0,      /**< Waiting for EOL tool to send
                                                   the target device's BLE address. */
   ePS_WAIT_FOR_BLE_ROLE,                    /**< Waiting for host device's BLE
                                                   role in an ongoing priring procedure. */
   ePS_WAIT_FOR_PAIRING_TRIGGER,             /**< Waiting for EOL tool to trigger
                                                   for pairing with target device. */
   ePS_START_SCANNING,                       /**< Start scanning (if the current device's
                                                   role is central). */
   ePS_START_DIRECTED_ADVERTISING,           /**< Start directed advertising
                                                   (if the current
                                                   device's role is peripheral). */
   ePS_WAIT_FOR_TARGET_DEVICE_DISCOVERY,     /**< Wait for target device discovery.
                                                   (if the current device's
                                                   role is central). */
   ePS_WAIT_FOR_CONNECTION_OPENED_FROM_TARGET_DEVICE,
                                             /**< Wait for connection being opened
                                                   from the target device. (if the current
                                                   device's role is peripheral). */
   // ePS_READ_REMOTE_DEVICE_CERT,              /**< Read the remote device's device
   //                                                 certificate over GATT. (applies
   //                                                 to both: central and peripheral). */

} PairingState_E;

/**
 * @enum          ReadTargetDeviceCertState_E
 * @brief         Enums of different states reading target device's device certificate.
 */
typedef enum
{
   eRTDC_DISCOVER_BATL_DEVICE_PROVISIONING_SERVICE,
   eRTDC_WAIT_FOR_DEVICE_PROVISIONING_SERVICE_DISCOVERY,
   eRTDC_INQUIRE_FOR_DEVICE_CERT_AVAILABILITY,
   eRTDC_WAIT_FOR_DEVICE_CERT_AVAILABILITY,
   eRTDC_INQUIRE_FOR_DEVICE_CERT_LENGTH,
   eRTDC_GET_DEVICE_CERT_LENGTH,
   eRTDC_GET_DEVICE_CERT,
} ReadTargetDeviceCertState_E;

// /**
//  * @enum          BLERole_E
//  * @brief         Enums of different roles for pairing.
//  */
// typedef enum
// {
//    eBR_UNASSIGNED = 0,                       /**< Unassigned BLE role. */
//    eBR_CENTRAL,                              /**< Central. */
//    eBR_PERIPHERAL,                           /**< Peripheral. */
// } BLERole_E;

// /**
//  * @enum          ECUType_E
//  * @brief         Enums of different ECU types.
//  */
// typedef enum
// {
//    eET_UNDEFINED = 0,                        /**< Undefined device type. */
//    eET_VCU,                                  /**< VCU. */
//    eET_KEYFOB,                               /**< Keyfob. */
// } ECUType_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        ReadDeviceCert_T
 * @brief         Structure used to pass the arguments for reading device certificate
 *                FSM.
 */
typedef struct
{
   byte_array t_value;                       /**< Value read from remote GATT server. */
   uint32_t u32_serviceHandle;               /**< Discovered service handle. */
   uint16_t u16_offset;                      /**< Offset of the value read from
                                                   remote GATT server. */
   uint16_t u16_charHandle;                  /**< Discovered characteristic handle. */
   uint8_t u8_attrOpCode;                    /**< Attribute opcode of read characteristic. */
   uint8_t u8_connHandle;                    /**< Discovered connection handle. */
} ReadDeviceCert_T;

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
extern sl_status_t gt_PairingFSM(sl_bt_msg_t *stpt_evt);

#endif //!_PAIRING_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
