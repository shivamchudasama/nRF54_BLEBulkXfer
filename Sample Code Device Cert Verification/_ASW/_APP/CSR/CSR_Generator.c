/**
 * @file          CSR_Generator.c
 * @brief         Source file containing CSR generator functionality.
 * @date          27/08/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "CSR_Generator.h"
#include "CSR_Generator_Config.h"
#include "DER.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           <Define name>
 * @brief         <Define details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
// Definition of all the enums
/**
 * @enum          <Enum name>
 * @brief         <Enum details>.
 */

// Declarations of all the enum variables
/**
 * @var           <Variable name>
 * @brief         <Variable details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
// Definition of all the structures
/**
 * @struct        <Structure name>
 * @brief         <Structure details>.
 */

// Declarations of all the structure variables
/**
 * @var           <Variable name>
 * @brief         <Variable details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                   UNIONS                                   */
/*                                                                            */
/******************************************************************************/
// Definition of all the unions
/**
 * @union         <Union name>
 * @brief         <Union details>.
 */

// Declarations of all the union variables
/**
 * @var           <Variable name>
 * @brief         <Variable details>.
 */

/******************************************************************************/
/*                                                                            */
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/*                                                                            */
/******************************************************************************/
static sl_status_t st_GenerateStaticAuthData(void);
static sl_status_t st_ExportStaticAuthData(uint8_t *u8pt_data, size_t t_dataSize, size_t *tpt_dataLength);
static sl_status_t st_GenerateDeviceECKeys(mbedtls_svc_key_id_t *tpt_ECDHKey,
   mbedtls_svc_key_id_t *tpt_signingKey);
static sl_status_t st_GenerateUUID(uint8_t *const u8pt_UUID);
static sl_status_t st_EraseChipAndResetDeviceProvisioningRCB(void);

/******************************************************************************/
/*                                                                            */
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                              PUBLIC VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           gst_CSROutput
 * @brief         Instance of CSROutput_T structure.
 */
CSROutput_T gst_CSROutput = { 0 };

/**
 * @var           gst_CSRConfig
 * @brief         CSR configuration.
 */
CSRConfig_T gst_CSRConfig = {
  .b_isStaticAuth2Generated = CSR_GENERATOR_CONFIG_GENERATE_STATIC_AUTH,
  .b_isStaticAuthOnDevice = 0,
  .b_isKey2Generated = CSR_GENERATOR_CONFIG_GENERATE_EC_KEY,
  .b_isKeyAvailable = 0,
  .b_isCertAvailable = CSR_GENERATOR_CONFIG_CERTIFICATE_ON_DEVICE,
  .b_isCSRAvailable = CSR_GENERATOR_CONFIG_CSR_ON_DEVICE,
  .u8_certPositionOnNVM3 = DEVICE_CERTIFICATE_NVM3_START_KEY,
  .u8_CSRPositionOnNVM3 = DEVICE_CSR_NVM3_START_KEY,
  .t_subjectNameFieldCnt = 6,
  .stpt_subjectNameField = NULL,
};

/**
 * @var           gst_deviceProvisioningRCB
 * @brief         Device provisioning record control block structure.
 */
DeviceProvisioningRCB_T gst_deviceProvisioningRCB = {
   .u32_bitmap = 0,
   .u8ar_positionNVM3 = {
      [POS_DEVICE_CERTIFICATE] = DEVICE_CERTIFICATE_NVM3_START_KEY,
      [POS_DEVICE_EC_KEY] = 0,
      [POS_STATIC_AUTH_DATA] = 0,
      [POS_CSR] = DEVICE_CSR_NVM3_START_KEY,
   },
   .u16_maxLinkDataLen = CHAIN_LINK_DATA_LEN,
};

/**
 * @var           gu8ar_deviceCertDER
 * @brief         Buffer to hold the device certificate in DER format.
 */
uint8_t gu8ar_deviceCertDER[CHAIN_LINK_DATA_LEN * CHAIN_LINK_DATA_NUM] = { 0 };

/**
 * @var           gu32_deviceCertDERLen
 * @brief         Length of the device certificate.
 */
uint32_t gu32_deviceCertDERLen = 0;

/**
 * @var           gb_isCSRAvailable
 * @brief         Flag to indicate if the CSR available.
 */
bool gb_isCSRAvailable = false;

/**
 * @var           gb_isDeviceCertAvailable
 * @brief         Flag to indicate if the device certificate present on NVM3.
 */
bool gb_isDeviceCertAvailable = false;

/**
 * @var           gb_isDeviceCertVerified
 * @brief         Flag to indicate if the device certificate has been verified.
 */
bool gb_isDeviceCertVerified = false;

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
// /**
//  * @var           gst_CSROutput
//  * @brief         Pointer to the CSROutput_T structure in RAM.
//  */
// static volatile CSROutput_T * const gst_CSROutput = \
//    (CSROutput_T *)CSR_GENERATOR_CSR_RAM_ADDRESS;

/**
 * @var           sscar_BTMeshCommonNameUUID
 * @brief         Bluetooth Mesh Common Name UUID.
 */
static char sscar_BTMeshCommonNameUUID[37] = { 0 };

/**
 * @var           sstar_subjectNameFields
 * @brief         CSR Subject name fields.
 */
