/**
 * @file          test_devicecert_store.c
 * @brief         Host unit tests for the certificate storage in
 *                _ASW/_DEVICE_CERT/DeviceCert.c (gt_StoreCACert,
 *                gt_StoreDeviceCert, gt_LoadStoredCerts, gt_RemoveStoredCerts,
 *                gv_ClearCertData) against an in-memory PSA ITS. DeviceCert.c is
 *                included to reach its static helpers.
 *                Contract: _DOC/Provisioning/README.md (storage).
 *
 * @date          02/10/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#include "DeviceCert.c"
#include "unity.h"

/******************************************************************************/
/*  Stubs: CSR configuration and an in-memory ITS                             */
/******************************************************************************/
CSRConfig_T gst_CSRConfig = {
   .u8_certPositionOnITS = DEVICE_CERTIFICATE_ITS_OFFSET,
   .u8_CACertPositionOnITS = DEVICE_CA_CERT_ITS_OFFSET,
   .u8_CSRPositionOnITS = DEVICE_CSR_ITS_OFFSET,
};

#define ITS_SLOTS                            (4U)
#define ITS_MAX_ENTRY                        (1100U)

typedef struct
{
   bool b_used;
   psa_storage_uid_t t_uid;
   size_t t_len;
   uint8_t u8ar_data[ITS_MAX_ENTRY];
} ItsEntry_T;

static ItsEntry_T sstar_its[ITS_SLOTS];
static psa_status_t st_setRet;
static psa_status_t st_getRet;
static psa_status_t st_removeRet;
static size_t st_getShort;      /* bytes psa_its_get() leaves out (short read) */
static uint32_t su32_setCalls;

static ItsEntry_T *sstpt_Find(psa_storage_uid_t t_uid)
{
   uint32_t i;

   for (i = 0U; i < ITS_SLOTS; i++)
   {
      if (sstar_its[i].b_used && (sstar_its[i].t_uid == t_uid)) { return &sstar_its[i]; }
   }
   return NULL;
}

psa_status_t psa_its_set(psa_storage_uid_t uid, size_t data_length, const void *p_data,
   psa_storage_create_flags_t create_flags)
{
   ItsEntry_T *e = sstpt_Find(uid);
   uint32_t i;

   su32_setCalls++;
   TEST_ASSERT_EQUAL_UINT32(PSA_STORAGE_FLAG_NONE, create_flags);
   TEST_ASSERT_TRUE_MESSAGE(data_length <= ITS_MAX_ENTRY, "above CONFIG_SECURE_STORAGE_ITS_MAX_DATA_SIZE");
   if (st_setRet != PSA_SUCCESS) { return st_setRet; }
   for (i = 0U; (e == NULL) && (i < ITS_SLOTS); i++)
   {
      if (!sstar_its[i].b_used) { e = &sstar_its[i]; }
   }
   TEST_ASSERT_NOT_NULL(e);
   e->b_used = true;
   e->t_uid = uid;
   e->t_len = data_length;
   (void)memcpy(e->u8ar_data, p_data, data_length);
   return PSA_SUCCESS;
}

psa_status_t psa_its_get(psa_storage_uid_t uid, size_t data_offset, size_t data_size,
   void *p_data, size_t *p_data_length)
{
   ItsEntry_T *e = sstpt_Find(uid);

   TEST_ASSERT_EQUAL_UINT32(0U, data_offset);
   if (st_getRet != PSA_SUCCESS) { return st_getRet; }
   if (e == NULL) { return PSA_ERROR_DOES_NOT_EXIST; }
   TEST_ASSERT_TRUE(data_size >= e->t_len);
   (void)memcpy(p_data, e->u8ar_data, e->t_len - st_getShort);
   *p_data_length = e->t_len - st_getShort;
   return PSA_SUCCESS;
}

psa_status_t psa_its_get_info(psa_storage_uid_t uid, struct psa_storage_info_t *p_info)
{
   ItsEntry_T *e = sstpt_Find(uid);

   if (e == NULL) { return PSA_ERROR_DOES_NOT_EXIST; }
   p_info->size = e->t_len;
   p_info->flags = PSA_STORAGE_FLAG_NONE;
   return PSA_SUCCESS;
}

psa_status_t psa_its_remove(psa_storage_uid_t uid)
{
   ItsEntry_T *e = sstpt_Find(uid);

   if (st_removeRet != PSA_SUCCESS) { return st_removeRet; }
   if (e == NULL) { return PSA_ERROR_DOES_NOT_EXIST; }
   e->b_used = false;
   return PSA_SUCCESS;
}

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/
#define UID_DEV                              (0x4445564345525401ULL)
#define UID_CA                               (0x4445564345525403ULL)

