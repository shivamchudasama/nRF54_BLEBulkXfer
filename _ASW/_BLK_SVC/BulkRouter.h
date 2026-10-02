/**
 * @file          BulkRouter.h
 * @brief         Header file containing the appType router of the BulkXfer Server.
 *
 *                The BulkXfer Server takes a single set of receive callbacks. The
 *                router owns that set and forwards every callback to the module
 *                that registered the appType range it belongs to, so several
 *                modules (hex upload, provisioning) share one Server.
 * @date          01/10/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _BULK_ROUTER_H
#define _BULK_ROUTER_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>
#include "BulkXfer.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           BULK_ROUTER_MAX_ROUTES
 * @brief         Maximum number of registered appType ranges.
 */
#define BULK_ROUTER_MAX_ROUTES               (4U)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        BulkRoute_T
 * @brief         One module's appType range and its receive callbacks. Every
 *                callback is optional: a range without fpt_onRxStart or
 *                fpt_onRxData rejects transfers, one without fpt_onRxShort ignores
 *                short messages. The callbacks run on the BulkXfer engine thread
 *                with the BulkXfer lock held (see the BulkXfer API reference).
 */
typedef struct
{
   uint8_t u8_firstAppType;                  /**< First appType of the range.            */
   uint8_t u8_lastAppType;                   /**< Last appType (inclusive).              */
   BlkRxStart_F fpt_onRxStart;               /**< Accept or reject a transfer.           */
   BlkRxData_F fpt_onRxData;                 /**< In-order chunk.                        */
   BlkRxDone_F fpt_onRxDone;                 /**< Transfer result.                       */
   BlkRxShort_F fpt_onRxShort;               /**< Short message.                         */
} BulkRoute_T;

/******************************************************************************/
/*                                                                            */
/*                                   UNIONS                                   */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/
extern int gi_BulkRouter_Register(const BulkRoute_T *stpt_route);
extern int gi_BulkRouter_Start(void);

#endif //!_BULK_ROUTER_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
