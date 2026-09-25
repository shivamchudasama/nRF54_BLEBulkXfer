/**
 * @file          DataStore.c
 * @brief         Source file containing the receive-side data store.
 *
 *                Each hex segment arrives as one BulkXfer transfer of type
 *                DS_APP_TYPE_SEGMENT whose object is [u32 LE start address][data].
 *                The data is collected in su8ar_segBuf. Once the transfer ends with
 *                eBS_OK (CRC-32 verified), the dump thread logs it and then tells the
 *                client with a DS_APP_TYPE_STORED short message. New transfers are
 *                rejected until the dump has finished. With CONFIG_DS_HEX_DUMP the
 *                dump prints every byte as "0xAAAAAAAA: xx xx ..." lines (slow, for
 *                debugging); without it, a single summary line with the CRC-32.
 * @date          25/09/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "DataStore.h"
#include <errno.h>
#include <string.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/logging/log_ctrl.h>
#include "BulkXfer.h"
#include "BulkSvc.h"
#include "AppLog.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           DS_BYTES_PER_LINE
 * @brief         Data bytes printed per dump line.
 */
#define DS_BYTES_PER_LINE                    (16U)

/**
 * @def           DS_REPORT_LEN
 * @brief         Payload length of the RESULT and STORED short messages:
 *                [u8 status][u32 LE address][u32 LE length].
 */
#define DS_REPORT_LEN                        (9U)

/**
 * @def           DS_SHORT_TIMEOUT_MS
 * @brief         Maximum wait for a CTRL credit when sending a short message.
 */
#define DS_SHORT_TIMEOUT_MS                  (100)

/**
 * @def           DS_LOG_BACKLOG_MAX
 * @brief         The dump thread waits while more log messages than this are pending,
 *                so the deferred log buffer never overflows and drops lines.
 */
#define DS_LOG_BACKLOG_MAX                   (8U)

/**
 * @def           DS_DUMP_STACK_SIZE
 * @brief         Stack size of the dump thread.
 */
#define DS_DUMP_STACK_SIZE                   (1024)

/**
 * @def           DS_DUMP_PRIORITY
 * @brief         Priority of the dump thread. Below the BulkXfer engine thread
 *                (BLK_THREAD_PRIORITY) so that dumping never delays the link.
 */
#define DS_DUMP_PRIORITY                     (10)

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
static int si_OnRxStart(uint8_t u8_appType, uint32_t u32_totalLen);
static int si_OnRxData(uint8_t u8_appType, uint32_t u32_offset,
   const uint8_t *u8pt_data, uint16_t u16_len);
static void sv_OnRxDone(uint8_t u8_appType, BlkStatus_E e_status, uint32_t u32_totalLen);
static void sv_OnRxShort(uint8_t u8_appType, const uint8_t *u8pt_data, uint8_t u8_len);
static void sv_SendReport(uint8_t u8_appType, uint8_t u8_status, uint32_t u32_addr,
   uint32_t u32_len);
static void sv_DumpSegment(void);
static void sv_DumpThread(void *vpt_p1, void *vpt_p2, void *vpt_p3);

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
 * @var           su8ar_segBuf
 * @brief         Data bytes of the segment being received or dumped.
 */
static uint8_t su8ar_segBuf[DS_BUF_SIZE];

/**
 * @var           su8ar_addrHdr
 * @brief         Little-endian start address received in front of the segment data.
 */
static uint8_t su8ar_addrHdr[DS_ADDR_HDR_LEN];

/**
 * @var           su32_segAddr
 * @brief         Start address of the segment handed to the dump thread.
 */
static uint32_t su32_segAddr = 0U;

/**
 * @var           su32_segLen
 * @brief         Number of data bytes of the segment handed to the dump thread.
 */
static uint32_t su32_segLen = 0U;

/**
 * @var           st_dumpBusy
 * @brief         Set from a verified segment until its dump has finished. While set,
 *                su8ar_segBuf belongs to the dump thread and new transfers are rejected.
 */
static atomic_t st_dumpBusy = ATOMIC_INIT(0);

