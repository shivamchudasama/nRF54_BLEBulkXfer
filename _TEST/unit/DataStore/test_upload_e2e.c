/**
 * @file          test_upload_e2e.c
 * @brief         Hex upload end to end on the host: the real BulkXfer Server
 *                engine and the real DataStore, over the simulated link in
 *                sim_link.h, with its scripted peer as the upload client.
 *                The segment is the first one of AA00000100.hex, and the
 *                expected device output and CTRL frames are the ones captured
 *                from a real upload (_LOG/). Storing the upload as a file runs on
 *                the real File System Manager over the in-memory volume of
 *                shim/fs_sim.c. Contract: _DOC/HexUpload/PROTOCOL.md.
 *
 * @date          29/09/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/* Server role only, as in the firmware (BLK_ENABLE_CLIENT=0) */
#include "BulkXfer_Core.c"
#include "BulkXfer_Server.c"
#include "BulkXfer_Client.c"
#include "sim_link.h"
#include "DataStore.c"
#include "BulkRouter.c"
#include "FileSysManager.c"
#include "FileSysManagerFSM.c"
#include "wire_vectors.h"

/* _BLK_SVC stand-in: the CTRL attribute is the one the simulated link knows */
const struct bt_gatt_attr *gstpt_BulkSvc_Init(void)
{
   return &sst_ctrlAttr;
}

/* + 1: the oversize rejection test announces one byte more than fits */
static uint8_t su8ar_obj[DS_ADDR_HDR_LEN + DS_BUF_SIZE + 1U];

static uint32_t su32_MakeObject(uint32_t u32_addr, const uint8_t *u8pt_data, uint32_t u32_len)
{
   sys_put_le32(u32_addr, su8ar_obj);
   (void)memcpy(&su8ar_obj[DS_ADDR_HDR_LEN], u8pt_data, u32_len);
   return DS_ADDR_HDR_LEN + u32_len;
}

static const ReportVector_T *sstpt_Report(const char *cpt_name)
{
   uint32_t i;

   for (i = 0U; i < ARRAY_SIZE(gstar_vecReports); i++)
   {
      if (strcmp(gstar_vecReports[i].cpt_name, cpt_name) == 0) { return &gstar_vecReports[i]; }
   }
   TEST_FAIL_MESSAGE(cpt_name);
   return NULL;
}

/** The last short message the peer received on CTRL must be this golden report. */
static void sv_AssertPeerGotReport(const char *cpt_name)
{
   const ReportVector_T *v = sstpt_Report(cpt_name);

   TEST_ASSERT_EQUAL_UINT16_MESSAGE(sizeof(v->u8ar_wire), sst_peer.u16_shortFrameLen, cpt_name);
   TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(v->u8ar_wire, sst_peer.u8ar_shortFrame,
      sizeof(v->u8ar_wire), cpt_name);
}

/** Upload one object with appType 0x10 and wait for END (or ABORT). */
static void sv_Upload(uint32_t u32_objLen)
{
   sv_PeerStartSend(DS_APP_TYPE_SEGMENT, su8ar_obj, u32_objLen, 4U, false);
   TEST_ASSERT_TRUE_MESSAGE(sb_RunUntil(sb_PeerTxDone, 60000), "no END from the server");
}

/* The dump thread's file system calls block: run the File System Manager */
static void sv_RunFsmgr(void)
{
   while (sb_ProcessNext(K_NO_WAIT)) {}
}

void setUp(void)
{
   atomic_set(&st_dumpBusy, 0);
   k_msgq_purge(&sst_dsEventQ);
   atomic_set(&st_uploadOpen, 0);
   gv_SimLogClear();

   // A formatted volume, mounted by the File System Manager as at boot
   gv_SimFsReset();
   gv_SimFsFormat(false);
   k_msgq_purge(&sst_fsmgrMsgQ);
   sb_mounted = false;
   (void)memset(&sst_FSMGRContext, 0, sizeof(sst_FSMGRContext));
   gv_SimRunThread(sv_FSMGR_Thread);
   sfpt_simBlockHook = sv_RunFsmgr;

   sv_SimConnect(247U);
}

void tearDown(void)
{
   sv_Disconnect();
}

