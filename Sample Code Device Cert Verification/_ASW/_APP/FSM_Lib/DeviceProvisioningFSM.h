/**
 * @file          DeviceProvisioningFSM.h
 * @brief         Header file containing device provisioning FSM.
 * @date          17/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _DEVICE_PROVISIONING_FSM_H
#define _DEVICE_PROVISIONING_FSM_H

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
#include "DeviceProvisioningFSM_Types.h"
#include "CSR_Generator.h"
#include "CSR_Generator_Types.h"
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
 * @enum          DeviceProvisioningFSMState_E
 * @brief         Enums for different state of device provisioning FSM.
 */
/* State enum */
typedef enum {
   eDPS_WAIT_FOR_CERT_GENERATION,
   eDPS_WAIT_FOR_CERT_LENGTH,
   eDPS_WAIT_FOR_CERT,
   eDPS_WAIT_FOR_WRITING_CERT,
   eDPS_DEVICE_CERT_WRITTEN
} DeviceProvisioningFSMState_E;

/**
 * @enum          DeviceProvisioningFSMEventID_E
 * @brief         Enums for different events of device provisioning FSM.
 */
typedef enum {
   eDPE_ALL_EVENTS = 0,
} DeviceProvisioningFSMEventID_E;

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
extern FSMContext_T gst_deviceProvisioningFSMContext;
extern FSMInstance_T gst_deviceProvisioningFSMInstance;
extern FSMEvent_T gst_deviceProvisioningFSMEvent;
extern FSMStateAttr_T gstar_deviceProvisioningFSMStateAttrs[];

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/
extern bool gb_WaitForCertGeneration_DeviceProvisioningFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitForCertLength_DeviceProvisioningFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitForCert_DeviceProvisioningFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_WaitForWritingCert_DeviceProvisioningFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
extern bool gb_DeviceCertWritten_DeviceProvisioningFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);

#endif //!_DEVICE_PROVISIONING_FSM_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
