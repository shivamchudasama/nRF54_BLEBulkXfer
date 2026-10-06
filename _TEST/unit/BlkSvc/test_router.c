/**
 * @file          test_router.c
 * @brief         Host unit tests for the BulkXfer appType router
 *                (_ASW/_BLK_SVC/BulkRouter.c): registration rules and dispatch
 *                of the Server callbacks by appType range. BulkRouter.c is
 *                included; the GATT service and the BulkXfer Server are stubbed.
 *
 * @date          01/10/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#include "BulkRouter.c"
#include "unity.h"

/******************************************************************************/
/*  Stubs of the GATT service and the BulkXfer Server                         */
/******************************************************************************/
static struct bt_gatt_attr sst_ctrlAttr;
static BlkSrvCfg_T sst_cfg;
static int si_initRet;
static uint32_t su32_initCalls;

const struct bt_gatt_attr *gstpt_BulkSvc_Init(void)
{
   return &sst_ctrlAttr;
}

int gi_BLKS_Init(const BlkSrvCfg_T *stpt_cfg)
{
   su32_initCalls++;
   sst_cfg = *stpt_cfg;
   return si_initRet;
}

/******************************************************************************/
/*  Recording module callbacks (two modules: A and B)                         */
/******************************************************************************/
typedef struct
{
   uint32_t u32_start, u32_data, u32_done, u32_short;
   uint8_t u8_lastType;
   uint32_t u32_lastLen;
   uint32_t u32_lastOffset;
   BlkStatus_E e_lastStatus;
   int i_startRet;
   int i_dataRet;
} Rec_T;

static Rec_T sst_a;
static Rec_T sst_b;

#define REC_FUNCS(X, R) \
   static int si_##X##Start(uint8_t t, uint32_t l) \
   { R.u32_start++; R.u8_lastType = t; R.u32_lastLen = l; return R.i_startRet; } \
   static int si_##X##Data(uint8_t t, uint32_t o, const uint8_t *d, uint16_t l) \
   { (void)d; R.u32_data++; R.u8_lastType = t; R.u32_lastOffset = o; R.u32_lastLen = l; return R.i_dataRet; } \
   static void sv_##X##Done(uint8_t t, BlkStatus_E s, uint32_t l) \
   { R.u32_done++; R.u8_lastType = t; R.e_lastStatus = s; R.u32_lastLen = l; } \
   static void sv_##X##Short(uint8_t t, const uint8_t *d, uint8_t l) \
   { (void)d; R.u32_short++; R.u8_lastType = t; R.u32_lastLen = l; }

REC_FUNCS(A, sst_a)
REC_FUNCS(B, sst_b)

static BulkRoute_T st_Route(uint8_t u8_first, uint8_t u8_last, bool b_a)
{
   BulkRoute_T st_r;

   st_r.u8_firstAppType = u8_first;
   st_r.u8_lastAppType = u8_last;
   st_r.fpt_onRxStart = b_a ? si_AStart : si_BStart;
   st_r.fpt_onRxData = b_a ? si_AData : si_BData;
   st_r.fpt_onRxDone = b_a ? sv_ADone : sv_BDone;
   st_r.fpt_onRxShort = b_a ? sv_AShort : sv_BShort;
   return st_r;
}

void setUp(void)
{
   su8_routeCnt = 0U;
   sb_isStarted = false;
   gv_BulkRouter_ClearFilter();
   si_initRet = 0;
   su32_initCalls = 0U;
   (void)memset(&sst_cfg, 0, sizeof(sst_cfg));
   (void)memset(&sst_a, 0, sizeof(sst_a));
   (void)memset(&sst_b, 0, sizeof(sst_b));
   gv_SimLogClear();
}

void tearDown(void) {}

/** Register A = 0x10, B = 0x20..0x2F and start. */
static void sv_StartTwoModules(void)
{
   BulkRoute_T st_a = st_Route(0x10U, 0x10U, true);
   BulkRoute_T st_b = st_Route(0x20U, 0x2FU, false);

   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Register(&st_a));
   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Register(&st_b));
   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Start());
}