/**
 * @var           sst_dumpSem
 * @brief         Wakes the dump thread when a verified segment is ready.
 */
static K_SEM_DEFINE(sst_dumpSem, 0, 1);

/**
 * @var           sst_dumpThread
 * @brief         Low-priority thread that prints received segments.
 */
K_THREAD_DEFINE(sst_dumpThread, DS_DUMP_STACK_SIZE, sv_DumpThread, NULL, NULL, NULL,
   DS_DUMP_PRIORITY, 0, 0);

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
 * @private       si_OnRxStart
 * @brief         BlkRxStart_F: accept a hex segment that fits the buffer while the
 *                buffer is free.
 * @param[in]     u8_appType Application type announced by the client.
 * @param[in]     u32_totalLen Object size (address header + data).
 * @return        0 to accept, non-zero to reject (client sees eBS_REJECTED).
 */
static int si_OnRxStart(uint8_t u8_appType, uint32_t u32_totalLen)
{
   int i_ret = 0;

   // Check if the transfer is a hex segment
   if (u8_appType != DS_APP_TYPE_SEGMENT)
   {
      APP_LOG_WRN("rejected: unknown type 0x%02x", u8_appType);
      i_ret = -ENOTSUP;
   }
   // Check if the object holds an address and at least one data byte, and fits the buffer
   else if ((u32_totalLen <= DS_ADDR_HDR_LEN) ||
      (u32_totalLen > (DS_ADDR_HDR_LEN + DS_BUF_SIZE)))
   {
      APP_LOG_WRN("rejected: length %u outside %u..%u", u32_totalLen,
         DS_ADDR_HDR_LEN + 1U, DS_ADDR_HDR_LEN + DS_BUF_SIZE);
      i_ret = -EMSGSIZE;
   }
   // Check if the previous segment is still being dumped
   else if (atomic_get(&st_dumpBusy) != 0)
   {
      APP_LOG_WRN("rejected: previous segment still being dumped");
      i_ret = -EBUSY;
   }
   else
   {
      APP_LOG_INF("segment announced: %u bytes", u32_totalLen - DS_ADDR_HDR_LEN);
   }

   return i_ret;
}

/**
 * @private       si_OnRxData
 * @brief         BlkRxData_F: store an in-order chunk. Object bytes 0..3 are the address
 *                header, the rest goes into su8ar_segBuf. The first chunk usually carries
 *                both, so it is split.
 * @param[in]     u8_appType Application type (unused, checked at START).
 * @param[in]     u32_offset Object offset of the chunk.
 * @param[in]     u8pt_data Chunk data, valid only during the call.
 * @param[in]     u16_len Chunk length.
 * @return        0 to continue, non-zero aborts with eBS_SINK_ERROR.
 */
static int si_OnRxData(uint8_t u8_appType, uint32_t u32_offset,
   const uint8_t *u8pt_data, uint16_t u16_len)
{
   uint32_t u32_hdrBytes = 0U;
   uint32_t u32_bufOffset = 0U;

   ARG_UNUSED(u8_appType);

   // Check if the chunk starts inside the address header
   if (u32_offset < DS_ADDR_HDR_LEN)
   {
      u32_hdrBytes = MIN(DS_ADDR_HDR_LEN - u32_offset, (uint32_t)u16_len);
      memcpy(&su8ar_addrHdr[u32_offset], u8pt_data, u32_hdrBytes);
      u32_offset += u32_hdrBytes;
      u8pt_data += u32_hdrBytes;
      u16_len -= (uint16_t)u32_hdrBytes;
   }

   // Check if data bytes remain after the header part
   if (u16_len > 0U)
   {
      u32_bufOffset = u32_offset - DS_ADDR_HDR_LEN;

      // Check if the chunk fits the buffer (guaranteed by the length check at START)
      if ((u32_bufOffset + u16_len) > DS_BUF_SIZE)
      {
         APP_LOG_ERR("chunk at %u + %u overflows the buffer", u32_bufOffset, u16_len);
         return -ENOSPC;
      }

      memcpy(&su8ar_segBuf[u32_bufOffset], u8pt_data, u16_len);
   }

   APP_LOG_DBG("chunk offset %u, %u bytes", u32_offset, u16_len);

   return 0;
}

