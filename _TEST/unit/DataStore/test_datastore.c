/**
 * @file          test_datastore.c
 * @brief         Host unit tests for _ASW/_DATA_STORE against the hex-upload
 *                contract in _DOC/HexUpload/PROTOCOL.md (section numbers
 *                below refer to it). DataStore.c is included so its private
 *                callbacks and state can be driven directly. It registers with
 *                the real router (SETURouter.c, included too), as in the
 *                firmware, and the tests drive the Server callbacks the router
 *                hands to the stubbed SETU Server API.
 *
 *                Storing an upload as a file (BEGIN / COMMIT, §7) runs on the
 *                real File System Manager (included too) over the in-memory
 *                volume of shim/fs_sim.c, so the tests check the file itself.
 *
 *                Built twice by _TEST/CMakeLists.txt: as is (summary line)
 *                and with -DCONFIG_DS_HEX_DUMP=1 (full hex dump).
 *
 * @date          29/09/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#include "DataStore.c"
#include "SETURouter.c"
#include "FileSysManager.c"
#include "FileSysManagerFSM.c"
#include "unity.h"
#include "wire_vectors.h"

/******************************************************************************/
/*  Stubs of the SETU Server and the GATT service                         */
/******************************************************************************/
static struct bt_gatt_attr sst_ctrlAttr;
static SETUSrvCfg_T sst_cfg;                  /* what the router passed to the Server */
static int si_initRet;                       /* what gi_SETUS_Init returns         */
static int si_sendRet;                       /* what gi_SETUS_SendShort returns    */

typedef struct
{
   uint8_t u8_type;
   uint8_t u8_len;
   uint8_t u8ar_payload[SETU_MAX_SHORT_PAYLOAD];
   int64_t i64_timeoutMs;
} SentShort_T;

static SentShort_T sstar_sent[8];
static uint32_t su32_sentCount;

const struct bt_gatt_attr *gstpt_SETUSvc_Init(void)
{
   return &sst_ctrlAttr;
}

int gi_SETUS_Init(const SETUSrvCfg_T *stpt_cfg)
{
   sst_cfg = *stpt_cfg;
   return si_initRet;
}

int gi_SETUS_SendShort(uint8_t u8_appType, const void *vpt_data, uint8_t u8_len,
   k_timeout_t t_timeout)
{
   SentShort_T *stpt_s = &sstar_sent[su32_sentCount % ARRAY_SIZE(sstar_sent)];

   stpt_s->u8_type = u8_appType;
   stpt_s->u8_len = u8_len;
   (void)memcpy(stpt_s->u8ar_payload, vpt_data, u8_len);
   stpt_s->i64_timeoutMs = t_timeout.ms;
   su32_sentCount++;
   return si_sendRet;
}

/* The dump thread blocks only in gi_FSMGR_Call(): run the File System Manager */
void gv_SimOnBlock(struct k_sem *stpt_sem)
{
   (void)stpt_sem;
   while (sb_ProcessNext(K_NO_WAIT)) {}
}

void gv_SimRegisterTimer(struct k_timer *t)
{
   (void)t;
}

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/
static uint8_t su8ar_obj[DS_ADDR_HDR_LEN + DS_BUF_SIZE + 1U];

/** Build [u32 LE addr][data] in su8ar_obj; returns the object length. */
static uint32_t su32_MakeObject(uint32_t u32_addr, const uint8_t *u8pt_data, uint32_t u32_len)
{
   sys_put_le32(u32_addr, su8ar_obj);
   (void)memcpy(&su8ar_obj[DS_ADDR_HDR_LEN], u8pt_data, u32_len);
   return DS_ADDR_HDR_LEN + u32_len;
}

/** Feed an object through the Server callbacks in u16_chunk-byte pieces. */
static void sv_Receive(uint32_t u32_objLen, uint16_t u16_chunk, SETUStatus_E e_status)
{
   uint32_t u32_off;

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, sst_cfg.fpt_onRxStart(DS_APP_TYPE_SEGMENT, u32_objLen),
      "START rejected");
   for (u32_off = 0U; u32_off < u32_objLen; u32_off += u16_chunk)
   {
      uint16_t u16_n = (uint16_t)MIN((uint32_t)u16_chunk, u32_objLen - u32_off);
      TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxData(DS_APP_TYPE_SEGMENT, u32_off,
         &su8ar_obj[u32_off], u16_n));
   }
   sst_cfg.fpt_onRxDone(DS_APP_TYPE_SEGMENT, e_status, u32_objLen);
}

/** The last short message must be the golden report vector at u32_idx. */
static void sv_AssertLastReport(uint32_t u32_idx)
{
   const ReportVector_T *v = &gstar_vecReports[u32_idx];
   const SentShort_T *s;

   TEST_ASSERT_TRUE_MESSAGE(su32_sentCount > 0U, "no short message sent");
   s = &sstar_sent[(su32_sentCount - 1U) % ARRAY_SIZE(sstar_sent)];
   // wire = [len][type][payload]
   TEST_ASSERT_EQUAL_HEX8_MESSAGE(v->u8ar_wire[1], s->u8_type, v->cpt_name);
   TEST_ASSERT_EQUAL_UINT8_MESSAGE(v->u8ar_wire[0], s->u8_len, v->cpt_name);
   TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(&v->u8ar_wire[2], s->u8ar_payload, v->u8ar_wire[0],
      v->cpt_name);
}

