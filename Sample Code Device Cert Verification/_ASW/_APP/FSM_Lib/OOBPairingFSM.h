/**
 * @file          OOBPairingFSM.h
 * @brief         Header file containing FSM for out-of-band pairing state of pairing FSM.
 * @date          29/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _OOB_PAIRING_FSM_H
#define _OOB_PAIRING_FSM_H

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
#include "OOBPairingFSM_Types.h"
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
 * @enum          OOBPairingFSMState_E
 * @brief         Enums for different state of out-of-band pairing FSM.
 */
typedef enum
{
   eOPS_,
} OOBPairingFSMState_E;

/**
 * @enum          OOBPairingFSMEventID_E
 * @brief         Enums for different events of out-of-band pairing FSM.
 */
typedef enum
{
   eRDCE_ALL_EVENTS = 0,
} OOBPairingFSMEventID_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        OOBPairingFSMContext_T
 * @brief         Custom context structure for out-of-band pairing FSM.
 */
typedef struct
{

} OOBPairingFSMContext_T;

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
extern FSMContext_T gst_OOBPairingFSMContext;
extern FSMInstance_T gst_OOBPairingFSMInstance;
extern FSMEvent_T gst_OOBPairingFSMEvent;
extern FSMStateAttr_T gstar_OOBPairingFSMStateAttrs[];

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/
extern bool gb_WaitingForDeviceProvisioningServiceDiscovery_OOBPairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event);
#endif //!_OOB_PAIRING_FSM_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
