/**
 * @file          test_helpers.c
 * @brief         Host unit tests for _ASW/_GENERIX/Helpers.c.
 * @date          29/09/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "Helpers.h"

void setUp(void) {}
void tearDown(void) {}

static void test_ReverseCopy(void)
{
   const uint8_t u8ar_src[5] = { 1U, 2U, 3U, 4U, 5U };
   const uint8_t u8ar_exp[5] = { 5U, 4U, 3U, 2U, 1U };
   uint8_t u8ar_dst[6] = { 0U, 0U, 0U, 0U, 0U, 0xEEU };

   gv_ReverseByteOrder(u8ar_dst, u8ar_src, sizeof(u8ar_src));
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_exp, u8ar_dst, sizeof(u8ar_exp));
   TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xEEU, u8ar_dst[5], "wrote past the end");
}

static void test_ReverseInPlaceEvenAndOdd(void)
{
   uint8_t u8ar_even[4] = { 0xA1U, 0xB2U, 0xC3U, 0xD4U };
   const uint8_t u8ar_evenExp[4] = { 0xD4U, 0xC3U, 0xB2U, 0xA1U };
   uint8_t u8ar_odd[3] = { 1U, 2U, 3U };
   const uint8_t u8ar_oddExp[3] = { 3U, 2U, 1U };

   gv_ReverseByteOrder(u8ar_even, u8ar_even, sizeof(u8ar_even));
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_evenExp, u8ar_even, sizeof(u8ar_even));
   gv_ReverseByteOrder(u8ar_odd, u8ar_odd, sizeof(u8ar_odd));
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_oddExp, u8ar_odd, sizeof(u8ar_odd));
}

/** A 128-bit UUID, the use in the project (little-endian stack <-> string order). */
static void test_ReverseUuidRoundTrip(void)
{
   uint8_t u8ar_uuid[16];
   uint8_t u8ar_rev[16];
   uint8_t u8ar_back[16];
   uint32_t i;

   for (i = 0U; i < sizeof(u8ar_uuid); i++) { u8ar_uuid[i] = (uint8_t)(i * 17U); }
   gv_ReverseByteOrder(u8ar_rev, u8ar_uuid, sizeof(u8ar_uuid));
   TEST_ASSERT_EQUAL_HEX8(u8ar_uuid[15], u8ar_rev[0]);
   gv_ReverseByteOrder(u8ar_back, u8ar_rev, sizeof(u8ar_rev));
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_uuid, u8ar_back, sizeof(u8ar_uuid));
}

static void test_ReverseZeroAndOneByte(void)
{
   uint8_t u8ar_src[1] = { 0x5AU };
   uint8_t u8ar_dst[1] = { 0x00U };

   gv_ReverseByteOrder(u8ar_dst, u8ar_src, 0U);
   TEST_ASSERT_EQUAL_HEX8(0x00U, u8ar_dst[0]);
   gv_ReverseByteOrder(u8ar_dst, u8ar_src, 1U);
   TEST_ASSERT_EQUAL_HEX8(0x5AU, u8ar_dst[0]);
   gv_ReverseByteOrder(u8ar_src, u8ar_src, 1U);
   TEST_ASSERT_EQUAL_HEX8(0x5AU, u8ar_src[0]);
}

static void test_ReverseNullIsNoOp(void)
{
   uint8_t u8ar_buf[2] = { 1U, 2U };

   gv_ReverseByteOrder(NULL, u8ar_buf, 2U);
   gv_ReverseByteOrder(u8ar_buf, NULL, 2U);
   TEST_ASSERT_EQUAL_HEX8(1U, u8ar_buf[0]);
   TEST_ASSERT_EQUAL_HEX8(2U, u8ar_buf[1]);
}

int main(void)
{
   (void)setvbuf(stdout, NULL, _IONBF, 0);
   UNITY_BEGIN();
   RUN_TEST(test_ReverseCopy);
   RUN_TEST(test_ReverseInPlaceEvenAndOdd);
   RUN_TEST(test_ReverseUuidRoundTrip);
   RUN_TEST(test_ReverseZeroAndOneByte);
   RUN_TEST(test_ReverseNullIsNoOp);
   return UNITY_END();
}