/******************************************************************************/
/*  Tests                                                                     */
/******************************************************************************/
/** §4: END OK, RESULT at once, STORED after the dump; §6: the SEG line. */
static void test_UploadHexSegmentLikeTheRealDevice(void)
{
   uint32_t u32_len = su32_MakeObject(VEC_HEX_SEG_ADDR, gu8ar_vecHexSeg, VEC_HEX_SEG_LEN);

   // The object is what the real client sent: same length and CRC as its START
   TEST_ASSERT_EQUAL_UINT32(gstar_vecFrames[VEC_HEX_START_IDX].u32ar_f[2], u32_len);
   TEST_ASSERT_EQUAL_HEX32(gstar_vecFrames[VEC_HEX_START_IDX].u32ar_f[5],
      crc32_ieee(su8ar_obj, u32_len));

   sv_Upload(u32_len);
   TEST_ASSERT_EQUAL_INT(eBS_OK, sst_peer.i_txDone);

   // RESULT follows END right away
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerShortNotify, 100));
   sv_AssertPeerGotReport("result_ok_from_log");

   // The dump thread logs the segment, frees the buffer and sends STORED
   sst_peer.i_shortNotifies = 0;
   gv_SimRunThread(sv_DumpThread);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerShortNotify, 200));
   sv_AssertPeerGotReport("stored_ok_from_log");
   TEST_ASSERT_NOT_NULL_MESSAGE(gcpt_SimLogFind(VEC_HEX_SEG_LINE), "SEG line differs from device log");
   TEST_ASSERT_EQUAL_MEMORY(gu8ar_vecHexSeg, su8ar_segBuf, VEC_HEX_SEG_LEN);
}

/** §4.4 / §5: a segment sent before STORED is rejected; after STORED it is accepted. */
static void test_SegmentBeforeStoredIsRejected(void)
{
   uint32_t u32_len = su32_MakeObject(0x00020000UL, gu8ar_vecHexSeg, 1000U);

   sv_Upload(u32_len);
   TEST_ASSERT_EQUAL_INT(eBS_OK, sst_peer.i_txDone);

   // No dump yet: the buffer is still owned by the dump thread. The server
   // refuses with ABORT(by receiver, REJECTED) - see _DOC/BulkXfer/README.md
   sv_Upload(u32_len);
   TEST_ASSERT_EQUAL_INT(PEER_ABORT_BASE + eBS_REJECTED, sst_peer.i_txDone);

   gv_SimRunThread(sv_DumpThread);
   sv_Settle(10);
   sv_Upload(u32_len);
   TEST_ASSERT_EQUAL_INT(eBS_OK, sst_peer.i_txDone);
}

/** §5: every rejection rule, as the client sees it on the air. */
static void test_RejectionsOnTheAir(void)
{
   uint8_t u8ar_one[1] = { 0xAAU };

   // Wrong appType
   sv_PeerStartSend(0x42U, gu8ar_vecHexSeg, 100U, 4U, false);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerTxDone, 1000));
   TEST_ASSERT_EQUAL_INT(PEER_ABORT_BASE + eBS_REJECTED, sst_peer.i_txDone);

   // Address only, no data
   sv_Upload(su32_MakeObject(0x1000U, u8ar_one, 0U));
   TEST_ASSERT_EQUAL_INT(PEER_ABORT_BASE + eBS_REJECTED, sst_peer.i_txDone);

   // One byte over 4 + 65536
   sv_PeerStartSend(DS_APP_TYPE_SEGMENT, su8ar_obj, DS_ADDR_HDR_LEN + DS_BUF_SIZE + 1U, 4U, false);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerTxDone, 1000));
   TEST_ASSERT_EQUAL_INT(PEER_ABORT_BASE + eBS_REJECTED, sst_peer.i_txDone);

   // Smallest valid segment still works afterwards
   sv_Upload(su32_MakeObject(0x1000U, u8ar_one, 1U));
   TEST_ASSERT_EQUAL_INT(eBS_OK, sst_peer.i_txDone);
}

/** §3: a corrupted transfer ends CRC_ERROR, RESULT carries 0/0, nothing is dumped. */
static void test_CrcErrorIsDiscarded(void)
{
   uint32_t u32_len = su32_MakeObject(0x3000U, gu8ar_vecHexSeg, 500U);

   sv_PeerStartSend(DS_APP_TYPE_SEGMENT, su8ar_obj, u32_len, 4U, true);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerTxDone, 60000));
   TEST_ASSERT_EQUAL_INT(eBS_CRC_ERROR, sst_peer.i_txDone);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerShortNotify, 100));
   sv_AssertPeerGotReport("result_crc_error");
   TEST_ASSERT_EQUAL_INT(0, atomic_get(&st_dumpBusy));
}

