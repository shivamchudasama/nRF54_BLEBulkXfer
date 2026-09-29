/**
 * @file          test_engine.c
 * @brief         Host tests of the real BulkXfer engine (Core + Server +
 *                Client) against the simulated BLE link and scripted peer in
 *                sim_link.h. Built three times by _TEST/CMakeLists.txt to prove
 *                each role builds and works alone:
 *
 *                  bulkxfer_engine_both    both roles
 *                  bulkxfer_engine_server  -DBLK_ENABLE_CLIENT=0
 *                  bulkxfer_engine_client  -DBLK_ENABLE_SERVER=0
 *
 *                Pass -v to print the engine's log lines.
 *
 * @date          24/09/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

/* The engine sources are included (found through -I _LIB/BulkXfer) so the
   tests can drive and inspect their private state */
#include "BulkXfer_Core.c"
#include "BulkXfer_Server.c"
#include "BulkXfer_Client.c"
#include "sim_link.h"

/******************************************************************************/
/*  Device-side application (callbacks registered with BulkXfer)              */
/******************************************************************************/
static uint8_t su8ar_devRx[MAX_OBJ];
static uint32_t su32_devRxLen = 0U;
static bool sb_devRxOrderError = false;
static int si_devRxDone = STATUS_NONE;
static uint8_t su8_devRxType = 0U;
static int si_devTxDone = STATUS_NONE;
static uint32_t su32_devRejectAbove = MAX_OBJ;
static int si_devRxStartCalls = 0;
static int si_devSrvShortCount = 0;          /* client -> device server shorts */
static int si_devCliShortCount = 0;          /* server -> device client shorts */
static uint8_t su8_devShortType = 0U;
static uint8_t su8_devShortLen = 0U;
static int si_devReady = STATUS_NONE;
static int si_devReadyCalls = 0;

TEST_HELPER int si_DevRxStart(uint8_t t, uint32_t n)
{
   (void)t;
   si_devRxStartCalls++;
   su32_devRxLen = 0U;
   return (n > su32_devRejectAbove) ? -ENOMEM : 0;
}

TEST_HELPER int si_DevRxData(uint8_t t, uint32_t off, const uint8_t *d, uint16_t n)
{
   (void)t;
   // Chunks must arrive contiguous and exactly once
   if (off != su32_devRxLen) { sb_devRxOrderError = true; }
   (void)memcpy(&su8ar_devRx[off], d, n);
   su32_devRxLen = off + n;
   return 0;
}

TEST_HELPER void sv_DevRxDone(uint8_t t, BlkStatus_E s, uint32_t n)
{
   (void)n;
   su8_devRxType = t;
   si_devRxDone = (int)s;
}

TEST_HELPER void sv_DevTxDone(uint8_t t, BlkStatus_E s)
{
   (void)t;
   si_devTxDone = (int)s;
}

TEST_HELPER void sv_DevSrvShort(uint8_t t, const uint8_t *d, uint8_t n)
{
   (void)d;
   si_devSrvShortCount++;
   su8_devShortType = t;
   su8_devShortLen = n;
}

TEST_HELPER void sv_DevCliShort(uint8_t t, const uint8_t *d, uint8_t n)
{
   (void)d;
   si_devCliShortCount++;
   su8_devShortType = t;
   su8_devShortLen = n;
}

TEST_HELPER void sv_DevReady(struct bt_conn *c, int i_status)
{
   TEST_ASSERT_EQUAL_PTR(&sst_conn, c);
   si_devReadyCalls++;
   si_devReady = i_status;
}

TEST_HELPER bool sb_DevTxDone(void) { return si_devTxDone != STATUS_NONE; }
TEST_HELPER bool sb_DevRxDone(void) { return si_devRxDone != STATUS_NONE; }
TEST_HELPER bool sb_BothDone(void) { return sb_DevTxDone() && sb_DevRxDone(); }
TEST_HELPER bool sb_DevSrvShort(void) { return si_devSrvShortCount > 0; }
TEST_HELPER bool sb_DevCliShort(void) { return si_devCliShortCount > 0; }
TEST_HELPER bool sb_DevReady(void) { return si_devReadyCalls > 0; }

/******************************************************************************/
/*  Fixtures                                                                  */
/******************************************************************************/
static uint8_t su8ar_pattern[MAX_OBJ];

