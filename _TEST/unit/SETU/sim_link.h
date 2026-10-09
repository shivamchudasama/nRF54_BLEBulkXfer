/**
 * @file          sim_link.h
 * @brief         Simulated BLE link and scripted peer for host tests of the
 *                real SETU engine. The peer is an independent
 *                implementation of the other side of both roles:
 *
 *                  device Client --WwR DATA-->  peer server (GATT DB model)
 *                  device Client <--CTRL ntf--  peer server
 *                  device Server <--WwR DATA--  peer client
 *                  device Server --CTRL ntf-->  peer client
 *
 *                Not a normal header: #include it exactly once, after
 *                SETU_Core.c / _Server.c / _Client.c, because it drives the
 *                engine's private state (sst_SETU_wakeSem, sv_EngineRunOnce).
 *                The including test provides the device-side application
 *                (the callbacks registered with gi_SETUS_Init / gi_SETUC_Init).
 *                Used by test_engine.c and test_upload_e2e.c.
 *
 * @date          24/09/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#ifndef SIM_LINK_H
#define SIM_LINK_H

#include "unity.h"

#define MAX_OBJ              (120U * 1024U)
#define STATUS_NONE          (-1000)
#define PEER_ABORT_BASE      (100)    /* peer tx ended by device ABORT      */
#define PEER_RX_ABORT_BASE   (200)    /* peer rx ended by device ABORT      */

/* Helpers used only by one role are unused in a single-role build */
#define TEST_HELPER          static __attribute__((unused))

/******************************************************************************/
/*  Peer GATT database (hosted by the peer, discovered by the device Client)  */
/******************************************************************************/
#define H_SVC                (10U)
#define H_DATA_DECL          (11U)
#define H_DATA               (12U)
#define H_CTRL_DECL          (13U)
#define H_CTRL               (14U)
#define H_CTRL_CCC           (15U)
#define H_CAPS_DECL          (16U)
#define H_CAPS               (17U)
#define H_SVC_END            (20U)

TEST_HELPER bool sb_dbHasService = true;
TEST_HELPER bool sb_dbHasCtrl = true;
TEST_HELPER bool sb_subscribeFails = false;

/******************************************************************************/
/*  Simulated link                                                            */
/******************************************************************************/
#define LINK_HOST_BUFS       (10U)    /* like CONFIG_BT_ATT_TX_COUNT         */
#define LINK_PER_EVENT       (6U)     /* packets per connection event        */

typedef enum { ePDU_NOTIFY, ePDU_WRITE } PduKind_E;

typedef struct
{
   uint8_t u8ar_data[260];
   uint16_t u16_len;
   PduKind_E e_kind;
   bt_gatt_complete_func_t fpt_func;
} LinkPdu_T;

static LinkPdu_T sstar_linkQ[LINK_HOST_BUFS];
static uint32_t su32_linkHead = 0U;
static uint32_t su32_linkCount = 0U;
static struct bt_conn sst_conn = { 1 };
/* A second live link with no peer model behind it (a host link while the
   device also talks to the peer on sst_conn). GATT client operations on it
   fail with -ENOTCONN; it exists so the roles can be moved between links. */
TEST_HELPER struct bt_conn sst_conn2 = { 2 };
static struct bt_gatt_attr sst_ctrlAttr = { NULL, NULL, 0U };
TEST_HELPER struct bt_gatt_attr sst_dataAttr = { NULL, NULL, 0U };
static bool sb_connected = false;
static bool sb_peerSubscribed = true;        /* peer client subscribed to device CTRL */
static uint16_t su16_mtu = 247U;
TEST_HELPER uint32_t su32_maxWrites = 0U;    /* max device writes queued in the host   */
static uint32_t su32_writesQueued = 0U;
static struct k_timer *sstpt_timers[16];
static uint32_t su32_timerCount = 0U;

/* Pending asynchronous GATT client operations of the device */
static struct bt_gatt_discover_params *sstpt_pendDiscover = NULL;
static struct bt_gatt_subscribe_params *sstpt_pendSubscribe = NULL;
static struct bt_gatt_exchange_params *sstpt_pendMtu = NULL;
static struct bt_gatt_subscribe_params *sstpt_devSub = NULL;   /* active subscription */
TEST_HELPER int si_mtuExchanges = 0;
TEST_HELPER int si_unsubscribes = 0;

static void sv_PeerOnFrame(PduKind_E e_kind, const uint8_t *u8pt_buf, uint16_t u16_len);