static SubjectNameField_T sstar_subjectNameFields[] =
{
   {
      .t_nameLen = 1,
      .t_valueLen = (sizeof(CSR_GENERATOR_SUBJECT_COUNTRY) - 1),
      .cpt_name = "C",                       // Shorthand for "countryName"
      .cpt_value = CSR_GENERATOR_SUBJECT_COUNTRY
   },
   {
      .t_nameLen = 2,
      .t_valueLen = (sizeof(CSR_GENERATOR_SUBJECT_STATE) - 1),
      .cpt_name = "ST",                      // Shorthand for "stateOrProvinceName"
      .cpt_value = CSR_GENERATOR_SUBJECT_STATE
   },
   {
      .t_nameLen = 1,
      .t_valueLen = (sizeof(CSR_GENERATOR_SUBJECT_LOCALITY) - 1),
      .cpt_name = "L",                       // Shorthand for "locality"
      .cpt_value = CSR_GENERATOR_SUBJECT_LOCALITY
   },
   {
      .t_nameLen = 1,
      .t_valueLen = (sizeof(CSR_GENERATOR_SUBJECT_ORGANIZATION) - 1),
      .cpt_name = "O",                       // Shorthand for "organization"
      .cpt_value = CSR_GENERATOR_SUBJECT_ORGANIZATION
   },
   {
      .t_nameLen = 2,
      .t_valueLen = (sizeof(CSR_GENERATOR_SUBJECT_ORGANIZATION_UNIT) - 1),
      .cpt_name = "OU",                      // Shorthand for "organizationalUnitName"
      .cpt_value = CSR_GENERATOR_SUBJECT_ORGANIZATION_UNIT
   },
   {
      .t_nameLen = 2,
      .t_valueLen = 36,                      // 128-bit Mesh device UUID as hexes and dashes
      .cpt_name = "CN",                      // Shorthand for "commonName"
      .cpt_value = sscar_BTMeshCommonNameUUID// To be filled in
   },
};

/**
 * @var           su8ar_authData
 * @brief         Static authentication data.
 */
static uint8_t su8ar_authData[CRYPTO_AUTH_256_LEN] = { 0 };

/**
 * @var           su8ar_namespace
 * @brief         UUID Namespace.
 */
static const uint8_t su8ar_namespace[16] = {
  0x70, 0x0b, 0xaf, 0xdf,
  0xd5, 0xec, 0xc3, 0x9b,
  0x37, 0x04, 0xa1, 0x2c,
  0x07, 0x67, 0x86, 0x9c,
};

/**
 * @var           su8ar_CSR
 * @brief         Buffer to hold the CSR in DER format.
 */
static uint8_t su8ar_CSRDER[CHAIN_LINK_DATA_LEN * CHAIN_LINK_DATA_NUM] = { 0 };

/**
 * @var           su32_CSRLen
 * @brief         Length of the CSR.
 */
static uint32_t su32_CSRDERLen = 0;

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                        PRIVATE FUNCTION DEFINITIONS                        */
/*                                                                            */
/******************************************************************************/
/**
 * @private       st_GenerateStaticAuthData
 * @brief         Generate static authentication data (random key of 256 bits).
 * @return        Converted (from psa_status_t to sl_status_t) status of the initialization.
 *                SL_STATUS_OK in case of no error.
 */
static sl_status_t st_GenerateStaticAuthData(void)
{
   sl_status_t t_retVal = SL_STATUS_OK;
   mbedtls_svc_key_id_t t_key = STATIC_AUTH_DATA_ID;
   psa_algorithm_t t_algo = PSA_ALG_NONE;
   psa_key_attributes_t st_keyAttr = PSA_KEY_ATTRIBUTES_INIT;
   psa_key_lifetime_t t_keyLifetime = PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT, CSR_GENERATOR_KEY_LOCATION);

   // Set the key algorithm as PSA_ALG_NONE
   psa_set_key_algorithm(&st_keyAttr, t_algo);

   // Set the key type as PSA_KEY_TYPE_RAW_DATA
   psa_set_key_type(&st_keyAttr, PSA_KEY_TYPE_RAW_DATA);

   // Set the key size as 256 bits
   psa_set_key_bits(&st_keyAttr, CRYPTO_AUTH_256_LEN * 8);

   // Set the key usage flags as PSA_KEY_USAGE_EXPORT (permitted operation)
   psa_set_key_usage_flags(&st_keyAttr, PSA_KEY_USAGE_EXPORT);

   // Set the key ID (declares the key as persistent and stores it in the
   // specified location (STATIC_AUTH_DATA_ID)).
   psa_set_key_id(&st_keyAttr, t_key);

   // Set the key lifetime (location = CSR_GENERATOR_KEY_LOCATION &
   // persistence = PSA_KEY_PERSISTENCE_DEFAULT)
   psa_set_key_lifetime(&st_keyAttr, t_keyLifetime);

   // Generate key as per the above key attributes
   t_retVal = gt_PSAStatus2SLStatus(psa_generate_key(&st_keyAttr, &t_key));

   return t_retVal;
}

/**
 * @private       st_ExportStaticAuthData
 * @brief         Export static authentication data after reading it from NVM3.
 * @param[out]    u8pt_data Pointer to the data buffer.
 * @param[in]     t_dataSize Size of the data buffer.
 * @param[out]    tpt_dataLength Pointer to the variable which stores the length of
 *                the exported data.
 * @return        Converted (from psa_status_t to sl_status_t) status of the initialization.
 *                SL_STATUS_OK in case of no error.
 */
static sl_status_t st_ExportStaticAuthData(uint8_t *u8pt_data, size_t t_dataSize, size_t *tpt_dataLength)
{
   sl_status_t t_retVal = SL_STATUS_FAIL;

   // Check if the data buffer and data length are valid
   if ((u8pt_data) && (tpt_dataLength))
   {
      t_retVal = \
         gt_PSAStatus2SLStatus(psa_export_key((mbedtls_svc_key_id_t)STATIC_AUTH_DATA_ID, \
            u8pt_data, t_dataSize, tpt_dataLength));
   }

   return t_retVal;
}

/**
 * @private       st_GenerateDeviceECKeys
 * @brief         Generate signing key, export it and import it as device EC key.
 * @param[in]     tpt_ECDHKey Pointer to the ECDH key ID.
 * @param[in]     tpt_signingKey Pointer to the signing key ID.
 * @return        Converted (from psa_status_t to sl_status_t) status of the initialization.
 *                SL_STATUS_OK in case of no error.
 */