/**
 * @private       sv_OnRxDone
 * @brief         BlkRxDone_F: hand a verified segment to the dump thread, or discard a
 *                failed one. Reports the result to the client either way.
 * @param[in]     u8_appType Application type.
 * @param[in]     e_status Result of the incoming transfer.
 * @param[in]     u32_totalLen Object size (address header + data).
 * @return        None.
 */
static void sv_OnRxDone(uint8_t u8_appType, BlkStatus_E e_status, uint32_t u32_totalLen)
{
   uint32_t u32_addr = 0U;
   uint32_t u32_len = 0U;

   // Check if the segment is complete and CRC-verified
   if (e_status == eBS_OK)
   {
      u32_addr = sys_get_le32(su8ar_addrHdr);
      u32_len = u32_totalLen - DS_ADDR_HDR_LEN;

      APP_LOG_INF("segment received: addr 0x%08x, %u bytes", u32_addr, u32_len);

      su32_segAddr = u32_addr;
      su32_segLen = u32_len;
      atomic_set(&st_dumpBusy, 1);
      k_sem_give(&sst_dumpSem);
   }
   else
   {
      APP_LOG_WRN("transfer type 0x%02x failed, status %u, data discarded", u8_appType,
         (unsigned int)e_status);
   }

   // Best effort: the link may already be gone (eBS_DISCONNECTED)
   sv_SendReport(DS_APP_TYPE_RESULT, (uint8_t)e_status, u32_addr, u32_len);
}

/**
 * @private       sv_OnRxShort
 * @brief         BlkRxShort_F: no client -> server short messages are defined yet;
 *                log and ignore.
 * @param[in]     u8_appType Application type.
 * @param[in]     u8pt_data Payload (unused).
 * @param[in]     u8_len Payload length.
 * @return        None.
 */
static void sv_OnRxShort(uint8_t u8_appType, const uint8_t *u8pt_data, uint8_t u8_len)
{
   ARG_UNUSED(u8pt_data);

   APP_LOG_INF("short message type 0x%02x, %u bytes ignored", u8_appType, u8_len);
}

/**
 * @private       sv_SendReport
 * @brief         Send a [u8 status][u32 LE address][u32 LE length] short message on CTRL.
 * @param[in]     u8_appType DS_APP_TYPE_RESULT or DS_APP_TYPE_STORED.
 * @param[in]     u8_status BlkStatus_E value (0 = OK).
 * @param[in]     u32_addr Segment start address.
 * @param[in]     u32_len Segment data length.
 * @return        None.
 */
static void sv_SendReport(uint8_t u8_appType, uint8_t u8_status, uint32_t u32_addr,
   uint32_t u32_len)
{
   uint8_t u8ar_report[DS_REPORT_LEN];
   int i_ret = 0;

   u8ar_report[0] = u8_status;
   sys_put_le32(u32_addr, &u8ar_report[1]);
   sys_put_le32(u32_len, &u8ar_report[5]);

   i_ret = gi_BLKS_SendShort(u8_appType, u8ar_report, sizeof(u8ar_report),
      K_MSEC(DS_SHORT_TIMEOUT_MS));

   // Check if the report could not be sent (no link, not subscribed, no credit)
   if (i_ret != 0)
   {
      APP_LOG_DBG("report 0x%02x not sent (%d)", u8_appType, i_ret);
   }
}

/**
 * @private       sv_DumpSegment
 * @brief         Log the segment in su8ar_segBuf. With CONFIG_DS_HEX_DUMP, print it as
 *                "0xAAAAAAAA: xx xx ..." lines, DS_BYTES_PER_LINE bytes each, framed by
 *                start and end lines; otherwise print one summary line.
 * @return        None.
 */