void gv_SimRegisterTimer(struct k_timer *t)
{
   uint32_t i;
   for (i = 0U; i < su32_timerCount; i++)
   {
      if (sstpt_timers[i] == t) { return; }
   }
   TEST_ASSERT_TRUE(su32_timerCount < ARRAY_SIZE(sstpt_timers));
   sstpt_timers[su32_timerCount++] = t;
}

static void sv_SimFireTimers(void)
{
   uint32_t i;
   for (i = 0U; i < su32_timerCount; i++)
   {
      if (sstpt_timers[i]->running && (sstpt_timers[i]->deadline <= gi64_simNowMs))
      {
         sstpt_timers[i]->running = false;
         sstpt_timers[i]->expiry(sstpt_timers[i]);
      }
   }
}

/** Peer answers one pending discovery request, like the stack would. */
static void sv_SimRunDiscover(struct bt_gatt_discover_params *p)
{
   static struct bt_gatt_service_val st_svc;
   static struct bt_gatt_chrc star_chrc[3];
   static struct bt_gatt_attr st_attr;
   uint16_t au16_decl[3] = { H_DATA_DECL, H_CTRL_DECL, H_CAPS_DECL };
   uint32_t i;

   (void)memset(&st_attr, 0, sizeof(st_attr));

   if (p->type == BT_GATT_DISCOVER_PRIMARY)
   {
      if (sb_dbHasService && (bt_uuid_cmp(p->uuid, BT_UUID_SETU_SVC) == 0))
      {
         st_svc.uuid = BT_UUID_SETU_SVC;
         st_svc.end_handle = H_SVC_END;
         st_attr.handle = H_SVC;
         st_attr.user_data = &st_svc;
         if (p->func(&sst_conn, &st_attr, p) == BT_GATT_ITER_STOP) { return; }
      }
      (void)p->func(&sst_conn, NULL, p);
      return;
   }

   if (p->type == BT_GATT_DISCOVER_CHARACTERISTIC)
   {
      star_chrc[0].uuid = BT_UUID_SETU_DATA;
      star_chrc[0].value_handle = H_DATA;
      star_chrc[0].properties = BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP;
      star_chrc[1].uuid = BT_UUID_SETU_CTRL;
      star_chrc[1].value_handle = H_CTRL;
      star_chrc[1].properties = BT_GATT_CHRC_NOTIFY;
      star_chrc[2].uuid = BT_UUID_SETU_CAPS;
      star_chrc[2].value_handle = H_CAPS;
      star_chrc[2].properties = 0x02U;
      for (i = 0U; i < 3U; i++)
      {
         if ((i == 1U) && !sb_dbHasCtrl) { continue; }
         if ((au16_decl[i] < p->start_handle) || (au16_decl[i] > p->end_handle)) { continue; }
         st_attr.handle = au16_decl[i];
         st_attr.user_data = &star_chrc[i];
         if (p->func(&sst_conn, &st_attr, p) == BT_GATT_ITER_STOP) { return; }
      }
      (void)p->func(&sst_conn, NULL, p);
      return;
   }

   // Descriptor: only CTRL has a CCC; the device must bound the search range
   TEST_ASSERT_TRUE_MESSAGE(p->end_handle < H_CAPS_DECL, "CCC search not bounded to CTRL");
   if (sb_dbHasCtrl && (bt_uuid_cmp(p->uuid, BT_UUID_GATT_CCC) == 0)
      && (H_CTRL_CCC >= p->start_handle) && (H_CTRL_CCC <= p->end_handle))
   {
      st_attr.handle = H_CTRL_CCC;
      if (p->func(&sst_conn, &st_attr, p) == BT_GATT_ITER_STOP) { return; }
   }
   (void)p->func(&sst_conn, NULL, p);
}

/** Run one pending GATT client operation (one ATT round trip). */
static bool sb_SimRunGattOp(void)
{
   struct bt_gatt_discover_params *pd = sstpt_pendDiscover;
   struct bt_gatt_subscribe_params *ps = sstpt_pendSubscribe;
   struct bt_gatt_exchange_params *pm = sstpt_pendMtu;

   if (pm != NULL)
   {
      sstpt_pendMtu = NULL;
      pm->func(&sst_conn, 0U, pm);
      return true;
   }
   if (pd != NULL)
   {
      sstpt_pendDiscover = NULL;
      sv_SimRunDiscover(pd);
      return true;
   }
   if (ps != NULL)
   {
      sstpt_pendSubscribe = NULL;
      if (sb_subscribeFails)
      {
         ps->subscribe(&sst_conn, 0x03U, ps);
      }
      else
      {
         sstpt_devSub = ps;
         ps->subscribe(&sst_conn, 0U, ps);
      }
      return true;
   }
   return false;
}