/******************************************************************************/
/*  Registration                                                              */
/******************************************************************************/
static void test_RegisterRejectsInvalidRanges(void)
{
   BulkRoute_T st_r = st_Route(0x20U, 0x10U, true);

   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_BulkRouter_Register(NULL));
   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_BulkRouter_Register(&st_r));
   st_r = st_Route(0xE0U, 0xF0U, true);
   TEST_ASSERT_EQUAL_INT_MESSAGE(-EINVAL, gi_BulkRouter_Register(&st_r), "0xF0.. is BulkXfer's");
   st_r = st_Route(0xEFU, 0xEFU, true);
   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Register(&st_r));
   st_r = st_Route(0x00U, 0x00U, false);
   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Register(&st_r));
}

static void test_RegisterRejectsOverlaps(void)
{
   BulkRoute_T st_r = st_Route(0x20U, 0x2FU, true);

   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Register(&st_r));
   st_r = st_Route(0x2FU, 0x30U, false);
   TEST_ASSERT_EQUAL_INT(-EEXIST, gi_BulkRouter_Register(&st_r));
   st_r = st_Route(0x10U, 0x20U, false);
   TEST_ASSERT_EQUAL_INT(-EEXIST, gi_BulkRouter_Register(&st_r));
   st_r = st_Route(0x24U, 0x25U, false);
   TEST_ASSERT_EQUAL_INT(-EEXIST, gi_BulkRouter_Register(&st_r));
   st_r = st_Route(0x00U, 0xEFU, false);
   TEST_ASSERT_EQUAL_INT(-EEXIST, gi_BulkRouter_Register(&st_r));
   st_r = st_Route(0x30U, 0x30U, false);
   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Register(&st_r));
}

static void test_RegisterTableFull(void)
{
   BulkRoute_T st_r;
   uint8_t i;

   for (i = 0U; i < BULK_ROUTER_MAX_ROUTES; i++)
   {
      st_r = st_Route((uint8_t)(i * 2U), (uint8_t)(i * 2U), true);
      TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Register(&st_r));
   }
   st_r = st_Route(0x80U, 0x80U, true);
   TEST_ASSERT_EQUAL_INT(-ENOMEM, gi_BulkRouter_Register(&st_r));
}

static void test_RegisterAfterStartIsRefused(void)
{
   BulkRoute_T st_r = st_Route(0x40U, 0x40U, true);

   sv_StartTwoModules();
   TEST_ASSERT_EQUAL_INT(-EALREADY, gi_BulkRouter_Register(&st_r));
}

static void test_RegisterCopiesTheRoute(void)
{
   BulkRoute_T st_r = st_Route(0x10U, 0x10U, true);

   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Register(&st_r));
   st_r.u8_firstAppType = 0x00U;
   st_r.fpt_onRxStart = si_BStart;
   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Start());
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(0x10U, 5U));
   TEST_ASSERT_EQUAL_UINT32(1U, sst_a.u32_start);
   TEST_ASSERT_EQUAL_UINT32(0U, sst_b.u32_start);
}

/******************************************************************************/
/*  Start                                                                     */
/******************************************************************************/
static void test_StartConfiguresTheServer(void)
{
   sv_StartTwoModules();
   TEST_ASSERT_EQUAL_UINT32(1U, su32_initCalls);
   TEST_ASSERT_EQUAL_PTR(&sst_ctrlAttr, sst_cfg.stpt_ctrlAttr);
   TEST_ASSERT_NOT_NULL(sst_cfg.fpt_onRxStart);
   TEST_ASSERT_NOT_NULL(sst_cfg.fpt_onRxData);
   TEST_ASSERT_NOT_NULL(sst_cfg.fpt_onRxDone);
   TEST_ASSERT_NOT_NULL(sst_cfg.fpt_onRxShort);
   TEST_ASSERT_FALSE_MESSAGE(sst_cfg.b_autoTuneLink, "_BLE negotiates the link itself");
   TEST_ASSERT_EQUAL_INT(-EALREADY, gi_BulkRouter_Start());
   TEST_ASSERT_EQUAL_UINT32(1U, su32_initCalls);
}

static void test_StartFailureCanBeRetried(void)
{
   si_initRet = -EIO;
   TEST_ASSERT_EQUAL_INT(-EIO, gi_BulkRouter_Start());
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("gi_BLKS_Init failed"));
   si_initRet = 0;
   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Start());
}

