/**
 * @file          test_gatt_cb.c
 * @brief         Host unit tests for _LIB/GATT_CB, written against the contract
 *                in _DOC/GATT_CB/API_REFERENCE.md (section numbers below refer
 *                to it).
 *
 * @date          29/09/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "GATT_GenericCallbacks.h"

/******************************************************************************/
/*  Fixtures                                                                  */
/******************************************************************************/
#define FIXED_LEN            (4U)
#define VAR_LEN              (16U)

static uint8_t su8ar_fixed[FIXED_LEN];
static uint8_t su8ar_var[VAR_LEN];
static struct k_mutex sst_mutex;
static GATTCharDescriptor_T sst_fixed;
static GATTCharDescriptor_T sst_var;
static struct bt_gatt_attr sst_fixedAttr;
static struct bt_gatt_attr sst_varAttr;
static struct bt_conn sst_conn = { 1 };

/* What the hooks saw */
static int si_hookCalls;
static int si_hookMutexHeld;                 /* sst_mutex.i_held inside the hook */
static uint16_t su16_hookLen;
static uint16_t su16_hookOffset;
static uint8_t su8_hookFlags;
static uint16_t su16_hookActualLen;          /* descriptor's actual length then  */
static uint8_t su8ar_hookBuf[VAR_LEN];       /* buffer contents then             */
static uint8_t su8ar_hookData[VAR_LEN];      /* vpt_data contents then           */
static ssize_t st_hookRet;                   /* value the hook returns           */

static ssize_t st_ReadHook(struct bt_conn *c, const struct bt_gatt_attr *a, void *b,
   uint16_t u16_len, uint16_t u16_off)
{
   const GATTCharDescriptor_T *d = a->user_data;

   (void)c;
   si_hookCalls++;
   si_hookMutexHeld = sst_mutex.i_held;
   su16_hookLen = u16_len;
   su16_hookOffset = u16_off;
   su16_hookActualLen = d->u16_actualLen;
   (void)memcpy(su8ar_hookBuf, b, MIN(u16_len, sizeof(su8ar_hookBuf)));
   return st_hookRet;
}

static ssize_t st_WriteHook(struct bt_conn *c, const struct bt_gatt_attr *a, const void *b,
   uint16_t u16_len, uint16_t u16_off, uint8_t u8_flags)
{
   const GATTCharDescriptor_T *d = a->user_data;

   (void)c;
   si_hookCalls++;
   si_hookMutexHeld = sst_mutex.i_held;
   su16_hookLen = u16_len;
   su16_hookOffset = u16_off;
   su8_hookFlags = u8_flags;
   su16_hookActualLen = d->u16_actualLen;
   (void)memcpy(su8ar_hookBuf, b, MIN(u16_len, sizeof(su8ar_hookBuf)));
   (void)memcpy(su8ar_hookData, d->vpt_data, d->u16_dataLen);
   return st_hookRet;
}

void setUp(void)
{
   static const uint8_t scu8ar_init[FIXED_LEN] = { 0x11U, 0x22U, 0x33U, 0x44U };
   GATTCharDescriptor_T st_fixed = { su8ar_fixed, FIXED_LEN, FIXED_LEN, false, &sst_mutex, NULL, NULL };
   GATTCharDescriptor_T st_var = { su8ar_var, VAR_LEN, 0U, true, &sst_mutex, NULL, NULL };

   (void)memcpy(su8ar_fixed, scu8ar_init, sizeof(su8ar_fixed));
   (void)memset(su8ar_var, 0xEE, sizeof(su8ar_var));
   (void)memset(&sst_mutex, 0, sizeof(sst_mutex));
   sst_fixed = st_fixed;
   sst_var = st_var;
   sst_fixedAttr.user_data = &sst_fixed;
   sst_varAttr.user_data = &sst_var;

   si_hookCalls = 0;
   si_hookMutexHeld = -1;
   su8_hookFlags = 0U;
   st_hookRet = 0;
   gv_SimLogClear();
}

/** Every path must leave the mutex released. */
void tearDown(void)
{
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, sst_mutex.i_held, "mutex left locked");
}

