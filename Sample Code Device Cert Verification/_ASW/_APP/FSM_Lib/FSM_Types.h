/**
 * @file          FSM_Types.h
 * @brief         Type definitions for the Finite State Machine module (BATL compliant)
 * @date          31/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _FSM_TYPES_H
#define _FSM_TYPES_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include "FSM_Config.h"

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        FSMEvent_T
 * @brief         Represents an event dispatched to the FSM.
 */
typedef struct
{
   uint32_t u32_eventID;                     /**< Event identifier. */
   void *vpt_eventParam;                     /**< Optional event parameter.
                                                   If any event comes with any
                                                   additional parameters, it can
                                                   be passed here. */
} FSMEvent_T;

/**
 * @struct        FSMContext_T
 * @brief         Structure for standard FSM context.
 */
typedef struct
{
   void *vpt_customData;                     /**< Application can have its own
                                                   context data for each FSM. It
                                                   needs to point to this. Application
                                                   needs to typecaste this before
                                                   using it as its a void pointer. */
} FSMContext_T;

/**
 * @typedef       FSMStateHandler_F
 * @brief         Prototype for a state handler. Returns true if event handled.
 * @param[in]     stpt_FSMContext User context pointer passed at FSM initialization.
 * @param[in]     stpt_event Event pointer.
 * @return        true if handled, false otherwise.
 */
typedef bool (*FSMStateHandler_F)(FSMContext_T *stpt_FSMContext, const FSMEvent_T *stpt_event);

/**
 * @struct        FSMStateAttr_T
 * @brief         Structure containing different attributes of state like,
 *                state handler, (Only in case of hierarchical state machine
 *                specification - parent state, last active child state, etc.).
 *                If deep history is enabled, last active child tracks the
 *                previous substate.
 */
typedef struct FSMStateAttr_T
{
   FSMStateHandler_F fpt_handler;            /**< State handler. */
   uint32_t u32_flags;                       /**< Flags for any state. */
#if FSM_ENABLE_HSM
   const struct FSMStateAttr_T *stpt_parent; /**< Parent state in hierarchy. */
#if FSM_ENABLE_DEEP_HISTORY
   const struct FSMStateAttr_T *stpt_lastActiveChild;
                                             /**< Last active child for deep history. */
#endif // FSM_ENABLE_DEEP_HISTORY
#endif // FSM_ENABLE_HSM
   uint8_t u8_stateID;                       /**< State ID. */
   const uint32_t *u32pt_allowedEventIDs;    /**< pointer to list of event IDs. */
   uint8_t u8_allowedEventCount;             /**< number of event IDs in the list. */
} FSMStateAttr_T;

/**
 * @struct        FSMInstance_T
 * @brief         FSM instance, tracks user context and current state pointer.
 */
typedef struct
{
   FSMContext_T *stpt_FSMContext;            /**< User data passed to state handlers. */
   const FSMStateAttr_T *stpt_currentState;  /**< Current state attributes. */
   FSMStateHandler_F fpt_unhandledEventCb;   /**< Callback on unhandled events. */
} FSMInstance_T;

#endif // _FSM_TYPES_H