/** Connect; bind the Server and (if b_attach) attach the Client. */
static void sv_ConnectEx(uint16_t u16_mtu, bool b_attach)
{
   si_devTxDone = STATUS_NONE;
   si_devRxDone = STATUS_NONE;
   su32_devRxLen = 0U;
   sb_devRxOrderError = false;
   si_devSrvShortCount = 0;
   si_devCliShortCount = 0;
   su32_devRejectAbove = MAX_OBJ;
   si_devReady = STATUS_NONE;
   si_devReadyCalls = 0;
   sv_SimConnect(u16_mtu);
#if BLK_ENABLE_CLIENT
   if (b_attach)
   {
      TEST_ASSERT_EQUAL_INT(0, gi_BLKC_Attach(&sst_conn));
      TEST_ASSERT_TRUE_MESSAGE(sb_RunUntil(sb_DevReady, 1000), "attach did not finish");
      TEST_ASSERT_EQUAL_INT(0, si_devReady);
      TEST_ASSERT_TRUE(gb_BLKC_IsReady());
   }
#else
   (void)b_attach;
#endif // BLK_ENABLE_CLIENT
}

static void sv_Connect(uint16_t u16_mtu)
{
   sv_ConnectEx(u16_mtu, true);
}

void setUp(void)
{
   // Undo peer-database faults a failed test may have left behind
   sb_dbHasService = true;
   sb_dbHasCtrl = true;
   sb_subscribeFails = false;
#if BLK_ENABLE_CLIENT
   sst_BLKC_cfg.b_autoTuneLink = false;
#endif // BLK_ENABLE_CLIENT
}

void tearDown(void)
{
   sv_Disconnect();
}

/******************************************************************************/
/*  Initialisation (runs first; every other test depends on it)               */
/******************************************************************************/
static void test_Init(void)
{
#if BLK_ENABLE_SERVER
   {
      BlkSrvCfg_T st_srv = { 0 };

      TEST_ASSERT_EQUAL_INT(-EINVAL, gi_BLKS_Init(&st_srv));
      st_srv.stpt_ctrlAttr = &sst_ctrlAttr;
      st_srv.fpt_onRxStart = si_DevRxStart;
      st_srv.fpt_onRxData = si_DevRxData;
      st_srv.fpt_onRxDone = sv_DevRxDone;
      st_srv.fpt_onRxShort = sv_DevSrvShort;
      TEST_ASSERT_EQUAL_INT(0, gi_BLKS_Init(&st_srv));
      TEST_ASSERT_EQUAL_INT(-EALREADY, gi_BLKS_Init(&st_srv));
   }
#endif // BLK_ENABLE_SERVER
#if BLK_ENABLE_CLIENT
   {
      BlkCliCfg_T st_cli = { 0 };

      TEST_ASSERT_EQUAL_INT(-EPERM, gi_BLKC_Attach(&sst_conn));
      st_cli.fpt_onReady = sv_DevReady;
      st_cli.fpt_onTxDone = sv_DevTxDone;
      st_cli.fpt_onRxShort = sv_DevCliShort;
      TEST_ASSERT_EQUAL_INT(0, gi_BLKC_Init(&st_cli));
      TEST_ASSERT_EQUAL_INT(-EALREADY, gi_BLKC_Init(&st_cli));
   }
#endif // BLK_ENABLE_CLIENT
}

/******************************************************************************/
/*  Client (device -> peer) tests                                             */
/******************************************************************************/
#if BLK_ENABLE_CLIENT
/** Client -> peer server, assorted sizes (MTU 247). */
static void test_ClientSizes(void)
{
   static const uint32_t su32ar_sizes[] = { 0U, 1U, 239U, 240U, 241U, 480U, 100000U };
   char car_msg[32];
   uint32_t i;

   for (i = 0U; i < ARRAY_SIZE(su32ar_sizes); i++)
   {
      (void)snprintf(car_msg, sizeof(car_msg), "size %u", (unsigned)su32ar_sizes[i]);
      sv_Connect(247U);
      su32_maxWrites = 0U;
      TEST_ASSERT_EQUAL_INT_MESSAGE(0, gi_BLKC_SendBuffer(0x10U, su8ar_pattern, su32ar_sizes[i]),
         car_msg);
      TEST_ASSERT_TRUE_MESSAGE(sb_RunUntil(sb_DevTxDone, 60000), car_msg);
      TEST_ASSERT_EQUAL_INT_MESSAGE(eBS_OK, si_devTxDone, car_msg);
      TEST_ASSERT_EQUAL_INT_MESSAGE(eBS_OK, sst_peer.i_rxDone, car_msg);
      TEST_ASSERT_EQUAL_UINT32_MESSAGE(su32ar_sizes[i], sst_peer.u32_rxTotal, car_msg);
      if (su32ar_sizes[i] > 0U)
      {
         TEST_ASSERT_EQUAL_MEMORY_MESSAGE(su8ar_pattern, sst_peer.u8ar_rx, su32ar_sizes[i], car_msg);
      }
      TEST_ASSERT_TRUE_MESSAGE(su32_maxWrites <= BLK_CLI_WRITE_INFLIGHT_MAX, car_msg);
      TEST_ASSERT_TRUE_MESSAGE(sst_peer.u32_rxMaxAhead <= BLK_WINDOW_DEFAULT, car_msg);
      TEST_ASSERT_EQUAL_UINT_MESSAGE(BLK_CLI_WRITE_INFLIGHT_MAX, sst_BLKC_credits.count, car_msg);
      sv_Disconnect();
   }
}

