/**
 * @file          FSM.c
 * @brief         Finite State Machine implementation (BATL compliant)
 * @date          31/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "FSM.h"

/******************************************************************************/
/*                                                                            */
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/*                                                                            */
/******************************************************************************/
static void sv_FSM_EnterState(FSMInstance_T *stpt_FSMInstance, \
   const FSMStateAttr_T *stpt_entryState);
static void sv_FSM_ExitState(FSMInstance_T *stpt_FSMInstance, \
   const FSMStateAttr_T *stpt_exitState);
#if FSM_ENABLE_HSM
static const FSMStateAttr_T* sstpt_FSM_FindLCA(const FSMStateAttr_T *stpt_s1, const FSMStateAttr_T *stpt_s2);
#endif // FSM_ENABLE_HSM
static bool sb_FSM_IsEventAllowed(const FSMStateAttr_T *stpt_currentState, \
   const FSMEvent_T *stpt_dispatchedEvent);

/******************************************************************************/
/*                                                                            */
/*                        PRIVATE FUNCTION DEFINITIONS                        */
/*                                                                            */
/******************************************************************************/
/**
 * @private       gv_FSsv_FSM_EnterStateM_Init
 * @brief         Enter a state and invoke its entry action. If deep-history is enabled,
 *                recursively enter the recorded last-active child state.
 * @param[inout]  stpt_FSMInstance Pointer to the FSM instance.
 * @param[in]     stpt_entryState Pointer to attribute of target FSM state.
 */
static void sv_FSM_EnterState(FSMInstance_T *stpt_FSMInstance, \
   const FSMStateAttr_T *stpt_entryState)
{
   // Create an entry event with dedicated special ID
   FSMEvent_T st_entryEvent = {
      .u32_eventID = FSM_EVENT_ENTRY,
      .vpt_eventParam = NULL
   };

   // Check if the entry state is valid
   if (stpt_entryState == NULL)
   {
      return;
   }

   // Execute entry event handler
   stpt_entryState->fpt_handler(stpt_FSMInstance->stpt_FSMContext, &st_entryEvent);

#if FSM_ENABLE_DEEP_HISTORY
   // If this state has a recorded last_active_child (history), enter it recursively
   if (stpt_entryState->stpt_lastActiveChild != NULL)
   {
      sv_FSM_EnterState(stpt_FSMInstance, stpt_entryState->stpt_lastActiveChild);
   }
#endif // FSM_ENABLE_DEEP_HISTORY
}

/**
 * @private       sv_FSM_ExitState
 * @brief         Exit a state and invoke its exit action.
 * @param[inout]  stpt_FSMInstance Pointer to the FSM instance (FSMInstance_T).
 * @param[in]     stpt_exitState Pointer to state to exit (FSMStateAttr_T).
 */
static void sv_FSM_ExitState(FSMInstance_T *stpt_FSMInstance, const FSMStateAttr_T *stpt_exitState)
{
   // Create an exit event with dedicated special ID
   FSMEvent_T st_exitEvent = {
      .u32_eventID = FSM_EVENT_EXIT,
      .vpt_eventParam = NULL
   };

   // Check if state is valid
   if (stpt_exitState == NULL)
   {
      return;
   }

   // Execute exit event handler
   stpt_exitState->fpt_handler(stpt_FSMInstance->stpt_FSMContext, &st_exitEvent);
}

#if FSM_ENABLE_HSM
/**
 * @private       sstpt_FSM_FindLCA
 * @brief         Find the least common ancestor (LCA) of two states in the hierarchy.
 * @param[in]     stpt_s1 Pointer to first state.
 * @param[in]     stpt_s2 Pointer to second state.
 * @return        Pointer to LCA state; NULL if none found.
 */
static const FSMStateAttr_T* sstpt_FSM_FindLCA(const FSMStateAttr_T *stpt_s1, const FSMStateAttr_T *stpt_s2)
{
   const FSMStateAttr_T *stpt_a = stpt_s1;
   while (stpt_a != NULL)
   {
      const FSMStateAttr_T *stpt_b = stpt_s2;
      while (stpt_b != NULL)
      {
         if (stpt_a == stpt_b) { return stpt_a; }
         stpt_b = stpt_b->stpt_parent;
      }
      stpt_a = stpt_a->stpt_parent;
   }
   return NULL;
}
#endif // FSM_ENABLE_HSM

