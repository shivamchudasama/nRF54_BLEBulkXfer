/**
 * @file          CSRAvailableFSM.h
 * @brief         Header file containing FSM for CSR available state of MainFSM.
 * @date          17/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _CSR_AVAILABLE_FSM_H
#define _CSR_AVAILABLE_FSM_H

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
#include "CSRAvailableFSM_Types.h"
#include "CSR_Generator.h"
#include "DeviceProvisioningFSM.h"
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
 * @enum          CSRAvailableFSMState_E
 * @brief         Enums for different state of CSR available FSM.
 */
typedef enum {
   eCAS_WAITING_FOR_SYSTEM_BOOT,
   eCAS_WAITING_FOR_CONNECTION_OPEN,
   eCAS_WAITING_FOR_DEVICE_PROVISIONING_TO_COMPLETE,
   eCAS_DEVICE_PROVISIONING_COMPLETED,
   eCAS_WAITING_FOR_CONNECTION_CLOSED,
} CSRAvailableFSMState_E;

/**
 * @enum          CSRAvailableFSMEventID_E
 * @brief         Enums for different events of CSR available FSM.
 */
typedef enum {
   eCAE_ALL_EVENTS = 0,
} CSRAvailableFSMEventID_E;

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
extern FSMContext_T gst_CSRAvailableFSMContext;
extern FSMInstance_T gst_CSRAvailableFSMInstance;
extern FSMEvent_T gst_CSRAvailableFSMEvent;
extern FSMStateAttr_T gstar_CSRAvailableFSMStateAttrs[];

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/
extern bool gb_WaitingForSystemBoot_CSRAvailableFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForConnectionOpen_CSRAvailableFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForDeviceProvision2Complete_CSRAvailableFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_DeviceProvisioningCompleted_CSRAvailableFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitingForConnectionClosed_CSRAvailableFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);

#endif //!_CSR_AVAILABLE_FSM_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