static uint32_t su32_ReportIdx(const char *cpt_name)
{
   uint32_t i;

   for (i = 0U; i < ARRAY_SIZE(gstar_vecReports); i++)
   {
      if (strcmp(gstar_vecReports[i].cpt_name, cpt_name) == 0) { return i; }
   }
   TEST_FAIL_MESSAGE(cpt_name);
   return 0U;
}

void setUp(void)
{
   // Fresh DataStore state (the module has no reset API)
   atomic_set(&st_dumpBusy, 0);
   k_msgq_purge(&sst_dsEventQ);
   atomic_set(&st_uploadOpen, 0);
   si_uploadErr = 0;

   // A formatted, mounted volume and a fresh File System Manager
   gv_SimFsReset();
   gv_SimFsFormat(false);
   k_msgq_purge(&sst_fsmgrMsgQ);
   sb_mounted = false;
   (void)memset(&sst_FSMGRContext, 0, sizeof(sst_FSMGRContext));
   gv_SimRunThread(sv_FSMGR_Thread);
   (void)memset(su8ar_segBuf, 0, sizeof(su8ar_segBuf));
   (void)memset(su8ar_addrHdr, 0, sizeof(su8ar_addrHdr));

   // Fresh router (it has no reset API either)
   su8_routeCnt = 0U;
   sb_isStarted = false;

   si_initRet = 0;
   si_sendRet = 0;
   su32_sentCount = 0U;
   gu32_simLogBuffered = 0U;
   gv_SimLogClear();
   // As main() does: register, then start the Server
   TEST_ASSERT_EQUAL_INT(0, gi_DataStore_Init());
   TEST_ASSERT_EQUAL_INT(0, gi_SETURouter_Start());
}

void tearDown(void) {}

/******************************************************************************/
/*  Initialisation                                                            */
/******************************************************************************/
static void test_InitRegistersServer(void)
{
   TEST_ASSERT_EQUAL_PTR(&sst_ctrlAttr, sst_cfg.stpt_ctrlAttr);
   TEST_ASSERT_NOT_NULL(sst_cfg.fpt_onRxStart);
   TEST_ASSERT_NOT_NULL(sst_cfg.fpt_onRxData);
   TEST_ASSERT_NOT_NULL(sst_cfg.fpt_onRxDone);
   TEST_ASSERT_NOT_NULL(sst_cfg.fpt_onRxShort);
   TEST_ASSERT_FALSE_MESSAGE(sst_cfg.b_autoTuneLink, "_BLE negotiates the link itself");

   // Registering after the Server has started is refused and logged
   TEST_ASSERT_EQUAL_INT(-EALREADY, gi_DataStore_Init());
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("gi_SETURouter_Register failed"));

   // A Server start failure is reported
   su8_routeCnt = 0U;
   sb_isStarted = false;
   si_initRet = -EIO;
   TEST_ASSERT_EQUAL_INT(0, gi_DataStore_Init());
   TEST_ASSERT_EQUAL_INT(-EIO, gi_SETURouter_Start());
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("gi_SETUS_Init failed"));
}

/******************************************************************************/
/*  §5 Rejections                                                             */
/******************************************************************************/
static void test_StartAcceptsOnlySegmentType(void)
{
   static const uint8_t scu8ar_bad[] = { 0x00U, VEC_APP_RESULT, 0x0FU, VEC_APP_STORED, 0xEFU };
   uint32_t i;

   TEST_ASSERT_EQUAL_HEX8(VEC_APP_SEGMENT, DS_APP_TYPE_SEGMENT);
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(VEC_APP_SEGMENT, 100U));
   for (i = 0U; i < ARRAY_SIZE(scu8ar_bad); i++)
   {
      TEST_ASSERT_NOT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(scu8ar_bad[i], 100U));
   }
}

static void test_StartLengthLimits(void)
{
   uint32_t u32_len;

   // Object <= 4 bytes has no data
   for (u32_len = 0U; u32_len <= DS_ADDR_HDR_LEN; u32_len++)
   {
      TEST_ASSERT_NOT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(DS_APP_TYPE_SEGMENT, u32_len));
   }
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(DS_APP_TYPE_SEGMENT, DS_ADDR_HDR_LEN + 1U));

   // Up to 4 + 65536 bytes
   TEST_ASSERT_EQUAL_UINT32(65536U, DS_BUF_SIZE);
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(DS_APP_TYPE_SEGMENT, DS_ADDR_HDR_LEN + 65536U));
   TEST_ASSERT_NOT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(DS_APP_TYPE_SEGMENT, DS_ADDR_HDR_LEN + 65537U));
   TEST_ASSERT_NOT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(DS_APP_TYPE_SEGMENT, 0xFFFFFFFFUL));
}

