/**
 * @file          test_frame.c
 * @brief         Host unit tests for BulkXfer_Frame.c, plus the golden wire
 *                vectors in _TEST/vectors/wire.json that the Python client is
 *                checked against as well.
 *
 * @date          22/09/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "BulkXfer_Frame.h"
#include "BulkXfer_Types.h"
#include "wire_vectors.h"

void setUp(void) {}
void tearDown(void) {}

/******************************************************************************/
/*  Codec                                                                     */
/******************************************************************************/
static void test_ShortRoundTrip(void)
{
   uint8_t u8ar_buf[BLK_MAX_FRAME_LEN];
   uint8_t u8ar_data[BLK_MAX_SHORT_PAYLOAD];
   BlkFrame_T st_f;
   uint16_t u16_len = 0U;

   (void)memset(u8ar_data, 0xA5, sizeof(u8ar_data));

   // Maximum payload (242) fills exactly one 244-byte frame
   u16_len = gu16_BLK_EncodeShort(u8ar_buf, sizeof(u8ar_buf), 0x12U, u8ar_data,
      sizeof(u8ar_data));
   TEST_ASSERT_EQUAL_UINT16(BLK_MAX_FRAME_LEN, u16_len);
   TEST_ASSERT_EQUAL_UINT8(BLK_MAX_SHORT_PAYLOAD, u8ar_buf[0]);
   TEST_ASSERT_EQUAL_INT(0, gi_BLK_FrameParse(u8ar_buf, u16_len, &st_f));
   TEST_ASSERT_EQUAL_HEX8(0x12U, st_f.u8_type);
   TEST_ASSERT_EQUAL_UINT8(BLK_MAX_SHORT_PAYLOAD, st_f.u8_payloadLen);
   TEST_ASSERT_EQUAL_MEMORY(u8ar_data, st_f.u8pt_payload, sizeof(u8ar_data));

   // Empty payload
   u16_len = gu16_BLK_EncodeShort(u8ar_buf, sizeof(u8ar_buf), 0x00U, NULL, 0U);
   TEST_ASSERT_EQUAL_UINT16(2U, u16_len);
   TEST_ASSERT_EQUAL_INT(0, gi_BLK_FrameParse(u8ar_buf, u16_len, &st_f));
   TEST_ASSERT_EQUAL_UINT8(0U, st_f.u8_payloadLen);

   // Reserved type and too-small buffer are refused
   TEST_ASSERT_EQUAL_UINT16(0U, gu16_BLK_EncodeShort(u8ar_buf, sizeof(u8ar_buf), eBFT_START,
      u8ar_data, 1U));
   TEST_ASSERT_EQUAL_UINT16(0U, gu16_BLK_EncodeShort(u8ar_buf, 10U, 0x01U, u8ar_data, 9U));
}

static void test_StartRoundTrip(void)
{
   uint8_t u8ar_buf[BLK_CTRL_FRAME_MAX_LEN];
   BlkFrame_T st_f;
   uint16_t u16_len = gu16_BLK_EncodeStart(u8ar_buf, sizeof(u8ar_buf), 7U, 0x33U,
      0x12345678UL, 240U, 16U, 0xCAFEBABEUL);

   TEST_ASSERT_EQUAL_UINT16(14U, u16_len);
   // Little-endian check of totalLen on the wire
   TEST_ASSERT_EQUAL_HEX8(0x78U, u8ar_buf[4]);
   TEST_ASSERT_EQUAL_HEX8(0x12U, u8ar_buf[7]);
   TEST_ASSERT_EQUAL_INT(0, gi_BLK_FrameParse(u8ar_buf, u16_len, &st_f));
   TEST_ASSERT_EQUAL_HEX8(eBFT_START, st_f.u8_type);
   TEST_ASSERT_EQUAL_UINT8(7U, st_f.u_body.st_start.u8_xferId);
   TEST_ASSERT_EQUAL_HEX8(0x33U, st_f.u_body.st_start.u8_appType);
   TEST_ASSERT_EQUAL_HEX32(0x12345678UL, st_f.u_body.st_start.u32_totalLen);
   TEST_ASSERT_EQUAL_UINT8(240U, st_f.u_body.st_start.u8_chunkSize);
   TEST_ASSERT_EQUAL_UINT8(16U, st_f.u_body.st_start.u8_window);
   TEST_ASSERT_EQUAL_HEX32(0xCAFEBABEUL, st_f.u_body.st_start.u32_crc32);
}

