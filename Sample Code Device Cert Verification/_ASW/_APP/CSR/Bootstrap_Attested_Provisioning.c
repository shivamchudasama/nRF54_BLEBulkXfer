/**
 * @file          Bootstrap_Attested_Provisioning.c
 * @brief         Standalone reference implementation for bootstrap-certificate-
 *                based device provisioning.
 * @date          13/04/2026
 * @author        Codex
 * @copyright     Bajaj Auto Technology Limited (BATL)
 *
 * @details
 * This file intentionally does not modify or hook into the current CSR/device
 * certificate flow. It provides a new, self-contained reference path for the
 * following provisioning strategy:
 *
 * 1.  A bootstrap certificate is provisioned at EOL and stored in secure flash.
 * 2.  During CSR generation, the device generates its UUID.
 * 3.  The device computes a binding hash over the bootstrap certificate using
 *     the device UUID as the HMAC key.
 * 4.  The device generates a normal DER CSR using its device EC key.
 * 5.  The device wraps {UUID, binding hash, CSR} into a transport payload.
 * 6.  The provisioning tool extracts the UUID and binding hash, validates the
 *     bootstrap certificate against its own reference copy, and only then signs
 *     the CSR with the intermediate certificate.
 * 7.  The tool returns the certificate chain
 *     {device certificate, intermediate certificate, CA/root certificate}.
 * 8.  The device stores all three certificates in secure flash.
 *
 * Important note:
 * The current DER encoder does not expose an API for adding custom CSR
 * extensions/attributes. To avoid touching existing code, this module carries
 * the bootstrap attestation data in a transport wrapper around the CSR.
 * When DER extension support is later added, the UUID/hash payload can move
 * into a custom CSR extension OID without changing the rest of this flow.
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "CSR_Generator.h"
#include "CSR_Generator_Config.h"
#include "DER.h"
#include "DeviceCert.h"

#include <stdio.h>

#include "mbedtls/x509.h"
#include "mbedtls/x509_csr.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           BOOTSTRAP_ATTESTED_REQ_MAGIC
 * @brief         Magic number for the bootstrap-attested provisioning request.
 */
#define BOOTSTRAP_ATTESTED_REQ_MAGIC        (0x42535251UL) /* "BSRQ" */

/**
 * @def           BOOTSTRAP_ATTESTED_REQ_VERSION
 * @brief         Version of the wrapper format used in this file.
 */
#define BOOTSTRAP_ATTESTED_REQ_VERSION      (1U)

/**
 * @def           BOOTSTRAP_BINDING_HASH_LEN
 * @brief         SHA-256 / HMAC-SHA-256 output length.
 */
#define BOOTSTRAP_BINDING_HASH_LEN          (32U)

/**
 * @def           BOOTSTRAP_CERT_START_KEY
 * @brief         Example NVM3 start key for the bootstrap certificate.
 */
#define BOOTSTRAP_CERT_START_KEY            (17U)

/**
 * @def           BOOTSTRAP_DEVICE_CERT_START_KEY
 * @brief         Example NVM3 start key for the signed device certificate.
 */
#define BOOTSTRAP_DEVICE_CERT_START_KEY     (25U)

/**
 * @def           BOOTSTRAP_INTER_CERT_START_KEY
 * @brief         Example NVM3 start key for the intermediate certificate.
 */
#define BOOTSTRAP_INTER_CERT_START_KEY      (33U)

/**
 * @def           BOOTSTRAP_ROOT_CERT_START_KEY
 * @brief         Example NVM3 start key for the root/CA certificate.
 */
#define BOOTSTRAP_ROOT_CERT_START_KEY       (41U)

/**
 * @def           BOOTSTRAP_DEVICE_EC_KEY_ID
 * @brief         Persistent PSA key identifier for the device key used with
 *                the standalone bootstrap-attested CSR flow.
 */
#define BOOTSTRAP_DEVICE_EC_KEY_ID          (CSR_GENERATOR_NVM3_REGION | 0x00FB)

/**
 * @def           BOOTSTRAP_MAX_CERT_LEN
 * @brief         Maximum certificate size handled by this flow.
 */
#define BOOTSTRAP_MAX_CERT_LEN              (CHAIN_LINK_DATA_LEN * CHAIN_LINK_DATA_NUM)

/**
 * @def           BOOTSTRAP_MAX_CSR_LEN
 * @brief         Maximum CSR size handled by this flow.
 */