static void test_StartRejectedWhileDumping(void)
{
   uint8_t u8ar_data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

   sv_Receive(su32_MakeObject(0x1000U, u8ar_data, sizeof(u8ar_data)), 240U, eBS_OK);
   TEST_ASSERT_EQUAL_INT(-EBUSY, sst_cfg.fpt_onRxStart(DS_APP_TYPE_SEGMENT, 100U));

   // STORED frees the buffer
   gv_SimRunThread(sv_DumpThread);
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(DS_APP_TYPE_SEGMENT, 100U));
}

/******************************************************************************/
/*  §3 Segment format and reports                                             */
/******************************************************************************/
static void test_AddressHeaderSplitAcrossChunks(void)
{
   uint8_t u8ar_data[37];
   uint32_t i;

   for (i = 0U; i < sizeof(u8ar_data); i++) { u8ar_data[i] = (uint8_t)(0x40U + i); }

   // 1-, 3- and 7-byte chunks put the 4-byte header boundary mid-chunk
   for (i = 1U; i <= 7U; i += 2U)
   {
      setUp();
      sv_Receive(su32_MakeObject(0x08040201UL, u8ar_data, sizeof(u8ar_data)), (uint16_t)i, eBS_OK);
      TEST_ASSERT_EQUAL_HEX32(0x08040201UL, su32_segAddr);
      TEST_ASSERT_EQUAL_UINT32(sizeof(u8ar_data), su32_segLen);
      TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_data, su8ar_segBuf, sizeof(u8ar_data));
   }
}

static void test_DataPastBufferIsSinkError(void)
{
   uint8_t u8ar_two[2] = { 0 };

   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(DS_APP_TYPE_SEGMENT, DS_ADDR_HDR_LEN + 65536U));
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxData(DS_APP_TYPE_SEGMENT,
      DS_ADDR_HDR_LEN + 65534U, u8ar_two, 2U));
   TEST_ASSERT_NOT_EQUAL_INT(0, sst_cfg.fpt_onRxData(DS_APP_TYPE_SEGMENT,
      DS_ADDR_HDR_LEN + 65535U, u8ar_two, 2U));
}

static void test_ResultOkMatchesLog(void)
{
   sv_Receive(su32_MakeObject(VEC_HEX_SEG_ADDR, gu8ar_vecHexSeg, VEC_HEX_SEG_LEN), 240U, eBS_OK);
   TEST_ASSERT_EQUAL_UINT32(1U, su32_sentCount);
   sv_AssertLastReport(su32_ReportIdx("result_ok_from_log"));
   TEST_ASSERT_TRUE_MESSAGE(sstar_sent[0].i64_timeoutMs >= 0, "report must not wait for ever");
}

static void test_ResultFailureCarriesZeroAddressAndLength(void)
{
   uint8_t u8ar_data[16] = { 0 };

   sv_Receive(su32_MakeObject(0x2000U, u8ar_data, sizeof(u8ar_data)), 240U, eBS_CRC_ERROR);
   sv_AssertLastReport(su32_ReportIdx("result_crc_error"));
   // A failed segment is discarded whole and does not hold the buffer
   TEST_ASSERT_EQUAL_INT(0, atomic_get(&st_dumpBusy));
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(DS_APP_TYPE_SEGMENT, 100U));
}

static void test_ReportSendFailureIsHarmless(void)
{
   uint8_t u8ar_data[16] = { 0 };

   si_sendRet = -ENOTCONN;
   sv_Receive(su32_MakeObject(0x3000U, u8ar_data, sizeof(u8ar_data)), 240U, eBS_OK);
   gv_SimRunThread(sv_DumpThread);
   TEST_ASSERT_EQUAL_INT(0, atomic_get(&st_dumpBusy));
   TEST_ASSERT_EQUAL_UINT32(2U, su32_sentCount);
}

static void test_ClientShortMessagesIgnored(void)
{
   uint8_t u8ar_p[3] = { 1, 2, 3 };

   sst_cfg.fpt_onRxShort(0x05U, u8ar_p, sizeof(u8ar_p));
   TEST_ASSERT_EQUAL_UINT32(0U, su32_sentCount);
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("ignored"));
}

/******************************************************************************/
/*  §6 Serial output and STORED                                               */
/******************************************************************************/
static void test_StoredAfterDumpMatchesLog(void)
{
   sv_Receive(su32_MakeObject(VEC_HEX_SEG_ADDR, gu8ar_vecHexSeg, VEC_HEX_SEG_LEN), 240U, eBS_OK);
   gv_SimRunThread(sv_DumpThread);
   TEST_ASSERT_EQUAL_UINT32(2U, su32_sentCount);
   sv_AssertLastReport(su32_ReportIdx("stored_ok_from_log"));
   TEST_ASSERT_EQUAL_INT(0, atomic_get(&st_dumpBusy));
}

