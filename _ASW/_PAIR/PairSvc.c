/**
 * @file          PairSvc.c
 * @brief         Source file containing the Pairing GATT service. It only holds
 *                the attribute table and forwards to Pair.c, which owns the
 *                logic (and its tests):
 *                  CONTROL (Write): gt_Pair_OnControlWrite() validates and
 *                       queues START / CANCEL / UNPAIR.
 *                  STATUS (Read, Notify): gv_Pair_GetStatus() fills the value
 *                       on each read; Pair.c notifies it on every change.
 *                  SECURED (Write, LESC encryption required): the stack refuses
 *                       a write on a link below level 4 itself;
 *                       gt_Pair_OnSecuredWrite() checks it comes from the peer.
 *                Like BulkSvc.c, it calls nothing but the BT stack and is left to
 *                the firmware build.
 * @date          06/10/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "PairSvc.h"
#include <errno.h>
#include <zephyr/bluetooth/gatt.h>
#include "GATT_GenericCallbacks.h"
#include "Pair.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           PAIR_CONTROL_MAX_LEN
 * @brief         Longest CONTROL write (START).
 */
#define PAIR_CONTROL_MAX_LEN                 (PAIR_START_LEN)

/**
 * @def           PAIR_SECURED_MAX_LEN
 * @brief         Longest SECURED write.
 */
#define PAIR_SECURED_MAX_LEN                 (4U)

/**
 * @def           PAIR_STATUS_ATTR_IDX
 * @brief         Index of the STATUS value attribute in the service: service,
 *                CONTROL declaration and value, STATUS declaration, STATUS value.
 */