/** One connection event: deliver queued PDUs to the peer. */
static void sv_LinkEvent(void)
{
   uint32_t u32_n = MIN(su32_linkCount, LINK_PER_EVENT);

   while (u32_n-- > 0U)
   {
      LinkPdu_T *stpt_pdu = &sstar_linkQ[su32_linkHead];
      su32_linkHead = (su32_linkHead + 1U) % LINK_HOST_BUFS;
      su32_linkCount--;
      if (stpt_pdu->e_kind == ePDU_WRITE) { su32_writesQueued--; }
      sv_PeerOnFrame(stpt_pdu->e_kind, stpt_pdu->u8ar_data, stpt_pdu->u16_len);
      if (stpt_pdu->fpt_func != NULL) { stpt_pdu->fpt_func(&sst_conn, NULL); }
   }
   (void)sb_SimRunGattOp();
   gi64_simNowMs++;
   sv_SimFireTimers();
}

/** Optional: run another simulated thread when a wait blocks (e.g. the File
    System Manager, for a gi_FSMGR_Call() on the waiting thread). */
static void (*sfpt_simBlockHook)(void) = NULL;

void gv_SimOnBlock(struct k_sem *stpt_sem)
{
   (void)stpt_sem;
   if (sfpt_simBlockHook != NULL)
   {
      sfpt_simBlockHook();
   }
   // A blocked credit wait means the engine waits for the link to drain
   if (su32_linkCount > 0U)
   {
      sv_LinkEvent();
   }
}

static int si_LinkQueue(PduKind_E e_kind, const void *vpt_data, uint16_t u16_len,
   bt_gatt_complete_func_t fpt_func)
{
   LinkPdu_T *stpt_pdu;

   if (u16_len > (su16_mtu - 3U)) { return -EMSGSIZE; }
   if (su32_linkCount >= LINK_HOST_BUFS) { return -ENOMEM; }

   stpt_pdu = &sstar_linkQ[(su32_linkHead + su32_linkCount) % LINK_HOST_BUFS];
   (void)memcpy(stpt_pdu->u8ar_data, vpt_data, u16_len);
   stpt_pdu->u16_len = u16_len;
   stpt_pdu->e_kind = e_kind;
   stpt_pdu->fpt_func = fpt_func;
   su32_linkCount++;
   if (e_kind == ePDU_WRITE)
   {
      su32_writesQueued++;
      su32_maxWrites = MAX(su32_maxWrites, su32_writesQueued);
   }
   return 0;
}

int bt_gatt_notify_cb(struct bt_conn *conn, struct bt_gatt_notify_params *params)
{
   if (!sb_connected || (conn != &sst_conn)) { return -ENOTCONN; }
   if (!sb_peerSubscribed) { return -EINVAL; }
   if (params->attr != &sst_ctrlAttr) { return -EINVAL; }
   return si_LinkQueue(ePDU_NOTIFY, params->data, params->len, params->func);
}

int bt_gatt_write_without_response_cb(struct bt_conn *conn, uint16_t handle,
   const void *data, uint16_t length, bool sign, bt_gatt_complete_func_t func, void *user_data)
{
   (void)sign; (void)user_data;
   if (!sb_connected || (conn != &sst_conn)) { return -ENOTCONN; }
   if (handle != H_DATA) { return -EINVAL; }
   return si_LinkQueue(ePDU_WRITE, data, length, func);
}

int bt_gatt_discover(struct bt_conn *conn, struct bt_gatt_discover_params *params)
{
   if (!sb_connected || (conn != &sst_conn)) { return -ENOTCONN; }
   TEST_ASSERT_NULL_MESSAGE(sstpt_pendDiscover, "second discovery while one is pending");
   sstpt_pendDiscover = params;
   return 0;
}

int bt_gatt_subscribe(struct bt_conn *conn, struct bt_gatt_subscribe_params *params)
{
   if (!sb_connected || (conn != &sst_conn)) { return -ENOTCONN; }
   TEST_ASSERT_EQUAL_UINT16(H_CTRL, params->value_handle);
   TEST_ASSERT_EQUAL_UINT16(H_CTRL_CCC, params->ccc_handle);
   TEST_ASSERT_EQUAL_UINT16(BT_GATT_CCC_NOTIFY, params->value);
   TEST_ASSERT_TRUE_MESSAGE((params->flags[0] & (1L << BT_GATT_SUBSCRIBE_FLAG_VOLATILE)) != 0,
      "subscription must be volatile");
   sstpt_pendSubscribe = params;
   return 0;
}