static void test_StoredHighAddressFullBuffer(void)
{
   static uint8_t su8ar_big[DS_BUF_SIZE];

   (void)memset(su8ar_big, 0x5A, sizeof(su8ar_big));
   sv_Receive(su32_MakeObject(0xF0100000UL, su8ar_big, sizeof(su8ar_big)), 240U, eBS_OK);
   gv_SimRunThread(sv_DumpThread);
   sv_AssertLastReport(su32_ReportIdx("stored_high_address"));
}

#if !defined(CONFIG_DS_HEX_DUMP)
/** Default build: one summary line whose CRC equals the client's START CRC. */
static void test_SummaryLineMatchesDeviceLog(void)
{
   sv_Receive(su32_MakeObject(VEC_HEX_SEG_ADDR, gu8ar_vecHexSeg, VEC_HEX_SEG_LEN), 240U, eBS_OK);
   gv_SimLogClear();
   gv_SimRunThread(sv_DumpThread);
   TEST_ASSERT_EQUAL_UINT32(1U, gu32_SimLogCount());
   TEST_ASSERT_EQUAL_STRING(VEC_HEX_SEG_LINE, gcpt_SimLogLine(0U));
   TEST_ASSERT_EQUAL_HEX32(gstar_vecFrames[VEC_HEX_START_IDX].u32ar_f[5],
      crc32_ieee(su8ar_obj, DS_ADDR_HDR_LEN + VEC_HEX_SEG_LEN));
}
#else
/** CONFIG_DS_HEX_DUMP: start line, every byte, end line. */
static void test_HexDumpEveryByte(void)
{
   char car_exp[80];
   uint32_t u32_lines = (VEC_HEX_SEG_LEN + 15U) / 16U;
   uint32_t u32_pos;
   uint32_t i;
   int n;

   sv_Receive(su32_MakeObject(VEC_HEX_SEG_ADDR, gu8ar_vecHexSeg, VEC_HEX_SEG_LEN), 240U, eBS_OK);
   gv_SimLogClear();
   gv_SimRunThread(sv_DumpThread);

   TEST_ASSERT_EQUAL_UINT32(u32_lines + 2U, gu32_SimLogCount());
   TEST_ASSERT_EQUAL_STRING("SEG start addr=0x00000000 len=6560 crc=0xe2e72827", gcpt_SimLogLine(0U));
   for (u32_pos = 0U; u32_pos < VEC_HEX_SEG_LEN; u32_pos += 16U)
   {
      n = snprintf(car_exp, sizeof(car_exp), "0x%08x:", (unsigned)(VEC_HEX_SEG_ADDR + u32_pos));
      for (i = u32_pos; (i < (u32_pos + 16U)) && (i < VEC_HEX_SEG_LEN); i++)
      {
         n += snprintf(&car_exp[n], sizeof(car_exp) - (size_t)n, " %02x", gu8ar_vecHexSeg[i]);
      }
      TEST_ASSERT_EQUAL_STRING(car_exp, gcpt_SimLogLine(1U + (u32_pos / 16U)));
   }
   TEST_ASSERT_EQUAL_STRING("SEG end addr=0x00000000 len=6560", gcpt_SimLogLine(u32_lines + 1U));
}

static void test_HexDumpPartialLastLine(void)
{
   uint8_t u8ar_data[18];
   uint32_t i;

   for (i = 0U; i < sizeof(u8ar_data); i++) { u8ar_data[i] = (uint8_t)i; }
   sv_Receive(su32_MakeObject(0x00010000UL, u8ar_data, sizeof(u8ar_data)), 240U, eBS_OK);
   gv_SimLogClear();
   gv_SimRunThread(sv_DumpThread);
   TEST_ASSERT_EQUAL_UINT32(4U, gu32_SimLogCount());
   TEST_ASSERT_EQUAL_STRING("0x00010000: 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f",
      gcpt_SimLogLine(1U));
   TEST_ASSERT_EQUAL_STRING("0x00010010: 10 11", gcpt_SimLogLine(2U));
}

static void test_HexDumpWaitsForLogBacklog(void)
{
   uint8_t u8ar_data[16] = { 0 };
   int64_t i64_start;

   sv_Receive(su32_MakeObject(0x0U, u8ar_data, sizeof(u8ar_data)), 240U, eBS_OK);
   gu32_simLogBuffered = 20U;
   i64_start = gi64_simNowMs;
   gv_SimRunThread(sv_DumpThread);
   // Drained to DS_LOG_BACKLOG_MAX (8) before the data line was logged
   TEST_ASSERT_EQUAL_INT(20 - DS_LOG_BACKLOG_MAX, (int)(gi64_simNowMs - i64_start));
}
#endif // CONFIG_DS_HEX_DUMP

/******************************************************************************/
/*  §7 Storing the upload as a file: BEGIN, segments, COMMIT                  */
/******************************************************************************/
#define FW_DIR               VEC_HEXF_DIR
#define TEMP_PATH            VEC_HEXF_DIR "/" VEC_HEXF_TEMP_NAME

