/**
 * @file          test_pair_oob.c
 * @brief         Host unit tests for the signed OOB data of pairing
 *                (_ASW/_PAIR/PairOob.c), with the REAL TF-PSA-Crypto. Nothing
 *                cryptographic is stubbed: a frame signed here verifies, the
 *                vector frame signed by Python's cryptography (wire.json
 *                "pairing.oob") verifies here, and a frame altered in any
 *                signed byte, or checked with the addresses swapped, does not.
 *                Contract: _DOC/Pairing/PROTOCOL.md §4.
 *
 * @date          06/10/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#include "PairOob.c"
#include "unity.h"
#include "wire_vectors.h"

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/
static psa_key_id_t st_signKey;
static psa_key_id_t st_verifyKey;

/** Import the vector's private key for signing (volatile). */
static psa_key_id_t st_ImportPrivate(void)
{
   psa_key_attributes_t st_attr = PSA_KEY_ATTRIBUTES_INIT;
   psa_key_id_t t_id = 0;

   psa_set_key_type(&st_attr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
   psa_set_key_bits(&st_attr, 256);
   psa_set_key_usage_flags(&st_attr, PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_VERIFY_MESSAGE);
   psa_set_key_algorithm(&st_attr, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, psa_import_key(&st_attr, gu8ar_vecPairOobPrivKey,
      VEC_PAIR_VEC_PRIVKEY_LEN, &t_id));
   psa_reset_key_attributes(&st_attr);
   return t_id;
}

/** Import the vector's public key for verification, as
    ge_VerifyRemoteDeviceCertificate() imports a peer key. */
static psa_key_id_t st_ImportPublic(void)
{
   psa_key_attributes_t st_attr = PSA_KEY_ATTRIBUTES_INIT;
   psa_key_id_t t_id = 0;

   psa_set_key_type(&st_attr, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
   psa_set_key_bits(&st_attr, 256);
   psa_set_key_usage_flags(&st_attr, PSA_KEY_USAGE_VERIFY_MESSAGE | PSA_KEY_USAGE_VERIFY_HASH);
   psa_set_key_algorithm(&st_attr, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, psa_import_key(&st_attr, gu8ar_vecPairOobPubKey,
      VEC_PAIR_VEC_PUBKEY_LEN, &t_id));
   psa_reset_key_attributes(&st_attr);
   return t_id;
}

void setUp(void)
{
   st_signKey = st_ImportPrivate();
   st_verifyKey = st_ImportPublic();
}

void tearDown(void)
{
   (void)psa_destroy_key(st_signKey);
   (void)psa_destroy_key(st_verifyKey);
}

/******************************************************************************/
/*  Layout                                                                    */
/******************************************************************************/
static void test_SizesMatchTheProtocol(void)
{
   TEST_ASSERT_EQUAL_UINT(VEC_PAIR_OOB_FRAME_LEN, PAIR_OOB_FRAME_LEN);
   TEST_ASSERT_EQUAL_UINT(VEC_PAIR_OOB_SIGNED_LEN, PAIR_OOB_SIGNED_LEN);
   TEST_ASSERT_EQUAL_UINT(VEC_PAIR_VEC_SENDER_LEN, PAIR_ADDR_LEN);
   TEST_ASSERT_EQUAL_UINT(VEC_PAIR_VEC_SIG_LEN, PAIR_OOB_SIG_LEN);
}

static void test_SignedMessageIsRandConfirmSenderReceiver(void)
{
   uint8_t u8ar_msg[PAIR_OOB_SIGNED_LEN];

   sv_BuildMessage(gu8ar_vecPairOobR, gu8ar_vecPairOobC, gu8ar_vecPairOobSender,
      gu8ar_vecPairOobReceiver, u8ar_msg);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(gu8ar_vecPairOobMessage, u8ar_msg, VEC_PAIR_VEC_MESSAGE_LEN);
}

/******************************************************************************/
/*  Sign and verify                                                           */
/******************************************************************************/
static void test_SignedFrameVerifies(void)
{
   uint8_t u8ar_frame[PAIR_OOB_FRAME_LEN];

   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, gt_PairOob_Sign(st_signKey, gu8ar_vecPairOobR,
      gu8ar_vecPairOobC, gu8ar_vecPairOobSender, gu8ar_vecPairOobReceiver, u8ar_frame));
   // [r][c] in clear, then the signature
   TEST_ASSERT_EQUAL_HEX8_ARRAY(gu8ar_vecPairOobR, &u8ar_frame[0], 16U);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(gu8ar_vecPairOobC, &u8ar_frame[16], 16U);
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, gt_PairOob_Verify(st_verifyKey, u8ar_frame,
      gu8ar_vecPairOobSender, gu8ar_vecPairOobReceiver));
}