/******************************************************************************/
/*  Dispatch                                                                  */
/******************************************************************************/
static void test_TransferGoesToTheOwningRange(void)
{
   uint8_t u8ar_d[3] = { 1, 2, 3 };

   sv_StartTwoModules();
   // Range boundaries of B
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(0x20U, 100U));
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(0x2FU, 7U));
   TEST_ASSERT_EQUAL_UINT32(2U, sst_b.u32_start);
   TEST_ASSERT_EQUAL_UINT32(7U, sst_b.u32_lastLen);
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxData(0x2FU, 4U, u8ar_d, 3U));
   TEST_ASSERT_EQUAL_UINT32(4U, sst_b.u32_lastOffset);
   sst_cfg.fpt_onRxDone(0x2FU, eBS_CRC_ERROR, 7U);
   TEST_ASSERT_EQUAL_UINT32(1U, sst_b.u32_done);
   TEST_ASSERT_EQUAL_INT(eBS_CRC_ERROR, sst_b.e_lastStatus);
   // A saw none of it
   TEST_ASSERT_EQUAL_UINT32(0U, sst_a.u32_start + sst_a.u32_data + sst_a.u32_done);

   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(0x10U, 9U));
   TEST_ASSERT_EQUAL_UINT32(1U, sst_a.u32_start);
}

static void test_OwnerAnswersArePassedBack(void)
{
   uint8_t u8_d = 0U;

   sv_StartTwoModules();
   sst_b.i_startRet = -EBUSY;
   sst_b.i_dataRet = -ENOSPC;
   TEST_ASSERT_EQUAL_INT(-EBUSY, sst_cfg.fpt_onRxStart(0x25U, 1U));
   TEST_ASSERT_EQUAL_INT(-ENOSPC, sst_cfg.fpt_onRxData(0x25U, 0U, &u8_d, 1U));
}

static void test_UnroutedTypesAreRejected(void)
{
   uint8_t u8_d = 0U;

   sv_StartTwoModules();
   TEST_ASSERT_EQUAL_INT(-ENOTSUP, sst_cfg.fpt_onRxStart(0x11U, 10U));
   TEST_ASSERT_EQUAL_INT(-ENOTSUP, sst_cfg.fpt_onRxStart(0x1FU, 10U));
   TEST_ASSERT_EQUAL_INT(-ENOTSUP, sst_cfg.fpt_onRxStart(0x30U, 10U));
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("rejected: unknown type 0x30"));
   // Not reachable through BulkXfer (START was rejected), but harmless
   TEST_ASSERT_EQUAL_INT(-EIO, sst_cfg.fpt_onRxData(0x30U, 0U, &u8_d, 1U));
   sst_cfg.fpt_onRxDone(0x30U, eBS_OK, 1U);
   TEST_ASSERT_EQUAL_UINT32(0U, sst_a.u32_done + sst_b.u32_done);
}

static void test_RangeWithoutReceiverRejectsTransfers(void)
{
   BulkRoute_T st_r = st_Route(0x40U, 0x41U, true);
   uint8_t u8_d = 0U;

   st_r.fpt_onRxData = NULL;
   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Register(&st_r));
   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Start());
   TEST_ASSERT_EQUAL_INT(-ENOTSUP, sst_cfg.fpt_onRxStart(0x40U, 1U));
   TEST_ASSERT_EQUAL_UINT32(0U, sst_a.u32_start);
   TEST_ASSERT_EQUAL_INT(-EIO, sst_cfg.fpt_onRxData(0x40U, 0U, &u8_d, 1U));
}

static void test_RangeWithoutStartAcceptsEverything(void)
{
   BulkRoute_T st_r = st_Route(0x40U, 0x41U, true);

   st_r.fpt_onRxStart = NULL;
   st_r.fpt_onRxDone = NULL;
   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Register(&st_r));
   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Start());
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(0x41U, 1U));
   sst_cfg.fpt_onRxDone(0x41U, eBS_OK, 1U);
   TEST_ASSERT_EQUAL_UINT32(0U, sst_a.u32_done);
}

static void test_ShortMessagesAreRoutedOrLogged(void)
{
   uint8_t u8ar_p[2] = { 9, 9 };
   BulkRoute_T st_c = st_Route(0x40U, 0x40U, true);

   st_c.fpt_onRxShort = NULL;
   TEST_ASSERT_EQUAL_INT(0, gi_BulkRouter_Register(&st_c));
   sv_StartTwoModules();

   sst_cfg.fpt_onRxShort(0x22U, u8ar_p, 2U);
   TEST_ASSERT_EQUAL_UINT32(1U, sst_b.u32_short);
   TEST_ASSERT_EQUAL_HEX8(0x22U, sst_b.u8_lastType);

   // No range, and a range without a short handler: logged and dropped
   sst_cfg.fpt_onRxShort(0x05U, u8ar_p, 2U);
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("short message type 0x05, 2 bytes ignored"));
   sst_cfg.fpt_onRxShort(0x40U, u8ar_p, 1U);
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("short message type 0x40, 1 bytes ignored"));
   TEST_ASSERT_EQUAL_UINT32(0U, sst_a.u32_short);
   TEST_ASSERT_EQUAL_UINT32(1U, sst_b.u32_short);
}