/******************************************************************************/
/*  §3 gt_GATT_GenericRead                                                    */
/******************************************************************************/
static void test_ReadFixedWholeValueUnderMutex(void)
{
   uint8_t u8ar_buf[8] = { 0 };

   TEST_ASSERT_EQUAL_INT(FIXED_LEN, gt_GATT_GenericRead(&sst_conn, &sst_fixedAttr, u8ar_buf,
      sizeof(u8ar_buf), 0U));
   TEST_ASSERT_EQUAL_HEX8_ARRAY(su8ar_fixed, u8ar_buf, FIXED_LEN);
   TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, sst_mutex.u32_locks, "read must take the mutex");
}

static void test_ReadHonoursOffsetAndLength(void)
{
   uint8_t u8ar_buf[8] = { 0 };

   // Offset inside the value
   TEST_ASSERT_EQUAL_INT(2, gt_GATT_GenericRead(&sst_conn, &sst_fixedAttr, u8ar_buf,
      sizeof(u8ar_buf), 2U));
   TEST_ASSERT_EQUAL_HEX8(0x33U, u8ar_buf[0]);
   TEST_ASSERT_EQUAL_HEX8(0x44U, u8ar_buf[1]);

   // Stack buffer smaller than the value: clipped
   TEST_ASSERT_EQUAL_INT(1, gt_GATT_GenericRead(&sst_conn, &sst_fixedAttr, u8ar_buf, 1U, 0U));

   // Offset exactly at the end: 0 bytes, not an error
   TEST_ASSERT_EQUAL_INT(0, gt_GATT_GenericRead(&sst_conn, &sst_fixedAttr, u8ar_buf,
      sizeof(u8ar_buf), FIXED_LEN));

   // Offset past the end
   TEST_ASSERT_EQUAL_INT(BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET),
      gt_GATT_GenericRead(&sst_conn, &sst_fixedAttr, u8ar_buf, sizeof(u8ar_buf), FIXED_LEN + 1U));
}

static void test_ReadVariableReturnsOnlyActualLength(void)
{
   uint8_t u8ar_buf[VAR_LEN];

   TEST_ASSERT_EQUAL_INT(0, gt_GATT_GenericRead(&sst_conn, &sst_varAttr, u8ar_buf,
      sizeof(u8ar_buf), 0U));
   sst_var.u16_actualLen = 3U;
   TEST_ASSERT_EQUAL_INT(3, gt_GATT_GenericRead(&sst_conn, &sst_varAttr, u8ar_buf,
      sizeof(u8ar_buf), 0U));
}

static void test_ReadNullDataIsUnlikelyError(void)
{
   uint8_t u8ar_buf[4];

   sst_fixed.vpt_data = NULL;
   TEST_ASSERT_EQUAL_INT(BT_GATT_ERR(BT_ATT_ERR_UNLIKELY),
      gt_GATT_GenericRead(&sst_conn, &sst_fixedAttr, u8ar_buf, sizeof(u8ar_buf), 0U));
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("vpt_data is NULL"));
}

static void test_ReadHookRunsAfterCopyWithoutMutex(void)
{
   uint8_t u8ar_buf[8] = { 0 };

   sst_fixed.fpt_customReadCb = st_ReadHook;
   TEST_ASSERT_EQUAL_INT(FIXED_LEN - 1U, gt_GATT_GenericRead(&sst_conn, &sst_fixedAttr, u8ar_buf,
      sizeof(u8ar_buf), 1U));
   TEST_ASSERT_EQUAL_INT(1, si_hookCalls);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, si_hookMutexHeld, "hook must run with the mutex released");
   TEST_ASSERT_EQUAL_UINT16(sizeof(u8ar_buf), su16_hookLen);
   TEST_ASSERT_EQUAL_UINT16(1U, su16_hookOffset);
   TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x22U, su8ar_hookBuf[0], "buffer must be filled before the hook");
}