#define BOOTSTRAP_MAX_CSR_LEN               (1024U)

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        BootstrapAttestedRequestHdr_T
 * @brief         Transport wrapper header for the new provisioning flow.
 */
typedef struct __attribute__((__packed__))
{
   uint32_t u32_magic;                       /**< Wrapper magic */
   uint8_t u8_version;                      /**< Wrapper version */
   uint8_t u8ar_deviceUUID[UUID_LEN];       /**< Device UUID used as HMAC key */
   uint8_t u8ar_bindingHash[BOOTSTRAP_BINDING_HASH_LEN];
                                             /**< HMAC-SHA256(UUID, bootstrap cert) */
   uint16_t u16_CSRLen;                     /**< CSR DER length in bytes */
   uint8_t u8ar_CSR[];                      /**< CSR DER payload */
} BootstrapAttestedRequestHdr_T;

/**
 * @struct        BootstrapCSRPackage_T
 * @brief         Output package created by the device and consumed by the tool.
 */
typedef struct
{
   uint8_t u8ar_payload[sizeof(BootstrapAttestedRequestHdr_T) + BOOTSTRAP_MAX_CSR_LEN];
   size_t t_payloadLen;
} BootstrapCSRPackage_T;

/**
 * @struct        BootstrapProvisionedChain_T
 * @brief         Certificate chain returned by the provisioning tool.
 */
typedef struct
{
   uint8_t *u8pt_deviceCert;
   uint32_t u32_deviceCertLen;
   uint8_t *u8pt_intermediateCert;
   uint32_t u32_intermediateCertLen;
   uint8_t *u8pt_rootCert;
   uint32_t u32_rootCertLen;
} BootstrapProvisionedChain_T;

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           su8ar_namespace
 * @brief         UUID namespace reused from the existing CSR flow.
 */
static const uint8_t su8ar_namespace[16] = {
   0x70, 0x0b, 0xaf, 0xdf,
   0xd5, 0xec, 0xc3, 0x9b,
   0x37, 0x04, 0xa1, 0x2c,
   0x07, 0x67, 0x86, 0x9c,
};

/******************************************************************************/
/*                                                                            */
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/*                                                                            */
/******************************************************************************/
static sl_status_t st_BootstrapFlow_GenerateUUID(uint8_t *u8pt_UUID);
static sl_status_t st_BootstrapFlow_CalculateSHA256(const uint8_t *u8pt_data,
   size_t t_len, uint8_t *u8pt_hash);
static sl_status_t st_BootstrapFlow_CalculateBindingHash(
   const uint8_t *u8pt_bootstrapCert, size_t t_bootstrapCertLen,
   const uint8_t *u8pt_UUID, size_t t_UUIDLen, uint8_t *u8pt_bindingHash);
static sl_status_t st_BootstrapFlow_ReadBootstrapCertificate(
   uint8_t *u8pt_bootstrapCert, uint32_t *u32pt_bootstrapCertLen);
static sl_status_t st_BootstrapFlow_GenerateDeviceKey(
   mbedtls_svc_key_id_t *tpt_deviceKey);
static void sv_BootstrapFlow_UUID2String(const uint8_t *u8pt_UUID,
   char *cpt_UUIDString, size_t t_UUIDStringLen);
static sl_status_t st_BootstrapFlow_BuildSubjectFields(
   SubjectNameField_T *stpt_subjectFields, size_t t_subjectFieldsCnt,
   char *cpt_UUIDString, size_t t_UUIDStringLen);
static sl_status_t st_BootstrapFlow_ParseRequest(
   const uint8_t *u8pt_payload, size_t t_payloadLen,
   const BootstrapAttestedRequestHdr_T **stppt_header);

/******************************************************************************/
/*                                                                            */
/*                        PRIVATE FUNCTION DEFINITIONS                        */
/*                                                                            */
/******************************************************************************/
/**
 * @private       st_BootstrapFlow_GenerateUUID
 * @brief         Generate the device UUID exactly as the existing CSR flow does.
 * @param[out]    u8pt_UUID Pointer to the output UUID buffer.
 * @return        SL_STATUS_OK upon success, error code otherwise.
 */