/** Client -> peer server, peer drops frame 300 (after seq wrap) -> NACK. */
static void test_ClientLoss(void)
{
   int64_t i64_start;

   sv_Connect(247U);
   sst_peer.i32_dropAbsOnce = 300;
   i64_start = gi64_simNowMs;
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_SendBuffer(0x11U, su8ar_pattern, 100000U));
   TEST_ASSERT_TRUE(sb_RunUntil(sb_DevTxDone, 60000));
   // Recovery must come from the NACK, not from the ACK timeout
   TEST_ASSERT_TRUE_MESSAGE((gi64_simNowMs - i64_start) < (int64_t)BLK_TX_ACK_TIMEOUT_MS,
      "recovered by timeout, not by NACK");
   TEST_ASSERT_EQUAL_INT(eBS_OK, si_devTxDone);
   TEST_ASSERT_EQUAL_UINT32(1U, sst_peer.u32_nacksSent);
   TEST_ASSERT_EQUAL_MEMORY(su8ar_pattern, sst_peer.u8ar_rx, 100000U);
}

/** Client -> peer server, peer never answers -> TIMEOUT. */
static void test_ClientTimeout(void)
{
   int64_t i64_start;

   sv_Connect(247U);
   sst_peer.b_silent = true;
   i64_start = gi64_simNowMs;
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_SendBuffer(0x12U, su8ar_pattern, 5000U));
   TEST_ASSERT_TRUE(sb_RunUntil(sb_DevTxDone, 60000));
   TEST_ASSERT_EQUAL_INT(eBS_TIMEOUT, si_devTxDone);
   TEST_ASSERT_TRUE((gi64_simNowMs - i64_start) >= (int64_t)(BLK_TX_ACK_TIMEOUT_MS * BLK_TX_MAX_RETRIES));
   TEST_ASSERT_FALSE(gb_BLKC_IsTxBusy());
}

/** Peer server ACKs only when the client goes quiet -> client stops at the window. */
static void test_ClientWindowStall(void)
{
   sv_Connect(247U);
   sst_peer.b_ackOnlyWhenIdle = true;
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_SendBuffer(0x40U, su8ar_pattern, 50000U));
   TEST_ASSERT_TRUE(sb_RunUntil(sb_DevTxDone, 60000));
   TEST_ASSERT_EQUAL_INT(eBS_OK, si_devTxDone);
   TEST_ASSERT_EQUAL_UINT32(BLK_WINDOW_DEFAULT, sst_peer.u32_rxMaxAhead);
   TEST_ASSERT_EQUAL_MEMORY(su8ar_pattern, sst_peer.u8ar_rx, 50000U);
}

/** Client, MTU 23 (16-byte chunks, ~6250 frames, many seq wraps). */
static void test_ClientSmallMtu(void)
{
   sv_Connect(23U);
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_SendBuffer(0x21U, su8ar_pattern, 100000U));
   TEST_ASSERT_TRUE(sb_RunUntil(sb_DevTxDone, 120000));
   TEST_ASSERT_EQUAL_INT(eBS_OK, si_devTxDone);
   TEST_ASSERT_EQUAL_MEMORY(su8ar_pattern, sst_peer.u8ar_rx, 100000U);
   TEST_ASSERT_EQUAL_UINT16(18U, gu16_BLKC_GetMaxShortPayload());
   TEST_ASSERT_EQUAL_INT(-EMSGSIZE, gi_BLKC_SendShort(0x05U, su8ar_pattern, 19U, K_MSEC(10)));
}

