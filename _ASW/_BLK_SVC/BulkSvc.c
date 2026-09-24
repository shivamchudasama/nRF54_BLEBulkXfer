/**
 * @file          BulkSvc.c
 * @brief         Source file containing the BulkXfer GATT service. This device is the
 *                BulkXfer receiver (Server role), so it hosts:
 *                  DATA (Write | Write Without Response): client -> server frames.
 *                       gt_GATT_GenericWrite + custom write hook that forwards every
 *                       frame to gt_BLKS_DataWriteHook().
 *                  CTRL (Notify): server -> client control frames and short messages,
 *                       sent by BulkXfer itself. No read/write callbacks.
 *                  Caps (Read): BlkCaps_T, served by gt_GATT_GenericRead.
 * @date          25/09/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "BulkSvc.h"
#include "GATT_GenericCallbacks.h"
#include "BulkXfer.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/

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
static ssize_t st_OnBulkDataWrite(struct bt_conn *stpt_connHandle,
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
 * @var           su8ar_dataFrame
 * @brief         Copy of the last DATA write made by gt_GATT_GenericWrite (one full
 *                frame). BulkXfer reads the frame from the stack buffer, not from here.
 */
static uint8_t su8ar_dataFrame[BLK_MAX_FRAME_LEN];

/**
 * @var           sst_caps
 * @brief         Capability record served to the client.
 */
static BlkCaps_T sst_caps;

/**
 * @var           sst_dataFrameDesc
 * @brief         Descriptor for the 'DATA' characteristic. Variable length up to
 *                BLK_MAX_FRAME_LEN; the hook passes every frame to BulkXfer. No mutex:
 *                the buffer is only touched in BLE RX context.
 */
static GATTCharDescriptor_T sst_dataFrameDesc = {
   .vpt_data          = su8ar_dataFrame,
   .u16_dataLen       = sizeof(su8ar_dataFrame),
   .u16_actualLen     = 0U,
   .b_variableLength  = true,
   .stpt_mutex        = NULL,
   .fpt_customReadCb  = NULL,
   .fpt_customWriteCb = st_OnBulkDataWrite,
};

/**
 * @var           sst_capsDesc
 * @brief         Descriptor for the 'Caps' characteristic (fixed length). Written once
 *                by gstpt_BulkSvc_Init() before advertising starts.
 */
static GATTCharDescriptor_T sst_capsDesc = {
   .vpt_data          = &sst_caps,
   .u16_dataLen       = sizeof(sst_caps),
   .u16_actualLen     = sizeof(sst_caps),
   .b_variableLength  = false,
   .stpt_mutex        = NULL,
   .fpt_customReadCb  = NULL,
   .fpt_customWriteCb = NULL,
};

/******************************************************************************/
/*                                                                            */
/*                              PUBLIC VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           gst_bulkXferSvc
 * @brief         BulkXfer service instance. BT_GATT_SERVICE_DEFINE gives it external
 *                linkage. Defined after the private descriptors it references.
 */
BT_GATT_SERVICE_DEFINE(gst_bulkXferSvc,
   BT_GATT_PRIMARY_SERVICE(BT_UUID_BLK_SVC),

   // DATA: client -> server frames (START / DATA / ABORT / short messages)
   BT_GATT_CHARACTERISTIC(BT_UUID_BLK_DATA,
      BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
      BT_GATT_PERM_WRITE,
      NULL, gt_GATT_GenericWrite, &sst_dataFrameDesc),

   // CTRL: server -> client frames (ACK / NACK / END / ABORT / short messages)
   BT_GATT_CHARACTERISTIC(BT_UUID_BLK_CTRL,
      BT_GATT_CHRC_NOTIFY,
      BT_GATT_PERM_NONE,
      NULL, NULL, NULL),
   BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

   // Caps: protocol capabilities
   BT_GATT_CHARACTERISTIC(BT_UUID_BLK_CAPS,
      BT_GATT_CHRC_READ,
      BT_GATT_PERM_READ,
      gt_GATT_GenericRead, NULL, &sst_capsDesc),
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
 * @private       st_OnBulkDataWrite
 * @brief         Custom write hook of the DATA characteristic. Forwards the frame to
 *                the BulkXfer Server.
 * @param[in]     stpt_connHandle Connection that wrote.
 * @param[in]     stpt_attr DATA attribute.
 * @param[in]     vpt_buf Written bytes (one frame).
 * @param[in]     u16_length Number of bytes.
 * @param[in]     u16_offset Write offset.
 * @param[in]     u8_flags Write flags.
 * @return        Result of gt_BLKS_DataWriteHook().
 */
static ssize_t st_OnBulkDataWrite(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset, uint8_t u8_flags)
{
   return gt_BLKS_DataWriteHook(stpt_connHandle, stpt_attr, vpt_buf, u16_length,
      u16_offset, u8_flags);
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gstpt_BulkSvc_Init
 * @brief         Publish the capability record and locate the CTRL value attribute
 *                for BlkSrvCfg_T.stpt_ctrlAttr.
 *
 *                Scans the service's own attribute array rather than calling
 *                bt_gatt_find_by_uuid(): that one only searches static services after
 *                bt_enable() has run (it checks last_static_handle, set by
 *                bt_gatt_init()), and this is called before bt_enable().
 * @return        CTRL value attribute (never NULL for this static service).
 */
const struct bt_gatt_attr *gstpt_BulkSvc_Init(void)
{
   BlkCaps_T st_caps;
   const struct bt_gatt_attr *stpt_ctrlAttr = NULL;

   gv_BLKS_GetCaps(&st_caps);
   gv_GATT_LocalWrite(&sst_capsDesc, &st_caps, sizeof(st_caps));

   // Find the value attribute (not the declaration), which notify and the CCC
   // lookup expect. Only the value attribute carries the CTRL UUID itself.
   for (size_t i = 0U; i < gst_bulkXferSvc.attr_count; i++)
   {
      // Check if this is the CTRL value attribute
      if (bt_uuid_cmp(gst_bulkXferSvc.attrs[i].uuid, BT_UUID_BLK_CTRL) == 0)
      {
         stpt_ctrlAttr = &gst_bulkXferSvc.attrs[i];
         break;
      }
   }

   return stpt_ctrlAttr;
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