static sl_status_t st_BootstrapFlow_GenerateUUID(uint8_t *u8pt_UUID)
{
   sl_status_t t_retVal = SL_STATUS_OK;
   uint64_t u64_UID = 0;
   uint8_t u8ar_temp[24] = { 0 };
   uint8_t u8ar_digest[CRYPTO_SHA_1_LEN] = { 0 };

   if (!u8pt_UUID)
   {
      return SL_STATUS_INVALID_PARAMETER;
   }

   u64_UID = SYSTEM_GetUnique();

   memcpy(u8ar_temp, su8ar_namespace, sizeof(su8ar_namespace));
   u8ar_temp[16] = (uint8_t)((u64_UID >> 56) & 0xFFU);
   u8ar_temp[17] = (uint8_t)((u64_UID >> 48) & 0xFFU);
   u8ar_temp[18] = (uint8_t)((u64_UID >> 40) & 0xFFU);
   u8ar_temp[19] = (uint8_t)((u64_UID >> 32) & 0xFFU);
   u8ar_temp[20] = (uint8_t)((u64_UID >> 24) & 0xFFU);
   u8ar_temp[21] = (uint8_t)((u64_UID >> 16) & 0xFFU);
   u8ar_temp[22] = (uint8_t)((u64_UID >>  8) & 0xFFU);
   u8ar_temp[23] = (uint8_t)((u64_UID >>  0) & 0xFFU);

   t_retVal = gt_CalculateSHA1(u8ar_temp, sizeof(u8ar_temp), u8ar_digest);

   if (SL_STATUS_OK == t_retVal)
   {
      memcpy(u8pt_UUID, u8ar_digest, UUID_LEN);
      u8pt_UUID[7] &= 0x0FU;
      u8pt_UUID[7] |= 0x50U;
      u8pt_UUID[8] &= 0x3FU;
      u8pt_UUID[8] |= 0x80U;
   }

   return t_retVal;
}

/**
 * @private       st_BootstrapFlow_CalculateSHA256
 * @brief         Calculate SHA-256 over arbitrary input data.
 * @param[in]     u8pt_data Pointer to the input data.
 * @param[in]     t_len Input data length.
 * @param[out]    u8pt_hash Pointer to the output hash buffer.
 * @return        SL_STATUS_OK upon success, error code otherwise.
 */
static sl_status_t st_BootstrapFlow_CalculateSHA256(const uint8_t *u8pt_data,
   size_t t_len, uint8_t *u8pt_hash)
{
   sl_status_t t_retVal = SL_STATUS_OK;
   psa_hash_operation_t st_operation = PSA_HASH_OPERATION_INIT;
   size_t t_outputLen = 0;

   if ((!u8pt_data) || (!t_len) || (!u8pt_hash))
   {
      return SL_STATUS_INVALID_PARAMETER;
   }

   t_retVal = gt_PSAStatus2SLStatus(psa_hash_setup(&st_operation, PSA_ALG_SHA_256));

   if (SL_STATUS_OK == t_retVal)
   {
      t_retVal = gt_PSAStatus2SLStatus(psa_hash_update(&st_operation, u8pt_data, t_len));
   }

   if (SL_STATUS_OK == t_retVal)
   {
      t_retVal = gt_PSAStatus2SLStatus(psa_hash_finish(&st_operation, u8pt_hash,
         BOOTSTRAP_BINDING_HASH_LEN, &t_outputLen));
   }

   if (SL_STATUS_OK != t_retVal)
   {
      (void)psa_hash_abort(&st_operation);
   }

   return t_retVal;
}

/**
 * @private       st_BootstrapFlow_CalculateBindingHash
 * @brief         Calculate a UUID-bound hash over the bootstrap certificate.
 * @param[in]     u8pt_bootstrapCert Pointer to bootstrap certificate bytes.
 * @param[in]     t_bootstrapCertLen Bootstrap certificate length.
 * @param[in]     u8pt_UUID Pointer to device UUID used as HMAC key.
 * @param[in]     t_UUIDLen Device UUID length.
 * @param[out]    u8pt_bindingHash Pointer to 32-byte output buffer.
 * @return        SL_STATUS_OK upon success, error code otherwise.
 */