/** Client: local abort, disconnect mid-transfer, API preconditions. */
static void test_ClientAbortAndDisconnect(void)
{
   // Local abort: device reports ABORTED, peer server gets ABORT(by sender)
   sv_Connect(247U);
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_SendBuffer(0x30U, su8ar_pattern, 100000U));
   TEST_ASSERT_EQUAL_INT(-EBUSY, gi_BLKC_SendBuffer(0x30U, su8ar_pattern, 10U));
   sv_Settle(20);
   gv_BLKC_AbortTx();
   TEST_ASSERT_TRUE(sb_RunUntil(sb_DevTxDone, 1000));
   TEST_ASSERT_EQUAL_INT(eBS_ABORTED, si_devTxDone);
   sv_Settle(10);
   TEST_ASSERT_EQUAL_INT(PEER_RX_ABORT_BASE + eBS_ABORTED, sst_peer.i_rxDone);
   sv_Disconnect();

   // Disconnect while writes are queued in the host: their completion
   // callbacks never run, so the engine must restore the credits itself
   sv_Connect(247U);
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_SendBuffer(0x33U, su8ar_pattern, 100000U));
   sst_BLK_wakeSem.count = 0U;
   sv_EngineRunOnce();               /* START                              */
   sv_LinkEvent();                   /* peer ACKs START                    */
   sst_BLK_wakeSem.count = 0U;
   sv_EngineRunOnce();               /* DATA burst takes the credits       */
   TEST_ASSERT_TRUE(su32_linkCount > 0U);
   TEST_ASSERT_TRUE(sst_BLKC_credits.count < BLK_CLI_WRITE_INFLIGHT_MAX);
   TEST_ASSERT_TRUE(gb_BLKC_IsTxBusy());
   sv_Disconnect();
   TEST_ASSERT_EQUAL_INT(eBS_DISCONNECTED, si_devTxDone);
   TEST_ASSERT_FALSE(gb_BLKC_IsTxBusy());
   TEST_ASSERT_FALSE(gb_BLKC_IsReady());
   TEST_ASSERT_EQUAL_UINT(BLK_CLI_WRITE_INFLIGHT_MAX, sst_BLKC_credits.count);

   // Not attached / attach not finished / double attach
   TEST_ASSERT_EQUAL_INT(-ENOTCONN, gi_BLKC_SendBuffer(0x31U, su8ar_pattern, 10U));
   TEST_ASSERT_EQUAL_INT(-ENOTCONN, gi_BLKC_SendShort(0x31U, su8ar_pattern, 1U, K_NO_WAIT));
   sv_ConnectEx(247U, false);
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_Attach(&sst_conn));
   TEST_ASSERT_EQUAL_INT(-EALREADY, gi_BLKC_Attach(&sst_conn));
   TEST_ASSERT_EQUAL_INT(-EAGAIN, gi_BLKC_SendBuffer(0x31U, su8ar_pattern, 10U));
   TEST_ASSERT_EQUAL_UINT16(0U, gu16_BLKC_GetMaxShortPayload());
   TEST_ASSERT_TRUE(sb_RunUntil(sb_DevReady, 1000));
   TEST_ASSERT_EQUAL_INT(0, si_devReady);

   // Reattached link works normally again
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_SendBuffer(0x32U, su8ar_pattern, 20000U));
   TEST_ASSERT_TRUE(sb_RunUntil(sb_DevTxDone, 60000));
   TEST_ASSERT_EQUAL_INT(eBS_OK, si_devTxDone);
}