/******************************************************************************/
/*  Filter                                                                    */
/******************************************************************************/
static void test_FilterRejectsOtherTransfersAtStart(void)
{
   sv_StartTwoModules();
   gv_BulkRouter_SetFilter(0x20U, 0x2FU);

   // Outside: rejected before the owner is asked
   TEST_ASSERT_EQUAL_INT(-ENOTSUP, sst_cfg.fpt_onRxStart(0x10U, 5U));
   TEST_ASSERT_EQUAL_UINT32(0U, sst_a.u32_start);
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("rejected: type 0x10 filtered"));

   // Inside, boundaries included: the owner decides as before
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(0x20U, 5U));
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(0x2FU, 5U));
   TEST_ASSERT_EQUAL_UINT32(2U, sst_b.u32_start);

   // Cleared: every range reachable again
   gv_BulkRouter_ClearFilter();
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(0x10U, 5U));
   TEST_ASSERT_EQUAL_UINT32(1U, sst_a.u32_start);
}

static void test_FilterDropsOtherShortMessages(void)
{
   uint8_t u8ar_p[2] = { 9, 9 };

   sv_StartTwoModules();
   gv_BulkRouter_SetFilter(0x10U, 0x10U);
   sst_cfg.fpt_onRxShort(0x27U, u8ar_p, 2U);
   TEST_ASSERT_EQUAL_UINT32(0U, sst_b.u32_short);
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("short message type 0x27 filtered"));
   sst_cfg.fpt_onRxShort(0x10U, u8ar_p, 2U);
   TEST_ASSERT_EQUAL_UINT32(1U, sst_a.u32_short);

   // A new filter replaces the previous one
   gv_BulkRouter_SetFilter(0x20U, 0x2FU);
   sst_cfg.fpt_onRxShort(0x27U, u8ar_p, 2U);
   TEST_ASSERT_EQUAL_UINT32(1U, sst_b.u32_short);
   sst_cfg.fpt_onRxShort(0x10U, u8ar_p, 2U);
   TEST_ASSERT_EQUAL_UINT32(1U, sst_a.u32_short);
}

static void test_FilterLetsAnAcceptedTransferFinish(void)
{
   uint8_t u8_d = 0U;

   sv_StartTwoModules();
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxStart(0x10U, 1U));
   gv_BulkRouter_SetFilter(0x20U, 0x2FU);
   TEST_ASSERT_EQUAL_INT(0, sst_cfg.fpt_onRxData(0x10U, 0U, &u8_d, 1U));
   sst_cfg.fpt_onRxDone(0x10U, eBS_OK, 1U);
   TEST_ASSERT_EQUAL_UINT32(1U, sst_a.u32_data);
   TEST_ASSERT_EQUAL_UINT32(1U, sst_a.u32_done);
}

int main(void)
{
   (void)setvbuf(stdout, NULL, _IONBF, 0);
   UNITY_BEGIN();
   RUN_TEST(test_RegisterRejectsInvalidRanges);
   RUN_TEST(test_RegisterRejectsOverlaps);
   RUN_TEST(test_RegisterTableFull);
   RUN_TEST(test_RegisterAfterStartIsRefused);
   RUN_TEST(test_RegisterCopiesTheRoute);
   RUN_TEST(test_StartConfiguresTheServer);
   RUN_TEST(test_StartFailureCanBeRetried);
   RUN_TEST(test_TransferGoesToTheOwningRange);
   RUN_TEST(test_OwnerAnswersArePassedBack);
   RUN_TEST(test_UnroutedTypesAreRejected);
   RUN_TEST(test_RangeWithoutReceiverRejectsTransfers);
   RUN_TEST(test_RangeWithoutStartAcceptsEverything);
   RUN_TEST(test_ShortMessagesAreRoutedOrLogged);
   RUN_TEST(test_FilterRejectsOtherTransfersAtStart);
   RUN_TEST(test_FilterDropsOtherShortMessages);
   RUN_TEST(test_FilterLetsAnAcceptedTransferFinish);
   return UNITY_END();
}