static sl_status_t st_BootstrapFlow_CalculateBindingHash(
   const uint8_t *u8pt_bootstrapCert, size_t t_bootstrapCertLen,
   const uint8_t *u8pt_UUID, size_t t_UUIDLen, uint8_t *u8pt_bindingHash)
{
   sl_status_t t_retVal = SL_STATUS_OK;
   psa_key_attributes_t st_keyAttr = PSA_KEY_ATTRIBUTES_INIT;
   mbedtls_svc_key_id_t t_hmacKeyID = 0;
   size_t t_macLen = 0;

   if ((!u8pt_bootstrapCert) || (!t_bootstrapCertLen) || (!u8pt_UUID) ||
      (!t_UUIDLen) || (!u8pt_bindingHash))
   {
      return SL_STATUS_INVALID_PARAMETER;
   }

   psa_set_key_type(&st_keyAttr, PSA_KEY_TYPE_HMAC);
   psa_set_key_bits(&st_keyAttr, t_UUIDLen * 8U);
   psa_set_key_usage_flags(&st_keyAttr, PSA_KEY_USAGE_SIGN_MESSAGE);
   psa_set_key_algorithm(&st_keyAttr, PSA_ALG_HMAC(PSA_ALG_SHA_256));
   psa_set_key_lifetime(&st_keyAttr,
      PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_VOLATILE,
         CSR_GENERATOR_KEY_LOCATION));

   t_retVal = gt_PSAStatus2SLStatus(psa_import_key(&st_keyAttr, u8pt_UUID,
      t_UUIDLen, &t_hmacKeyID));

   if (SL_STATUS_OK == t_retVal)
   {
      t_retVal = gt_PSAStatus2SLStatus(psa_mac_compute(t_hmacKeyID,
         PSA_ALG_HMAC(PSA_ALG_SHA_256), u8pt_bootstrapCert, t_bootstrapCertLen,
         u8pt_bindingHash, BOOTSTRAP_BINDING_HASH_LEN, &t_macLen));
   }

   if (0 != t_hmacKeyID)
   {
      (void)psa_destroy_key(t_hmacKeyID);
   }

   return t_retVal;
}

/**
 * @private       st_BootstrapFlow_ReadBootstrapCertificate
 * @brief         Read the bootstrap certificate already provisioned at EOL.
 * @param[out]    u8pt_bootstrapCert Pointer to output certificate buffer.
 * @param[out]    u32pt_bootstrapCertLen Pointer to output length variable.
 * @return        SL_STATUS_OK upon success, error code otherwise.
 */
static sl_status_t st_BootstrapFlow_ReadBootstrapCertificate(
   uint8_t *u8pt_bootstrapCert, uint32_t *u32pt_bootstrapCertLen)
{
   if ((!u8pt_bootstrapCert) || (!u32pt_bootstrapCertLen))
   {
      return SL_STATUS_INVALID_PARAMETER;
   }

   return gt_GetCertificate(u8pt_bootstrapCert, u32pt_bootstrapCertLen,
      CSR_GENERATOR_NVM3_REGION, BOOTSTRAP_CERT_START_KEY);
}

/**
 * @private       st_BootstrapFlow_GenerateDeviceKey
 * @brief         Generate the persistent device EC key used to sign the CSR.
 * @param[out]    tpt_deviceKey Pointer to returned key identifier.
 * @return        SL_STATUS_OK if the key was created, SL_STATUS_ALREADY_EXISTS
 *                if it was already present, or another error code otherwise.
 */
static sl_status_t st_BootstrapFlow_GenerateDeviceKey(
   mbedtls_svc_key_id_t *tpt_deviceKey)
{
   sl_status_t t_retVal = SL_STATUS_OK;
   psa_key_attributes_t st_keyAttr = PSA_KEY_ATTRIBUTES_INIT;
   mbedtls_svc_key_id_t t_keyID = BOOTSTRAP_DEVICE_EC_KEY_ID;

   if (!tpt_deviceKey)
   {
      return SL_STATUS_INVALID_PARAMETER;
   }

   psa_set_key_algorithm(&st_keyAttr, CSR_GENERATOR_EC_KEY_ALGO);
   psa_set_key_type(&st_keyAttr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
   psa_set_key_bits(&st_keyAttr, CRYPTO_EC_PRIVATE_KEY_LEN * 8U);
   psa_set_key_usage_flags(&st_keyAttr, CSR_GENERATOR_EC_KEY_USAGE);
   psa_set_key_id(&st_keyAttr, t_keyID);
   psa_set_key_lifetime(&st_keyAttr,
      PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT,
         CSR_GENERATOR_KEY_LOCATION));

   t_retVal = gt_PSAStatus2SLStatus(psa_generate_key(&st_keyAttr, &t_keyID));

   if ((SL_STATUS_OK == t_retVal) || (SL_STATUS_ALREADY_EXISTS == t_retVal))
   {
      *tpt_deviceKey = BOOTSTRAP_DEVICE_EC_KEY_ID;
   }

   return t_retVal;
}

/**
 * @private       sv_BootstrapFlow_UUID2String
 * @brief         Convert binary UUID to standard textual representation.
 * @param[in]     u8pt_UUID Pointer to 16-byte UUID.
 * @param[out]    cpt_UUIDString Pointer to output string.
 * @param[in]     t_UUIDStringLen Size of output string buffer.
 * @return        None.
 */