static sl_status_t st_GenerateDeviceECKeys(mbedtls_svc_key_id_t *tpt_ECDHKey,
   mbedtls_svc_key_id_t *tpt_signingKey)
{
   sl_status_t t_retVal = SL_STATUS_OK;

   // Check if any of the key pointers are not valid
   if ((!tpt_ECDHKey) || (!tpt_signingKey))
   {
      t_retVal = SL_STATUS_INVALID_PARAMETER;
   }
   else
   {
      // Prepare for generating signing key
      mbedtls_svc_key_id_t t_signingKeyID = 0;
      psa_key_type_t t_signingKeyType = PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1);
      size_t t_signingKeyBits = CRYPTO_EC_PRIVATE_KEY_LEN * 8;
      psa_key_usage_t t_signingKeyUsage = (PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_EXPORT);
      psa_algorithm_t t_signingKeyAlgo = PSA_ALG_ECDSA(PSA_ALG_SHA_256);
      psa_key_attributes_t st_signingKeyAttr = PSA_KEY_ATTRIBUTES_INIT;
      psa_key_lifetime_t t_signingKeyLifetime = PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_VOLATILE, CSR_GENERATOR_KEY_LOCATION);

#if 1
      // Prepare for generating ECDH key
      mbedtls_svc_key_id_t t_ECDHKeyID = DEVICE_EC_KEY_ID;
      psa_key_type_t t_ECDHKeyType = PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1);
      size_t t_ECDHKeyBits = CRYPTO_EC_PRIVATE_KEY_LEN * 8;
      psa_key_usage_t t_ECDHKeyUsage = CSR_GENERATOR_EC_KEY_USAGE;
      psa_algorithm_t t_ECDHKeyAlgo = CSR_GENERATOR_EC_KEY_ALGO;
      psa_key_attributes_t st_ECDHKeyAttr = PSA_KEY_ATTRIBUTES_INIT;
      psa_key_lifetime_t t_ECDHKeyLifetime = PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT, CSR_GENERATOR_KEY_LOCATION);
#endif // 1

      uint8_t u8ar_signingKey[32];
      size_t t_signingKeyLen = sizeof(u8ar_signingKey);

      // Generate signing key
      // Set the key algorithm as PSA_ALG_SHA_256 for ECDSA
      psa_set_key_algorithm(&st_signingKeyAttr, t_signingKeyAlgo);

      // Set the key type as PSA_ECC_FAMILY_SECP_R1 key pair
      psa_set_key_type(&st_signingKeyAttr, t_signingKeyType);

      // Set the key size as 256 bits
      psa_set_key_bits(&st_signingKeyAttr, t_signingKeyBits);

      // Set the key usage flags as PSA_KEY_USAGE_SIGN_MESSAGE & PSA_KEY_USAGE_VERIFY_MESSAGE
      // (permitted operation)
      psa_set_key_usage_flags(&st_signingKeyAttr, t_signingKeyUsage);

      // Set the key lifetime (location = CSR_GENERATOR_KEY_LOCATION &
      // persistence = PSA_KEY_PERSISTENCE_VOLATILE)
      psa_set_key_lifetime(&st_signingKeyAttr, t_signingKeyLifetime);

      // Generate key as per the above key attributes
      t_retVal = gt_PSAStatus2SLStatus(psa_generate_key(&st_signingKeyAttr, &t_signingKeyID));

      // Check if the signing key has been generated successfully
      if (SL_STATUS_OK == t_retVal)
      {
#if 1
         // Set the key algorithm as PSA_ALG_SHA_256 for ECDSA
         psa_set_key_algorithm(&st_ECDHKeyAttr, t_ECDHKeyAlgo);

         // Set the key type as PSA_ECC_FAMILY_SECP_R1 key pair
         psa_set_key_type(&st_ECDHKeyAttr, t_ECDHKeyType);

         // Set the key size as 256 bits
         psa_set_key_bits(&st_ECDHKeyAttr, t_ECDHKeyBits);

         // Set the key usage flags as PSA_KEY_USAGE_SIGN_HASH & PSA_KEY_USAGE_EXPORT
         // (permitted operation)
         psa_set_key_usage_flags(&st_ECDHKeyAttr, t_ECDHKeyUsage);

         // Set the key lifetime (location = CSR_GENERATOR_KEY_LOCATION &
         // persistence = PSA_KEY_PERSISTENCE_DEFAULT)
         psa_set_key_lifetime(&st_ECDHKeyAttr, t_ECDHKeyLifetime);

         // Set the key ID (declares the key as persistent and stores it in the
         // specified location (STATIC_AUTH_DATA_ID)).
         psa_set_key_id(&st_ECDHKeyAttr, t_ECDHKeyID);
#endif // 1
         // Export the signing key
         t_retVal = gt_PSAStatus2SLStatus(psa_export_key(t_signingKeyID, u8ar_signingKey, t_signingKeyLen, &t_signingKeyLen));

         // Check if the signing key has been exported successfully
         if (SL_STATUS_OK == t_retVal)
         {
#if 1
            // Import the signing key as ECDH key
            t_retVal = gt_PSAStatus2SLStatus(psa_import_key(&st_ECDHKeyAttr, u8ar_signingKey, t_signingKeyLen, &t_ECDHKeyID));
#endif // 1
            // Check if the ECDH key has been imported successfully
            if (SL_STATUS_OK == t_retVal)
            {
#if 1
               *tpt_ECDHKey = t_ECDHKeyID;
               // t_ECDHKeyID = 0;
#endif // 1
               *tpt_signingKey = t_signingKeyID;
#if 1
               t_signingKeyID = 0;

               // // Check if the keys haven't been destroyed successfully
               // if ((SL_STATUS_OK != psa_destroy_key(t_ECDHKeyID)) || \
               //    (SL_STATUS_OK != psa_destroy_key(t_signingKeyID)))
               // {
               //    t_retVal = SL_STATUS_FAIL;
               // }
#endif // 1
            }
         }
      }
   }

   return t_retVal;
}