static void test_DataFrame(void)
{
   uint8_t u8ar_buf[BLK_MAX_FRAME_LEN];
   BlkFrame_T st_f;
   uint16_t u16_len = 0U;
   uint16_t u16_idx = 0U;

   // Full 240-byte chunk at MTU 247
   u16_len = gu16_BLK_EncodeDataHeader(u8ar_buf, sizeof(u8ar_buf), 3U, 255U,
      BLK_MAX_CHUNK_LEN);
   TEST_ASSERT_EQUAL_UINT16(BLK_MAX_FRAME_LEN, u16_len);
   for (u16_idx = 0U; u16_idx < BLK_MAX_CHUNK_LEN; u16_idx++)
   {
      u8ar_buf[BLK_DATA_HDR_LEN + u16_idx] = (uint8_t)u16_idx;
   }
   TEST_ASSERT_EQUAL_INT(0, gi_BLK_FrameParse(u8ar_buf, u16_len, &st_f));
   TEST_ASSERT_EQUAL_HEX8(eBFT_DATA, st_f.u8_type);
   TEST_ASSERT_EQUAL_UINT8(3U, st_f.u_body.st_data.u8_xferId);
   TEST_ASSERT_EQUAL_UINT8(255U, st_f.u_body.st_data.u8_seq);
   TEST_ASSERT_EQUAL_UINT8(BLK_MAX_CHUNK_LEN, st_f.u_body.st_data.u8_dataLen);
   TEST_ASSERT_EQUAL_UINT8(239U, st_f.u_body.st_data.u8pt_data[239]);

   // Zero-length data and overflowing buffers are refused
   TEST_ASSERT_EQUAL_UINT16(0U, gu16_BLK_EncodeDataHeader(u8ar_buf, sizeof(u8ar_buf), 1U, 0U, 0U));
   TEST_ASSERT_EQUAL_UINT16(0U, gu16_BLK_EncodeDataHeader(u8ar_buf, 20U, 1U, 0U, 17U));
   TEST_ASSERT_EQUAL_UINT16(20U, gu16_BLK_EncodeDataHeader(u8ar_buf, 20U, 1U, 0U, 16U));

   // A DATA frame without any data byte is malformed
   u8ar_buf[0] = 2U;
   u8ar_buf[1] = eBFT_DATA;
   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_BLK_FrameParse(u8ar_buf, 4U, &st_f));
}

static void test_ControlFrames(void)
{
   uint8_t u8ar_buf[BLK_CTRL_FRAME_MAX_LEN];
   BlkFrame_T st_f;
   uint16_t u16_len = 0U;

   u16_len = gu16_BLK_EncodeAck(u8ar_buf, sizeof(u8ar_buf), 9U, 200U, 16U);
   TEST_ASSERT_EQUAL_UINT16(5U, u16_len);
   TEST_ASSERT_EQUAL_INT(0, gi_BLK_FrameParse(u8ar_buf, u16_len, &st_f));
   TEST_ASSERT_EQUAL_HEX8(eBFT_ACK, st_f.u8_type);
   TEST_ASSERT_EQUAL_UINT8(200U, st_f.u_body.st_ack.u8_seq);
   TEST_ASSERT_EQUAL_UINT8(16U, st_f.u_body.st_ack.u8_window);
   TEST_ASSERT_EQUAL_UINT8(9U, st_f.u_body.st_ack.u8_xferId);

   u16_len = gu16_BLK_EncodeNack(u8ar_buf, sizeof(u8ar_buf), 9U, 4U, eBS_NO_RESOURCES);
   TEST_ASSERT_EQUAL_INT(0, gi_BLK_FrameParse(u8ar_buf, u16_len, &st_f));
   TEST_ASSERT_EQUAL_HEX8(eBFT_NACK, st_f.u8_type);
   TEST_ASSERT_EQUAL_UINT8(4U, st_f.u_body.st_nack.u8_seq);
   TEST_ASSERT_EQUAL_UINT8(eBS_NO_RESOURCES, st_f.u_body.st_nack.u8_reason);

   u16_len = gu16_BLK_EncodeEnd(u8ar_buf, sizeof(u8ar_buf), 9U, eBS_CRC_ERROR);
   TEST_ASSERT_EQUAL_UINT16(4U, u16_len);
   TEST_ASSERT_EQUAL_INT(0, gi_BLK_FrameParse(u8ar_buf, u16_len, &st_f));
   TEST_ASSERT_EQUAL_HEX8(eBFT_END, st_f.u8_type);
   TEST_ASSERT_EQUAL_UINT8(eBS_CRC_ERROR, st_f.u_body.st_end.u8_status);

   u16_len = gu16_BLK_EncodeAbort(u8ar_buf, sizeof(u8ar_buf), 9U, eBS_REJECTED,
      eBAD_BY_RECEIVER);
   TEST_ASSERT_EQUAL_INT(0, gi_BLK_FrameParse(u8ar_buf, u16_len, &st_f));
   TEST_ASSERT_EQUAL_HEX8(eBFT_ABORT, st_f.u8_type);
   TEST_ASSERT_EQUAL_UINT8(eBS_REJECTED, st_f.u_body.st_abort.u8_reason);
   TEST_ASSERT_EQUAL_UINT8(eBAD_BY_RECEIVER, st_f.u_body.st_abort.u8_dir);

   // Buffer one byte too small
   TEST_ASSERT_EQUAL_UINT16(0U, gu16_BLK_EncodeAck(u8ar_buf, 4U, 1U, 1U, 1U));
}