/**
 * @private       sb_FSM_IsEventAllowed
 * @brief         Check if the dispatched event is allowed for the current state.
 * @param[in]     stpt_currentState Pointer to the current state attribute.
 * @param[in]     stpt_dispatchedEvent Pointer to state to dispatched event.
 */
static bool sb_FSM_IsEventAllowed(const FSMStateAttr_T *stpt_currentState, \
   const FSMEvent_T *stpt_dispatchedEvent)
{
   bool b_retVal = false;
   uint32_t u32_eventID;

   // Check if dispatched state and event are valid
   if ((NULL != stpt_currentState) && (NULL != stpt_dispatchedEvent))
   {
      // If no restriction provided, allow all
      if ((stpt_currentState->u32pt_allowedEventIDs == NULL) || \
         (stpt_currentState->u8_allowedEventCount == 0))
      {
         b_retVal = true;
      }
      else
      {
         u32_eventID = stpt_dispatchedEvent->u32_eventID;

         // Loop through all the allowed event IDs
         for (uint8_t u8_lpIdx = 0; u8_lpIdx < stpt_currentState->u8_allowedEventCount; \
            u8_lpIdx++)
         {
            // Check if the dispatched event is matching with any allowed event
            if (stpt_currentState->u32pt_allowedEventIDs[u8_lpIdx] == u32_eventID)
            {
               b_retVal = true;
               break;
            }
         }
      }
   }

   return b_retVal;
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gv_FSM_Init
 * @brief         Initialize the FSM instance and enter the initial state.
 *                This function initializes internal FSM pointers (context and
 *                current state - as per the initial state) and invokes entry actions
 *                for the initial_state. If deep-history is enabled,
 *                the entry will traverse into recorded last-active child states.
 * @param[inout]  stpt_FSMInstance Pointer to FSM instance (FSMInstance_T).
 * @param[in]     stpt_FSMContext FSM context pointer passed to state handlers.
 * @param[in]     stpt_initialState Pointer to the attribute of initial state.
 * @return        void
 */
void gv_FSM_Init(FSMInstance_T *stpt_FSMInstance, void *stpt_FSMContext,
   const FSMStateAttr_T *stpt_initialState)
{
   stpt_FSMInstance->stpt_FSMContext = stpt_FSMContext;
   stpt_FSMInstance->stpt_currentState = stpt_initialState;
   sv_FSM_EnterState(stpt_FSMInstance, stpt_initialState);
}

/**
 * @public        gv_FSM_SetUnhandledEventCallback
 * @brief         Set the callback function for unhandled events.
 * @param[in]     stpt_FSMInstance Pointer to FSM instance (FSMInstance_T).
 * @param[in]     fpt_callBack Pointer to the callback function.
 * @return        void
 */
void gv_FSM_SetUnhandledEventCallback(FSMInstance_T *stpt_FSMInstance, \
   FSMStateHandler_F fpt_callBack)
{
   stpt_FSMInstance->fpt_unhandledEventCb = fpt_callBack;
}

/**
 * @public        gb_FSM_Dispatch
 * @brief         Dispatch an event to the current state. If the state does not
 *                handle the event, it will bubble up to parent states.
 *                The handler prototype returns `bool` to indicate whether it
 *                handled the event. This function iteratively invokes handlers
 *                up the state hierarchy until the event is handled or the root
 *                is reached. If no state handles the given event, default unhandled
 *                event handler shall handle it.
 * @param[inout]  stpt_FSMInstance Pointer to FSM instance (FSMInstance_T).
 * @param[in]     stpt_event Pointer to event to dispatch (FSMEvent_T).
 * @return        True if the event has handled in current (or any of the parent state -
 *                in case of HSM) state or default unhandled event handler.
 */
bool gb_FSM_Dispatch(FSMInstance_T *stpt_FSMInstance, const FSMEvent_T *stpt_event)
{
   const FSMStateAttr_T *stpt_currentState = stpt_FSMInstance->stpt_currentState;
   bool b_isEventHandled = false;
   uint8_t u8_depth = 0;

   // Check if the dispatched event is allowed to be handled in current state
   if (sb_FSM_IsEventAllowed(stpt_currentState, stpt_event))
   {
#if FSM_ENABLE_HSM
      // Bubble event up the state hierarchy until handled or root
      while ((NULL != stpt_currentState) && (false == b_isEventHandled) && \
         (u8_depth < FSM_MAX_HIERARCHY_DEPTH))
      {
         b_isEventHandled = \
            stpt_currentState->fpt_handler(stpt_FSMInstance->stpt_FSMContext, stpt_event);

         // Check if the event wasn't handled
         if (false == b_isEventHandled)
         {
            // Event not handled, bubble up to parent state
            // Check if there is still a parent state left
            if (NULL != stpt_currentState->stpt_parent)
            {
               // Bubble up to parent state
               stpt_currentState = stpt_currentState->stpt_parent;

               // Increment depth counter
               u8_depth++;
            }
         }
      }
#else
      // Check if current state exists
      if (NULL != stpt_currentState)
      {
         // Non-hierarchical: only current state handles the event
         b_isEventHandled = \
            stpt_currentState->fpt_handler(stpt_FSMInstance->stpt_FSMContext, stpt_event);
      }
#endif // FSM_ENABLE_HSM
   }

   // Check if the event is unhandled & user callback if registered
   if ((!b_isEventHandled) && (NULL != stpt_FSMInstance->fpt_unhandledEventCb))
   {
      // Invoke unhandled event handler
      b_isEventHandled = \
         stpt_FSMInstance->fpt_unhandledEventCb(stpt_FSMInstance->stpt_FSMContext, stpt_event);
   }

   return b_isEventHandled;
}

/**
 * @public        gv_FSM_Transition
 * @brief         Transition the FSM from the current state to a new state.
 *                Behavior:
 *                 1. If new state equals current state, no action is taken.
 *                 2. Finds Least Common Ancestor (LCA) between old and new states.
 *                 3. Exits states from the current state up to (but not including) LCA.
 *                 4. Saves last-active child pointers if deep-history is enabled.
 *                 5. Enters states from LCA down to new state, invoking entry actions and
 *                    recursively entering history substates if configured.
 * @param[inout]  stpt_FSMInstance Pointer to FSM instance (FSMInstance_T).
 * @param[in]     stpt_newState Pointer to target new state (FSMStateAttr_T).
 * @return        void
 */
void gv_FSM_Transition(FSMInstance_T *stpt_FSMInstance, const FSMStateAttr_T *stpt_newState)
{
   const FSMStateAttr_T *stpt_oldState = stpt_FSMInstance->stpt_currentState;

   // Check if application is trying to transition to the same state
   if (stpt_FSMInstance->stpt_currentState == stpt_newState)
   {
      // Same state transition is not allowed
      return;
   }

#if FSM_ENABLE_HSM
   // Find least common ancestor between old and new states
   const FSMStateAttr_T *stpt_LCA = sstpt_FSM_FindLCA(stpt_oldState, stpt_newState);

   // Exit states up to LCA, possibly saving last active substates for deep history
   const FSMStateAttr_T *stpt_state = stpt_oldState;
   while ((stpt_state != NULL) && (stpt_state != stpt_LCA))
   {
#if FSM_ENABLE_DEEP_HISTORY
      // Check if the parent exists
      if (stpt_state->stpt_parent != NULL)
      {
         // Save last active child for deep history
         ((FSMStateAttr_T*)stpt_state->stpt_parent)->stpt_lastActiveChild = stpt_state;
      }
#endif // FSM_ENABLE_DEEP_HISTORY
      // Exit from the current state (old state)
      sv_FSM_ExitState(stpt_FSMInstance, stpt_state);
      stpt_state = stpt_state->stpt_parent;
   }

   stpt_FSMInstance->stpt_currentState = stpt_newState;

   // Enter states from LCA down to new state including history if enabled
   // Max depth, adjust if needed
   const FSMStateAttr_T *stptar_stack[16];
   int32_t s32_depth = 0;
   for (const FSMStateAttr_T *stpt_s = stpt_newState;
      (stpt_s != NULL) && (stpt_s != stpt_LCA);
      stpt_s = stpt_s->stpt_parent)
   {
      stptar_stack[s32_depth++] = stpt_s;
   }

   while (--s32_depth >= 0)
   {
      // Enter the new state
      sv_FSM_EnterState(stpt_FSMInstance, stptar_stack[s32_depth]);
   }
#else
   // Exit from the current state (old state)
   sv_FSM_ExitState(stpt_FSMInstance, stpt_oldState);

   // Enter the new state
   sv_FSM_EnterState(stpt_FSMInstance, stpt_newState);

   // Assign the new state as a current state
   stpt_FSMInstance->stpt_currentState = stpt_newState;
#endif // FSM_ENABLE_HSM
}

/******************************************************************************/
/*                                                                            */
/*                                COPYRIGHT                                   */
/*                                                                            */
/******************************************************************************/
/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author: Shivam Chudasama [SC]
 */