/** Client attach: missing service / CTRL, subscribe failure, disconnect, MTU. */
static void test_ClientAttach(void)
{
   uint8_t u8ar_f[BLK_CTRL_FRAME_MAX_LEN];

   sb_dbHasService = false;
   sv_ConnectEx(247U, false);
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_Attach(&sst_conn));
   TEST_ASSERT_TRUE(sb_RunUntil(sb_DevReady, 1000));
   TEST_ASSERT_EQUAL_INT(-ENOENT, si_devReady);
   TEST_ASSERT_FALSE(gb_BLKC_IsReady());
   sb_dbHasService = true;
   sv_Disconnect();

   sb_dbHasCtrl = false;
   sv_ConnectEx(247U, false);
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_Attach(&sst_conn));
   TEST_ASSERT_TRUE(sb_RunUntil(sb_DevReady, 1000));
   TEST_ASSERT_EQUAL_INT(-ENOENT, si_devReady);
   sb_dbHasCtrl = true;
   // A failed attach releases the binding: attaching again works
   si_devReadyCalls = 0;
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_Attach(&sst_conn));
   TEST_ASSERT_TRUE(sb_RunUntil(sb_DevReady, 1000));
   TEST_ASSERT_EQUAL_INT(0, si_devReady);
   sv_Disconnect();

   sb_subscribeFails = true;
   sv_ConnectEx(247U, false);
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_Attach(&sst_conn));
   TEST_ASSERT_TRUE(sb_RunUntil(sb_DevReady, 1000));
   TEST_ASSERT_EQUAL_INT(-EIO, si_devReady);
   sb_subscribeFails = false;
   sv_Disconnect();

   // Link lost during discovery: exactly one fpt_onReady(-ENOTCONN)
   sv_ConnectEx(247U, false);
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_Attach(&sst_conn));
   sv_Disconnect();
   sv_Settle(10);
   TEST_ASSERT_EQUAL_INT(1, si_devReadyCalls);
   TEST_ASSERT_EQUAL_INT(-ENOTCONN, si_devReady);

   // b_autoTuneLink: MTU exchange first, discovery from its callback
   sst_BLKC_cfg.b_autoTuneLink = true;
   si_mtuExchanges = 0;
   sv_Connect(247U);
   TEST_ASSERT_EQUAL_INT(1, si_mtuExchanges);
   sst_BLKC_cfg.b_autoTuneLink = false;

   // Sender frames notified on CTRL are not valid there and are dropped
   (void)gu16_BLK_EncodeStart(u8ar_f, sizeof(u8ar_f), 7U, 0x42U, 1000U, 240U, 16U, 0U);
   sv_PeerNotify(u8ar_f, BLK_CTRL_FRAME_MAX_LEN);
   sv_Settle(10);
   TEST_ASSERT_EQUAL_INT(STATUS_NONE, si_devTxDone);
   TEST_ASSERT_EQUAL_UINT32(0U, sst_BLKC_slab.used);
}
#endif // BLK_ENABLE_CLIENT

/******************************************************************************/
/*  Server (peer -> device) tests                                             */
/******************************************************************************/
#if BLK_ENABLE_SERVER
/** Peer client -> server, assorted sizes, burst 4. */
static void test_ServerSizes(void)
{
   static const uint32_t su32ar_sizes[] = { 0U, 1U, 240U, 241U, 100000U };
   char car_msg[32];
   uint32_t i;

   for (i = 0U; i < ARRAY_SIZE(su32ar_sizes); i++)
   {
      (void)snprintf(car_msg, sizeof(car_msg), "size %u", (unsigned)su32ar_sizes[i]);
      sv_Connect(247U);
      sv_PeerStartSend(0x42U, su8ar_pattern, su32ar_sizes[i], 4U, false);
      TEST_ASSERT_TRUE_MESSAGE(sb_RunUntil(sb_PeerTxDone, 60000), car_msg);
      TEST_ASSERT_EQUAL_INT_MESSAGE(eBS_OK, sst_peer.i_txDone, car_msg);
      TEST_ASSERT_EQUAL_INT_MESSAGE(eBS_OK, si_devRxDone, car_msg);
      TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x42U, su8_devRxType, car_msg);
      TEST_ASSERT_EQUAL_UINT32_MESSAGE(su32ar_sizes[i], su32_devRxLen, car_msg);
      TEST_ASSERT_FALSE_MESSAGE(sb_devRxOrderError, car_msg);
      if (su32ar_sizes[i] > 0U)
      {
         TEST_ASSERT_EQUAL_MEMORY_MESSAGE(su8ar_pattern, su8ar_devRx, su32ar_sizes[i], car_msg);
      }
      TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, sst_BLKS_slab.used, car_msg);
      TEST_ASSERT_EQUAL_UINT_MESSAGE(BLK_SRV_NOTIFY_INFLIGHT_MAX, sst_BLKS_credits.count, car_msg);
      sv_Disconnect();
   }
}

/** Peer client -> server, bursts of 16 into a 6-deep RX pool -> overflow NACKs. */
static void test_ServerOverflow(void)
{
   int64_t i64_start;

   sv_Connect(247U);
   i64_start = gi64_simNowMs;
   sv_PeerStartSend(0x42U, su8ar_pattern, 100000U, 16U, false);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerTxDone, 120000));
   // Recovery must come from NACKs, not from the peer's 1500 ms timeout
   TEST_ASSERT_TRUE_MESSAGE((gi64_simNowMs - i64_start) < 1500, "recovered by timeout, not by NACK");
   TEST_ASSERT_EQUAL_INT(eBS_OK, sst_peer.i_txDone);
   TEST_ASSERT_EQUAL_INT(eBS_OK, si_devRxDone);
   TEST_ASSERT_TRUE(sst_peer.u32_nacksRcvd > 0U);
   TEST_ASSERT_FALSE(sb_devRxOrderError);
   TEST_ASSERT_EQUAL_MEMORY(su8ar_pattern, su8ar_devRx, 100000U);
}

