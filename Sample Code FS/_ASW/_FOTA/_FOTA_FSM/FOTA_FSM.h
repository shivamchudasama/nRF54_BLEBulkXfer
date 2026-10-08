/**
 * @file          FOTA_FSM.h
 * @brief         Header file containing FOTA FSM related definitions and declarations
 * @date          13/02/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _FOTA_FSM_H
#define _FOTA_FSM_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/smf.h>
#include "AppLog.h"
#include "FOTA_GATT.h"
#include "FOTAEvent.h"
#include "FOTA_TP.h"
#include "Helpers.h"
#include "TransferMsgTypes.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           FOTA_START_SIGNATURE
 * @brief         Signature indicating the FOTA start.
 */
#define FOTA_START_SIGNATURE                 (0xAABBCCDDU)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          FOTAState_E
 * @brief         FOTA state machine states.
 */
typedef enum
{
   eFS_IDLE,                                 /**< Idle state */
   eFS_RECEIVING_MANIFEST,                   /**< Receiving firmware manifest */
   eFS_RECEIVING_METADATA,                   /**< Receiving firmware metadata */
   eFS_RECEIVING_DATA,                       /**< Receiving firmware data */
   eFS_VALIDATE_IMAGE,                       /**< Validating received firmware image */
   eFS_STAGE_IMAGE,                          /**< Staging firmware image for update */
   eFS_COMPLETED,                            /**< FOTA completed */
   eFS_ABORT,                                /**< FOTA aborted */
   eFS_STATE_MAX                             /**< Maximum state value */
} FOTAState_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        FOTAStateMachineCtx_T
 * @brief         FOTA state machine context structure.
 */
typedef struct
{
   struct smf_ctx st_SMFCtx;                 /**< State machine context */
   bool b_isEventPending;                    /**< Flag to indicate if the event
                                                   is pending to handle. */
   FOTAEvent_T st_FOTAEvent;                 /**< FOTA event structure */
} FOTAStateMachineCtx_T;

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
extern const struct smf_state gst_FOTAStates[eFS_STATE_MAX];

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/

#endif //!_FOTA_FSM_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