static void test_Malformed(void)
{
   uint8_t u8ar_buf[8] = { 0 };
   BlkFrame_T st_f;

   // Shorter than the header
   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_BLK_FrameParse(u8ar_buf, 1U, &st_f));
   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_BLK_FrameParse(NULL, 4U, &st_f));

   // len byte disagrees with the ATT length (both directions)
   u8ar_buf[0] = 3U;
   u8ar_buf[1] = 0x01U;
   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_BLK_FrameParse(u8ar_buf, 4U, &st_f));
   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_BLK_FrameParse(u8ar_buf, 6U, &st_f));
   TEST_ASSERT_EQUAL_INT(0, gi_BLK_FrameParse(u8ar_buf, 5U, &st_f));

   // Control frame with wrong payload size
   u8ar_buf[0] = 2U;
   u8ar_buf[1] = eBFT_ACK;
   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_BLK_FrameParse(u8ar_buf, 4U, &st_f));

   // Unknown reserved type
   u8ar_buf[0] = 0U;
   u8ar_buf[1] = 0xFFU;
   TEST_ASSERT_EQUAL_INT(-ENOTSUP, gi_BLK_FrameParse(u8ar_buf, 2U, &st_f));
}

static void test_SeqWrap(void)
{
   // The engine expands 8-bit seqs with base + (uint8_t)(seq - (uint8_t)base).
   // Verify the arithmetic across the 255 -> 0 wrap for a 128-frame window.
   uint32_t u32_base = 250U;
   uint8_t u8_seq = (uint8_t)(u32_base + 127U);
   uint32_t u32_abs = u32_base + (uint8_t)(u8_seq - (uint8_t)u32_base);

   TEST_ASSERT_EQUAL_UINT32(377U, u32_abs);

   u32_base = 0x1FFFFU;
   u8_seq = 0x03U;
   u32_abs = u32_base + (uint8_t)(u8_seq - (uint8_t)u32_base);
   TEST_ASSERT_EQUAL_HEX32(0x20003U, u32_abs);
}