static void test_ReadHookZeroKeepsNonZeroReplaces(void)
{
   uint8_t u8ar_buf[8];

   sst_fixed.fpt_customReadCb = st_ReadHook;

   st_hookRet = 0;
   TEST_ASSERT_EQUAL_INT(FIXED_LEN, gt_GATT_GenericRead(&sst_conn, &sst_fixedAttr, u8ar_buf,
      sizeof(u8ar_buf), 0U));

   st_hookRet = 2;
   TEST_ASSERT_EQUAL_INT(2, gt_GATT_GenericRead(&sst_conn, &sst_fixedAttr, u8ar_buf,
      sizeof(u8ar_buf), 0U));

   st_hookRet = BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
   TEST_ASSERT_EQUAL_INT(BT_GATT_ERR(BT_ATT_ERR_UNLIKELY),
      gt_GATT_GenericRead(&sst_conn, &sst_fixedAttr, u8ar_buf, sizeof(u8ar_buf), 0U));
}

static void test_ReadAssertsOnMissingDescriptor(void)
{
   uint8_t u8ar_buf[4];
   struct bt_gatt_attr st_bare = { NULL, NULL, 0U };

   TEST_ASSERT_TRUE(SIM_EXPECT_ASSERT(
      (void)gt_GATT_GenericRead(&sst_conn, &st_bare, u8ar_buf, sizeof(u8ar_buf), 0U)));
   TEST_ASSERT_TRUE(SIM_EXPECT_ASSERT(
      (void)gt_GATT_GenericRead(&sst_conn, NULL, u8ar_buf, sizeof(u8ar_buf), 0U)));
}

/******************************************************************************/
/*  §3 gt_GATT_GenericWrite                                                   */
/******************************************************************************/
static void test_WriteFixedExactLength(void)
{
   const uint8_t u8ar_in[FIXED_LEN] = { 1U, 2U, 3U, 4U };

   TEST_ASSERT_EQUAL_INT(FIXED_LEN, gt_GATT_GenericWrite(&sst_conn, &sst_fixedAttr, u8ar_in,
      FIXED_LEN, 0U, 0U));
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_in, su8ar_fixed, FIXED_LEN);
   TEST_ASSERT_EQUAL_UINT16_MESSAGE(FIXED_LEN, sst_fixed.u16_actualLen,
      "fixed-length actual length is never changed");
   TEST_ASSERT_EQUAL_UINT32(1U, sst_mutex.u32_locks);
}

static void test_WriteFixedWrongLengthRejectedUntouched(void)
{
   const uint8_t u8ar_in[FIXED_LEN + 1U] = { 9U, 9U, 9U, 9U, 9U };
   const uint8_t u8ar_before[FIXED_LEN] = { 0x11U, 0x22U, 0x33U, 0x44U };

   // Short, long, and first piece of a long write
   TEST_ASSERT_EQUAL_INT(BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN),
      gt_GATT_GenericWrite(&sst_conn, &sst_fixedAttr, u8ar_in, FIXED_LEN - 1U, 0U, 0U));
   TEST_ASSERT_EQUAL_INT(BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN),
      gt_GATT_GenericWrite(&sst_conn, &sst_fixedAttr, u8ar_in, FIXED_LEN + 1U, 0U, 0U));
   TEST_ASSERT_EQUAL_INT(BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN),
      gt_GATT_GenericWrite(&sst_conn, &sst_fixedAttr, u8ar_in, 2U, 0U, 0U));
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_before, su8ar_fixed, FIXED_LEN);
   TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, sst_mutex.u32_locks, "rejected before locking");
}

/**
 * §3 accepts any fixed-length write that ends exactly at u16_dataLen, so the
 * last piece of a split write is accepted and changes only the tail. §6 says
 * such a value "cannot be written in pieces"; this pins down what the code
 * actually does.
 */
static void test_WriteFixedTailPieceIsAccepted(void)
{
   const uint8_t u8ar_in[2] = { 0xAAU, 0xBBU };
   const uint8_t u8ar_after[FIXED_LEN] = { 0x11U, 0x22U, 0xAAU, 0xBBU };

   TEST_ASSERT_EQUAL_INT(2, gt_GATT_GenericWrite(&sst_conn, &sst_fixedAttr, u8ar_in, 2U, 2U, 0U));
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_after, su8ar_fixed, FIXED_LEN);
}