static void sv_BootstrapFlow_UUID2String(const uint8_t *u8pt_UUID,
   char *cpt_UUIDString, size_t t_UUIDStringLen)
{
   if ((!u8pt_UUID) || (!cpt_UUIDString) || (t_UUIDStringLen < 37U))
   {
      return;
   }

   (void)snprintf(cpt_UUIDString, t_UUIDStringLen,
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      u8pt_UUID[0], u8pt_UUID[1], u8pt_UUID[2], u8pt_UUID[3],
      u8pt_UUID[4], u8pt_UUID[5], u8pt_UUID[6], u8pt_UUID[7],
      u8pt_UUID[8], u8pt_UUID[9], u8pt_UUID[10], u8pt_UUID[11],
      u8pt_UUID[12], u8pt_UUID[13], u8pt_UUID[14], u8pt_UUID[15]);
}

/**
 * @private       st_BootstrapFlow_BuildSubjectFields
 * @brief         Build CSR subject fields with UUID as Common Name.
 * @param[out]    stpt_subjectFields Pointer to 6-entry subject field array.
 * @param[in]     t_subjectFieldsCnt Number of subject field entries.
 * @param[out]    cpt_UUIDString Pointer to textual UUID buffer.
 * @param[in]     t_UUIDStringLen Length of textual UUID buffer.
 * @return        SL_STATUS_OK upon success, error code otherwise.
 */
static sl_status_t st_BootstrapFlow_BuildSubjectFields(
   SubjectNameField_T *stpt_subjectFields, size_t t_subjectFieldsCnt,
   char *cpt_UUIDString, size_t t_UUIDStringLen)
{
   if ((!stpt_subjectFields) || (t_subjectFieldsCnt < 6U) || (!cpt_UUIDString) ||
      (t_UUIDStringLen < 37U))
   {
      return SL_STATUS_INVALID_PARAMETER;
   }

   memset(stpt_subjectFields, 0, sizeof(SubjectNameField_T) * t_subjectFieldsCnt);

   stpt_subjectFields[0].t_nameLen = 1U;
   stpt_subjectFields[0].t_valueLen = sizeof(CSR_GENERATOR_SUBJECT_COUNTRY) - 1U;
   stpt_subjectFields[0].cpt_name = "C";
   stpt_subjectFields[0].cpt_value = CSR_GENERATOR_SUBJECT_COUNTRY;

   stpt_subjectFields[1].t_nameLen = 2U;
   stpt_subjectFields[1].t_valueLen = sizeof(CSR_GENERATOR_SUBJECT_STATE) - 1U;
   stpt_subjectFields[1].cpt_name = "ST";
   stpt_subjectFields[1].cpt_value = CSR_GENERATOR_SUBJECT_STATE;

   stpt_subjectFields[2].t_nameLen = 1U;
   stpt_subjectFields[2].t_valueLen = sizeof(CSR_GENERATOR_SUBJECT_LOCALITY) - 1U;
   stpt_subjectFields[2].cpt_name = "L";
   stpt_subjectFields[2].cpt_value = CSR_GENERATOR_SUBJECT_LOCALITY;

   stpt_subjectFields[3].t_nameLen = 1U;
   stpt_subjectFields[3].t_valueLen = sizeof(CSR_GENERATOR_SUBJECT_ORGANIZATION) - 1U;
   stpt_subjectFields[3].cpt_name = "O";
   stpt_subjectFields[3].cpt_value = CSR_GENERATOR_SUBJECT_ORGANIZATION;

   stpt_subjectFields[4].t_nameLen = 2U;
   stpt_subjectFields[4].t_valueLen = sizeof(CSR_GENERATOR_SUBJECT_ORGANIZATION_UNIT) - 1U;
   stpt_subjectFields[4].cpt_name = "OU";
   stpt_subjectFields[4].cpt_value = CSR_GENERATOR_SUBJECT_ORGANIZATION_UNIT;

   stpt_subjectFields[5].t_nameLen = 2U;
   stpt_subjectFields[5].t_valueLen = 36U;
   stpt_subjectFields[5].cpt_name = "CN";
   stpt_subjectFields[5].cpt_value = cpt_UUIDString;

   return SL_STATUS_OK;
}

