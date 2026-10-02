/**
 * @file          PairingFSM.h
 * @brief         Header file containing FSM for device certificate verified state of MainFSM.
 * @date          17/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _PAIRING_FSM_H
#define _PAIRING_FSM_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "FSM.h"
#include "FSM_Types.h"
#include "PairingFSM_Types.h"
#include "Pairing_Config.h"
#include "CSR_Generator.h"
#include "DeviceProvisioningFSM.h"
#include "ReadDeviceCertFSM.h"
#include "sl_bt_api.h"
#include "app_assert.h"
#include "app_types.h"
#include "gatt_db.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           OOB_RANDOM_DATA_LEN
 * @brief         Length of OOB random data.
 */
#define OOB_RANDOM_DATA_LEN                  (16)

/**
 * @def           OOB_CONFIRM_DATA_LEN
 * @brief         Length of OOB confirm data.
 */
#define OOB_CONFIRM_DATA_LEN                 (16)

/**
 * @def           OOB_DATA_LEN
 * @brief         Length of OOB data.
 */
#define OOB_DATA_LEN                         (OOB_RANDOM_DATA_LEN + OOB_CONFIRM_DATA_LEN)

/**
 * @def           OOB_SIGNATURE_LEN
 * @brief         Length of OOB signature data.
 */
#define OOB_SIGNATURE_LEN                    (64)

/**
 * @def           OOB_SIGNED_DATA_LEN
 * @brief         Length of OOB signed data data.
 */
#define OOB_SIGNED_DATA_LEN                  (OOB_DATA_LEN + OOB_SIGNATURE_LEN)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          PairingFSMState_E
 * @brief         Enums for different state of pairing FSM.
 */
typedef enum {
   ePS_WAITING_FOR_SYSTEM_BOOT,
   ePS_WAITING_FOR_CONNECTION_OPEN,
   ePS_WAITING_FOR_TARGET_DEVICE_BLE_ADDR,
   ePS_WAITING_FOR_CURRENT_DEVICE_BLE_ROLE,
   ePS_WAITING_FOR_PAIRING_TRIGGER,
   ePS_WAITING_FOR_TARGET_DEVICE_DISCOVERY,
   ePS_WAITING_FOR_CONNECTION_OPENED_FROM_TARGET_DEVICE,
   ePS_READ_REMOTE_DEVICE_CERT,
   ePS_WAITING_FOR_PAIRING_SERVICE_DISCOVERY,
   ePS_INQUIRE_FOR_OOB_DATA,
   ePS_WAITING_FOR_OOB_DATA,
   ePS_WAITING_FOR_PAIRING_CONFIRMATION,
   ePS_DEVICE_PAIRED
} PairingFSMState_E;

/**
 * @enum          PairingFSMEventID_E
 * @brief         Enums for different events of pairing FSM.
 */
typedef enum {
   ePE_ALL_EVENTS = 0,
} PairingFSMEventID_E;

/**
 * @enum          ECUType_E
 * @brief         Enums of different ECU types.
 */
typedef enum
{
   eET_UNDEFINED = 0,                        /**< Undefined device type. */
   eET_VCU,                                  /**< VCU. */
   eET_KEYFOB,                               /**< Keyfob. */
} ECUType_E;

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
extern FSMContext_T gst_pairingFSMContext;
extern FSMInstance_T gst_pairingFSMInstance;
extern FSMEvent_T gst_pairingFSMEvent;
extern FSMStateAttr_T gstar_pairingFSMStateAttrs[];

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/
extern sl_status_t gt_UpdatePairingReadinessOverGATT(void);
extern bool gb_WaitingForSystemBoot_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForConnectionOpen_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForTargetDeviceBLEAddr_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForCurrentDeviceBLERole_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForPairingTrigger_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForTargetDeviceDiscovery_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForConnectionOpenedFromTargetDevice_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_ReadRemoteDeviceCert_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForPairingServiceDiscovery_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_InquireForOOBData_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForOOBData_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForSetOOBDataByPeripheralDevice_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForSetOOBDataByCentralDevice_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForPairingConfirmation_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_DevicePaired_PairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
#endif //!_PAIRING_FSM_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