/** Unsubscribe on a live link: the peer stops notifying at once (the CCC
    write and its NULL-data notification are not modelled). */
int bt_gatt_unsubscribe(struct bt_conn *conn, struct bt_gatt_subscribe_params *params)
{
   if (!sb_connected || (conn != &sst_conn)) { return -ENOTCONN; }
   TEST_ASSERT_EQUAL_PTR_MESSAGE(sstpt_devSub, params, "unsubscribe of an unknown subscription");
   si_unsubscribes++;
   sstpt_devSub = NULL;
   return 0;
}

int bt_gatt_exchange_mtu(struct bt_conn *conn, struct bt_gatt_exchange_params *params)
{
   if (!sb_connected || (conn != &sst_conn)) { return -ENOTCONN; }
   si_mtuExchanges++;
   sstpt_pendMtu = params;
   return 0;
}

bool bt_gatt_is_subscribed(struct bt_conn *conn, const struct bt_gatt_attr *attr,
   uint16_t ccc_type)
{
   (void)conn; (void)ccc_type;
   TEST_ASSERT_EQUAL_PTR(&sst_ctrlAttr, attr);
   return sb_peerSubscribed;
}

uint16_t bt_gatt_get_mtu(struct bt_conn *conn)
{
   (void)conn;
   return su16_mtu;
}

/******************************************************************************/
/*  Peer - independent implementation of the other side of both roles         */
/******************************************************************************/
typedef struct
{
   /* peer as server: device Client -> peer */
   uint8_t u8ar_rx[MAX_OBJ];
   bool b_rxActive;
   uint8_t u8_rxId, u8_rxType, u8_rxChunk, u8_rxWindow, u8_sinceAck;
   uint32_t u32_rxTotal, u32_rxCrcExp, u32_rxCrc, u32_rxNext, u32_rxFrames;
   bool b_rxNackSent;
   int i_rxDone;
   int32_t i32_dropAbsOnce;         /* simulate app-level loss of one frame */
   bool b_ackOnlyWhenIdle;          /* ACK only once the sender goes quiet   */
   bool b_silent;                   /* never answer                         */
   uint32_t u32_nacksSent;
   uint32_t u32_rxAckedAbs;         /* last ACK/NACK position sent          */
   uint32_t u32_rxMaxAhead;         /* max frames seen beyond that position */

   /* peer as client: peer -> device Server */
   const uint8_t *u8pt_tx;
   uint32_t u32_txLen;
   bool b_txActive, b_txStarted;
   uint8_t u8_txId, u8_txChunk, u8_txWindow;
   uint32_t u32_txFrames, u32_txNext, u32_txAcked, u32_txBurst;
   int64_t i64_txLastProgress;
   int i_txDone;
   uint32_t u32_nacksRcvd;

   /* short messages from the device, per channel (last one kept) */
   int i_shortWrites, i_shortNotifies;
   uint8_t u8_shortType, u8_shortLen;
   uint8_t u8ar_short[256];
   uint8_t u8ar_shortFrame[260];    /* whole frame as it was on the air     */
   uint16_t u16_shortFrameLen;
} Peer_T;

static Peer_T sst_peer;

/** Peer client writes a frame to the device Server's DATA characteristic. */
static void sv_PeerWrite(const uint8_t *u8pt_buf, uint16_t u16_len)
{
#if SETU_ENABLE_SERVER
   // Write Without Response: a hook error just loses the frame
   (void)gt_SETUS_DataWriteHook(&sst_conn, &sst_dataAttr, u8pt_buf, u16_len, 0U,
      BT_GATT_WRITE_FLAG_CMD);
#else
   (void)u8pt_buf; (void)u16_len;
#endif // SETU_ENABLE_SERVER
}

/** Peer server notifies a frame on its CTRL characteristic to the device Client. */
static void sv_PeerNotify(const uint8_t *u8pt_buf, uint16_t u16_len)
{
   if (sstpt_devSub != NULL)
   {
      (void)sstpt_devSub->notify(&sst_conn, sstpt_devSub, u8pt_buf, u16_len);
   }
}