/**
 * @private       st_BootstrapFlow_ParseRequest
 * @brief         Validate the request wrapper and return the parsed header.
 * @param[in]     u8pt_payload Pointer to request payload.
 * @param[in]     t_payloadLen Payload length.
 * @param[out]    stppt_header Pointer to parsed header pointer.
 * @return        SL_STATUS_OK upon success, error code otherwise.
 */
static sl_status_t st_BootstrapFlow_ParseRequest(
   const uint8_t *u8pt_payload, size_t t_payloadLen,
   const BootstrapAttestedRequestHdr_T **stppt_header)
{
   const BootstrapAttestedRequestHdr_T *stpt_header = NULL;
   size_t t_headerSize = sizeof(BootstrapAttestedRequestHdr_T);

   if ((!u8pt_payload) || (!t_payloadLen) || (!stppt_header))
   {
      return SL_STATUS_INVALID_PARAMETER;
   }

   if (t_payloadLen < t_headerSize)
   {
      return SL_STATUS_INVALID_COUNT;
   }

   stpt_header = (const BootstrapAttestedRequestHdr_T *)u8pt_payload;

   if ((BOOTSTRAP_ATTESTED_REQ_MAGIC != stpt_header->u32_magic) ||
      (BOOTSTRAP_ATTESTED_REQ_VERSION != stpt_header->u8_version))
   {
      return SL_STATUS_INVALID_SIGNATURE;
   }

   if ((t_headerSize + stpt_header->u16_CSRLen) != t_payloadLen)
   {
      return SL_STATUS_INVALID_COUNT;
   }

   *stppt_header = stpt_header;
   return SL_STATUS_OK;
}

/******************************************************************************/
/*                                                                            */
/*                         PUBLIC FUNCTION DEFINITIONS                        */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gt_BootstrapFlow_GenerateProvisioningRequest
 * @brief         Device-side flow for generating the bootstrap-attested CSR
 *                package.
 * @param[out]    stpt_package Pointer to output package.
 * @return        SL_STATUS_OK upon success, error code otherwise.
 *
 * @details
 * This is the new device code flow:
 * 1. Read bootstrap certificate from secure flash.
 * 2. Generate device UUID.
 * 3. Calculate HMAC-SHA256(UUID, bootstrap certificate).
 * 4. Generate/reuse persistent device EC key.
 * 5. Generate DER CSR.
 * 6. Wrap UUID + binding hash + CSR into a provisioning request payload.
 */