static void test_VectorFrameFromPythonVerifies(void)
{
   TEST_ASSERT_EQUAL_HEX8_ARRAY(gu8ar_vecPairOobR, &gu8ar_vecPairOobFrame[0], 16U);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(gu8ar_vecPairOobSig, &gu8ar_vecPairOobFrame[32], 64U);
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, gt_PairOob_Verify(st_verifyKey, gu8ar_vecPairOobFrame,
      gu8ar_vecPairOobSender, gu8ar_vecPairOobReceiver));
}

static void test_EveryAlteredByteIsRejected(void)
{
   uint8_t u8ar_frame[PAIR_OOB_FRAME_LEN];
   char car_msg[24];
   uint32_t i;

   for (i = 0U; i < PAIR_OOB_FRAME_LEN; i++)
   {
      (void)memcpy(u8ar_frame, gu8ar_vecPairOobFrame, sizeof(u8ar_frame));
      u8ar_frame[i] ^= 0x01U;
      (void)snprintf(car_msg, sizeof(car_msg), "byte %u", (unsigned)i);
      TEST_ASSERT_EQUAL_INT_MESSAGE(PSA_ERROR_INVALID_SIGNATURE, gt_PairOob_Verify(st_verifyKey,
         u8ar_frame, gu8ar_vecPairOobSender, gu8ar_vecPairOobReceiver), car_msg);
   }
}

static void test_AddressesAreBound(void)
{
   uint8_t u8ar_other[PAIR_ADDR_LEN];

   // Made for another receiver, or replayed in the other direction
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_SIGNATURE, gt_PairOob_Verify(st_verifyKey,
      gu8ar_vecPairOobFrame, gu8ar_vecPairOobReceiver, gu8ar_vecPairOobSender));
   (void)memcpy(u8ar_other, gu8ar_vecPairOobReceiver, sizeof(u8ar_other));
   u8ar_other[1] ^= 0x80U;
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_SIGNATURE, gt_PairOob_Verify(st_verifyKey,
      gu8ar_vecPairOobFrame, gu8ar_vecPairOobSender, u8ar_other));
   // The address type is signed too
   (void)memcpy(u8ar_other, gu8ar_vecPairOobSender, sizeof(u8ar_other));
   u8ar_other[0] ^= 0x01U;
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_SIGNATURE, gt_PairOob_Verify(st_verifyKey,
      gu8ar_vecPairOobFrame, u8ar_other, gu8ar_vecPairOobReceiver));
}

static void test_WrongKeyIsRejected(void)
{
   psa_key_attributes_t st_attr = PSA_KEY_ATTRIBUTES_INIT;
   psa_key_id_t t_other = 0;
   uint8_t u8ar_frame[PAIR_OOB_FRAME_LEN];

   psa_set_key_type(&st_attr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
   psa_set_key_bits(&st_attr, 256);
   psa_set_key_usage_flags(&st_attr, PSA_KEY_USAGE_SIGN_MESSAGE);
   psa_set_key_algorithm(&st_attr, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, psa_generate_key(&st_attr, &t_other));

   // Signed by a key the certificate does not carry
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, gt_PairOob_Sign(t_other, gu8ar_vecPairOobR,
      gu8ar_vecPairOobC, gu8ar_vecPairOobSender, gu8ar_vecPairOobReceiver, u8ar_frame));
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_SIGNATURE, gt_PairOob_Verify(st_verifyKey,
      u8ar_frame, gu8ar_vecPairOobSender, gu8ar_vecPairOobReceiver));
   (void)psa_destroy_key(t_other);
}