static void test_WriteVariableGrowsNeverShrinks(void)
{
   const uint8_t u8ar_ten[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
   const uint8_t u8ar_four[4] = { 0xA0U, 0xA1U, 0xA2U, 0xA3U };
   uint8_t u8ar_buf[VAR_LEN];

   TEST_ASSERT_EQUAL_INT(10, gt_GATT_GenericWrite(&sst_conn, &sst_varAttr, u8ar_ten, 10U, 0U, 0U));
   TEST_ASSERT_EQUAL_UINT16(10U, sst_var.u16_actualLen);

   // §6: a shorter remote write leaves the old tail in place
   TEST_ASSERT_EQUAL_INT(4, gt_GATT_GenericWrite(&sst_conn, &sst_varAttr, u8ar_four, 4U, 0U, 0U));
   TEST_ASSERT_EQUAL_UINT16(10U, sst_var.u16_actualLen);
   TEST_ASSERT_EQUAL_INT(10, gt_GATT_GenericRead(&sst_conn, &sst_varAttr, u8ar_buf,
      sizeof(u8ar_buf), 0U));
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_four, u8ar_buf, 4U);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(&u8ar_ten[4], &u8ar_buf[4], 6U);
}

static void test_WriteVariableOffsetGapExtendsLength(void)
{
   const uint8_t u8ar_in[2] = { 0x55U, 0x66U };

   // §6: bytes before the offset are not zero-filled
   TEST_ASSERT_EQUAL_INT(2, gt_GATT_GenericWrite(&sst_conn, &sst_varAttr, u8ar_in, 2U, 8U, 0U));
   TEST_ASSERT_EQUAL_UINT16(10U, sst_var.u16_actualLen);
   TEST_ASSERT_EQUAL_HEX8(0xEEU, su8ar_var[0]);
   TEST_ASSERT_EQUAL_HEX8(0x55U, su8ar_var[8]);
}

static void test_WriteVariableBounds(void)
{
   uint8_t u8ar_in[VAR_LEN + 1U] = { 0 };

   // Up to the buffer end is fine
   TEST_ASSERT_EQUAL_INT(VAR_LEN, gt_GATT_GenericWrite(&sst_conn, &sst_varAttr, u8ar_in,
      VAR_LEN, 0U, 0U));
   // One byte past the end, and an offset + length that would wrap 16 bits
   TEST_ASSERT_EQUAL_INT(BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN),
      gt_GATT_GenericWrite(&sst_conn, &sst_varAttr, u8ar_in, VAR_LEN + 1U, 0U, 0U));
   TEST_ASSERT_EQUAL_INT(BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN),
      gt_GATT_GenericWrite(&sst_conn, &sst_varAttr, u8ar_in, 2U, 0xFFFFU, 0U));
   TEST_ASSERT_EQUAL_UINT16(VAR_LEN, sst_var.u16_actualLen);
}

static void test_WriteNullDataIsUnlikelyError(void)
{
   const uint8_t u8ar_in[FIXED_LEN] = { 0 };

   sst_fixed.vpt_data = NULL;
   TEST_ASSERT_EQUAL_INT(BT_GATT_ERR(BT_ATT_ERR_UNLIKELY),
      gt_GATT_GenericWrite(&sst_conn, &sst_fixedAttr, u8ar_in, FIXED_LEN, 0U, 0U));
}