sl_status_t gt_BootstrapFlow_GenerateProvisioningRequest(
   BootstrapCSRPackage_T *stpt_package)
{
   sl_status_t t_retVal = SL_STATUS_OK;
   uint8_t u8ar_bootstrapCert[BOOTSTRAP_MAX_CERT_LEN] = { 0 };
   uint8_t u8ar_UUID[UUID_LEN] = { 0 };
   uint8_t u8ar_bindingHash[BOOTSTRAP_BINDING_HASH_LEN] = { 0 };
   uint8_t u8ar_CSRDER[BOOTSTRAP_MAX_CSR_LEN] = { 0 };
   uint32_t u32_bootstrapCertLen = 0;
   size_t t_CSRDERLen = 0;
   size_t t_wrapperLen = 0;
   mbedtls_svc_key_id_t t_deviceKey = 0;
   char scar_UUIDString[37] = { 0 };
   SubjectNameField_T star_subjectFields[6];
   BootstrapAttestedRequestHdr_T *stpt_header = NULL;

   if (!stpt_package)
   {
      return SL_STATUS_INVALID_PARAMETER;
   }

   memset(stpt_package, 0, sizeof(*stpt_package));

   t_retVal = gt_CryptoInit();

   if (SL_STATUS_OK == t_retVal)
   {
      t_retVal = st_BootstrapFlow_ReadBootstrapCertificate(u8ar_bootstrapCert,
         &u32_bootstrapCertLen);
   }

   if (SL_STATUS_OK == t_retVal)
   {
      t_retVal = st_BootstrapFlow_GenerateUUID(u8ar_UUID);
   }

   if (SL_STATUS_OK == t_retVal)
   {
      sv_BootstrapFlow_UUID2String(u8ar_UUID, scar_UUIDString, sizeof(scar_UUIDString));
      t_retVal = st_BootstrapFlow_CalculateBindingHash(u8ar_bootstrapCert,
         u32_bootstrapCertLen, u8ar_UUID, sizeof(u8ar_UUID), u8ar_bindingHash);
   }

   if (SL_STATUS_OK == t_retVal)
   {
      t_retVal = st_BootstrapFlow_GenerateDeviceKey(&t_deviceKey);
      if (SL_STATUS_ALREADY_EXISTS == t_retVal)
      {
         t_retVal = SL_STATUS_OK;
      }
   }

   if (SL_STATUS_OK == t_retVal)
   {
      t_retVal = st_BootstrapFlow_BuildSubjectFields(star_subjectFields,
         sizeof(star_subjectFields) / sizeof(star_subjectFields[0]),
         scar_UUIDString, sizeof(scar_UUIDString));
   }

   if (SL_STATUS_OK == t_retVal)
   {
      t_retVal = gt_DER_EncodeCSR(star_subjectFields,
         sizeof(star_subjectFields) / sizeof(star_subjectFields[0]),
         t_deviceKey, u8ar_CSRDER, sizeof(u8ar_CSRDER), &t_CSRDERLen);
   }

   if (SL_STATUS_OK == t_retVal)
   {
      t_wrapperLen = sizeof(BootstrapAttestedRequestHdr_T) + t_CSRDERLen;

      if (t_wrapperLen > sizeof(stpt_package->u8ar_payload))
      {
         t_retVal = SL_STATUS_WOULD_OVERFLOW;
      }
   }

   if (SL_STATUS_OK == t_retVal)
   {
      stpt_header = (BootstrapAttestedRequestHdr_T *)stpt_package->u8ar_payload;
      stpt_header->u32_magic = BOOTSTRAP_ATTESTED_REQ_MAGIC;
      stpt_header->u8_version = BOOTSTRAP_ATTESTED_REQ_VERSION;
      memcpy(stpt_header->u8ar_deviceUUID, u8ar_UUID, sizeof(u8ar_UUID));
      memcpy(stpt_header->u8ar_bindingHash, u8ar_bindingHash,
         sizeof(u8ar_bindingHash));
      stpt_header->u16_CSRLen = (uint16_t)t_CSRDERLen;
      memcpy(stpt_header->u8ar_CSR, u8ar_CSRDER, t_CSRDERLen);
      stpt_package->t_payloadLen = t_wrapperLen;

      app_log_info("Bootstrap-attested provisioning request generated." APP_LOG_NL);
      app_log_info("UUID: ");
      app_log_hexdump_info(u8ar_UUID, sizeof(u8ar_UUID));
      app_log_append(APP_LOG_NL);
      app_log_info("Bootstrap binding hash: ");
      app_log_hexdump_info(u8ar_bindingHash, sizeof(u8ar_bindingHash));
      app_log_append(APP_LOG_NL);
   }

   return t_retVal;
}

/**
 * @public        gt_BootstrapFlow_ToolValidateRequest
 * @brief         Tool-side validation of the bootstrap-attested CSR package.
 * @param[in]     u8pt_request Pointer to request payload.
 * @param[in]     t_requestLen Request payload length.
 * @param[in]     u8pt_referenceBootstrapCert Pointer to the original bootstrap
 *                certificate bytes known to the provisioning tool.
 * @param[in]     t_referenceBootstrapCertLen Reference bootstrap certificate
 *                length.
 * @return        SL_STATUS_OK if the request is valid, error code otherwise.
 *
 * @details
 * This function models the logic that should run in the provisioning tool:
 * 1. Parse the wrapper.
 * 2. Extract UUID and bootstrap-binding hash.
 * 3. Recompute HMAC-SHA256(UUID, original bootstrap certificate).
 * 4. Compare with the hash sent by the device.
 * 5. Parse the CSR DER to ensure the payload is structurally valid.
 * 6. Only then proceed to certificate issuance with the intermediate CA.
 */