static void sv_PeerCtrl3(uint8_t u8_type, uint8_t a, uint8_t b, uint8_t c)
{
   uint8_t u8ar_f[5] = { 3U, u8_type, a, b, c };
   sv_PeerNotify(u8ar_f, sizeof(u8ar_f));
}

static void sv_PeerRxFinish(void)
{
   uint8_t u8_st = (sst_peer.u32_rxCrc == sst_peer.u32_rxCrcExp) ? eBS_OK : eBS_CRC_ERROR;
   uint8_t u8ar_f[4] = { 2U, eBFT_END, sst_peer.u8_rxId, u8_st };

   sv_PeerNotify(u8ar_f, sizeof(u8ar_f));
   sst_peer.b_rxActive = false;
   sst_peer.i_rxDone = u8_st;
}

static void sv_PeerOnFrame(PduKind_E e_kind, const uint8_t *u8pt_buf, uint16_t u16_len)
{
   SETUFrame_T f;
   uint8_t u8_diff;
   uint32_t u32_abs;

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, gi_SETU_FrameParse(u8pt_buf, u16_len, &f),
      "device sent a malformed frame");
   TEST_ASSERT_TRUE_MESSAGE(u16_len <= (su16_mtu - 3U), "device frame exceeds ATT_MTU - 3");

   if (f.u8_type <= SETU_APP_TYPE_MAX)
   {
      if (e_kind == ePDU_WRITE) { sst_peer.i_shortWrites++; } else { sst_peer.i_shortNotifies++; }
      sst_peer.u8_shortType = f.u8_type;
      sst_peer.u8_shortLen = f.u8_payloadLen;
      (void)memcpy(sst_peer.u8ar_short, f.u8pt_payload, f.u8_payloadLen);
      (void)memcpy(sst_peer.u8ar_shortFrame, u8pt_buf, u16_len);
      sst_peer.u16_shortFrameLen = u16_len;
      return;
   }

   // Sender frames travel as writes, receiver frames as notifications
   if (e_kind == ePDU_WRITE)
   {
      TEST_ASSERT_TRUE_MESSAGE((f.u8_type == eBFT_START) || (f.u8_type == eBFT_DATA)
         || ((f.u8_type == eBFT_ABORT) && (f.u_body.st_abort.u8_dir == eBAD_BY_SENDER)),
         "receiver frame sent as a write");
   }
   else
   {
      TEST_ASSERT_TRUE_MESSAGE((f.u8_type == eBFT_ACK) || (f.u8_type == eBFT_NACK)
         || (f.u8_type == eBFT_END)
         || ((f.u8_type == eBFT_ABORT) && (f.u_body.st_abort.u8_dir == eBAD_BY_RECEIVER)),
         "sender frame sent as a notification");
   }

   if (sst_peer.b_silent) { return; }

   switch (f.u8_type)
   {
      case eBFT_START:
         sst_peer.b_rxActive = true;
         sst_peer.u8_rxId = f.u_body.st_start.u8_xferId;
         sst_peer.u8_rxType = f.u_body.st_start.u8_appType;
         sst_peer.u8_rxChunk = f.u_body.st_start.u8_chunkSize;
         sst_peer.u8_rxWindow = MIN(f.u_body.st_start.u8_window, 16U);
         sst_peer.u32_rxTotal = f.u_body.st_start.u32_totalLen;
         sst_peer.u32_rxCrcExp = f.u_body.st_start.u32_crc32;
         sst_peer.u32_rxCrc = 0U;
         sst_peer.u32_rxNext = 0U;
         sst_peer.u8_sinceAck = 0U;
         sst_peer.b_rxNackSent = false;
         sst_peer.u32_rxFrames = (sst_peer.u32_rxTotal + sst_peer.u8_rxChunk - 1U) / sst_peer.u8_rxChunk;
         TEST_ASSERT_EQUAL_UINT8_MESSAGE(MIN(su16_mtu - 3U, SETU_MAX_FRAME_LEN) - SETU_DATA_HDR_LEN,
            sst_peer.u8_rxChunk, "chunk size must be min(MTU - 3, 244) - 4");
         if (sst_peer.u32_rxFrames == 0U) { sv_PeerRxFinish(); break; }
         sst_peer.u32_rxAckedAbs = 0U;
         sst_peer.u32_rxMaxAhead = 0U;
         sv_PeerCtrl3(eBFT_ACK, sst_peer.u8_rxId, 0U, sst_peer.u8_rxWindow);
         break;

      case eBFT_DATA:
         if (!sst_peer.b_rxActive || (f.u_body.st_data.u8_xferId != sst_peer.u8_rxId)) { break; }
         u8_diff = (uint8_t)(f.u_body.st_data.u8_seq - (uint8_t)sst_peer.u32_rxNext);
         // How far beyond the last acknowledged position did the device send?
         if (u8_diff < 128U)
         {
            sst_peer.u32_rxMaxAhead = MAX(sst_peer.u32_rxMaxAhead,
               sst_peer.u32_rxNext + u8_diff + 1U - sst_peer.u32_rxAckedAbs);
         }
         if (u8_diff == 0U)
         {
            if (sst_peer.i32_dropAbsOnce == (int32_t)sst_peer.u32_rxNext)
            {
               sst_peer.i32_dropAbsOnce = -1;
               break;
            }
            (void)memcpy(&sst_peer.u8ar_rx[sst_peer.u32_rxNext * sst_peer.u8_rxChunk],
               f.u_body.st_data.u8pt_data, f.u_body.st_data.u8_dataLen);
            sst_peer.u32_rxCrc = crc32_ieee_update(sst_peer.u32_rxCrc,
               f.u_body.st_data.u8pt_data, f.u_body.st_data.u8_dataLen);
            sst_peer.u32_rxNext++;
            sst_peer.u8_sinceAck++;
            sst_peer.b_rxNackSent = false;
            if (sst_peer.u32_rxNext == sst_peer.u32_rxFrames) { sv_PeerRxFinish(); break; }
            if (!sst_peer.b_ackOnlyWhenIdle && (sst_peer.u8_sinceAck >= (sst_peer.u8_rxWindow / 2U)))
            {
               sst_peer.u8_sinceAck = 0U;
               sst_peer.u32_rxAckedAbs = sst_peer.u32_rxNext;
               sv_PeerCtrl3(eBFT_ACK, sst_peer.u8_rxId, (uint8_t)sst_peer.u32_rxNext,
                  sst_peer.u8_rxWindow);
            }
         }
         else if (u8_diff < 128U)
         {
            if (!sst_peer.b_rxNackSent)
            {
               sst_peer.b_rxNackSent = true;
               sst_peer.u32_nacksSent++;
               sst_peer.u32_rxAckedAbs = sst_peer.u32_rxNext;
               sv_PeerCtrl3(eBFT_NACK, sst_peer.u8_rxId, (uint8_t)sst_peer.u32_rxNext,
                  eBS_OUT_OF_ORDER);
            }
         }
         else
         {
            sst_peer.u32_rxAckedAbs = sst_peer.u32_rxNext;
            sv_PeerCtrl3(eBFT_ACK, sst_peer.u8_rxId, (uint8_t)sst_peer.u32_rxNext,
               sst_peer.u8_rxWindow);
         }
         break;

      case eBFT_ACK:
         if (!sst_peer.b_txActive || (f.u_body.st_ack.u8_xferId != sst_peer.u8_txId)) { break; }
         if (!sst_peer.b_txStarted)
         {
            if (f.u_body.st_ack.u8_seq == 0U)
            {
               sst_peer.b_txStarted = true;
               sst_peer.u8_txWindow = MIN(sst_peer.u8_txWindow, f.u_body.st_ack.u8_window);
               sst_peer.i64_txLastProgress = gi64_simNowMs;
            }
            break;
         }
         u32_abs = sst_peer.u32_txAcked + (uint8_t)(f.u_body.st_ack.u8_seq - (uint8_t)sst_peer.u32_txAcked);
         if ((u32_abs > sst_peer.u32_txAcked) && (u32_abs <= sst_peer.u32_txNext))
         {
            sst_peer.u32_txAcked = u32_abs;
            sst_peer.i64_txLastProgress = gi64_simNowMs;
         }
         break;

      case eBFT_NACK:
         if (!sst_peer.b_txActive || (f.u_body.st_nack.u8_xferId != sst_peer.u8_txId)) { break; }
         sst_peer.u32_nacksRcvd++;
         u32_abs = sst_peer.u32_txAcked + (uint8_t)(f.u_body.st_nack.u8_seq - (uint8_t)sst_peer.u32_txAcked);
         if (u32_abs <= sst_peer.u32_txNext)
         {
            sst_peer.u32_txAcked = u32_abs;
            sst_peer.u32_txNext = u32_abs;
            sst_peer.i64_txLastProgress = gi64_simNowMs;
         }
         break;

      case eBFT_END:
         if (sst_peer.b_txActive && (f.u_body.st_end.u8_xferId == sst_peer.u8_txId))
         {
            sst_peer.b_txActive = false;
            sst_peer.i_txDone = f.u_body.st_end.u8_status;
         }
         break;

      case eBFT_ABORT:
         if ((f.u_body.st_abort.u8_dir == eBAD_BY_RECEIVER) && sst_peer.b_txActive
            && (f.u_body.st_abort.u8_xferId == sst_peer.u8_txId))
         {
            sst_peer.b_txActive = false;
            sst_peer.i_txDone = PEER_ABORT_BASE + f.u_body.st_abort.u8_reason;
         }
         else if ((f.u_body.st_abort.u8_dir == eBAD_BY_SENDER) && sst_peer.b_rxActive
            && (f.u_body.st_abort.u8_xferId == sst_peer.u8_rxId))
         {
            sst_peer.b_rxActive = false;
            sst_peer.i_rxDone = PEER_RX_ABORT_BASE + f.u_body.st_abort.u8_reason;
         }
         break;

      default:
         TEST_FAIL_MESSAGE("unexpected frame type");
         break;
   }
}

