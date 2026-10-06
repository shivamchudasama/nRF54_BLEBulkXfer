/**
 * @file          BulkRouter.c
 * @brief         Source file containing the appType router of the BulkXfer Server.
 *
 *                Registration happens at start-up, before gi_BulkRouter_Start(),
 *                so the route table is read-only once callbacks can arrive and
 *                needs no lock. Dispatch is stateless: every BulkXfer callback
 *                carries the appType, and a rejected START never reaches
 *                fpt_onRxDone, so each call goes to the range it belongs to.
 * @date          01/10/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "BulkRouter.h"
#include <zephyr/sys/atomic.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include "BulkSvc.h"
#include "AppLog.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           ROUTER_FILTER_ON
 * @brief         Set in st_filter while a filter applies; bits 15..8 hold the
 *                first allowed appType, bits 7..0 the last.
 */
#define ROUTER_FILTER_ON                     (0x10000)

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

/******************************************************************************/
/*                                                                            */
/*                                   UNIONS                                   */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/*                                                                            */
/******************************************************************************/
static const BulkRoute_T *sstpt_FindRoute(uint8_t u8_appType);
static bool sb_IsAllowed(uint8_t u8_appType);
static int si_RouteRxStart(uint8_t u8_appType, uint32_t u32_totalLen);
static int si_RouteRxData(uint8_t u8_appType, uint32_t u32_offset,
   const uint8_t *u8pt_data, uint16_t u16_len);
static void sv_RouteRxDone(uint8_t u8_appType, BlkStatus_E e_status, uint32_t u32_totalLen);
static void sv_RouteRxShort(uint8_t u8_appType, const uint8_t *u8pt_data, uint8_t u8_len);