/******************************************************************************/
/*  Golden vectors (shared with the Python tests)                             */
/******************************************************************************/
static void test_VectorConstantsMatchLibrary(void)
{
   TEST_ASSERT_EQUAL_UINT(VEC_PROTOCOL_VERSION, BLK_PROTOCOL_VERSION);
   TEST_ASSERT_EQUAL_UINT(VEC_MAX_FRAME_LEN, BLK_MAX_FRAME_LEN);
   TEST_ASSERT_EQUAL_UINT(VEC_WINDOW, BLK_WINDOW_DEFAULT);
   TEST_ASSERT_EQUAL_UINT(VEC_APP_TYPE_MAX, BLK_APP_TYPE_MAX);

   TEST_ASSERT_EQUAL_HEX8(VEC_T_START, eBFT_START);
   TEST_ASSERT_EQUAL_HEX8(VEC_T_DATA, eBFT_DATA);
   TEST_ASSERT_EQUAL_HEX8(VEC_T_ACK, eBFT_ACK);
   TEST_ASSERT_EQUAL_HEX8(VEC_T_NACK, eBFT_NACK);
   TEST_ASSERT_EQUAL_HEX8(VEC_T_END, eBFT_END);
   TEST_ASSERT_EQUAL_HEX8(VEC_T_ABORT, eBFT_ABORT);

   TEST_ASSERT_EQUAL_UINT(VEC_ST_OK, eBS_OK);
   TEST_ASSERT_EQUAL_UINT(VEC_ST_CRC_ERROR, eBS_CRC_ERROR);
   TEST_ASSERT_EQUAL_UINT(VEC_ST_TIMEOUT, eBS_TIMEOUT);
   TEST_ASSERT_EQUAL_UINT(VEC_ST_ABORTED, eBS_ABORTED);
   TEST_ASSERT_EQUAL_UINT(VEC_ST_REMOTE_ABORTED, eBS_REMOTE_ABORTED);
   TEST_ASSERT_EQUAL_UINT(VEC_ST_REJECTED, eBS_REJECTED);
   TEST_ASSERT_EQUAL_UINT(VEC_ST_DISCONNECTED, eBS_DISCONNECTED);
   TEST_ASSERT_EQUAL_UINT(VEC_ST_SOURCE_ERROR, eBS_SOURCE_ERROR);
   TEST_ASSERT_EQUAL_UINT(VEC_ST_SINK_ERROR, eBS_SINK_ERROR);
   TEST_ASSERT_EQUAL_UINT(VEC_ST_PROTOCOL_ERROR, eBS_PROTOCOL_ERROR);
   TEST_ASSERT_EQUAL_UINT(VEC_ST_NO_RESOURCES, eBS_NO_RESOURCES);
   TEST_ASSERT_EQUAL_UINT(VEC_ST_OUT_OF_ORDER, eBS_OUT_OF_ORDER);

   TEST_ASSERT_EQUAL_UINT(VEC_DIR_BY_SENDER, eBAD_BY_SENDER);
   TEST_ASSERT_EQUAL_UINT(VEC_DIR_BY_RECEIVER, eBAD_BY_RECEIVER);
}

/** Encode every vector with the C encoders and compare with the golden bytes. */
static void test_VectorsEncode(void)
{
   uint8_t u8ar_buf[BLK_MAX_FRAME_LEN];
   uint16_t u16_len = 0U;
   uint32_t i;

   for (i = 0U; i < ARRAY_SIZE(gstar_vecFrames); i++)
   {
      const WireVector_T *v = &gstar_vecFrames[i];
      const uint32_t *f = v->u32ar_f;

      (void)memset(u8ar_buf, 0xEE, sizeof(u8ar_buf));
      switch (v->e_kind)
      {
         case eVK_START:
            u16_len = gu16_BLK_EncodeStart(u8ar_buf, sizeof(u8ar_buf), (uint8_t)f[0],
               (uint8_t)f[1], f[2], (uint8_t)f[3], (uint8_t)f[4], f[5]);
            break;
         case eVK_DATA:
            u16_len = gu16_BLK_EncodeDataHeader(u8ar_buf, sizeof(u8ar_buf), (uint8_t)f[0],
               (uint8_t)f[1], v->u8_dataLen);
            (void)memcpy(&u8ar_buf[BLK_DATA_HDR_LEN], v->u8ar_data, v->u8_dataLen);
            break;
         case eVK_ACK:
            u16_len = gu16_BLK_EncodeAck(u8ar_buf, sizeof(u8ar_buf), (uint8_t)f[0],
               (uint8_t)f[1], (uint8_t)f[2]);
            break;
         case eVK_NACK:
            u16_len = gu16_BLK_EncodeNack(u8ar_buf, sizeof(u8ar_buf), (uint8_t)f[0],
               (uint8_t)f[1], (uint8_t)f[2]);
            break;
         case eVK_END:
            u16_len = gu16_BLK_EncodeEnd(u8ar_buf, sizeof(u8ar_buf), (uint8_t)f[0],
               (uint8_t)f[1]);
            break;
         case eVK_ABORT:
            u16_len = gu16_BLK_EncodeAbort(u8ar_buf, sizeof(u8ar_buf), (uint8_t)f[0],
               (uint8_t)f[1], (uint8_t)f[2]);
            break;
         default:
            u16_len = gu16_BLK_EncodeShort(u8ar_buf, sizeof(u8ar_buf), (uint8_t)f[0],
               v->u8ar_data, v->u8_dataLen);
            break;
      }
      TEST_ASSERT_EQUAL_UINT16_MESSAGE(v->u8_wireLen, u16_len, v->cpt_name);
      TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(v->u8ar_wire, u8ar_buf, v->u8_wireLen, v->cpt_name);
   }
}