/** A full 64 KiB segment at a high address, as the client splits them. */
static void test_FullSizeSegment(void)
{
   static uint8_t su8ar_big[DS_BUF_SIZE];
   uint32_t i;

   for (i = 0U; i < DS_BUF_SIZE; i++) { su8ar_big[i] = (uint8_t)(i ^ (i >> 8)); }
   sv_Upload(su32_MakeObject(0xF0100000UL, su8ar_big, DS_BUF_SIZE));
   TEST_ASSERT_EQUAL_INT(eBS_OK, sst_peer.i_txDone);
   sst_peer.i_shortNotifies = 0;
   gv_SimRunThread(sv_DumpThread);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerShortNotify, 200));
   sv_AssertPeerGotReport("stored_high_address");
   TEST_ASSERT_EQUAL_MEMORY(su8ar_big, su8ar_segBuf, DS_BUF_SIZE);
}

/** The client sends one of the golden hex_file short frames. */
static void sv_PeerSendHexf(const char *cpt_name)
{
   uint32_t i;

   for (i = 0U; i < ARRAY_SIZE(gstar_vecHexfShorts); i++)
   {
      if (strcmp(gstar_vecHexfShorts[i].cpt_name, cpt_name) == 0)
      {
         sv_PeerWrite(gstar_vecHexfShorts[i].u8ar_wire, gstar_vecHexfShorts[i].u8_wireLen);
         return;
      }
   }
   TEST_FAIL_MESSAGE(cpt_name);
}

/** The last short message the peer received must be this golden hex_file frame. */
static void sv_AssertPeerGotHexf(const char *cpt_name)
{
   uint32_t i;

   for (i = 0U; i < ARRAY_SIZE(gstar_vecHexfShorts); i++)
   {
      if (strcmp(gstar_vecHexfShorts[i].cpt_name, cpt_name) == 0)
      {
         TEST_ASSERT_EQUAL_UINT16_MESSAGE(gstar_vecHexfShorts[i].u8_wireLen,
            sst_peer.u16_shortFrameLen, cpt_name);
         TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(gstar_vecHexfShorts[i].u8ar_wire,
            sst_peer.u8ar_shortFrame, gstar_vecHexfShorts[i].u8_wireLen, cpt_name);
         return;
      }
   }
   TEST_FAIL_MESSAGE(cpt_name);
}

/** §7: BEGIN, the segment, COMMIT on the air; the file holds the segment's record. */
static void test_StoreUploadAsFileOnTheAir(void)
{
   uint32_t u32_size = 0U;
   const uint8_t *u8pt;

   sst_peer.i_shortNotifies = 0;
   sv_PeerSendHexf("begin_fw1");
   sv_Settle(5);                              /* the engine hands BEGIN to the data store */
   gv_SimRunThread(sv_DumpThread);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerShortNotify, 200));
   sv_AssertPeerGotHexf("file_begin_ok");

   sv_Upload(su32_MakeObject(VEC_HEX_SEG_ADDR, gu8ar_vecHexSeg, VEC_HEX_SEG_LEN));
   TEST_ASSERT_EQUAL_INT(eBS_OK, sst_peer.i_txDone);
   sst_peer.i_shortNotifies = 0;
   gv_SimRunThread(sv_DumpThread);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerShortNotify, 200));
   sv_AssertPeerGotReport("stored_ok_from_log");

   sst_peer.i_shortNotifies = 0;
   sv_PeerSendHexf("commit");
   sv_Settle(5);
   gv_SimRunThread(sv_DumpThread);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerShortNotify, 200));
   sv_AssertPeerGotHexf("file_commit_ok");

   u8pt = gu8pt_SimFsData(VEC_HEXF_DIR "/FW1", &u32_size);
   TEST_ASSERT_NOT_NULL(u8pt);
   TEST_ASSERT_EQUAL_UINT32(VEC_HEXF_FILE_SIZE, u32_size);
   TEST_ASSERT_EQUAL_HEX32(VEC_HEXF_FILE_CRC, crc32_ieee(u8pt, u32_size));
}

int main(int argc, char **argv)
{
   (void)setvbuf(stdout, NULL, _IONBF, 0);
   gb_simVerbose = (argc > 1) && (strcmp(argv[1], "-v") == 0);

   // As main() does in the firmware, before any connection
   if ((gi_DataStore_Init() != 0) || (gi_BulkRouter_Start() != 0))
   {
      printf("gi_DataStore_Init / gi_BulkRouter_Start failed\n");
      return 1;
   }

   UNITY_BEGIN();
   RUN_TEST(test_UploadHexSegmentLikeTheRealDevice);
   RUN_TEST(test_SegmentBeforeStoredIsRejected);
   RUN_TEST(test_RejectionsOnTheAir);
   RUN_TEST(test_CrcErrorIsDiscarded);
   RUN_TEST(test_FullSizeSegment);
   RUN_TEST(test_StoreUploadAsFileOnTheAir);
   return UNITY_END();
}
