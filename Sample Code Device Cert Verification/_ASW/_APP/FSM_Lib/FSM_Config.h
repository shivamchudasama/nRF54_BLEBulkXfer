/**
 * @file          FSM_Config.h
 * @brief         Configuration macros for the Finite State Machine module
 * @date          31/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _FSM_CONFIG_H
#define _FSM_CONFIG_H

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           FSM_ENABLE_HSM
 * @brief         Enable (1) or disable (0) hierarchical state machine support.
 */
#define FSM_ENABLE_HSM                       (0)

#if (0 != FSM_ENABLE_HSM)
/**
 * @def           FSM_MAX_HIERARCHY_DEPTH
 * @brief         Maximum depth of the state hierarchy.
 */
#define FSM_MAX_HIERARCHY_DEPTH              (5)

/**
 * @def           FSM_ENABLE_DEEP_HISTORY
 * @brief         Enable (1) or disable (0) deep history state entry.
 */
#define FSM_ENABLE_DEEP_HISTORY              (1)
#endif // FSM_ENABLE_HSM

#endif // _FSM_CONFIG_H