/** Server: bad CRC -> CRC_ERROR; oversize -> REJECTED; unsubscribed -> ignored. */
static void test_ServerCrcAndReject(void)
{
   sv_Connect(247U);
   sv_PeerStartSend(0x42U, su8ar_pattern, 3000U, 4U, true);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerTxDone, 60000));
   TEST_ASSERT_EQUAL_INT(eBS_CRC_ERROR, sst_peer.i_txDone);
   TEST_ASSERT_EQUAL_INT(eBS_CRC_ERROR, si_devRxDone);

   si_devRxDone = STATUS_NONE;
   su32_devRejectAbove = 1000U;
   sv_PeerStartSend(0x42U, su8ar_pattern, 3000U, 4U, false);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerTxDone, 60000));
   TEST_ASSERT_EQUAL_INT(PEER_ABORT_BASE + eBS_REJECTED, sst_peer.i_txDone);
   TEST_ASSERT_EQUAL_INT(STATUS_NONE, si_devRxDone);

   // START from a client that cannot hear CTRL is not even offered to the app
   su32_devRejectAbove = MAX_OBJ;
   si_devRxStartCalls = 0;
   sb_peerSubscribed = false;
   sv_PeerStartSend(0x42U, su8ar_pattern, 3000U, 4U, false);
   sv_Settle(20);
   TEST_ASSERT_EQUAL_INT(0, si_devRxStartCalls);
   TEST_ASSERT_FALSE(gb_BLKS_IsRxBusy());
   TEST_ASSERT_EQUAL_INT(STATUS_NONE, sst_peer.i_txDone);
}

/** Server: local abort, disconnect mid-transfer, stale frames, wrong channel. */
static void test_ServerAbortAndStale(void)
{
   uint8_t u8ar_f[BLK_CTRL_FRAME_MAX_LEN];

   // Local abort of RX: peer client gets ABORT(by receiver)
   sv_Connect(247U);
   sv_PeerStartSend(0x42U, su8ar_pattern, 100000U, 4U, false);
   sv_Settle(20);
   TEST_ASSERT_TRUE(gb_BLKS_IsRxBusy());
   gv_BLKS_AbortRx();
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerTxDone, 1000));
   TEST_ASSERT_EQUAL_INT(PEER_ABORT_BASE + eBS_ABORTED, sst_peer.i_txDone);
   TEST_ASSERT_EQUAL_INT(eBS_ABORTED, si_devRxDone);
   sv_Disconnect();

   // Disconnect mid-transfer
   sv_Connect(247U);
   sv_PeerStartSend(0x42U, su8ar_pattern, 100000U, 4U, false);
   sv_Settle(30);
   sv_Disconnect();
   TEST_ASSERT_EQUAL_INT(eBS_DISCONNECTED, si_devRxDone);
   TEST_ASSERT_FALSE(gb_BLKS_IsRxBusy());
   TEST_ASSERT_EQUAL_UINT(BLK_SRV_NOTIFY_INFLIGHT_MAX, sst_BLKS_credits.count);

   // Frames queued before a disconnect are discarded after reconnect
   sv_Connect(247U);
   si_devRxStartCalls = 0;
   (void)gu16_BLK_EncodeStart(u8ar_f, sizeof(u8ar_f), 99U, 0x42U, 1000U, 240U, 16U, 0U);
   sv_PeerWrite(u8ar_f, BLK_CTRL_FRAME_MAX_LEN);    /* queued, engine not run */
   sb_connected = false;
   gv_BLK_OnDisconnected(&sst_conn);
   sb_connected = true;
   gv_BLKS_OnConnected(&sst_conn);
   sv_Settle(20);
   TEST_ASSERT_EQUAL_INT(0, si_devRxStartCalls);
   TEST_ASSERT_FALSE(sst_BLKS_session.b_active);
   TEST_ASSERT_EQUAL_UINT32(0U, sst_BLKS_slab.used);

   // Receiver frames written to DATA are not valid there and are dropped
   u8ar_f[0] = 3U; u8ar_f[1] = eBFT_ACK; u8ar_f[2] = 1U; u8ar_f[3] = 0U; u8ar_f[4] = 16U;
   sv_PeerWrite(u8ar_f, 5U);
   sv_Settle(10);
   TEST_ASSERT_EQUAL_UINT32(0U, sst_BLKS_slab.used);
}