static void test_WriteHookSeesUpdatedValueWithoutMutex(void)
{
   const uint8_t u8ar_in[3] = { 0x01U, 0x02U, 0x03U };

   sst_var.fpt_customWriteCb = st_WriteHook;
   TEST_ASSERT_EQUAL_INT(3, gt_GATT_GenericWrite(&sst_conn, &sst_varAttr, u8ar_in, 3U, 1U,
      BT_GATT_WRITE_FLAG_CMD));
   TEST_ASSERT_EQUAL_INT(1, si_hookCalls);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, si_hookMutexHeld, "hook must run with the mutex released");
   TEST_ASSERT_EQUAL_UINT16(3U, su16_hookLen);
   TEST_ASSERT_EQUAL_UINT16(1U, su16_hookOffset);
   TEST_ASSERT_EQUAL_HEX8_MESSAGE(BT_GATT_WRITE_FLAG_CMD, su8_hookFlags, "flags forwarded");
   TEST_ASSERT_EQUAL_UINT16_MESSAGE(4U, su16_hookActualLen, "actual length updated before the hook");
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_in, su8ar_hookBuf, 3U);
   TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(u8ar_in, &su8ar_hookData[1], 3U,
      "value copied before the hook");
}

static void test_WriteHookVetoHasNoRollback(void)
{
   const uint8_t u8ar_in[FIXED_LEN] = { 0xDEU, 0xADU, 0xBEU, 0xEFU };

   sst_fixed.fpt_customWriteCb = st_WriteHook;
   st_hookRet = BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
   TEST_ASSERT_EQUAL_INT(BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED),
      gt_GATT_GenericWrite(&sst_conn, &sst_fixedAttr, u8ar_in, FIXED_LEN, 0U, 0U));
   // §2: the rejected value stays in the local variable
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_in, su8ar_fixed, FIXED_LEN);
}

static void test_WriteHookNotCalledOnLengthError(void)
{
   const uint8_t u8ar_in[FIXED_LEN] = { 0 };

   sst_fixed.fpt_customWriteCb = st_WriteHook;
   (void)gt_GATT_GenericWrite(&sst_conn, &sst_fixedAttr, u8ar_in, 1U, 0U, 0U);
   TEST_ASSERT_EQUAL_INT(0, si_hookCalls);
}

/******************************************************************************/
/*  §4 Application-side API                                                   */
/******************************************************************************/
static void test_LocalReadClipsSilently(void)
{
   uint8_t u8ar_buf[FIXED_LEN] = { 0 };
   uint16_t u16_read = 0xFFFFU;

   gt_GATT_LocalRead(&sst_fixed, u8ar_buf, sizeof(u8ar_buf), &u16_read);
   TEST_ASSERT_EQUAL_UINT16(FIXED_LEN, u16_read);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(su8ar_fixed, u8ar_buf, FIXED_LEN);

   gt_GATT_LocalRead(&sst_fixed, u8ar_buf, 2U, &u16_read);
   TEST_ASSERT_EQUAL_UINT16(2U, u16_read);

   gt_GATT_LocalRead(&sst_var, u8ar_buf, sizeof(u8ar_buf), &u16_read);
   TEST_ASSERT_EQUAL_UINT16_MESSAGE(0U, u16_read, "empty variable-length value");
   TEST_ASSERT_TRUE(sst_mutex.u32_locks >= 3U);
}

static void test_LocalWriteFixed(void)
{
   const uint8_t u8ar_in[FIXED_LEN] = { 5U, 6U, 7U, 8U };
   const uint8_t u8ar_before[FIXED_LEN] = { 0x11U, 0x22U, 0x33U, 0x44U };

   // Wrong length: silently ignored (§4), logged
   gv_GATT_LocalWrite(&sst_fixed, u8ar_in, 3U);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_before, su8ar_fixed, FIXED_LEN);
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("fixed-length characteristic requires"));

   gv_GATT_LocalWrite(&sst_fixed, u8ar_in, FIXED_LEN);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_in, su8ar_fixed, FIXED_LEN);
}

static void test_LocalWriteVariableCanShrinkAndSkipsHook(void)
{
   const uint8_t u8ar_in[VAR_LEN + 1U] = { 0 };
   uint16_t u16_len = 0U;

   sst_var.fpt_customWriteCb = st_WriteHook;
   gv_GATT_LocalWrite(&sst_var, u8ar_in, 10U);
   gt_GATT_GetActualLen(&sst_var, &u16_len);
   TEST_ASSERT_EQUAL_UINT16(10U, u16_len);

   // Unlike a remote write, a local write can shrink the value
   gv_GATT_LocalWrite(&sst_var, u8ar_in, 3U);
   gt_GATT_GetActualLen(&sst_var, &u16_len);
   TEST_ASSERT_EQUAL_UINT16(3U, u16_len);

   // Over capacity: ignored
   gv_GATT_LocalWrite(&sst_var, u8ar_in, VAR_LEN + 1U);
   gt_GATT_GetActualLen(&sst_var, &u16_len);
   TEST_ASSERT_EQUAL_UINT16(3U, u16_len);

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, si_hookCalls, "local writes never call the hook");
}

