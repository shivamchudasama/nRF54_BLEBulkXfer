/**
 * @file          ReadDeviceCertFSM.h
 * @brief         Header file containing FSM for reading device certificate state of pairing FSM.
 * @date          17/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _READ_DEVICE_CERT_FSM_H
#define _READ_DEVICE_CERT_FSM_H

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
#include "ReadDeviceCertFSM_Types.h"
#include "PairingFSM_Types.h"
#include "CSR_Generator.h"
#include "DeviceProvisioningFSM.h"
#include "DeviceProvisioningFSM_Types.h"
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
 * @def           <Define name>
 * @brief         <Define details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          ReadDeviceCertFSMState_E
 * @brief         Enums for different state of pairing FSM.
 */
typedef enum
{
   eRDCS_WAITING_FOR_DEVICE_PROVISIONING_SERVICE_DISCOVERY,
   eRDCS_INQUIRE_FOR_DEVICE_CERT_AVAILABILITY,
   eRDCS_WAITING_FOR_DEVICE_CERT_AVAILABILITY,
   eRDCS_INQUIRE_FOR_DEVICE_CERT_LENGTH,
   eRDCS_WAITING_FOR_DEVICE_CERT_LENGTH,
   eRDCS_INQUIRE_FOR_BLOCK_SIZE,
   eRDCS_WAITING_FOR_BLOCK_SIZE,
   eRDCS_INQUIRE_FOR_CERT_BLOCK_1,
   eRDCS_WAITING_FOR_CERT_BLOCK_1,
   eRDCS_INQUIRE_FOR_CERT_BLOCK_2,
   eRDCS_WAITING_FOR_CERT_BLOCK_2,
   eRDCS_INQUIRE_FOR_CERT_BLOCK_3,
   eRDCS_WAITING_FOR_CERT_BLOCK_3,
   eRDCS_INQUIRE_FOR_CERT_BLOCK_4,
   eRDCS_WAITING_FOR_CERT_BLOCK_4,
   eRDCS_DEVICE_CERT_READ_AND_VERIFIED,
} ReadDeviceCertFSMState_E;

/**
 * @enum          ReadDeviceCertFSMEventID_E
 * @brief         Enums for different events of pairing FSM.
 */
typedef enum
{
   eRDCE_ALL_EVENTS = 0,
} ReadDeviceCertFSMEventID_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        ReadDeviceCertFSMContext_T
 * @brief         Custom context structure for reading device certificate FSM.
 */
typedef struct
{
   uint8_t u8_connHandle;                    /**< Connection handle with the target
                                                   device. */
   BLERole_E e_currentDeviceBLERole;         /**< Current device BLE role in pairing
                                                   procedure. */
} ReadDeviceCertFSMContext_T;

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
extern FSMContext_T gst_readDeviceCertFSMContext;
extern FSMInstance_T gst_readDeviceCertFSMInstance;
extern FSMEvent_T gst_readDeviceCertFSMEvent;
extern FSMStateAttr_T gstar_readDeviceCertFSMStateAttrs[];
extern uint16_t gu16_remoteDeviceCertDERLen;
extern uint8_t gu8ar_remoteDeviceCertDER[];

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/
extern bool gb_WaitingForDeviceProvisioningServiceDiscovery_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_InquireForDeviceCertAvailability_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForDeviceCertAvailability_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_InquireForDeviceCertLength_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForDeviceCertLength_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_InquireForBlockSize_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForBlockSize_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_InquireForCertBlock1_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForCertBlock1_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_InquireForCertBlock2_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForCertBlock2_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_InquireForCertBlock3_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForCertBlock3_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_InquireForCertBlock4_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForCertBlock4_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_DeviceCertReadAndVerified_ReadDeviceCertFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
#endif //!_READ_DEVICE_CERT_FSM_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