/** Server: DATA chunk shorter than announced -> PROTOCOL_ERROR; bad hook writes. */
static void test_ServerMalformedData(void)
{
   uint8_t u8ar_f[BLK_MAX_FRAME_LEN];

   sv_Connect(247U);
   sv_PeerStartSend(0x42U, su8ar_pattern, 1000U, 0U, false);  /* burst 0: manual */
   sv_Settle(5);
   TEST_ASSERT_TRUE(sst_peer.b_txStarted);
   (void)gu16_BLK_EncodeDataHeader(u8ar_f, sizeof(u8ar_f), sst_peer.u8_txId, 0U, 10U);
   sv_PeerWrite(u8ar_f, BLK_DATA_HDR_LEN + 10U);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerTxDone, 1000));
   TEST_ASSERT_EQUAL_INT(PEER_ABORT_BASE + eBS_PROTOCOL_ERROR, sst_peer.i_txDone);
   TEST_ASSERT_EQUAL_INT(eBS_PROTOCOL_ERROR, si_devRxDone);

   // Malformed writes are rejected by the hook with an ATT error
   u8ar_f[0] = 50U;
   TEST_ASSERT_EQUAL_INT(BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN),
      gt_BLKS_DataWriteHook(&sst_conn, &sst_dataAttr, u8ar_f, 10U, 0U, 0U));
   TEST_ASSERT_EQUAL_INT(BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET),
      gt_BLKS_DataWriteHook(&sst_conn, &sst_dataAttr, u8ar_f, 10U, 5U, 0U));
   TEST_ASSERT_EQUAL_INT(BT_GATT_ERR(BT_ATT_ERR_WRITE_NOT_PERMITTED),
      gt_BLKS_DataWriteHook(&sst_conn, &sst_dataAttr, u8ar_f, 10U, 0U, BT_GATT_WRITE_FLAG_PREPARE));
}
#endif // BLK_ENABLE_SERVER

/******************************************************************************/
/*  Both roles                                                                */
/******************************************************************************/
/** Short messages on both channels, size limits. */
static void test_ShortMessages(void)
{
   uint8_t u8ar_f[BLK_MAX_FRAME_LEN];
   uint16_t u16_len;

   sv_Connect(247U);
   (void)u8ar_f; (void)u16_len;

#if BLK_ENABLE_CLIENT
   // Device client -> peer server: Write Without Response on DATA
   TEST_ASSERT_EQUAL_UINT16(BLK_MAX_SHORT_PAYLOAD, gu16_BLKC_GetMaxShortPayload());
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_SendShort(0x05U, su8ar_pattern, 242U, K_MSEC(10)));
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerShortWrite, 1000));
   TEST_ASSERT_EQUAL_HEX8(0x05U, sst_peer.u8_shortType);
   TEST_ASSERT_EQUAL_UINT8(242U, sst_peer.u8_shortLen);
   TEST_ASSERT_EQUAL_MEMORY(su8ar_pattern, sst_peer.u8ar_short, 242U);
   TEST_ASSERT_EQUAL_INT(-EMSGSIZE, gi_BLKC_SendShort(0x05U, su8ar_pattern, 243U, K_MSEC(10)));
   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_BLKC_SendShort(eBFT_START, su8ar_pattern, 1U, K_MSEC(10)));

   // Peer server -> device client: CTRL notification
   u16_len = gu16_BLK_EncodeShort(u8ar_f, sizeof(u8ar_f), 0x08U, su8ar_pattern, 60U);
   sv_PeerNotify(u8ar_f, u16_len);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_DevCliShort, 1000));
   TEST_ASSERT_EQUAL_HEX8(0x08U, su8_devShortType);
   TEST_ASSERT_EQUAL_UINT8(60U, su8_devShortLen);
#endif // BLK_ENABLE_CLIENT