/** Peer client starts sending u32_len bytes of u8pt_data as one transfer. */
TEST_HELPER void sv_PeerStartSend(uint8_t u8_appType, const uint8_t *u8pt_data, uint32_t u32_len,
   uint32_t u32_burst, bool b_badCrc)
{
   uint8_t u8ar_f[SETU_CTRL_FRAME_MAX_LEN];
   uint32_t u32_crc = crc32_ieee_update(0U, u8pt_data, u32_len) ^ (b_badCrc ? 1U : 0U);

   sst_peer.u8pt_tx = u8pt_data;
   sst_peer.u32_txLen = u32_len;
   sst_peer.b_txActive = true;
   sst_peer.b_txStarted = false;
   sst_peer.u8_txId++;
   sst_peer.u8_txChunk = (uint8_t)(MIN(su16_mtu - 3U, SETU_MAX_FRAME_LEN) - SETU_DATA_HDR_LEN);
   sst_peer.u8_txWindow = 32U;
   sst_peer.u32_txFrames = (u32_len + sst_peer.u8_txChunk - 1U) / sst_peer.u8_txChunk;
   sst_peer.u32_txNext = 0U;
   sst_peer.u32_txAcked = 0U;
   sst_peer.u32_txBurst = u32_burst;
   sst_peer.i64_txLastProgress = gi64_simNowMs;
   sst_peer.i_txDone = STATUS_NONE;

   (void)gu16_SETU_EncodeStart(u8ar_f, sizeof(u8ar_f), sst_peer.u8_txId, u8_appType, u32_len,
      sst_peer.u8_txChunk, sst_peer.u8_txWindow, u32_crc);
   sv_PeerWrite(u8ar_f, SETU_CTRL_FRAME_MAX_LEN);
}