static void sv_FillCa(uint16_t u16_len)
{
   uint32_t i;

   gst_CACertData.u8_isCACertReceived = 1U;
   gst_CACertData.u16_CACertLen = u16_len;
   for (i = 0U; i < u16_len; i++) { gst_CACertData.u8ar_CACert[i] = (uint8_t)(i * 3U + 1U); }
}

static void sv_FillDev(uint16_t u16_len)
{
   uint32_t i;

   gst_deviceCertData.u8_isDeviceCertGenerated = 1U;
   gst_deviceCertData.u16_deviceCertLen = u16_len;
   for (i = 0U; i < u16_len; i++) { gst_deviceCertData.u8ar_DeviceCert[i] = (uint8_t)(i * 5U + 2U); }
}

static void sv_StorePair(uint16_t u16_caLen, uint16_t u16_devLen)
{
   sv_FillCa(u16_caLen);
   sv_FillDev(u16_devLen);
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, gt_StoreCACert());
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, gt_StoreDeviceCert());
   gv_ClearCertData();
}

void setUp(void)
{
   (void)memset(sstar_its, 0, sizeof(sstar_its));
   (void)memset(&gst_CACertData, 0, sizeof(gst_CACertData));
   (void)memset(&gst_deviceCertData, 0, sizeof(gst_deviceCertData));
   st_setRet = PSA_SUCCESS;
   st_getRet = PSA_SUCCESS;
   st_removeRet = PSA_SUCCESS;
   st_getShort = 0U;
   su32_setCalls = 0U;
   gv_SimLogClear();
}

void tearDown(void) {}

/******************************************************************************/
/*  Store                                                                     */
/******************************************************************************/
static void test_StoreWritesRecordUpToTheDerLength(void)
{
   ItsEntry_T *e;

   sv_FillCa(412U);
   sv_FillDev(DEVICE_CERT_MAX_DER_LEN);
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, gt_StoreCACert());
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, gt_StoreDeviceCert());

   // UIDs "DEVCERT" | 3 (CA) and | 1 (device); [flag][u16 len][DER], packed
   e = sstpt_Find(UID_CA);
   TEST_ASSERT_NOT_NULL(e);
   TEST_ASSERT_EQUAL_UINT32(3U + 412U, e->t_len);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(&gst_CACertData, e->u8ar_data, e->t_len);
   e = sstpt_Find(UID_DEV);
   TEST_ASSERT_NOT_NULL(e);
   TEST_ASSERT_EQUAL_UINT32(3U + DEVICE_CERT_MAX_DER_LEN, e->t_len);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(&gst_deviceCertData, e->u8ar_data, e->t_len);
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("CA certificate stored in trusted storage (412 bytes)"));
}

static void test_StoreRefusesAnInvalidRecord(void)
{
   // Not marked valid, empty, too long
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_ARGUMENT, gt_StoreCACert());
   sv_FillDev(0U);
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_ARGUMENT, gt_StoreDeviceCert());
   gst_deviceCertData.u16_deviceCertLen = DEVICE_CERT_MAX_DER_LEN + 1U;
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_ARGUMENT, gt_StoreDeviceCert());
   TEST_ASSERT_EQUAL_UINT32(0U, su32_setCalls);
}

static void test_StoreErrorIsReturned(void)
{
   sv_FillCa(100U);
   st_setRet = PSA_ERROR_INSUFFICIENT_STORAGE;
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INSUFFICIENT_STORAGE, gt_StoreCACert());
   TEST_ASSERT_NULL(sstpt_Find(UID_CA));
}

/******************************************************************************/
/*  Load                                                                      */
/******************************************************************************/
static void test_LoadRoundTrip(void)
{
   uint32_t i;

   sv_StorePair(300U, 500U);
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, gt_LoadStoredCerts());
   TEST_ASSERT_EQUAL_UINT8(1U, gst_CACertData.u8_isCACertReceived);
   TEST_ASSERT_EQUAL_UINT16(300U, gst_CACertData.u16_CACertLen);
   TEST_ASSERT_EQUAL_UINT8(1U, gst_deviceCertData.u8_isDeviceCertGenerated);
   TEST_ASSERT_EQUAL_UINT16(500U, gst_deviceCertData.u16_deviceCertLen);
   for (i = 0U; i < 500U; i++)
   {
      TEST_ASSERT_EQUAL_HEX8((uint8_t)(i * 5U + 2U), gst_deviceCertData.u8ar_DeviceCert[i]);
   }
   TEST_ASSERT_EQUAL_HEX8(1U, gst_CACertData.u8ar_CACert[0]);
}

static void test_LoadNothingStored(void)
{
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_DOES_NOT_EXIST, gt_LoadStoredCerts());
   TEST_ASSERT_EQUAL_UINT8(0U, gst_CACertData.u8_isCACertReceived);
   TEST_ASSERT_EQUAL_UINT8(0U, gst_deviceCertData.u8_isDeviceCertGenerated);
}