#if BLK_ENABLE_SERVER
   // Device server -> peer client: CTRL notification
   TEST_ASSERT_EQUAL_UINT16(BLK_MAX_SHORT_PAYLOAD, gu16_BLKS_GetMaxShortPayload());
   TEST_ASSERT_EQUAL_INT(0, gi_BLKS_SendShort(0x06U, su8ar_pattern, 100U, K_MSEC(10)));
   TEST_ASSERT_TRUE(sb_RunUntil(sb_PeerShortNotify, 1000));
   TEST_ASSERT_EQUAL_HEX8(0x06U, sst_peer.u8_shortType);
   TEST_ASSERT_EQUAL_UINT8(100U, sst_peer.u8_shortLen);
   sb_peerSubscribed = false;
   TEST_ASSERT_EQUAL_INT(-EACCES, gi_BLKS_SendShort(0x06U, su8ar_pattern, 1U, K_MSEC(10)));
   sb_peerSubscribed = true;

   // Peer client -> device server: write on DATA
   u16_len = gu16_BLK_EncodeShort(u8ar_f, sizeof(u8ar_f), 0x07U, su8ar_pattern, 100U);
   sv_PeerWrite(u8ar_f, u16_len);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_DevSrvShort, 1000));
   TEST_ASSERT_EQUAL_HEX8(0x07U, su8_devShortType);
   TEST_ASSERT_EQUAL_UINT8(100U, su8_devShortLen);
#endif // BLK_ENABLE_SERVER
}

#if BLK_ENABLE_SERVER && BLK_ENABLE_CLIENT
/** Both roles on one link: simultaneous transfers in both directions. */
static void test_Bidirectional(void)
{
   sv_Connect(247U);
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_SendBuffer(0x20U, su8ar_pattern, 60000U));
   sv_PeerStartSend(0x42U, &su8ar_pattern[1000], 50000U, 4U, false);
   TEST_ASSERT_TRUE(sb_RunUntil(sb_BothDone, 60000));
   TEST_ASSERT_EQUAL_INT(eBS_OK, si_devTxDone);
   TEST_ASSERT_EQUAL_INT(eBS_OK, si_devRxDone);
   TEST_ASSERT_EQUAL_MEMORY(su8ar_pattern, sst_peer.u8ar_rx, 60000U);
   TEST_ASSERT_EQUAL_MEMORY(&su8ar_pattern[1000], su8ar_devRx, 50000U);

   // Disconnect with transfers running in both directions
   si_devTxDone = STATUS_NONE;
   si_devRxDone = STATUS_NONE;
   TEST_ASSERT_EQUAL_INT(0, gi_BLKC_SendBuffer(0x31U, su8ar_pattern, 100000U));
   sv_PeerStartSend(0x42U, su8ar_pattern, 100000U, 4U, false);
   sv_Settle(30);
   sv_Disconnect();
   TEST_ASSERT_EQUAL_INT(eBS_DISCONNECTED, si_devTxDone);
   TEST_ASSERT_EQUAL_INT(eBS_DISCONNECTED, si_devRxDone);
}
#endif // BLK_ENABLE_SERVER && BLK_ENABLE_CLIENT

int main(int argc, char **argv)
{
   uint32_t i;

   (void)setvbuf(stdout, NULL, _IONBF, 0);
   gb_simVerbose = (argc > 1) && (strcmp(argv[1], "-v") == 0);

   for (i = 0U; i < MAX_OBJ; i++)
   {
      su8ar_pattern[i] = (uint8_t)((i * 31U) ^ (i >> 8));
   }

   printf("roles: server %d, client %d\n", BLK_ENABLE_SERVER, BLK_ENABLE_CLIENT);
   UNITY_BEGIN();
   RUN_TEST(test_Init);
#if BLK_ENABLE_CLIENT
   RUN_TEST(test_ClientSizes);
   RUN_TEST(test_ClientLoss);
   RUN_TEST(test_ClientTimeout);
   RUN_TEST(test_ClientWindowStall);
   RUN_TEST(test_ClientSmallMtu);
   RUN_TEST(test_ClientAbortAndDisconnect);
   RUN_TEST(test_ClientAttach);
#endif // BLK_ENABLE_CLIENT
#if BLK_ENABLE_SERVER
   RUN_TEST(test_ServerSizes);
   RUN_TEST(test_ServerOverflow);
   RUN_TEST(test_ServerCrcAndReject);
   RUN_TEST(test_ServerAbortAndStale);
   RUN_TEST(test_ServerMalformedData);
#endif // BLK_ENABLE_SERVER
   RUN_TEST(test_ShortMessages);
#if BLK_ENABLE_SERVER && BLK_ENABLE_CLIENT
   RUN_TEST(test_Bidirectional);
#endif // BLK_ENABLE_SERVER && BLK_ENABLE_CLIENT
   return UNITY_END();
}