/******************************************************************************/
/*                                                                            */
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           sstar_routes
 * @brief         Registered appType ranges (copies of the callers' structures).
 */
static BulkRoute_T sstar_routes[BULK_ROUTER_MAX_ROUTES];

/**
 * @var           su8_routeCnt
 * @brief         Number of entries used in sstar_routes.
 */
static uint8_t su8_routeCnt = 0U;

/**
 * @var           sb_isStarted
 * @brief         Set once the BulkXfer Server runs with the router callbacks.
 */
static bool sb_isStarted = false;

/**
 * @var           st_filter
 * @brief         appType filter for new transfers and short messages
 *                (ROUTER_FILTER_ON | first << 8 | last), 0 when none applies.
 *                Written by an application thread, read on the engine thread.
 */
static atomic_t st_filter = ATOMIC_INIT(0);

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
/**
 * @private       sstpt_FindRoute
 * @brief         Find the registered range an appType belongs to.
 * @param[in]     u8_appType Application type.
 * @return        The route, or NULL if no range holds the appType.
 */
static const BulkRoute_T *sstpt_FindRoute(uint8_t u8_appType)
{
   uint8_t u8_idx = 0U;

   for (u8_idx = 0U; u8_idx < su8_routeCnt; u8_idx++)
   {
      // Check if the appType lies in this range
      if ((u8_appType >= sstar_routes[u8_idx].u8_firstAppType) &&
         (u8_appType <= sstar_routes[u8_idx].u8_lastAppType))
      {
         return &sstar_routes[u8_idx];
      }
   }

   return NULL;
}

/**
 * @private       sb_IsAllowed
 * @brief         Whether the current filter lets an appType through.
 * @param[in]     u8_appType Application type.
 * @return        true if no filter applies or the type is inside it.
 */
static bool sb_IsAllowed(uint8_t u8_appType)
{
   atomic_val_t t_filter = atomic_get(&st_filter);

   // Check if a filter applies
   if ((t_filter & ROUTER_FILTER_ON) == 0)
   {
      return true;
   }

   return (u8_appType >= (uint8_t)((t_filter >> 8) & 0xFF)) &&
      (u8_appType <= (uint8_t)(t_filter & 0xFF));
}

/**
 * @private       si_RouteRxStart
 * @brief         BlkRxStart_F: forward to the owning range, reject unrouted types.
 * @param[in]     u8_appType Application type announced by the client.
 * @param[in]     u32_totalLen Object size.
 * @return        The owner's answer, or -ENOTSUP when the filter excludes the
 *                type or no range accepts transfers of this type.
 */
static int si_RouteRxStart(uint8_t u8_appType, uint32_t u32_totalLen)
{
   const BulkRoute_T *stpt_route = sstpt_FindRoute(u8_appType);

   // Check if the filter lets this type through on the current link
   if (!sb_IsAllowed(u8_appType))
   {
      APP_LOG_WRN("rejected: type 0x%02x filtered", u8_appType);
      return -ENOTSUP;
   }

   // Check if a module receives transfers of this type
   if ((stpt_route == NULL) || (stpt_route->fpt_onRxData == NULL))
   {
      APP_LOG_WRN("rejected: unknown type 0x%02x", u8_appType);
      return -ENOTSUP;
   }

   // Check if the owner wants to decide (no fpt_onRxStart accepts everything)
   if (stpt_route->fpt_onRxStart != NULL)
   {
      return stpt_route->fpt_onRxStart(u8_appType, u32_totalLen);
   }

   return 0;
}

/**
 * @private       si_RouteRxData
 * @brief         BlkRxData_F: forward to the owning range.
 * @param[in]     u8_appType Application type.
 * @param[in]     u32_offset Object offset of the chunk.
 * @param[in]     u8pt_data Chunk data, valid only during the call.
 * @param[in]     u16_len Chunk length.
 * @return        The owner's answer, or -EIO if the type has no receiver (not
 *                reachable: such a START is rejected).
 */
static int si_RouteRxData(uint8_t u8_appType, uint32_t u32_offset,
   const uint8_t *u8pt_data, uint16_t u16_len)
{
   const BulkRoute_T *stpt_route = sstpt_FindRoute(u8_appType);

   // Check if the transfer still has a receiver
   if ((stpt_route == NULL) || (stpt_route->fpt_onRxData == NULL))
   {
      return -EIO;
   }

   return stpt_route->fpt_onRxData(u8_appType, u32_offset, u8pt_data, u16_len);
}

/**
 * @private       sv_RouteRxDone
 * @brief         BlkRxDone_F: forward to the owning range.
 * @param[in]     u8_appType Application type.
 * @param[in]     e_status Result of the transfer.
 * @param[in]     u32_totalLen Size announced in START.
 * @return        None.
 */
static void sv_RouteRxDone(uint8_t u8_appType, BlkStatus_E e_status, uint32_t u32_totalLen)
{
   const BulkRoute_T *stpt_route = sstpt_FindRoute(u8_appType);

   // Check if the owner wants the result
   if ((stpt_route != NULL) && (stpt_route->fpt_onRxDone != NULL))
   {
      stpt_route->fpt_onRxDone(u8_appType, e_status, u32_totalLen);
   }
}

/**
 * @private       sv_RouteRxShort
 * @brief         BlkRxShort_F: forward to the owning range, log and drop others
 *                (filtered types included).
 * @param[in]     u8_appType Application type.
 * @param[in]     u8pt_data Payload, valid only during the call.
 * @param[in]     u8_len Payload length.
 * @return        None.
 */
static void sv_RouteRxShort(uint8_t u8_appType, const uint8_t *u8pt_data, uint8_t u8_len)
{
   const BulkRoute_T *stpt_route = sstpt_FindRoute(u8_appType);

   // Check if the filter lets this type through on the current link
   if (!sb_IsAllowed(u8_appType))
   {
      APP_LOG_WRN("short message type 0x%02x filtered", u8_appType);
   }
   // Check if a module handles short messages of this type
   else if ((stpt_route != NULL) && (stpt_route->fpt_onRxShort != NULL))
   {
      stpt_route->fpt_onRxShort(u8_appType, u8pt_data, u8_len);
   }
   else
   {
      APP_LOG_INF("short message type 0x%02x, %u bytes ignored", u8_appType, u8_len);
   }
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gi_BulkRouter_Register
 * @brief         Register an appType range and its callbacks (copied). Call at
 *                start-up, before gi_BulkRouter_Start().
 * @param[in]     stpt_route Range and callbacks.
 * @return        0 on success; -EINVAL for NULL, an inverted range or a range
 *                reaching above BLK_APP_TYPE_MAX; -EEXIST if it overlaps a
 *                registered range; -ENOMEM if BULK_ROUTER_MAX_ROUTES are used;
 *                -EALREADY after gi_BulkRouter_Start().
 */
int gi_BulkRouter_Register(const BulkRoute_T *stpt_route)
{
   uint8_t u8_idx = 0U;

   // Check if the route table is still open
   if (sb_isStarted)
   {
      return -EALREADY;
   }

   // Check if the range is valid
   if ((stpt_route == NULL) ||
      (stpt_route->u8_firstAppType > stpt_route->u8_lastAppType) ||
      (stpt_route->u8_lastAppType > BLK_APP_TYPE_MAX))
   {
      return -EINVAL;
   }

   for (u8_idx = 0U; u8_idx < su8_routeCnt; u8_idx++)
   {
      // Check if the range overlaps a registered one
      if ((stpt_route->u8_firstAppType <= sstar_routes[u8_idx].u8_lastAppType) &&
         (stpt_route->u8_lastAppType >= sstar_routes[u8_idx].u8_firstAppType))
      {
         return -EEXIST;
      }
   }

   // Check if there is room for another range
   if (su8_routeCnt >= BULK_ROUTER_MAX_ROUTES)
   {
      return -ENOMEM;
   }

   sstar_routes[su8_routeCnt] = *stpt_route;
   su8_routeCnt++;

   return 0;
}

/**
 * @public        gi_BulkRouter_Start
 * @brief         Initialise the BulkXfer GATT service and the BulkXfer Server with
 *                the router callbacks. Call once, after every module has
 *                registered and before advertising starts.
 * @return        0 on success, -EALREADY if already started, otherwise the error
 *                from gi_BLKS_Init().
 */
int gi_BulkRouter_Start(void)
{
   BlkSrvCfg_T st_cfg = { 0 };
   int i_ret = 0;

   // Check if the Server already runs
   if (sb_isStarted)
   {
      return -EALREADY;
   }

   st_cfg.stpt_ctrlAttr = gstpt_BulkSvc_Init();
   st_cfg.fpt_onRxStart = si_RouteRxStart;
   st_cfg.fpt_onRxData = si_RouteRxData;
   st_cfg.fpt_onRxDone = sv_RouteRxDone;
   st_cfg.fpt_onRxShort = sv_RouteRxShort;
   // ConnectionHandling.c already negotiates PHY, data length and MTU
   st_cfg.b_autoTuneLink = false;

   i_ret = gi_BLKS_Init(&st_cfg);

   // Check if the BulkXfer Server started
   if (i_ret != 0)
   {
      APP_LOG_ERR("gi_BLKS_Init failed (%d)", i_ret);
   }
   else
   {
      sb_isStarted = true;
      APP_LOG_INF("BulkXfer server ready, %u appType range(s)", su8_routeCnt);
   }

   return i_ret;
}

/**
 * @public        gv_BulkRouter_SetFilter
 * @brief         Accept only appTypes u8_first..u8_last (inclusive) from now
 *                on: other transfers are rejected at START (as unknown
 *                types) and other short messages are dropped. A transfer
 *                already accepted runs to its end. For a link whose peer may
 *                reach only one module, e.g. a peer device that may only
 *                pair. Any thread; replaces the previous filter.
 * @param[in]     u8_first First allowed appType.
 * @param[in]     u8_last Last allowed appType (inclusive).
 * @return        None.
 */
void gv_BulkRouter_SetFilter(uint8_t u8_first, uint8_t u8_last)
{
   (void)atomic_set(&st_filter,
      ROUTER_FILTER_ON | ((atomic_val_t)u8_first << 8) | (atomic_val_t)u8_last);
}

/**
 * @public        gv_BulkRouter_ClearFilter
 * @brief         Remove the filter: every registered range is reachable
 *                again. Any thread.
 * @return        None.
 */
void gv_BulkRouter_ClearFilter(void)
{
   (void)atomic_set(&st_filter, 0);
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