static const ShortVector_T *sstpt_Hexf(const char *cpt_name)
{
   uint32_t i;

   for (i = 0U; i < ARRAY_SIZE(gstar_vecHexfShorts); i++)
   {
      if (strcmp(gstar_vecHexfShorts[i].cpt_name, cpt_name) == 0) { return &gstar_vecHexfShorts[i]; }
   }
   TEST_FAIL_MESSAGE(cpt_name);
   return NULL;
}

/** The last short message the device sent must be this golden FILE frame. */
static void sv_AssertLastFile(const char *cpt_name)
{
   const ShortVector_T *v = sstpt_Hexf(cpt_name);
   const SentShort_T *s;

   TEST_ASSERT_TRUE_MESSAGE(su32_sentCount > 0U, "no short message sent");
   s = &sstar_sent[(su32_sentCount - 1U) % ARRAY_SIZE(sstar_sent)];
   TEST_ASSERT_EQUAL_HEX8_MESSAGE(v->u8ar_wire[1], s->u8_type, cpt_name);
   TEST_ASSERT_EQUAL_UINT8_MESSAGE(v->u8ar_wire[0], s->u8_len, cpt_name);
   TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(&v->u8ar_wire[2], s->u8ar_payload, v->u8ar_wire[0], cpt_name);
}

/** The client's short message, as the router hands it to the data store. */
static void sv_ClientShort(const char *cpt_vector)
{
   const ShortVector_T *v = sstpt_Hexf(cpt_vector);

   sst_cfg.fpt_onRxShort(v->u8ar_wire[1], &v->u8ar_wire[2], v->u8ar_wire[0]);
}

static void sv_ClientBegin(const char *cpt_name)
{
   sst_cfg.fpt_onRxShort(DS_APP_TYPE_BEGIN, (const uint8_t *)cpt_name, (uint8_t)strlen(cpt_name));
   gv_SimRunThread(sv_DumpThread);
}

static void sv_ClientCommit(void)
{
   sv_ClientShort("commit");
   gv_SimRunThread(sv_DumpThread);
}

static void sv_StoreVecSegment(void)
{
   sv_Receive(su32_MakeObject(VEC_HEX_SEG_ADDR, gu8ar_vecHexSeg, VEC_HEX_SEG_LEN), 240U, eBS_OK);
   gv_SimRunThread(sv_DumpThread);
}

static void test_FileVectorsMatchDefines(void)
{
   TEST_ASSERT_EQUAL_HEX8(VEC_HEXF_APP_BEGIN, DS_APP_TYPE_BEGIN);
   TEST_ASSERT_EQUAL_HEX8(VEC_HEXF_APP_COMMIT, DS_APP_TYPE_COMMIT);
   TEST_ASSERT_EQUAL_HEX8(VEC_HEXF_APP_FILE, DS_APP_TYPE_FILE);
   TEST_ASSERT_EQUAL_STRING(VEC_HEXF_DIR, DS_FILE_DIR);
   TEST_ASSERT_EQUAL_STRING(VEC_HEXF_TEMP_NAME, DS_TEMP_NAME);
   TEST_ASSERT_EQUAL_UINT32(VEC_HEXF_NAME_MAX, DS_NAME_MAX);
   TEST_ASSERT_EQUAL_UINT32(VEC_HEXF_FILE_REPLY_LEN, DS_FILE_REPLY_LEN);
   TEST_ASSERT_EQUAL_UINT32(sizeof(gu8ar_vecHexfRecordHdr), DS_RECORD_HDR_LEN);
}

static void test_RangeTakesNoOtherTransfers(void)
{
   uint8_t u8ar_p[2] = { 0 };

   // The whole 0x10-0x1F range is the data store's, but only 0x10 carries segments
   TEST_ASSERT_NOT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(DS_APP_TYPE_BEGIN, 100U));
   TEST_ASSERT_NOT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(DS_APP_TYPE_LAST, 100U));
   sst_cfg.fpt_onRxShort(0x15U, u8ar_p, sizeof(u8ar_p));
   TEST_ASSERT_EQUAL_UINT32(0U, su32_sentCount);
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("short message 0x15 ignored"));
}