/** Peer sends up to one burst of DATA frames. Returns true if it sent any. */
static bool sb_PeerPump(void)
{
   uint8_t u8ar_f[SETU_MAX_FRAME_LEN];
   uint32_t u32_sent = 0U;
   uint32_t u32_off;
   uint16_t u16_n;

   if (!sst_peer.b_txActive || !sst_peer.b_txStarted) { return false; }

   // Peer-side ACK timeout: go back to the last acknowledged frame
   if ((gi64_simNowMs - sst_peer.i64_txLastProgress) > 1500)
   {
      sst_peer.u32_txNext = sst_peer.u32_txAcked;
      sst_peer.i64_txLastProgress = gi64_simNowMs;
   }

   while ((sst_peer.u32_txNext < sst_peer.u32_txFrames)
      && ((sst_peer.u32_txNext - sst_peer.u32_txAcked) < sst_peer.u8_txWindow)
      && (u32_sent < sst_peer.u32_txBurst))
   {
      u32_off = sst_peer.u32_txNext * sst_peer.u8_txChunk;
      u16_n = (uint16_t)MIN((uint32_t)sst_peer.u8_txChunk, sst_peer.u32_txLen - u32_off);
      (void)gu16_SETU_EncodeDataHeader(u8ar_f, sizeof(u8ar_f), sst_peer.u8_txId,
         (uint8_t)sst_peer.u32_txNext, u16_n);
      (void)memcpy(&u8ar_f[SETU_DATA_HDR_LEN], &sst_peer.u8pt_tx[u32_off], u16_n);
      sv_PeerWrite(u8ar_f, (uint16_t)(SETU_DATA_HDR_LEN + u16_n));
      sst_peer.u32_txNext++;
      u32_sent++;
   }

   return u32_sent > 0U;
}