/******************************************************************************/
/*  Errors                                                                    */
/******************************************************************************/
static void test_NullArgumentsAreRefused(void)
{
   uint8_t u8ar_frame[PAIR_OOB_FRAME_LEN];
   const uint8_t *u8pt_s = gu8ar_vecPairOobSender;
   const uint8_t *u8pt_r = gu8ar_vecPairOobReceiver;

   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_ARGUMENT,
      gt_PairOob_Sign(st_signKey, NULL, gu8ar_vecPairOobC, u8pt_s, u8pt_r, u8ar_frame));
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_ARGUMENT,
      gt_PairOob_Sign(st_signKey, gu8ar_vecPairOobR, NULL, u8pt_s, u8pt_r, u8ar_frame));
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_ARGUMENT,
      gt_PairOob_Sign(st_signKey, gu8ar_vecPairOobR, gu8ar_vecPairOobC, NULL, u8pt_r, u8ar_frame));
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_ARGUMENT,
      gt_PairOob_Sign(st_signKey, gu8ar_vecPairOobR, gu8ar_vecPairOobC, u8pt_s, NULL, u8ar_frame));
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_ARGUMENT,
      gt_PairOob_Sign(st_signKey, gu8ar_vecPairOobR, gu8ar_vecPairOobC, u8pt_s, u8pt_r, NULL));
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_ARGUMENT,
      gt_PairOob_Verify(st_verifyKey, NULL, u8pt_s, u8pt_r));
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_ARGUMENT,
      gt_PairOob_Verify(st_verifyKey, gu8ar_vecPairOobFrame, NULL, u8pt_r));
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_ARGUMENT,
      gt_PairOob_Verify(st_verifyKey, gu8ar_vecPairOobFrame, u8pt_s, NULL));
}

static void test_PsaErrorsArePassedBack(void)
{
   uint8_t u8ar_frame[PAIR_OOB_FRAME_LEN];

   // No such key
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_HANDLE, gt_PairOob_Sign(0x7FFFAAAAU,
      gu8ar_vecPairOobR, gu8ar_vecPairOobC, gu8ar_vecPairOobSender, gu8ar_vecPairOobReceiver,
      u8ar_frame));
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_HANDLE, gt_PairOob_Verify(0x7FFFAAAAU,
      gu8ar_vecPairOobFrame, gu8ar_vecPairOobSender, gu8ar_vecPairOobReceiver));
   // A public key cannot sign
   TEST_ASSERT_NOT_EQUAL(PSA_SUCCESS, gt_PairOob_Sign(st_verifyKey, gu8ar_vecPairOobR,
      gu8ar_vecPairOobC, gu8ar_vecPairOobSender, gu8ar_vecPairOobReceiver, u8ar_frame));
}

int main(void)
{
   (void)setvbuf(stdout, NULL, _IONBF, 0);

   // Check if the crypto library is usable at all
   if (psa_crypto_init() != PSA_SUCCESS)
   {
      printf("psa_crypto_init failed\n");
      return 1;
   }

   UNITY_BEGIN();
   RUN_TEST(test_SizesMatchTheProtocol);
   RUN_TEST(test_SignedMessageIsRandConfirmSenderReceiver);
   RUN_TEST(test_SignedFrameVerifies);
   RUN_TEST(test_VectorFrameFromPythonVerifies);
   RUN_TEST(test_EveryAlteredByteIsRejected);
   RUN_TEST(test_AddressesAreBound);
   RUN_TEST(test_WrongKeyIsRejected);
   RUN_TEST(test_NullArgumentsAreRefused);
   RUN_TEST(test_PsaErrorsArePassedBack);
   return UNITY_END();
}