static void test_StoreUploadAsFile(void)
{
   uint32_t u32_size = 0U;
   const uint8_t *u8pt;

   sv_ClientShort("begin_fw1");
   gv_SimRunThread(sv_DumpThread);
   sv_AssertLastFile("file_begin_ok");
   TEST_ASSERT_TRUE(gb_DataStore_UploadOpen());
   TEST_ASSERT_TRUE(gb_SimFsExists(TEMP_PATH));

   // The segment: SEG line, then record, then STORED as without a file
   sv_StoreVecSegment();
   sv_AssertLastReport(su32_ReportIdx("stored_ok_from_log"));
   TEST_ASSERT_EQUAL_INT(0, atomic_get(&st_dumpBusy));

   sv_ClientCommit();
   sv_AssertLastFile("file_commit_ok");
   TEST_ASSERT_FALSE(gb_DataStore_UploadOpen());
   TEST_ASSERT_FALSE(gb_SimFsExists(TEMP_PATH));

   // The file holds one record: [u32 LE address][u32 LE length][data]
   u8pt = gu8pt_SimFsData(FW_DIR "/FW1", &u32_size);
   TEST_ASSERT_NOT_NULL(u8pt);
   TEST_ASSERT_EQUAL_UINT32(VEC_HEXF_FILE_SIZE, u32_size);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(gu8ar_vecHexfRecordHdr, u8pt, DS_RECORD_HDR_LEN);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(gu8ar_vecHexSeg, &u8pt[DS_RECORD_HDR_LEN], VEC_HEX_SEG_LEN);
   TEST_ASSERT_EQUAL_HEX32(VEC_HEXF_FILE_CRC, crc32_ieee(u8pt, u32_size));
   {
      char car_line[80];
      (void)snprintf(car_line, sizeof(car_line), "FILE %s/FW1 size=%u crc=0x%08x", FW_DIR,
         (unsigned)VEC_HEXF_FILE_SIZE, (unsigned)VEC_HEXF_FILE_CRC);
      TEST_ASSERT_NOT_NULL(gcpt_SimLogFind(car_line));
   }
   TEST_ASSERT_EQUAL_UINT32(0U, gu32_SimFsOpenHandles());
}

static void test_RecordsFollowEachOther(void)
{
   uint8_t u8ar_a[3] = { 1, 2, 3 };
   uint8_t u8ar_b[300];
   uint8_t u8ar_exp[8 + 3 + 8 + 300];
   uint32_t u32_size = 0U;
   uint32_t i;

   for (i = 0U; i < sizeof(u8ar_b); i++) { u8ar_b[i] = (uint8_t)(i + 9U); }
   sv_ClientBegin("two.bin");
   sv_Receive(su32_MakeObject(0x08000000UL, u8ar_a, sizeof(u8ar_a)), 240U, eBS_OK);
   gv_SimRunThread(sv_DumpThread);
   sv_Receive(su32_MakeObject(0x08010000UL, u8ar_b, sizeof(u8ar_b)), 240U, eBS_OK);
   gv_SimRunThread(sv_DumpThread);
   sv_ClientCommit();

   sys_put_le32(0x08000000UL, &u8ar_exp[0]);
   sys_put_le32(3U, &u8ar_exp[4]);
   (void)memcpy(&u8ar_exp[8], u8ar_a, 3U);
   sys_put_le32(0x08010000UL, &u8ar_exp[11]);
   sys_put_le32(300U, &u8ar_exp[15]);
   (void)memcpy(&u8ar_exp[19], u8ar_b, 300U);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_exp, gu8pt_SimFsData(FW_DIR "/two.bin", &u32_size), sizeof(u8ar_exp));
   TEST_ASSERT_EQUAL_UINT32(sizeof(u8ar_exp), u32_size);
}

static void test_SegmentsWithoutBeginAreNotStored(void)
{
   sv_StoreVecSegment();
   sv_AssertLastReport(su32_ReportIdx("stored_ok_from_log"));
   TEST_ASSERT_EQUAL_UINT32(0U, gu32_SimFsCalls(eSFS_WRITE));
   TEST_ASSERT_FALSE(gb_SimFsExists(FW_DIR));
}

static void test_CommitWithoutBegin(void)
{
   sv_ClientCommit();
   sv_AssertLastFile("file_commit_no_begin");
}

static void test_BeginBadNames(void)
{
   static const char *const scptar_bad[] =
   {
      "a/b", "..", ".", "upload.tmp", "UPLOAD.TMP", "sp ace", "x:y", "\\x", "\xe4",
   };
   char car_long[DS_NAME_MAX + 2U];
   uint32_t i;

   for (i = 0U; i < ARRAY_SIZE(scptar_bad); i++)
   {
      su32_sentCount = 0U;
      sv_ClientBegin(scptar_bad[i]);
      sv_AssertLastFile("file_begin_bad_name");
      TEST_ASSERT_FALSE_MESSAGE(gb_DataStore_UploadOpen(), scptar_bad[i]);
   }

   // Empty or too long: refused at once, on the SETU thread
   su32_sentCount = 0U;
   sst_cfg.fpt_onRxShort(DS_APP_TYPE_BEGIN, (const uint8_t *)"", 0U);
   sv_AssertLastFile("file_begin_bad_name");
   (void)memset(car_long, 'n', sizeof(car_long));
   car_long[DS_NAME_MAX + 1U] = '\0';
   su32_sentCount = 0U;
   sst_cfg.fpt_onRxShort(DS_APP_TYPE_BEGIN, (const uint8_t *)car_long, DS_NAME_MAX + 1U);
   sv_AssertLastFile("file_begin_bad_name");
   TEST_ASSERT_EQUAL_UINT32(0U, k_msgq_num_used_get(&sst_dsEventQ));

   // The longest name and every allowed character are fine
   car_long[DS_NAME_MAX] = '\0';
   sv_ClientBegin(car_long);
   sv_AssertLastFile("file_begin_ok");
   sv_ClientShort("begin_long_name");
   gv_SimRunThread(sv_DumpThread);
   sv_AssertLastFile("file_begin_ok");
}