static void test_LoadIncompletePairIsCorrupt(void)
{
   sv_StorePair(300U, 500U);
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, psa_its_remove(UID_DEV));
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_DATA_CORRUPT, gt_LoadStoredCerts());
   TEST_ASSERT_EQUAL_UINT8_MESSAGE(0U, gst_CACertData.u8_isCACertReceived, "a half pair is not kept");

   sv_StorePair(300U, 500U);
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, psa_its_remove(UID_CA));
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_DATA_CORRUPT, gt_LoadStoredCerts());
   TEST_ASSERT_EQUAL_UINT8(0U, gst_deviceCertData.u8_isDeviceCertGenerated);
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("Only one certificate of the pair is stored"));
}

static void test_LoadRejectsMalformedRecords(void)
{
   ItsEntry_T *e;

   // Size: header only, and larger than the record
   sv_StorePair(300U, 500U);
   sstpt_Find(UID_DEV)->t_len = 3U;
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_DATA_CORRUPT, gt_LoadStoredCerts());
   sstpt_Find(UID_DEV)->t_len = sizeof(DeviceCertData_T) + 1U;
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_DATA_CORRUPT, gt_LoadStoredCerts());

   // Flag not set
   sv_StorePair(300U, 500U);
   sstpt_Find(UID_CA)->u8ar_data[0] = 0U;
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_DATA_CORRUPT, gt_LoadStoredCerts());

   // Length field disagrees with the entry size
   sv_StorePair(300U, 500U);
   e = sstpt_Find(UID_DEV);
   e->u8ar_data[1] = (uint8_t)(e->u8ar_data[1] + 1U);
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_DATA_CORRUPT, gt_LoadStoredCerts());

   // Short read
   sv_StorePair(300U, 500U);
   st_getShort = 1U;
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_DATA_CORRUPT, gt_LoadStoredCerts());
   TEST_ASSERT_EQUAL_UINT8(0U, gst_CACertData.u8_isCACertReceived);
   TEST_ASSERT_EQUAL_UINT8(0U, gst_deviceCertData.u8_isDeviceCertGenerated);
}

static void test_LoadErrorIsReturned(void)
{
   // An AEAD failure in the secure storage, and a plain read error
   sv_StorePair(300U, 500U);
   st_getRet = PSA_ERROR_INVALID_SIGNATURE;
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_INVALID_SIGNATURE, gt_LoadStoredCerts());
   st_getRet = PSA_ERROR_STORAGE_FAILURE;
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_STORAGE_FAILURE, gt_LoadStoredCerts());
   TEST_ASSERT_EQUAL_UINT8(0U, gst_CACertData.u8_isCACertReceived);
   // Load never deletes: the caller decides
   TEST_ASSERT_NOT_NULL(sstpt_Find(UID_CA));
   TEST_ASSERT_NOT_NULL(sstpt_Find(UID_DEV));
}

/******************************************************************************/
/*  Remove                                                                    */
/******************************************************************************/
static void test_RemoveDeletesBothAndKeepsRam(void)
{
   sv_StorePair(300U, 500U);
   sv_FillCa(300U);
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, gt_RemoveStoredCerts());
   TEST_ASSERT_NULL(sstpt_Find(UID_CA));
   TEST_ASSERT_NULL(sstpt_Find(UID_DEV));
   TEST_ASSERT_EQUAL_UINT8_MESSAGE(1U, gst_CACertData.u8_isCACertReceived, "RAM is the caller's");

   // Nothing stored is not an error
   TEST_ASSERT_EQUAL_INT(PSA_SUCCESS, gt_RemoveStoredCerts());

   gv_ClearCertData();
   TEST_ASSERT_EQUAL_UINT8(0U, gst_CACertData.u8_isCACertReceived);
}

static void test_RemoveErrorIsReturned(void)
{
   sv_StorePair(300U, 500U);
   st_removeRet = PSA_ERROR_STORAGE_FAILURE;
   TEST_ASSERT_EQUAL_INT(PSA_ERROR_STORAGE_FAILURE, gt_RemoveStoredCerts());
}

int main(void)
{
   (void)setvbuf(stdout, NULL, _IONBF, 0);
   UNITY_BEGIN();
   RUN_TEST(test_StoreWritesRecordUpToTheDerLength);
   RUN_TEST(test_StoreRefusesAnInvalidRecord);
   RUN_TEST(test_StoreErrorIsReturned);
   RUN_TEST(test_LoadRoundTrip);
   RUN_TEST(test_LoadNothingStored);
   RUN_TEST(test_LoadIncompletePairIsCorrupt);
   RUN_TEST(test_LoadRejectsMalformedRecords);
   RUN_TEST(test_LoadErrorIsReturned);
   RUN_TEST(test_RemoveDeletesBothAndKeepsRam);
   RUN_TEST(test_RemoveErrorIsReturned);
   return UNITY_END();
}