#define PAIR_STATUS_ATTR_IDX                 (4U)

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
static ssize_t st_OnControlWrite(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset, uint8_t u8_flags);
static ssize_t st_OnStatusRead(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset);
static ssize_t st_OnSecuredWrite(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset, uint8_t u8_flags);

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
 * @var           su8ar_control
 * @brief         Last CONTROL write (gt_GATT_GenericWrite's copy).
 */
static uint8_t su8ar_control[PAIR_CONTROL_MAX_LEN];

/**
 * @var           su8ar_status
 * @brief         STATUS value served on reads, refreshed by the read hook.
 */
static uint8_t su8ar_status[PAIR_STATUS_LEN];

/**
 * @var           su8ar_secured
 * @brief         Last SECURED write.
 */
static uint8_t su8ar_secured[PAIR_SECURED_MAX_LEN];

/**
 * @var           sst_controlDesc
 * @brief         Descriptor for CONTROL (variable length, write hook).
 */
static GATTCharDescriptor_T sst_controlDesc = {
   .vpt_data          = su8ar_control,
   .u16_dataLen       = sizeof(su8ar_control),
   .u16_actualLen     = 0U,
   .b_variableLength  = true,
   .stpt_mutex        = NULL,
   .fpt_customReadCb  = NULL,
   .fpt_customWriteCb = st_OnControlWrite,
};

/**
 * @var           sst_statusDesc
 * @brief         Descriptor for STATUS (fixed length, read hook).
 */
static GATTCharDescriptor_T sst_statusDesc = {
   .vpt_data          = su8ar_status,
   .u16_dataLen       = sizeof(su8ar_status),
   .u16_actualLen     = sizeof(su8ar_status),
   .b_variableLength  = false,
   .stpt_mutex        = NULL,
   .fpt_customReadCb  = st_OnStatusRead,
   .fpt_customWriteCb = NULL,
};

/**
 * @var           sst_securedDesc
 * @brief         Descriptor for SECURED (variable length, write hook).
 */
static GATTCharDescriptor_T sst_securedDesc = {
   .vpt_data          = su8ar_secured,
   .u16_dataLen       = sizeof(su8ar_secured),
   .u16_actualLen     = 0U,
   .b_variableLength  = true,
   .stpt_mutex        = NULL,
   .fpt_customReadCb  = NULL,
   .fpt_customWriteCb = st_OnSecuredWrite,
};

/**
 * @var           gst_pairSvc
 * @brief         Pairing service instance. Attribute order matters:
 *                PAIR_STATUS_ATTR_IDX points at the STATUS value.
 */
BT_GATT_SERVICE_DEFINE(gst_pairSvc,
   BT_GATT_PRIMARY_SERVICE(BT_UUID_PAIR_SVC),
   // CONTROL: START / CANCEL / UNPAIR from the host
   BT_GATT_CHARACTERISTIC(BT_UUID_PAIR_CONTROL,
      BT_GATT_CHRC_WRITE,
      BT_GATT_PERM_WRITE,
      NULL, gt_GATT_GenericWrite, &sst_controlDesc),
   // STATUS: pairing state, read on demand and notified on change
   BT_GATT_CHARACTERISTIC(BT_UUID_PAIR_STATUS,
      BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
      BT_GATT_PERM_READ,
      gt_GATT_GenericRead, NULL, &sst_statusDesc),
   BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
   // SECURED: written by the central once the peer link is paired. LESC:
   // the stack itself refuses a write below level 4.
   BT_GATT_CHARACTERISTIC(BT_UUID_PAIR_SECURED,
      BT_GATT_CHRC_WRITE,
      BT_GATT_PERM_WRITE_LESC,
      NULL, gt_GATT_GenericWrite, &sst_securedDesc),
);

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
 * @private       st_OnControlWrite
 * @brief         Custom write hook of CONTROL: hand the command to Pair.c.
 * @param[in]     stpt_connHandle Connection that wrote.
 * @param[in]     stpt_attr CONTROL attribute (unused).
 * @param[in]     vpt_buf Written bytes.
 * @param[in]     u16_length Number of bytes.
 * @param[in]     u16_offset Write offset (unused: the generic write checked it).
 * @param[in]     u8_flags Write flags (unused).
 * @return        0 to accept, or the ATT error from gt_Pair_OnControlWrite().
 */
static ssize_t st_OnControlWrite(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset, uint8_t u8_flags)
{
   ARG_UNUSED(stpt_attr);
   ARG_UNUSED(u16_offset);
   ARG_UNUSED(u8_flags);

   return gt_Pair_OnControlWrite(stpt_connHandle, (const uint8_t *)vpt_buf, u16_length);
}

/**
 * @private       st_OnStatusRead
 * @brief         Custom read hook of STATUS: serve the current status.
 * @param[in]     stpt_connHandle Connection that reads (unused).
 * @param[in]     stpt_attr STATUS attribute.
 * @param[out]    vpt_buf Output buffer.
 * @param[in]     u16_length Requested length.
 * @param[in]     u16_offset Read offset.
 * @return        Bytes read, or an ATT error.
 */
static ssize_t st_OnStatusRead(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset)
{
   uint8_t u8ar_status[PAIR_STATUS_LEN];

   gv_Pair_GetStatus(u8ar_status);

   return bt_gatt_attr_read(stpt_connHandle, stpt_attr, vpt_buf, u16_length, u16_offset,
      u8ar_status, sizeof(u8ar_status));
}

/**
 * @private       st_OnSecuredWrite
 * @brief         Custom write hook of SECURED: hand the write to Pair.c.
 * @param[in]     stpt_connHandle Connection that wrote.
 * @param[in]     stpt_attr SECURED attribute (unused).
 * @param[in]     vpt_buf Written bytes.
 * @param[in]     u16_length Number of bytes.
 * @param[in]     u16_offset Write offset (unused).
 * @param[in]     u8_flags Write flags (unused).
 * @return        0 to accept, or the ATT error from gt_Pair_OnSecuredWrite().
 */
static ssize_t st_OnSecuredWrite(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset, uint8_t u8_flags)
{
   ARG_UNUSED(stpt_attr);
   ARG_UNUSED(u16_offset);
   ARG_UNUSED(u8_flags);

   return gt_Pair_OnSecuredWrite(stpt_connHandle, (const uint8_t *)vpt_buf, u16_length);
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gi_PairSvc_NotifyStatus
 * @brief         Notify STATUS to one connection (best effort).
 * @param[in]     stpt_conn Connection to notify (the host link); NULL does
 *                nothing.
 * @param[in]     u8pt_status Status bytes.
 * @param[in]     u16_len Number of bytes.
 * @return        0 if queued, -ENOTCONN for a NULL connection, -EACCES if it is
 *                not subscribed, otherwise the error of bt_gatt_notify().
 */
int gi_PairSvc_NotifyStatus(struct bt_conn *stpt_conn, const uint8_t *u8pt_status,
   uint16_t u16_len)
{
   const struct bt_gatt_attr *stpt_attr = &gst_pairSvc.attrs[PAIR_STATUS_ATTR_IDX];

   // Check if there is a host link to notify
   if (stpt_conn == NULL)
   {
      return -ENOTCONN;
   }

   // Check if the host has subscribed
   if (!bt_gatt_is_subscribed(stpt_conn, stpt_attr, BT_GATT_CCC_NOTIFY))
   {
      return -EACCES;
   }

   return bt_gatt_notify(stpt_conn, stpt_attr, u8pt_status, u16_len);
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