static void sv_DumpSegment(void)
{
   static const char sscar_hexDigits[] = "0123456789abcdef";
   char car_line[(DS_BYTES_PER_LINE * 3U) + 1U];
   uint32_t u32_crc = 0U;
   uint32_t u32_pos = 0U;
   uint32_t u32_lineLen = 0U;
   uint32_t u32_idx = 0U;

   // CRC over the whole object, equal to the CRC-32 in the client's START frame
   u32_crc = crc32_ieee(su8ar_addrHdr, sizeof(su8ar_addrHdr));
   u32_crc = crc32_ieee_update(u32_crc, su8ar_segBuf, su32_segLen);

   // Check if the full hex dump is disabled: the summary line is all that is printed
   if (!IS_ENABLED(CONFIG_DS_HEX_DUMP))
   {
      LOG_INF("SEG addr=0x%08x len=%u crc=0x%08x", su32_segAddr, su32_segLen, u32_crc);
      return;
   }

   LOG_INF("SEG start addr=0x%08x len=%u crc=0x%08x", su32_segAddr, su32_segLen, u32_crc);

   for (u32_pos = 0U; u32_pos < su32_segLen; u32_pos += DS_BYTES_PER_LINE)
   {
      u32_lineLen = MIN(DS_BYTES_PER_LINE, su32_segLen - u32_pos);

      for (u32_idx = 0U; u32_idx < u32_lineLen; u32_idx++)
      {
         car_line[(u32_idx * 3U)] = sscar_hexDigits[su8ar_segBuf[u32_pos + u32_idx] >> 4];
         car_line[(u32_idx * 3U) + 1U] = sscar_hexDigits[su8ar_segBuf[u32_pos + u32_idx] & 0x0FU];
         car_line[(u32_idx * 3U) + 2U] = ' ';
      }

      // Replace the trailing space with the terminator
      car_line[(u32_lineLen * 3U) - 1U] = '\0';

      // Wait for the log thread to drain, so no line is dropped
      while (log_buffered_cnt() > DS_LOG_BACKLOG_MAX)
      {
         k_msleep(1);
      }

      LOG_INF("0x%08x: %s", su32_segAddr + u32_pos, car_line);
   }

   LOG_INF("SEG end addr=0x%08x len=%u", su32_segAddr, su32_segLen);
}

/**
 * @private       sv_DumpThread
 * @brief         Wait for verified segments, dump each one, then free the buffer and
 *                tell the client with DS_APP_TYPE_STORED.
 * @param[in]     vpt_p1 Unused.
 * @param[in]     vpt_p2 Unused.
 * @param[in]     vpt_p3 Unused.
 * @return        None.
 */
static void sv_DumpThread(void *vpt_p1, void *vpt_p2, void *vpt_p3)
{
   uint32_t u32_addr = 0U;
   uint32_t u32_len = 0U;

   ARG_UNUSED(vpt_p1);
   ARG_UNUSED(vpt_p2);
   ARG_UNUSED(vpt_p3);

   while (true)
   {
      k_sem_take(&sst_dumpSem, K_FOREVER);

      sv_DumpSegment();

      // Copy before freeing the buffer: the next segment overwrites these
      u32_addr = su32_segAddr;
      u32_len = su32_segLen;
      atomic_set(&st_dumpBusy, 0);

      sv_SendReport(DS_APP_TYPE_STORED, (uint8_t)eBS_OK, u32_addr, u32_len);
   }
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gi_DataStore_Init
 * @brief         Initialise the BulkXfer Server with the data store callbacks. Call once,
 *                before advertising starts, so no connection arrives before the Server
 *                is ready.
 * @return        0 on success, otherwise the error from gi_BLKS_Init().
 */
int gi_DataStore_Init(void)
{
   BlkSrvCfg_T st_cfg = { 0 };
   int i_ret = 0;

   st_cfg.stpt_ctrlAttr = gstpt_BulkSvc_Init();
   st_cfg.fpt_onRxStart = si_OnRxStart;
   st_cfg.fpt_onRxData = si_OnRxData;
   st_cfg.fpt_onRxDone = sv_OnRxDone;
   st_cfg.fpt_onRxShort = sv_OnRxShort;
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
      APP_LOG_INF("BulkXfer server ready, segment buffer %u bytes", DS_BUF_SIZE);
   }

   return i_ret;
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