/**
 * @private       st_GenerateUUID
 * @brief         Generate a UUID.
 * @param[out]    u8pt_UUID Pointer to generated UUID data.
 * @return        SL_STATUS_OK upon successfully generation of UUID data, or an
 *                error code otherwise.
 */
static sl_status_t st_GenerateUUID(uint8_t *const u8pt_UUID)
{
   sl_status_t t_retVal = SL_STATUS_OK;

   // Check if the UUID pointer is valid
   if (!u8pt_UUID)
   {
      return SL_STATUS_INVALID_PARAMETER;
   }

   // UUID generation procedure shall follow the standard UUID format as defined in RFC4122
   // https://www.ietf.org/rfc/rfc4122.txt

   uint64_t u64_UID = SYSTEM_GetUnique();
   uint8_t u8ar_temp[24], u8ar_digest[CRYPTO_SHA_1_LEN];

   memcpy(u8ar_temp, su8ar_namespace, 16);
   u8ar_temp[16] = (u64_UID >> 56) & 0xFF;
   u8ar_temp[17] = (u64_UID >> 48) & 0xFF;
   u8ar_temp[18] = (u64_UID >> 40) & 0xFF;
   u8ar_temp[19] = (u64_UID >> 32) & 0xFF;
   u8ar_temp[20] = (u64_UID >> 24) & 0xFF;
   u8ar_temp[21] = (u64_UID >> 16) & 0xFF;
   u8ar_temp[22] = (u64_UID >>  8) & 0xFF;
   u8ar_temp[23] = (u64_UID >>  0) & 0xFF;

   t_retVal = gt_CalculateSHA1(u8ar_temp, sizeof(u8ar_temp), u8ar_digest);

   memcpy(u8pt_UUID, u8ar_digest, 16);
   u8pt_UUID[7] &= 0x0F; // Set the four most significant bits (bits 12 through 15) of the time_hi_and_version field
   u8pt_UUID[7] |= 0x50; // to the 4-bit version number as randomly generated
   u8pt_UUID[8] &= 0x3F; // Set the two most significant bits (bits 6 and 7) of the clock_seq_hi_and_reserved field
   u8pt_UUID[8] |= 0x80; // to zero and one, respectively

   return t_retVal;
}

/**
 * @private       st_EraseChipAndResetDeviceProvisioningRCB
 * @brief         Erase entire NVM3 and reset the device provisioning RCB.
 * @return        SL_STATUS_OK in case of no error.
 */
