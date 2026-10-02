/**
 * @file          FSM.h
 * @brief         Public API for the Finite State Machine module (BATL compliant)
 * @date          31/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _FSM_H
#define _FSM_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "FSM_Types.h"
#include "FSM_Config.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/* Special event IDs */
/**
 * @def           FSM_EVENT_ENTRY
 * @brief         Event ID for entering a state. This is fixed ID.
 */
#define FSM_EVENT_ENTRY                      (0xFFFFU)

/**
 * @def           FSM_EVENT_EXIT
 * @brief         Event ID for exiting a state. This is fixed ID.
 */
#define FSM_EVENT_EXIT                       (0xFFFEU)

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/
extern void gv_FSM_Init(FSMInstance_T *stpt_FSMInstance, void *stpt_FSMContext, const FSMStateAttr_T *stpt_initialState);
extern void gv_FSM_SetUnhandledEventCallback(FSMInstance_T *stpt_FSMInstance, FSMStateHandler_F fpt_cb);
extern bool gb_FSM_Dispatch(FSMInstance_T *stpt_FSMInstance, const FSMEvent_T *stpt_event);
extern void gv_FSM_Transition(FSMInstance_T *stpt_FSMInstance, const FSMStateAttr_T *stpt_newState);

#endif // _FSM_H