/** Parse every golden vector and check each field. */
static void test_VectorsParse(void)
{
   BlkFrame_T st_f;
   uint32_t i;

   for (i = 0U; i < ARRAY_SIZE(gstar_vecFrames); i++)
   {
      const WireVector_T *v = &gstar_vecFrames[i];
      const uint32_t *f = v->u32ar_f;

      TEST_ASSERT_EQUAL_INT_MESSAGE(0, gi_BLK_FrameParse(v->u8ar_wire, v->u8_wireLen, &st_f),
         v->cpt_name);
      switch (v->e_kind)
      {
         case eVK_START:
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(eBFT_START, st_f.u8_type, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[0], st_f.u_body.st_start.u8_xferId, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[1], st_f.u_body.st_start.u8_appType, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(f[2], st_f.u_body.st_start.u32_totalLen, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[3], st_f.u_body.st_start.u8_chunkSize, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[4], st_f.u_body.st_start.u8_window, v->cpt_name);
            TEST_ASSERT_EQUAL_HEX32_MESSAGE(f[5], st_f.u_body.st_start.u32_crc32, v->cpt_name);
            break;
         case eVK_DATA:
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(eBFT_DATA, st_f.u8_type, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[0], st_f.u_body.st_data.u8_xferId, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[1], st_f.u_body.st_data.u8_seq, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(v->u8_dataLen, st_f.u_body.st_data.u8_dataLen, v->cpt_name);
            TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(v->u8ar_data, st_f.u_body.st_data.u8pt_data,
               v->u8_dataLen, v->cpt_name);
            break;
         case eVK_ACK:
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(eBFT_ACK, st_f.u8_type, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[0], st_f.u_body.st_ack.u8_xferId, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[1], st_f.u_body.st_ack.u8_seq, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[2], st_f.u_body.st_ack.u8_window, v->cpt_name);
            break;
         case eVK_NACK:
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(eBFT_NACK, st_f.u8_type, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[0], st_f.u_body.st_nack.u8_xferId, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[1], st_f.u_body.st_nack.u8_seq, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[2], st_f.u_body.st_nack.u8_reason, v->cpt_name);
            break;
         case eVK_END:
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(eBFT_END, st_f.u8_type, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[0], st_f.u_body.st_end.u8_xferId, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[1], st_f.u_body.st_end.u8_status, v->cpt_name);
            break;
         case eVK_ABORT:
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(eBFT_ABORT, st_f.u8_type, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[0], st_f.u_body.st_abort.u8_xferId, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[1], st_f.u_body.st_abort.u8_reason, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(f[2], st_f.u_body.st_abort.u8_dir, v->cpt_name);
            break;
         default:
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(f[0], st_f.u8_type, v->cpt_name);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(v->u8_dataLen, st_f.u8_payloadLen, v->cpt_name);
            if (v->u8_dataLen > 0U)
            {
               TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(v->u8ar_data, st_f.u8pt_payload,
                  v->u8_dataLen, v->cpt_name);
            }
            break;
      }
   }
}

int main(void)
{
   (void)setvbuf(stdout, NULL, _IONBF, 0);
   UNITY_BEGIN();
   RUN_TEST(test_ShortRoundTrip);
   RUN_TEST(test_StartRoundTrip);
   RUN_TEST(test_DataFrame);
   RUN_TEST(test_ControlFrames);
   RUN_TEST(test_Malformed);
   RUN_TEST(test_SeqWrap);
   RUN_TEST(test_VectorConstantsMatchLibrary);
   RUN_TEST(test_VectorsEncode);
   RUN_TEST(test_VectorsParse);
   return UNITY_END();
}
