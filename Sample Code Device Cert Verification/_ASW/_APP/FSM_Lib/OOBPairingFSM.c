/**
 * @file          OOBPairingFSM.c
 * @brief         Source file containing FSM for out-of-band pairing state of pairing FSM.
 * @date          29/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "OOBPairingFSM.h"

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
// Definition of all the enums
/**
 * @enum          <Enum name>
 * @brief         <Enum details>.
 */

// Declarations of all the enum variables
/**
 * @var           <Variable name>
 * @brief         <Variable details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
// Definition of all the structures
/**
 * @struct        <Structure name>
 * @brief         <Structure details>.
 */

// Declarations of all the structure variables
/**
 * @var           <Variable name>
 * @brief         <Variable details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                   UNIONS                                   */
/*                                                                            */
/******************************************************************************/
// Definition of all the unions
/**
 * @union         <Union name>
 * @brief         <Union details>.
 */

// Declarations of all the union variables
/**
 * @var           <Variable name>
 * @brief         <Variable details>.
 */

/******************************************************************************/
/*                                                                            */
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                              PUBLIC VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           gst_OOBPairingFSMContext
 * @brief         Context for out-of-band pairing FSM.
 */
FSMContext_T gst_OOBPairingFSMContext;

/**
 * @var           gst_OOBPairingFSMInstance
 * @brief         Instance for out-of-band pairing FSM.
 */
FSMInstance_T gst_OOBPairingFSMInstance = {
   .stpt_FSMContext = &gst_OOBPairingFSMContext,
   .stpt_currentState = &gstar_OOBPairingFSMStateAttrs[0],
   .fpt_unhandledEventCb = NULL,
};

/**
 * @var           gst_OOBPairingFSMEvent
 * @brief         Event for out-of-band pairing state.
 */
FSMEvent_T gst_OOBPairingFSMEvent = { 0 };

/**
 * @var           gstar_OOBPairingFSMStateAttrs
 * @brief         State attributes for out-of-band pairing FSM.
 */
FSMStateAttr_T gstar_OOBPairingFSMStateAttrs[] = {
   // [eRDCS_WAITING_FOR_DEVICE_PROVISIONING_SERVICE_DISCOVERY] = {
   //    .fpt_handler = gb_WaitingForDeviceProvisioningServiceDiscovery_OOBPairingFSM_Handler,
   //    .u8_stateID = eRDCS_WAITING_FOR_DEVICE_PROVISIONING_SERVICE_DISCOVERY,
   // },
};

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                        PRIVATE FUNCTION DEFINITIONS                        */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gb_WaitingForDeviceProvisioningServiceDiscovery_OOBPairingFSM_Handler
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
bool gb_WaitingForDeviceProvisioningServiceDiscovery_OOBPairingFSM_Handler(FSMContext_T *stpt_FSMContext, \
   const FSMEvent_T *stpt_event)
{
   bool b_retVal = false;
   sl_bt_msg_t *stpt_evt = (sl_bt_msg_t *)(stpt_event->vpt_eventParam);

   // Check if the current event is entry event
   if (FSM_EVENT_ENTRY == stpt_event->u32_eventID)
   {
      // Entry event handling
   }
   // Check if the current event is exit event
   else if (FSM_EVENT_EXIT == stpt_event->u32_eventID)
   {
      // Exit event handling
   }
   else
   {
      // Switch through all the allowed events
      switch (SL_BT_MSG_ID(stpt_evt->header))
      {

      }
   }

   return b_retVal;
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