static void test_BeginBusyWhileAnotherFileIsOpen(void)
{
   FileSysMessage_T st_msg;

   // The file console holds the one open file
   (void)memset(&st_msg, 0, sizeof(st_msg));
   st_msg.e_command = eFSC_OPEN_FILE_WRITE;
   st_msg.u32_sizeOfData = 5U;
   (void)memcpy(st_msg.u8_data, "c.txt", 5U);
   TEST_ASSERT_EQUAL_INT(0, gi_FSMGR_Call(&st_msg));

   sv_ClientBegin("FW1");
   sv_AssertLastFile("file_begin_busy");
   TEST_ASSERT_FALSE(gb_DataStore_UploadOpen());
   TEST_ASSERT_TRUE(gb_FSMGR_IsFileOpen());
}

static void test_SecondBeginDiscardsUnfinished(void)
{
   uint8_t u8ar_a[4] = { 0xAA, 0xAA, 0xAA, 0xAA };
   uint8_t u8ar_b[2] = { 0xBB, 0xBB };
   uint32_t u32_size = 0U;
   const uint8_t *u8pt;

   sv_ClientBegin("A");
   sv_Receive(su32_MakeObject(0x100U, u8ar_a, sizeof(u8ar_a)), 240U, eBS_OK);
   gv_SimRunThread(sv_DumpThread);

   // The link was lost before COMMIT; the client starts again
   sv_ClientBegin("B");
   sv_AssertLastFile("file_begin_ok");
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("unfinished upload A discarded"));
   sv_Receive(su32_MakeObject(0x200U, u8ar_b, sizeof(u8ar_b)), 240U, eBS_OK);
   gv_SimRunThread(sv_DumpThread);
   sv_ClientCommit();

   TEST_ASSERT_FALSE(gb_SimFsExists(FW_DIR "/A"));
   u8pt = gu8pt_SimFsData(FW_DIR "/B", &u32_size);
   TEST_ASSERT_EQUAL_UINT32(DS_RECORD_HDR_LEN + 2U, u32_size);
   TEST_ASSERT_EQUAL_HEX8(0xBB, u8pt[DS_RECORD_HDR_LEN]);
}

static void test_StaleTempFileIsReplaced(void)
{
   uint8_t u8ar_old[64];
   uint32_t u32_size = 0U;

   // A temporary file left by a lost upload (before a reset) is longer than the new one
   (void)memset(u8ar_old, 0xEE, sizeof(u8ar_old));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPutDir(FW_DIR));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(TEMP_PATH, u8ar_old, sizeof(u8ar_old)));
   sv_ClientBegin("FW1");
   sv_AssertLastFile("file_begin_ok");
   (void)gu8pt_SimFsData(TEMP_PATH, &u32_size);
   TEST_ASSERT_EQUAL_UINT32(0U, u32_size);
}

static void test_CommitReplacesOlderFile(void)
{
   uint32_t u32_size = 0U;

   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPutDir(FW_DIR));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(FW_DIR "/FW1", "old", 3U));
   sv_ClientShort("begin_fw1");
   gv_SimRunThread(sv_DumpThread);
   sv_StoreVecSegment();
   sv_ClientCommit();
   sv_AssertLastFile("file_commit_ok");
   (void)gu8pt_SimFsData(FW_DIR "/FW1", &u32_size);
   TEST_ASSERT_EQUAL_UINT32(VEC_HEXF_FILE_SIZE, u32_size);
}

static void test_WriteFailureFailsUpload(void)
{
   const SentShort_T *s;

   sv_ClientShort("begin_fw1");
   gv_SimRunThread(sv_DumpThread);

   // A flash error while writing the record: STORED says SINK_ERROR
   gv_SimFsFailOnce(eSFS_WRITE, 0U, -EIO);
   sv_StoreVecSegment();
   s = &sstar_sent[(su32_sentCount - 1U) % ARRAY_SIZE(sstar_sent)];
   TEST_ASSERT_EQUAL_HEX8(DS_APP_TYPE_STORED, s->u8_type);
   TEST_ASSERT_EQUAL_HEX8(eBS_SINK_ERROR, s->u8ar_payload[0]);
   TEST_ASSERT_EQUAL_HEX32(VEC_HEX_SEG_LEN, sys_get_le32(&s->u8ar_payload[5]));
   TEST_ASSERT_EQUAL_INT(0, atomic_get(&st_dumpBusy));

   // Later segments are not written, and also say SINK_ERROR
   sv_StoreVecSegment();
   s = &sstar_sent[(su32_sentCount - 1U) % ARRAY_SIZE(sstar_sent)];
   TEST_ASSERT_EQUAL_HEX8(eBS_SINK_ERROR, s->u8ar_payload[0]);

   // COMMIT reports the first error and leaves nothing behind
   sv_ClientCommit();
   sv_AssertLastFile("file_commit_io");
   TEST_ASSERT_FALSE(gb_SimFsExists(TEMP_PATH));
   TEST_ASSERT_FALSE(gb_SimFsExists(FW_DIR "/FW1"));
   TEST_ASSERT_FALSE(gb_DataStore_UploadOpen());
   TEST_ASSERT_FALSE(gb_FSMGR_IsFileOpen());
}