static void test_GetActualLenFixed(void)
{
   uint16_t u16_len = 0U;

   gt_GATT_GetActualLen(&sst_fixed, &u16_len);
   TEST_ASSERT_EQUAL_UINT16(FIXED_LEN, u16_len);
}

static void test_LocalApiAssertsOnNull(void)
{
   uint8_t u8ar_buf[FIXED_LEN];
   uint16_t u16_n = 0U;

   TEST_ASSERT_TRUE(SIM_EXPECT_ASSERT(gt_GATT_LocalRead(NULL, u8ar_buf, 4U, &u16_n)));
   TEST_ASSERT_TRUE(SIM_EXPECT_ASSERT(gt_GATT_LocalRead(&sst_fixed, NULL, 4U, &u16_n)));
   TEST_ASSERT_TRUE(SIM_EXPECT_ASSERT(gt_GATT_LocalRead(&sst_fixed, u8ar_buf, 4U, NULL)));
   TEST_ASSERT_TRUE(SIM_EXPECT_ASSERT(gv_GATT_LocalWrite(NULL, u8ar_buf, 4U)));
   TEST_ASSERT_TRUE(SIM_EXPECT_ASSERT(gv_GATT_LocalWrite(&sst_fixed, NULL, 4U)));
   TEST_ASSERT_TRUE(SIM_EXPECT_ASSERT(gt_GATT_GetActualLen(NULL, &u16_n)));
   TEST_ASSERT_TRUE(SIM_EXPECT_ASSERT(gt_GATT_GetActualLen(&sst_fixed, NULL)));
   sst_fixed.vpt_data = NULL;
   TEST_ASSERT_TRUE(SIM_EXPECT_ASSERT(gt_GATT_LocalRead(&sst_fixed, u8ar_buf, 4U, &u16_n)));
}

int main(void)
{
   (void)setvbuf(stdout, NULL, _IONBF, 0);
   UNITY_BEGIN();
   RUN_TEST(test_ReadFixedWholeValueUnderMutex);
   RUN_TEST(test_ReadHonoursOffsetAndLength);
   RUN_TEST(test_ReadVariableReturnsOnlyActualLength);
   RUN_TEST(test_ReadNullDataIsUnlikelyError);
   RUN_TEST(test_ReadHookRunsAfterCopyWithoutMutex);
   RUN_TEST(test_ReadHookZeroKeepsNonZeroReplaces);
   RUN_TEST(test_ReadAssertsOnMissingDescriptor);
   RUN_TEST(test_WriteFixedExactLength);
   RUN_TEST(test_WriteFixedWrongLengthRejectedUntouched);
   RUN_TEST(test_WriteFixedTailPieceIsAccepted);
   RUN_TEST(test_WriteVariableGrowsNeverShrinks);
   RUN_TEST(test_WriteVariableOffsetGapExtendsLength);
   RUN_TEST(test_WriteVariableBounds);
   RUN_TEST(test_WriteNullDataIsUnlikelyError);
   RUN_TEST(test_WriteHookSeesUpdatedValueWithoutMutex);
   RUN_TEST(test_WriteHookVetoHasNoRollback);
   RUN_TEST(test_WriteHookNotCalledOnLengthError);
   RUN_TEST(test_LocalReadClipsSilently);
   RUN_TEST(test_LocalWriteFixed);
   RUN_TEST(test_LocalWriteVariableCanShrinkAndSkipsHook);
   RUN_TEST(test_GetActualLenFixed);
   RUN_TEST(test_LocalApiAssertsOnNull);
   return UNITY_END();
}