static sl_status_t st_EraseChipAndResetDeviceProvisioningRCB(void)
{
   sl_status_t t_retVal = SL_STATUS_OK;

   // Erase all NVM3 data related to CSR
   t_retVal = gt_EraseAllNVM3();
   app_assert_status(t_retVal);

   // Clear the device provisioning RCB as no device certificate is available
   ClrBit32(gst_deviceProvisioningRCB.u32_bitmap, POS_DEVICE_CERTIFICATE);
   gst_deviceProvisioningRCB.u8ar_positionNVM3[POS_DEVICE_CERTIFICATE] = 0;

   // Clear the device provisioning RCB as no EC key is available
   ClrBit32(gst_deviceProvisioningRCB.u32_bitmap, POS_DEVICE_EC_KEY);
   gst_deviceProvisioningRCB.u8ar_positionNVM3[POS_DEVICE_EC_KEY] = 0;

   // Clear the device provisioning RCB as no static authentication data is available
   ClrBit32(gst_deviceProvisioningRCB.u32_bitmap, POS_STATIC_AUTH_DATA);
   gst_deviceProvisioningRCB.u8ar_positionNVM3[POS_STATIC_AUTH_DATA] = 0;

   // Clear the device provisioning RCB as no CSR is available
   ClrBit32(gst_deviceProvisioningRCB.u32_bitmap, POS_CSR);
   gst_deviceProvisioningRCB.u8ar_positionNVM3[POS_CSR] = 0;

   return t_retVal;
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gv_GenerateCSR
 * @brief         Generate a Certificate Signing Request (CSR).
 * @param[in]     None
 * @param[out]    None
 * @param[inout]  None
 * @return        None.
 */
void gv_GenerateCSR(void)
{
   sl_status_t t_retVal = SL_STATUS_OK;
   sl_status_t t_MbedTLSRetVal = SL_STATUS_OK;
   size_t t_authLen = 0;
   mbedtls_svc_key_id_t t_signingKeyID = 0;
   mbedtls_svc_key_id_t t_ECDHKeyID = 0;
   uint8_t u8ar_UUID[UUID_LEN + 1] = { 0 };

   // Device certificate tag for NVM3
   uint16_t u16_certTagNVM3 = \
      (gst_deviceProvisioningRCB.u8ar_positionNVM3[POS_DEVICE_CERTIFICATE] | \
         CSR_GENERATOR_DEVICE_PROVISIONING_RCB_NVM3_TAG);
   // CSR tag for NVM3
   uint16_t u16_CSRTagNVM3 = \
      (gst_deviceProvisioningRCB.u8ar_positionNVM3[POS_CSR] | \
         CSR_GENERATOR_DEVICE_PROVISIONING_RCB_NVM3_TAG);
   uint8_t u8_data;
   size_t t_len = sizeof(u8_data);
   // Generate CSR
   unsigned char ucar_CSRBuffDER[1024] = { 0 };
   size_t t_CSRLenDER = 0;
   // Fill in CN to subject name
   char *cpt_CN = sstar_subjectNameFields[5].cpt_value;

   app_log_info("Starting BLE CSR generation for keyfob...." APP_LOG_NL);

   // // Erase all NVM3 data related to CSR
   // t_retVal = gt_EraseAllNVM3();
   // app_assert_status(t_retVal);

   // Initialize cryptographic library
   t_retVal = gt_CryptoInit();
   app_assert_status(t_retVal);

   // Read device provisioning record control block
   t_retVal = gt_ReadDeviceProvisioningRCB(&gst_deviceProvisioningRCB);

   // Update CSR configuration (as per the device provisioning RCB)
   gst_CSRConfig.b_isCertAvailable = \
      ChkBit32(gst_deviceProvisioningRCB.u32_bitmap, POS_DEVICE_CERTIFICATE);
   gst_CSRConfig.b_isCSRAvailable = \
      ChkBit32(gst_deviceProvisioningRCB.u32_bitmap, POS_CSR);
   gst_CSRConfig.b_isKeyAvailable = \
      ChkBit32(gst_deviceProvisioningRCB.u32_bitmap, POS_DEVICE_EC_KEY);

   switch (t_retVal)
   {
      // Device provisioning record control block read successfully
      case SL_STATUS_OK:
      {
         app_log_info("Provision record control block already exists." APP_LOG_NL);

         // Check if is the certificate available on device as per CSR configuration
         if (gst_CSRConfig.b_isCertAvailable)
         {
            // Read the device certificate
            t_retVal = gt_GetCertificate(gu8ar_deviceCertDER, &gu32_deviceCertDERLen, \
               CSR_GENERATOR_NVM3_REGION, u16_certTagNVM3);

            // Check if the device certificate was read successfully
            if (SL_STATUS_OK == t_retVal)
            {
               gb_isDeviceCertAvailable = true;

               app_log_info("Stored device certificate: ");
               app_log_hexdump_info(gu8ar_deviceCertDER, gu32_deviceCertDERLen);
               app_log_append(APP_LOG_NL);
            }
            else
            {
               app_log_append_info("Certificate not found, Erasing NVM3." APP_LOG_NL);

               // Erase chip and reset the device provisioning related data
               st_EraseChipAndResetDeviceProvisioningRCB();
            }
         }
         // Check if is the CSR available on device as per CSR configuration
         else if (gst_CSRConfig.b_isCSRAvailable)
         {
            // Get the CSR from NVM3
            t_retVal = gt_GetCertificate(su8ar_CSRDER, &su32_CSRDERLen, \
               CSR_GENERATOR_NVM3_REGION, u16_CSRTagNVM3);

            // Check if the CSR was read successfully
            if (SL_STATUS_OK == t_retVal)
            {
               app_log_info("Stored CSR: ");
               app_log_hexdump_info(su8ar_CSRDER, su32_CSRDERLen);
               app_log_append(APP_LOG_NL);

               // Populate the CSR to output structure
               for (uint16_t u16_lpIdx = 0; u16_lpIdx < su32_CSRDERLen; u16_lpIdx++)
               {
                  gst_CSROutput.u8ar_CSR[u16_lpIdx] = su8ar_CSRDER[u16_lpIdx];
               }

               gst_CSROutput.u16_CSRLen = (uint16_t)su32_CSRDERLen;
               gb_isCSRAvailable = true;
            }
            else
            {
               app_log_append_info("CSR not found, erasing NVM3" APP_LOG_NL);

               // Erase chip and reset the device provisioning related data
               st_EraseChipAndResetDeviceProvisioningRCB();
            }
         }
         // Check if is the key available on device as per CSR configuration
         if (gst_CSRConfig.b_isKeyAvailable)
         {
            // As we've generated the EC key pair, we only need to generate the CSR
            gst_CSRConfig.b_isKey2Generated = false;
         }
         else
         {
            // TODO: This is abrupt return, need to modify.
            return;
         }
      }
      break;

      case SL_STATUS_BT_PS_KEY_NOT_FOUND:
      {
         app_log_info("Provisioning record control block does not exist yet." APP_LOG_NL);
      }
      break;

      default:
      {
         app_log_info("Unexpected error occured when reading the provisioning record." APP_LOG_NL);
         // TODO: This is abrupt return, need to modify.
         return;
      }
      break;
   }

   // Check if either certificate found or generation of device provisioning data is pending
   if ((SL_STATUS_OK == t_retVal) || (SL_STATUS_BT_PS_KEY_NOT_FOUND == t_retVal))
   {
      // Reset the return value
      t_retVal = SL_STATUS_OK;

      // Check if static authentication data generation is requested
      if (!gst_CSRConfig.b_isStaticAuth2Generated)
      {
         app_log_info("Static authentication data is not requested." APP_LOG_NL);
      }
      else
      {
         // Generate static authentication data
         t_retVal = st_GenerateStaticAuthData();
         app_assert((t_retVal == SL_STATUS_OK) || (t_retVal == SL_STATUS_ALREADY_EXISTS), \
            "Failed to generate static authentication data." APP_LOG_NL);

         // Check if static authentication data was generated successfully
         if (t_retVal == SL_STATUS_OK)
         {
            app_log_info("Static authentication data generated successfully." APP_LOG_NL);
         }
         else
         {
            app_log_info("Static authentication data already exists." APP_LOG_NL);
         }

         // As the static authentication data has been generated, update the same
         // with device provisioning record control block.
         gv_UpdateDeviceProvisioningRCB(POS_STATIC_AUTH_DATA, true, 0);

         // Check if static authentication data was generated successfully or already exists
         if ((SL_STATUS_OK == t_retVal) || (SL_STATUS_ALREADY_EXISTS == t_retVal))
         {
            // Reset the return value
            t_retVal = SL_STATUS_OK;

            // Export static authentication data
            t_retVal = st_ExportStaticAuthData(su8ar_authData, sizeof(su8ar_authData), &t_authLen);
            app_assert((t_retVal == SL_STATUS_OK), "Failed to read static authentication data." APP_LOG_NL);

            // Check if static authentication data was exported successfully
            if (SL_STATUS_OK == t_retVal)
            {
               // Copy the static authentication data to CSR output structure
               for (size_t t_lpIdx = 0; t_lpIdx < t_authLen; t_lpIdx++)
               {
                  gst_CSROutput.u8ar_staticAuthData[t_lpIdx] = su8ar_authData[t_lpIdx];
               }
               app_log_info("Data: ");
               app_log_hexdump_info(su8ar_authData, CRYPTO_AUTH_256_LEN);
               app_log_append(APP_LOG_NL);
            }
         }
      }

      // Check if EC key generation is requested
      if (!gst_CSRConfig.b_isKey2Generated)
      {
         app_log_info("EC key creation is not requested." APP_LOG_NL);
      }
      else
      {
         // Generate EC keys
         // Generate persistent key for ECDH, temporary key for CSR signing
         t_retVal = st_GenerateDeviceECKeys(&t_ECDHKeyID, &t_signingKeyID);
         app_assert((t_retVal == SL_STATUS_OK) || (t_retVal == SL_STATUS_ALREADY_EXISTS), \
            "Failed to create EC key ITS." APP_LOG_NL);

         // Check if key already exists
         if (SL_STATUS_ALREADY_EXISTS == t_retVal)
         {
            app_log_info("Key already exists." APP_LOG_NL);
            // If it already exists (and is not exportable) we can't create an equivalent
            // signing key anymore.
            return;
         }
         else
         {
            app_log_info("Signing EC key created successfully in ITS, signing key ID is 0x%04X" APP_LOG_NL,
               (int)t_signingKeyID);
            app_log_info("ECDH EC key created successfully in ITS, ECDH key ID is 0x%04X" APP_LOG_NL,
               (int)t_ECDHKeyID);
         }

         // Check if EC keys were generated successfully
         if (SL_STATUS_OK == t_retVal)
         {
            // As the EC keys have been generated, update the same
            // with device provisioning record control block.
            gv_UpdateDeviceProvisioningRCB(POS_DEVICE_EC_KEY, true, 0);

            // Generate UUID
            t_retVal = st_GenerateUUID(u8ar_UUID);
            app_assert((t_retVal == SL_STATUS_OK), \
               "Failed to generate UUID, e: %d" APP_LOG_NL, (int)t_retVal);
            app_log_info("Generated UUID: ");
            app_log_hexdump_info(u8ar_UUID, UUID_LEN);
            app_log_append(APP_LOG_NL);

            // Check if UUID generated successfully
            if (SL_STATUS_OK == t_retVal)
            {
               for (uint8_t u8_lpIdx = 0; u8_lpIdx < UUID_LEN; u8_lpIdx++)
               {
                  // Check if index is: 4, 6, 8, 10
                  if ((u8_lpIdx > 3) && (u8_lpIdx < 11) && ((u8_lpIdx & 1) == 0))
                  {
                     *cpt_CN++ = '-';
                  }
                  sprintf(cpt_CN, "%02x", u8ar_UUID[u8_lpIdx]);
                  cpt_CN += 2;
               }

               // Assign subject name fields
               gst_CSRConfig.stpt_subjectNameField = sstar_subjectNameFields;

               // Generate CSR (in DER format)
               t_MbedTLSRetVal = gt_DER_EncodeCSR(
                  gst_CSRConfig.stpt_subjectNameField,
                                                // Subject name field
                  gst_CSRConfig.t_subjectNameFieldCnt,
                                                // Number of subject name field
                  t_signingKeyID,               // Signing key ID
                  ucar_CSRBuffDER,              // CSR buffer
                  sizeof(ucar_CSRBuffDER),      // CSR buffer length
                  &t_CSRLenDER                  // CSR buffer written length
               );
               app_assert((SL_STATUS_OK == t_MbedTLSRetVal), \
                  "Failed to write CSR PEM file, error: %d" APP_LOG_NL, t_MbedTLSRetVal);

               app_log_info("CSR created successfully." APP_LOG_NL);

               // // Set device certificate as present with the configured NVM3 ID.
               // gv_UpdateDeviceProvisioningRCB(POS_DEVICE_CERTIFICATE,
               //    gst_CSRConfig.b_isCertAvailable, gst_CSRConfig.u8_certPositionOnNVM3);

               // Copy the generated CSR into RAM
               gst_CSROutput.u16_CSRLen = (uint16_t)t_CSRLenDER;
               for (size_t t_lpIdx = 0; t_lpIdx < t_CSRLenDER; t_lpIdx++)
               {
                  gst_CSROutput.u8ar_CSR[t_lpIdx] = ucar_CSRBuffDER[t_lpIdx];
               }

               app_log_info("Generated CSR: ");
               app_log_hexdump_info(gst_CSROutput.u8ar_CSR, gst_CSROutput.u16_CSRLen);
               app_log_append(APP_LOG_NL);

               // Set CSR as present with the configured NVM3 ID.
               gv_UpdateDeviceProvisioningRCB(POS_CSR, true, \
                  gst_CSRConfig.u8_CSRPositionOnNVM3);

               // Store the generated CSR to NVM3
               t_retVal = gt_StoreCertificate(
                  gst_CSROutput.u8ar_CSR, // Buffer where the certificate is stored
                  gst_CSROutput.u16_CSRLen,
                                             // Variable where the size of the certificate is stored
                  CSR_GENERATOR_NVM3_REGION, // Region in NVM3 for device certificate
                  (CSR_GENERATOR_DEVICE_PROVISIONING_RCB_NVM3_TAG | \
                     gst_deviceProvisioningRCB.u8ar_positionNVM3[POS_CSR])
                                             // Starting key for device certificate
               );

               app_assert((t_retVal == SL_STATUS_OK), \
                  "Could not store the generated CSR on NVM3." APP_LOG_NL);
            }
         }
      }

      // Store the provisioning record control block
      t_retVal = st_WriteDeviceProvisioningRCB(&gst_deviceProvisioningRCB);
      app_assert((t_retVal == SL_STATUS_OK), \
         "Could not create the provisioning control block." APP_LOG_NL);

      app_log_info("Provisioning control block successfully created." APP_LOG_NL);
      gst_CSROutput.u8_isCSRGenerated = 1;
      gb_isCSRAvailable = 1;
   }
}

/**
 * @public        gt_CalculateSHA1
 * @brief         Calculate SHA-1 u8pt_hash.
 * @param[in]     u8pt_data Pointer to input data.
 * @param[in]     t_len Length of input data.
 * @param[out]    u8pt_hash Pointer to output hash buffer.
 * @return        SL_STATUS_OK upon successful hash calculation, or an error code otherwise.
 */
sl_status_t gt_CalculateSHA1(const uint8_t *u8pt_data, size_t t_len, uint8_t *u8pt_hash)
{
   psa_hash_operation_t st_operation = { 0 };
   psa_algorithm_t u32_algo = PSA_ALG_SHA_1;
   size_t t_outputLen = 0;
   uint8_t u8ar_result[CRYPTO_SHA_1_LEN];
   sl_status_t t_retVal = SL_STATUS_OK;

   if (!u8pt_data || !t_len  || !u8pt_hash)
   {
      t_retVal = SL_STATUS_INVALID_PARAMETER;
   }

   if (SL_STATUS_OK == t_retVal)
   {
      t_retVal = gt_PSAStatus2SLStatus(psa_hash_setup(&st_operation, u32_algo));

      if (SL_STATUS_OK == t_retVal)
      {
         t_retVal = gt_PSAStatus2SLStatus(psa_hash_update(&st_operation, u8pt_data, t_len));

         if (SL_STATUS_OK == t_retVal)
         {
            t_retVal = gt_PSAStatus2SLStatus(psa_hash_finish(&st_operation, u8ar_result, CRYPTO_SHA_1_LEN, &t_outputLen));

            if (SL_STATUS_OK == t_retVal)
            {
               memcpy(u8pt_hash, u8ar_result, CRYPTO_SHA_1_LEN);
            }
         }
      }
   }

   return t_retVal;
}

/**
 * @public        gt_ExportPubKey
 * @brief         Exports the public key.
 * @param[in]     t_key Pointer to input data.
 * @param[out]    u8pt_data Length of input data.
 * @param[in]     t_dataSize Pointer to output hash buffer.
 * @param[out]    tpt_dataLen Length of output hash buffer.
 * @return        SL_STATUS_OK upon successful hash calculation, or an error code otherwise.
 */
sl_status_t gt_ExportPubKey(mbedtls_svc_key_id_t t_key, uint8_t *u8pt_data, size_t t_dataSize, size_t *tpt_dataLen)
{
   psa_status_t t_retVal;

   // Check if parameters are valid
   if (!t_key || !u8pt_data || !t_dataSize || !tpt_dataLen)
   {
      t_retVal = SL_STATUS_INVALID_PARAMETER;
   }
   else
   {
      t_retVal = psa_export_public_key(t_key, u8pt_data, t_dataSize, tpt_dataLen);
   }

   return gt_PSAStatus2SLStatus(t_retVal);
}

/**
 * @public        gt_ReadDeviceProvisioningRCB
 * @brief         Read the provisioning record control block from NVM3.
 * @param[in]     stpt_RCB Pointer to the provisioning record control block structure.
 * @return        Status of the read operation.
 *                SL_STATUS_OK in case of no error.
 */
sl_status_t gt_ReadDeviceProvisioningRCB(DeviceProvisioningRCB_T *stpt_RCB)
{
   size_t t_len = sizeof(DeviceProvisioningRCB_T);

   // Read the device provisioning record control block from NVM3 area
   return gt_ReadRawNVM3(
      CSR_GENERATOR_NVM3_REGION,             // Region
      CSR_GENERATOR_DEVICE_PROVISIONING_RCB_NVM3_TAG,
                                             // Device provisioning RCB tag
      (uint8_t *)stpt_RCB,                   // Buffer
      &t_len                                 // Length
   );
}

/**
 * @public        gv_UpdateDeviceProvisioningRCB
 * @brief         Update the device provisioning control block. It will check if
 *                any particular entry is configured to be present on NVM3. If it
 *                is, then it flags the corrosponding bit and also updates its
 *                NVM3 ID.
 * @param[in]     i32_idx Index (position) of the control block entry.
 * @param[in]     b_isPresent Flag indicating if the entry is present.
 * @param[in]     u8_NVM3ID NVM3 ID associated with the entry.
 * @return        None.
 */
void gv_UpdateDeviceProvisioningRCB(int32_t i32_idx, bool b_isPresent, uint8_t u8_NVM3ID)
{
   // Check if the entry is configured to be present on NVM3
   if (b_isPresent)
   {
      SetBit32(gst_deviceProvisioningRCB.u32_bitmap, i32_idx);
      gst_deviceProvisioningRCB.u8ar_positionNVM3[i32_idx] = u8_NVM3ID;
   }
   else
   {
      ClrBit32(gst_deviceProvisioningRCB.u32_bitmap, i32_idx);
      gst_deviceProvisioningRCB.u8ar_positionNVM3[i32_idx] = 0;
   }
}

/**
 * @public        st_WriteDeviceProvisioningRCB
 * @brief         Write the provisioning control block.
 * @param[in]     stpt_block Pointer to the control block entry.
 * @return        None.
 */
sl_status_t st_WriteDeviceProvisioningRCB(DeviceProvisioningRCB_T *stpt_block)
{
   return gt_WriteRawNVM3(
      CSR_GENERATOR_NVM3_REGION,             // Region
      CSR_GENERATOR_DEVICE_PROVISIONING_RCB_NVM3_TAG,
                                             // Device provisioning RCB tag
      (uint8_t *)stpt_block,                 // Buffer
      sizeof(DeviceProvisioningRCB_T)        // Length
   );
}

/**
 * @private       gt_CryptoInit
 * @brief         It will check the system's security capability. It shall only
 *                initialize the cryptographic library if the security capability
 *                is of type "Root of Trust", "Secure Element", or "Secure Vault".
 * @return        Converted (from psa_status_t to sl_status_t) status of the initialization.
 *                SL_STATUS_OK in case of no error.
 */
sl_status_t gt_CryptoInit(void)
{
   sl_status_t t_retVal = SL_STATUS_OK;
   SYSTEM_SecurityCapability_TypeDef t_securityCapability;

   // Get the system security capability
   t_securityCapability = SYSTEM_GetSecurityCapability();

   app_log_info("Security capability is: ");

   // Check if the security capability is RoT
   if (securityCapabilityRoT == t_securityCapability)
   {
      app_log_append_info("Root of Trust" APP_LOG_NL);
   }
   // Check if the security capability is SE
   else if (securityCapabilitySE == t_securityCapability)
   {
      app_log_append_info("Secure Element" APP_LOG_NL);
   }
   // Check if the security capability is Secure Vault
   else if (securityCapabilityVault == t_securityCapability)
   {
      app_log_append_info("Secure Vault" APP_LOG_NL);
   }

   // Check if the security capability is supported
   if(t_securityCapability == securityCapabilityRoT || \
      t_securityCapability == securityCapabilitySE || \
      t_securityCapability == securityCapabilityVault)
   {
      // Initialize the cryptographic library
      t_retVal = gt_PSAStatus2SLStatus(psa_crypto_init());
   }

  return t_retVal;
}

/**
 * @public        gt_PSAStatus2SLStatus
 * @brief         Convert PSA status to SL status.
 * @param[in]     t_PSAStatus PSA status to convert.
 * @return        Converted sl_status_t.
 */
sl_status_t gt_PSAStatus2SLStatus(psa_status_t t_PSAStatus)
{
   sl_status_t t_retVal;

   switch (t_PSAStatus)
   {
      case PSA_SUCCESS:
      {
         t_retVal = SL_STATUS_OK;
      }
      break;
      case PSA_ERROR_GENERIC_ERROR:
      {
         t_retVal = SL_STATUS_FAIL;
      }
      break;
      case PSA_ERROR_NOT_SUPPORTED:
      {
         t_retVal = SL_STATUS_NOT_SUPPORTED;
      }
      break;
      case PSA_ERROR_NOT_PERMITTED:
      {
         t_retVal = SL_STATUS_PERMISSION;
      }
      break;
      case PSA_ERROR_BUFFER_TOO_SMALL:
      {
         t_retVal = SL_STATUS_WOULD_OVERFLOW;
      }
      break;
      case PSA_ERROR_ALREADY_EXISTS:
      {
         t_retVal = SL_STATUS_ALREADY_EXISTS;
      }
      break;
      case PSA_ERROR_DOES_NOT_EXIST:
      {
         t_retVal = SL_STATUS_FAIL;
      }
      break;
      case PSA_ERROR_BAD_STATE:
      {
         t_retVal = SL_STATUS_INVALID_STATE;
      }
      break;
      case PSA_ERROR_INVALID_ARGUMENT:
      {
         t_retVal = SL_STATUS_INVALID_PARAMETER;
      }
      break;
      case PSA_ERROR_INSUFFICIENT_MEMORY:
      {
         t_retVal = SL_STATUS_NO_MORE_RESOURCE;
      }
      break;
      case PSA_ERROR_INSUFFICIENT_STORAGE:
      {
         t_retVal = SL_STATUS_NO_MORE_RESOURCE;
      }
      break;
      case PSA_ERROR_COMMUNICATION_FAILURE:
      {
         t_retVal = SL_STATUS_IO;
      }
      break;
      case PSA_ERROR_STORAGE_FAILURE:
      {
         t_retVal = SL_STATUS_BT_HARDWARE;
      }
      break;
      case PSA_ERROR_HARDWARE_FAILURE:
      {
         t_retVal = SL_STATUS_BT_HARDWARE;
      }
      break;
      case PSA_ERROR_CORRUPTION_DETECTED:
      {
         t_retVal = SL_STATUS_BT_DATA_CORRUPTED;
      }
      break;
      case PSA_ERROR_INSUFFICIENT_ENTROPY:
      {
         t_retVal = SL_STATUS_BT_CRYPTO;
      }
      break;
      case PSA_ERROR_INVALID_SIGNATURE:
      {
         t_retVal = SL_STATUS_INVALID_SIGNATURE;
      }
      break;
      case PSA_ERROR_INVALID_PADDING:
      {
         t_retVal = SL_STATUS_BT_CRYPTO;
      }
      break;
      case PSA_ERROR_INSUFFICIENT_DATA:
      {
         t_retVal = SL_STATUS_BT_CRYPTO;
      }
      break;
      case PSA_ERROR_INVALID_HANDLE:
      {
         t_retVal = SL_STATUS_INVALID_HANDLE;
      }
      break;
      case PSA_ERROR_DATA_CORRUPT:
      {
         t_retVal = SL_STATUS_BT_DATA_CORRUPTED;
      }
      break;
      case PSA_ERROR_DATA_INVALID:
      {
         t_retVal = SL_STATUS_BT_CRYPTO;
      }
      break;
      default:
      {
         t_retVal = SL_STATUS_BT_UNSPECIFIED;
      }
      break;
   }

   return t_retVal;
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