static void test_CommitRenameFailure(void)
{
   const SentShort_T *s;

   sv_ClientBegin("FW1");
   sv_StoreVecSegment();
   gv_SimFsFailOnce(eSFS_RENAME, 0U, -EIO);
   sv_ClientCommit();
   s = &sstar_sent[(su32_sentCount - 1U) % ARRAY_SIZE(sstar_sent)];
   TEST_ASSERT_EQUAL_HEX8(DS_APP_TYPE_FILE, s->u8_type);
   TEST_ASSERT_EQUAL_HEX8(DS_APP_TYPE_COMMIT, s->u8ar_payload[0]);
   TEST_ASSERT_EQUAL_UINT8(eFSS_IO, s->u8ar_payload[1]);
   TEST_ASSERT_FALSE(gb_SimFsExists(TEMP_PATH));
   TEST_ASSERT_FALSE(gb_SimFsExists(FW_DIR "/FW1"));
}

static void test_BeginOnUnmountedVolume(void)
{
   const SentShort_T *s;

   gv_SimFsReset();
   gv_SimFsFailOnce(eSFS_MKFS, 0U, -EIO);
   k_msgq_purge(&sst_fsmgrMsgQ);
   sb_mounted = false;
   gv_SimRunThread(sv_FSMGR_Thread);

   sv_ClientBegin("FW1");
   s = &sstar_sent[(su32_sentCount - 1U) % ARRAY_SIZE(sstar_sent)];
   TEST_ASSERT_EQUAL_HEX8(DS_APP_TYPE_FILE, s->u8_type);
   TEST_ASSERT_EQUAL_UINT8(eFSS_NOT_MOUNTED, s->u8ar_payload[1]);
   TEST_ASSERT_FALSE(gb_DataStore_UploadOpen());
}

static void test_EventQueueFullRefusesShort(void)
{
   uint32_t i;

   // The dump thread does not run: the queue fills, the next request is refused
   for (i = 0U; i < DS_EVENT_QUEUE_DEPTH; i++)
   {
      sv_ClientShort("begin_fw1");
   }
   TEST_ASSERT_EQUAL_UINT32(0U, su32_sentCount);
   sv_ClientShort("begin_fw1");
   sv_AssertLastFile("file_begin_busy");
   sv_ClientShort("commit");
   TEST_ASSERT_EQUAL_HEX8(DS_APP_TYPE_COMMIT,
      sstar_sent[(su32_sentCount - 1U) % ARRAY_SIZE(sstar_sent)].u8ar_payload[0]);
}

int main(void)
{
   (void)setvbuf(stdout, NULL, _IONBF, 0);
   UNITY_BEGIN();
   RUN_TEST(test_FileVectorsMatchDefines);
   RUN_TEST(test_RangeTakesNoOtherTransfers);
   RUN_TEST(test_StoreUploadAsFile);
   RUN_TEST(test_RecordsFollowEachOther);
   RUN_TEST(test_SegmentsWithoutBeginAreNotStored);
   RUN_TEST(test_CommitWithoutBegin);
   RUN_TEST(test_BeginBadNames);
   RUN_TEST(test_BeginBusyWhileAnotherFileIsOpen);
   RUN_TEST(test_SecondBeginDiscardsUnfinished);
   RUN_TEST(test_StaleTempFileIsReplaced);
   RUN_TEST(test_CommitReplacesOlderFile);
   RUN_TEST(test_WriteFailureFailsUpload);
   RUN_TEST(test_CommitRenameFailure);
   RUN_TEST(test_BeginOnUnmountedVolume);
   RUN_TEST(test_EventQueueFullRefusesShort);
   RUN_TEST(test_InitRegistersServer);
   RUN_TEST(test_StartAcceptsOnlySegmentType);
   RUN_TEST(test_StartLengthLimits);
   RUN_TEST(test_StartRejectedWhileDumping);
   RUN_TEST(test_AddressHeaderSplitAcrossChunks);
   RUN_TEST(test_DataPastBufferIsSinkError);
   RUN_TEST(test_ResultOkMatchesLog);
   RUN_TEST(test_ResultFailureCarriesZeroAddressAndLength);
   RUN_TEST(test_ReportSendFailureIsHarmless);
   RUN_TEST(test_ClientShortMessagesIgnored);
   RUN_TEST(test_StoredAfterDumpMatchesLog);
   RUN_TEST(test_StoredHighAddressFullBuffer);
#if !defined(CONFIG_DS_HEX_DUMP)
   RUN_TEST(test_SummaryLineMatchesDeviceLog);
#else
   RUN_TEST(test_HexDumpEveryByte);
   RUN_TEST(test_HexDumpPartialLastLine);
   RUN_TEST(test_HexDumpWaitsForLogBacklog);
#endif // CONFIG_DS_HEX_DUMP
   return UNITY_END();
}