/******************************************************************************/
/*  Scheduler                                                                 */
/******************************************************************************/
static bool sb_Step(void)
{
   bool b_active = false;

   // Engine: run while it has been kicked
   while (sst_SETU_wakeSem.count > 0U)
   {
      sst_SETU_wakeSem.count = 0U;
      sv_EngineRunOnce();
      b_active = true;
   }

   if ((su32_linkCount > 0U) || (sstpt_pendDiscover != NULL) || (sstpt_pendSubscribe != NULL)
      || (sstpt_pendMtu != NULL))
   {
      sv_LinkEvent();
      b_active = true;
   }

   if (sb_PeerPump())
   {
      gi64_simNowMs++;
      sv_SimFireTimers();
      b_active = true;
   }

   return b_active;
}

/** Nothing moved during a step: a lazy peer acknowledges now. */
static void sv_PeerIdle(void)
{
   if (sst_peer.b_ackOnlyWhenIdle && sst_peer.b_rxActive && (sst_peer.u8_sinceAck > 0U))
   {
      sst_peer.u8_sinceAck = 0U;
      sst_peer.u32_rxAckedAbs = sst_peer.u32_rxNext;
      sv_PeerCtrl3(eBFT_ACK, sst_peer.u8_rxId, (uint8_t)sst_peer.u32_rxNext,
         sst_peer.u8_rxWindow);
   }
}

/** Run the simulation until fpt_cond() holds or i64_maxMs of simulated time pass. */
static bool sb_RunUntil(bool (*fpt_cond)(void), int64_t i64_maxMs)
{
   int64_t i64_end = gi64_simNowMs + i64_maxMs;
   uint32_t u32_iter = 0U;

   while (!fpt_cond() && (gi64_simNowMs < i64_end) && (u32_iter++ < 20000000U))
   {
      if (!sb_Step())
      {
         sv_PeerIdle();
         gi64_simNowMs++;
         sv_SimFireTimers();
      }
   }

   return fpt_cond();
}

static void sv_Settle(int64_t i64_ms)
{
   int64_t i64_end = gi64_simNowMs + i64_ms;

   while (gi64_simNowMs < i64_end)
   {
      if (!sb_Step()) { gi64_simNowMs++; sv_SimFireTimers(); }
   }
}

TEST_HELPER bool sb_PeerTxDone(void) { return sst_peer.i_txDone != STATUS_NONE; }
TEST_HELPER bool sb_PeerShortWrite(void) { return sst_peer.i_shortWrites > 0; }
TEST_HELPER bool sb_PeerShortNotify(void) { return sst_peer.i_shortNotifies > 0; }

/******************************************************************************/
/*  Fixtures                                                                  */
/******************************************************************************/
/** Reset the link and the peer, connect and bind the device Server. */
static void sv_SimConnect(uint16_t u16_mtu)
{
   (void)memset(&sst_peer, 0, sizeof(sst_peer));
   sst_peer.i_rxDone = STATUS_NONE;
   sst_peer.i_txDone = STATUS_NONE;
   sst_peer.i32_dropAbsOnce = -1;
   su32_linkHead = 0U;
   su32_linkCount = 0U;
   su32_writesQueued = 0U;
   su16_mtu = u16_mtu;
   sb_peerSubscribed = true;
   sb_connected = true;
#if SETU_ENABLE_SERVER
   gv_SETUS_OnConnected(&sst_conn);
#endif // SETU_ENABLE_SERVER
}

/** Drop the link like the stack does; safe to call when not connected. */
static void sv_Disconnect(void)
{
   if (!sb_connected)
   {
      return;
   }
   sb_connected = false;
   su32_linkCount = 0U;              /* queued PDUs die with the link      */
   su32_writesQueued = 0U;
   sstpt_pendDiscover = NULL;        /* outstanding ATT requests are lost  */
   sstpt_pendSubscribe = NULL;
   sstpt_pendMtu = NULL;
   // Volatile subscription: removed by the stack, which says so with NULL data
   if (sstpt_devSub != NULL)
   {
      (void)sstpt_devSub->notify(&sst_conn, sstpt_devSub, NULL, 0U);
      sstpt_devSub = NULL;
   }
   gv_SETU_OnDisconnected(&sst_conn);
   sv_Settle(5);
}

#endif // SIM_LINK_H