sl_status_t gt_BootstrapFlow_ToolValidateRequest(const uint8_t *u8pt_request,
   size_t t_requestLen, const uint8_t *u8pt_referenceBootstrapCert,
   size_t t_referenceBootstrapCertLen)
{
   sl_status_t t_retVal = SL_STATUS_OK;
   const BootstrapAttestedRequestHdr_T *stpt_header = NULL;
   uint8_t u8ar_expectedHash[BOOTSTRAP_BINDING_HASH_LEN] = { 0 };
   mbedtls_x509_csr st_csrCtx;
   int i_mbedTLSRetVal = 0;

   if ((!u8pt_referenceBootstrapCert) || (!t_referenceBootstrapCertLen))
   {
      return SL_STATUS_INVALID_PARAMETER;
   }

   t_retVal = st_BootstrapFlow_ParseRequest(u8pt_request, t_requestLen,
      &stpt_header);

   if (SL_STATUS_OK == t_retVal)
   {
      t_retVal = st_BootstrapFlow_CalculateBindingHash(
         u8pt_referenceBootstrapCert, t_referenceBootstrapCertLen,
         stpt_header->u8ar_deviceUUID, sizeof(stpt_header->u8ar_deviceUUID),
         u8ar_expectedHash);
   }

   if ((SL_STATUS_OK == t_retVal) &&
      (0 != memcmp(u8ar_expectedHash, stpt_header->u8ar_bindingHash,
         sizeof(u8ar_expectedHash))))
   {
      t_retVal = SL_STATUS_INVALID_SIGNATURE;
   }

   if (SL_STATUS_OK == t_retVal)
   {
      mbedtls_x509_csr_init(&st_csrCtx);
      i_mbedTLSRetVal = mbedtls_x509_csr_parse_der(&st_csrCtx,
         stpt_header->u8ar_CSR, stpt_header->u16_CSRLen);

      if (0 != i_mbedTLSRetVal)
      {
         t_retVal = SL_STATUS_INVALID_PARAMETER;
      }

      mbedtls_x509_csr_free(&st_csrCtx);
   }

   return t_retVal;
}

/**
 * @public        gt_BootstrapFlow_StoreProvisionedChain
 * @brief         Device-side storage of the returned certificate chain.
 * @param[in]     stpt_chain Pointer to the returned certificate chain.
 * @return        SL_STATUS_OK upon success, error code otherwise.
 *
 * @details
 * This is the new post-provisioning storage flow:
 * 1. Store signed device certificate in secure flash.
 * 2. Store intermediate certificate in secure flash.
 * 3. Store root/CA certificate in secure flash.
 *
 * This function deliberately does not update the current provisioning FSM/RCB,
 * because the requirement was to avoid modifying existing code. It provides the
 * storage sequence that the final integration can call later.
 */
sl_status_t gt_BootstrapFlow_StoreProvisionedChain(
   const BootstrapProvisionedChain_T *stpt_chain)
{
   sl_status_t t_retVal = SL_STATUS_OK;

   if ((!stpt_chain) || (!stpt_chain->u8pt_deviceCert) ||
      (!stpt_chain->u32_deviceCertLen) || (!stpt_chain->u8pt_intermediateCert) ||
      (!stpt_chain->u32_intermediateCertLen) || (!stpt_chain->u8pt_rootCert) ||
      (!stpt_chain->u32_rootCertLen))
   {
      return SL_STATUS_INVALID_PARAMETER;
   }

   t_retVal = gt_StoreCertificate(stpt_chain->u8pt_deviceCert,
      stpt_chain->u32_deviceCertLen, CSR_GENERATOR_NVM3_REGION,
      BOOTSTRAP_DEVICE_CERT_START_KEY);

   if (SL_STATUS_OK == t_retVal)
   {
      t_retVal = gt_StoreCertificate(stpt_chain->u8pt_intermediateCert,
         stpt_chain->u32_intermediateCertLen, CSR_GENERATOR_NVM3_REGION,
         BOOTSTRAP_INTER_CERT_START_KEY);
   }

   if (SL_STATUS_OK == t_retVal)
   {
      t_retVal = gt_StoreCertificate(stpt_chain->u8pt_rootCert,
         stpt_chain->u32_rootCertLen, CSR_GENERATOR_NVM3_REGION,
         BOOTSTRAP_ROOT_CERT_START_KEY);
   }

   if (SL_STATUS_OK == t_retVal)
   {
      app_log_info("Bootstrap-attested certificate chain stored successfully." APP_LOG_NL);
   }

   return t_retVal;
}

/**
 * @public        gt_BootstrapFlow_ReferenceSequence
 * @brief         High-level end-to-end reference sequence for documentation and
 *                future integration.
 * @return        Always returns SL_STATUS_OK.
 *
 * @details
 * Intended call sequence:
 *
 * Device side:
 *    gt_BootstrapFlow_GenerateProvisioningRequest()
 *    -> send package over BLE to production-line tool
 *
 * Tool side:
 *    gt_BootstrapFlow_ToolValidateRequest()
 *    -> if valid, sign CSR using intermediate CA
 *    -> return {device cert, intermediate cert, root cert}
 *
 * Device side:
 *    gt_BootstrapFlow_StoreProvisionedChain()
 */
sl_status_t gt_BootstrapFlow_ReferenceSequence(void)
{
   return SL_STATUS_OK;
}
