/**
 * @file          DeviceCert.c
 * @brief         Source file containing device certificate
 * @date          05/09/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "DeviceCert.h"

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

/******************************************************************************/
/*                                                                            */
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/
extern uint16_t gu16_remoteDeviceCertDERLen;
extern uint8_t gu8ar_remoteDeviceCertDER[];

/******************************************************************************/
/*                                                                            */
/*                              PUBLIC VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           gcpt_rootCertPEM
 * @brief         Root certificate in PEM format.
 */
const char *gcpt_rootCertPEM = SL_BT_ROOT_CERT;

/**
 * @var           gt_remotePubKeyID
 * @brief         Remote public key ID.
 */
mbedtls_svc_key_id_t gt_remotePubKeyID = 0;

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           su8ar_receivedDeviceCertDER
 * @brief         Buffer to hold the received device certificate in DER format.
 */
static uint8_t su8ar_receivedDeviceCertDER[CHAIN_LINK_DATA_LEN * CHAIN_LINK_DATA_NUM] = { 0 };

/**
 * @var           su16_receivedCertDERLen
 * @brief         Length of the received device certificate.
 */
static uint16_t su16_receivedCertDERLen = 0;

/**
 * @var           sst_rootCertCtx
 * @brief         Root certificate context.
 */
static mbedtls_x509_crt sst_rootCertCtx;

// /**
//  * @var           se_currentState
//  * @brief         Current state of device provisioning FSM.
//  */
// static DeviceProvisioningState_E se_currentState = eDPS_WAIT_FOR_CERT_GENERATION;

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

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gt_VerifyOwnDeviceCertificate
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
sl_status_t gt_VerifyOwnDeviceCertificate(void)
{
   sl_status_t t_retVal = SL_STATUS_OK;
   int i_MbedTLSRetVal = 0;
   mbedtls_x509_crt st_devCertCtx;
   uint8_t u8ar_rootCertDER[CHAIN_LINK_DATA_LEN * CHAIN_LINK_DATA_NUM] = { 0 };
   size_t t_rootCertDERLen;
   uint32_t u32_flags;

   // Initialize cryptographic library
   t_retVal = gt_CryptoInit();
   app_assert_status(t_retVal);

   // Read device provisioning record control block
   t_retVal = gt_ReadDeviceProvisioningRCB(&gst_deviceProvisioningRCB);

   // Check if reading the device provisioning record control block was successful
   if (SL_STATUS_OK == t_retVal)
   {
      // Read the device certificate from NVM3
      t_retVal = gt_GetCertificate(
         gu8ar_deviceCertDER,                // Buffer to store the certificate
         &gu32_deviceCertDERLen,             // Variable to store the length of the certificate
         CSR_GENERATOR_NVM3_REGION,          // Region in NVM3 for device certificate
         (CSR_GENERATOR_DEVICE_PROVISIONING_RCB_NVM3_TAG | \
            gst_deviceProvisioningRCB.u8ar_positionNVM3[POS_DEVICE_CERTIFICATE])
                                             // Starting key for device certificate
      );

      // Check if the device certificate was read successfully
      if (SL_STATUS_OK == t_retVal)
      {
         // Initialize the device certificate context
         mbedtls_x509_crt_init(&st_devCertCtx);

         // Parse the device certificate (in PEM format)
         i_MbedTLSRetVal = mbedtls_x509_crt_parse(
            &st_devCertCtx,                  // Device certificate context
            (const unsigned char *)gu8ar_deviceCertDER,
                                             // Device certificate
            gu32_deviceCertDERLen            // Length of the device certificate
         );

         // Check if parsing the device certificate was successful
         if (0 == i_MbedTLSRetVal)
         {
            // Check if root certificate is available
            if (0 != strlen(gcpt_rootCertPEM))
            {
               // Initialize the root certificate context
               mbedtls_x509_crt_init(&sst_rootCertCtx);

               i_MbedTLSRetVal = mbedtls_base64_decode(
                  u8ar_rootCertDER,          // Buffer to store the decoded root certificate
                  sizeof(u8ar_rootCertDER),  // Size of the buffer
                  &t_rootCertDERLen,         // Length of the decoded root certificate
                  (const unsigned char *)gcpt_rootCertPEM,
                                             // Root certificate (in PEM format)
                  strlen(gcpt_rootCertPEM)   // Length of the root certificate
               );

               // Check if the root certificate was decoded successfully
               if (0 == i_MbedTLSRetVal)
               {
                  // Parse the root certificate (in DER format)
                  i_MbedTLSRetVal = mbedtls_x509_crt_parse(
                     &sst_rootCertCtx,       // Root certificate context
                     (const unsigned char *)u8ar_rootCertDER,
                                             // Root certificate
                     t_rootCertDERLen        // Length of the root certificate
                  );

                  // Check if parsing the root certificate was successful
                  if (0 == i_MbedTLSRetVal)
                  {
                     // Validate device certificate with the root certificate
                     i_MbedTLSRetVal = mbedtls_x509_crt_verify(
                        &st_devCertCtx,      // Context of device certificate to be verified
                        &sst_rootCertCtx,    // Root certificate context
                        NULL,                // List of CRL for root CA
                        NULL,                // Expected common name (CN)
                        &u32_flags,          // Verification flags (result of verification)
                        NULL,                // Verification callback
                        NULL                 // Context for verification callback
                     );

                     // Check if the device certificate is verified
                     if (0 == i_MbedTLSRetVal)
                     {
                        // Free all the certificate contexts
                        mbedtls_x509_crt_free(&st_devCertCtx);
                        mbedtls_x509_crt_free(&sst_rootCertCtx);

                        gb_isDeviceCertVerified = true;
                        app_log_info("Device certificate verification has passed" APP_LOG_NL);
                     }
                  }
               }
            }
         }
      }
   }

   // Check if the device certificate verification has been failed
   if ((SL_STATUS_OK != t_retVal) && (0 != i_MbedTLSRetVal))
   {
      app_log_info("Device certificate verification has been failed" APP_LOG_NL);
   }

   return t_retVal;
}

/**
 * @public        gt_VerifyRemoteDeviceCertificate
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
sl_status_t gt_VerifyRemoteDeviceCertificate(void)
{
   sl_status_t t_retVal = SL_STATUS_OK;
   int i_MbedTLSRetVal = 0;
   mbedtls_x509_crt st_devCertCtx;
   uint8_t u8ar_rootCertDER[CHAIN_LINK_DATA_LEN * CHAIN_LINK_DATA_NUM] = { 0 };
   size_t t_rootCertDERLen;
   uint32_t u32_flags;
   psa_key_attributes_t st_remoteDevicePubKeyAttr = PSA_KEY_ATTRIBUTES_INIT;

   // Initialize cryptographic library
   t_retVal = gt_CryptoInit();
   app_assert_status(t_retVal);

   // Check if the device certificate was read successfully
   if (SL_STATUS_OK == t_retVal)
   {
      // Initialize the remote device certificate context
      mbedtls_x509_crt_init(&st_devCertCtx);

      // Parse the remote device certificate (in PEM format)
      i_MbedTLSRetVal = mbedtls_x509_crt_parse(
         &st_devCertCtx,                     // Device certificate context
         (const unsigned char *)gu8ar_remoteDeviceCertDER,
                                             // Device certificate
         gu16_remoteDeviceCertDERLen         // Length of the device certificate
      );

      // Check if parsing the remote device certificate was successful
      if (0 == i_MbedTLSRetVal)
      {
         // Check if root certificate is available
         if (0 != strlen(gcpt_rootCertPEM))
         {
            // Initialize the root certificate context
            mbedtls_x509_crt_init(&sst_rootCertCtx);

            i_MbedTLSRetVal = mbedtls_base64_decode(
               u8ar_rootCertDER,             // Buffer to store the decoded root certificate
               sizeof(u8ar_rootCertDER),     // Size of the buffer
               &t_rootCertDERLen,            // Length of the decoded root certificate
               (const unsigned char *)gcpt_rootCertPEM,
                                             // Root certificate (in PEM format)
               strlen(gcpt_rootCertPEM)      // Length of the root certificate
            );

            // Check if the root certificate was decoded successfully
            if (0 == i_MbedTLSRetVal)
            {
               // Parse the root certificate (in DER format)
               i_MbedTLSRetVal = mbedtls_x509_crt_parse(
                  &sst_rootCertCtx,          // Root certificate context
                  (const unsigned char *)u8ar_rootCertDER,
                                             // Root certificate
                  t_rootCertDERLen           // Length of the root certificate
               );

               // Check if parsing the root certificate was successful
               if (0 == i_MbedTLSRetVal)
               {
                  // Validate remote device certificate with the root certificate
                  i_MbedTLSRetVal = mbedtls_x509_crt_verify(
                     &st_devCertCtx,         // Context of device certificate to be verified
                     &sst_rootCertCtx,       // Root certificate context
                     NULL,                   // List of CRL for root CA
                     NULL,                   // Expected common name (CN)
                     &u32_flags,             // Verification flags (result of verification)
                     NULL,                   // Verification callback
                     NULL                    // Context for verification callback
                  );

                  // Check if the remote device certificate is verified
                  if (0 == i_MbedTLSRetVal)
                  {
                     gb_isDeviceCertVerified = true;
                     app_log_info("Remote device certificate verification has passed" APP_LOG_NL);

                     // Get the public key from the remote device's device certificate and set attributes
                     psa_set_key_algorithm(&st_remoteDevicePubKeyAttr, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
                     psa_set_key_type(&st_remoteDevicePubKeyAttr, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
                     psa_set_key_usage_flags(&st_remoteDevicePubKeyAttr, PSA_KEY_USAGE_VERIFY_MESSAGE);

                     // Import the public key
                     t_retVal = gt_PSAStatus2SLStatus(psa_import_key(
                        &st_remoteDevicePubKeyAttr,
                                             // Attributes of the public key
                        &st_devCertCtx.pk_raw.p[PUB_KEY_OFFSET],
                                             // Pointer to the public key
                        EC_PUB_KEY_LEN,      // Length of the public key
                        &gt_remotePubKeyID   // Pointer to the public key ID
                     ));

                     // Free all the certificate contexts
                     mbedtls_x509_crt_free(&st_devCertCtx);
                     mbedtls_x509_crt_free(&sst_rootCertCtx);
                  }
               }
            }
         }
      }
   }

   // Check if the device certificate verification has been failed
   if ((SL_STATUS_OK != t_retVal) && (0 != i_MbedTLSRetVal))
   {
      app_log_info("Remote device certificate verification has been failed" APP_LOG_NL);
   }

   return t_retVal;
}

/**
 * @public        gt_GetCertificate
 * @brief         This function will read the device certificate from NVM3.
 *                This is a chain linked object, means the header will indicate
 *                if there is a next link.
 * @param[out]    u8pt_cert Pointer to the buffer where the certificate will be stored.
 * @param[out]    u32pt_certSize Pointer to the variable where the size of the
 *                certificate will be stored.
 * @param[in]     u32_certRegion Region in NVM3 where the certificate (first chain link)
 *                is stored.
 * @param[in]     u32_certTag Tag in NVM3 where the certificate (first chain link)
 *                is stored.
 * @return        SL_STATUS_OK if successful, error code otherwise.
 */
sl_status_t gt_GetCertificate(uint8_t *u8pt_cert, uint32_t *u32pt_certSize, \
   uint32_t u32_certRegion, uint32_t u32_certTag)
{
   sl_status_t t_retVal = SL_STATUS_OK;
   CertificateChainLink_T st_certificateChainLink;
   size_t t_certificateChainLinkSize = sizeof(st_certificateChainLink);
   uint8_t u8_isNextChainLinkExists = 0;
   *u32pt_certSize = 0;
   uint8_t u8_lpIdx = 0;

   // Check if any buffer is null
   if (u8pt_cert == NULL || u32pt_certSize == NULL)
   {
      t_retVal = SL_STATUS_INVALID_PARAMETER;
   }
   else
   {
      // Loop through all certificate chain links stored in NVM3
      do
      {
         // Read the NVM3 area for chain linked certificate
         t_retVal = gt_ReadRawNVM3(u32_certRegion, u32_certTag, \
            (uint8_t *)&st_certificateChainLink, &t_certificateChainLinkSize);

         // Check if reading the chain linked certificate was successful
         if (SL_STATUS_OK == t_retVal)
         {
            // Copy the certificate data to requested buffer
            memcpy(&u8pt_cert[u8_lpIdx * CHAIN_LINK_DATA_LEN], \
               st_certificateChainLink.u8ar_data, st_certificateChainLink.u16_dataLength);

            // Update if next chain link exists
            u8_isNextChainLinkExists = st_certificateChainLink.u16_header;

            // Increment the certificate size with the current chain link's data length
            *u32pt_certSize += st_certificateChainLink.u16_dataLength;

            // Increment the NVM3 tag and loop index
            u32_certTag++;
            u8_lpIdx++;
         }
         else
         {
            break;
         }
      }
      // Check if there are more chain links
      while (u8_isNextChainLinkExists != 0);
   }

   return t_retVal;
}

/**
 * @public        gt_StoreCertificate
 * @brief         This function will store the device certificate to NVM3.
 *                This is a chain linked object, means the header will indicate
 *                if there is a next link.
 * @param[out]    u8pt_cert Pointer to the buffer where the certificate is stored.
 * @param[out]    u32_certSize Size of the certificate.
 * @param[in]     u32_certRegion Region in NVM3 where the certificate (first chain link)
 *                is stored.
 * @param[in]     u32_certTag Tag in NVM3 where the certificate (first chain link)
 *                is stored.
 * @return        SL_STATUS_OK if successful, error code otherwise.
 */
sl_status_t gt_StoreCertificate(uint8_t *u8pt_cert, uint32_t u32_certSize, \
   uint32_t u32_certRegion, uint32_t u32_certTag)
{
   sl_status_t t_retVal = SL_STATUS_OK;
   CertificateChainLink_T st_certificateChainLink = { 0 };
   size_t t_certificateChainLinkSize = sizeof(st_certificateChainLink);
   uint8_t u8_lpIdx = 0;
   uint32_t u32_certSize2Process = u32_certSize;
   uint8_t u8_currentChunkSize = 0;

   // Check if any buffer is null
   if (u8pt_cert == NULL || u32_certSize == 0)
   {
      t_retVal = SL_STATUS_INVALID_PARAMETER;
   }
   else
   {
      // Read the certificate buffer and write it into the NVM3 (in chunks of chain link)
      while (u32_certSize2Process)
      {
         u8_currentChunkSize = \
            (u32_certSize2Process > CHAIN_LINK_DATA_LEN) ? CHAIN_LINK_DATA_LEN : u32_certSize2Process;

         // Copy the buffer data to certificate
         memcpy(st_certificateChainLink.u8ar_data, \
            &u8pt_cert[u8_lpIdx * CHAIN_LINK_DATA_LEN], u8_currentChunkSize);

         // Check if there are still few bytes left to process
         st_certificateChainLink.u16_header = \
            (u32_certSize2Process > u8_currentChunkSize) ? 1 : 0;

         // Update the length for current chain link
         st_certificateChainLink.u16_dataLength = u8_currentChunkSize;

         // Write the current chain link into NVM3
         t_retVal = gt_WriteRawNVM3(u32_certRegion, u32_certTag, \
            (uint8_t *)&st_certificateChainLink, t_certificateChainLinkSize);

         // Check if writing the chain linked certificate was successful
         if (SL_STATUS_OK == t_retVal)
         {
            // Decrement the certificate size to process
            u32_certSize2Process -= u8_currentChunkSize;

            // Increment the NVM3 tag and loop index
            u32_certTag++;
            u8_lpIdx++;
         }
         else
         {
            break;
         }
      }
   }

   return t_retVal;
}

// /**
//  * @public        gt_DeviceProvisioningFSM
//  * @brief         This function implements the device provisioning state machine.
//  * @param[in]     stpt_evt Pointer to the Bluetooth event.
//  * @return        SL_STATUS_OK if successful, error code otherwise.
//  */
// sl_status_t gt_DeviceProvisioningFSM(sl_bt_msg_t *stpt_evt)
// {
//    sl_status_t t_retVal = SL_STATUS_FAIL;

//    // Get different parameters from the GATT update event
//    uint16_t u16_attrHandle = stpt_evt->data.evt_gatt_server_attribute_value.attribute;
//    uint16_t u16_offset = stpt_evt->data.evt_gatt_server_attribute_value.offset;
//    uint16_t u16_valueLen = stpt_evt->data.evt_gatt_server_attribute_value.value.len;
//    uint8_t *u8pt_value = stpt_evt->data.evt_gatt_server_attribute_value.value.data;

//    // Device provisioning record control block structure
//    DeviceProvisioningRCB_T st_deviceProvisioningRCB = {
//       .u32_bitmap = 0,
//       .u8ar_positionNVM3 = {
//          [POS_DEVICE_CERTIFICATE] = DEVICE_CERTIFICATE_NVM3_START_KEY,
//          [POS_DEVICE_EC_KEY] = 0,
//          [POS_STATIC_AUTH_DATA] = 0,
//          [POS_CSR] = DEVICE_CSR_NVM3_START_KEY,
//       },
//       .u16_maxLinkDataLen = CHAIN_LINK_DATA_LEN,
//    };

//    uint16_t u16_certTagNVM3;

//    // Stores the remaining length of the device certificate
//    static uint32_t su32_remainingDeviceCertDERLen = 0;

//    // Stores the offset of received device certificate
//    static uint16_t su16_offset = 0;

//    // // Run the FSM as per the current state, each state is capable of handling
//    // // certain events in that state.
//    // switch (se_currentState)
//    // {
//    //    // // Waiting for the device certificate to be generated
//    //    // case eDPS_WAIT_FOR_CERT_GENERATION:
//    //    // {
//    //    //    // Check if the current event is for certificate generation
//    //    //    if (u16_attrHandle == gattdb_IS_DEVICE_CERT_GENERATED)
//    //    //    {
//    //    //       // Check if the device certificate has been generated successfully
//    //    //       if ((true == *u8pt_value))
//    //    //       {
//    //    //          app_log_info("Device certificate has been successfully generated." APP_LOG_NL);

//    //    //          // Update the current state
//    //    //          se_currentState = eDPS_WAIT_FOR_CERT_LENGTH;
//    //    //       }
//    //    //    }
//    //    //    else
//    //    //    {
//    //    //       app_log_info("Received attribute is not handled at current state of device provisioning state machine." APP_LOG_NL);
//    //    //    }
//    //    // }
//    //    // break;

//    //    // // Waiting for the device certificate length to be updated
//    //    // case eDPS_WAIT_FOR_CERT_LENGTH:
//    //    // {
//    //    //    // Check if the current event is for certificate length
//    //    //    if (u16_attrHandle == gattdb_DEVICE_CERT_LENGTH)
//    //    //    {
//    //    //       app_log_info("Device certificate length has been successfully received." APP_LOG_NL);
//    //    //       app_log_info("Device certificate length: %d" APP_LOG_NL, (*((uint16_t*)(u8pt_value))));
//    //    //       su16_receivedCertDERLen = (*((uint16_t*)(u8pt_value)));

//    //    //       // Check if the device certificate length is non-zero
//    //    //       if (su16_receivedCertDERLen)
//    //    //       {
//    //    //          // Update the remaining bytes for device certificate length
//    //    //          su32_remainingDeviceCertDERLen = su16_receivedCertDERLen;

//    //    //          // Update the current state
//    //    //          se_currentState = eDPS_WAIT_FOR_CERT;
//    //    //       }
//    //    //       else
//    //    //       {
//    //    //          app_log_info("Device certificate length is zero." APP_LOG_NL);
//    //    //       }
//    //    //    }
//    //    //    else
//    //    //    {
//    //    //       app_log_info("Received attribute is not handled at current state of device provisioning state machine." APP_LOG_NL);
//    //    //    }
//    //    // }
//    //    // break;

//    //    // // Waiting for the device certificate to be received.
//    //    // // Note: The current state is fall through state, means once it receives
//    //    // // entire device certificate over GATT, it goes to writing the device certificate
//    //    // // to NVM3 without next GATT event.
//    //    // case eDPS_WAIT_FOR_CERT:
//    //    // {
//    //    //    // Check if the current event is for certificate block (any block from 1 to 8)
//    //    //    // and there are still few bytes remaining to receive
//    //    //    if (((u16_attrHandle == gattdb_DEVICE_CERT_BLOCK1) || \
//    //    //       (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK2) || \
//    //    //       (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK3) || \
//    //    //       (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK4) || \
//    //    //       (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK5) || \
//    //    //       (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK6) || \
//    //    //       (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK7) || \
//    //    //       (u16_attrHandle == gattdb_DEVICE_CERT_BLOCK8)) && \
//    //    //       (su32_remainingDeviceCertDERLen))
//    //    //    {
//    //    //       app_log_info("Device certificate: " APP_LOG_NL);
//    //    //       for (uint8_t u8_lpIdx = 0; u8_lpIdx < u16_valueLen; u8_lpIdx++)
//    //    //       {
//    //    //          app_log("%02X ", u8pt_value[u8_lpIdx]);

//    //    //          // Accumulate the received device certificate
//    //    //          su8ar_receivedDeviceCertDER[su16_offset + u8_lpIdx] = u8pt_value[u8_lpIdx];
//    //    //       }
//    //    //       app_log_nl_info();

//    //    //       // Increment the offset by received device certificate length
//    //    //       su16_offset += u16_valueLen;

//    //    //       // Decrement the remaining length by received device certificate length
//    //    //       su32_remainingDeviceCertDERLen -= u16_valueLen;

//    //    //       // Check if entire device certificate has been received
//    //    //       if (0 == su32_remainingDeviceCertDERLen)
//    //    //       {
//    //    //          app_log_info("Entire certificate has been received." APP_LOG_NL);

//    //    //          // Update the current state
//    //    //          se_currentState = eDPS_WAIT_FOR_WRITING_DEVICE_CERT;
//    //    //       }
//    //    //    }
//    //    //    else
//    //    //    {
//    //    //       app_log_info("Received attribute is not handled at current state of device provisioning state machine." APP_LOG_NL);
//    //    //    }
//    //    // }

//    //    // // Check if the current state is waiting for certificate
//    //    // if (eDPS_WAIT_FOR_CERT == se_currentState)
//    //    // {
//    //    //    break;
//    //    // }

//    //    // // Waiting for writing device certificate into NVM3
//    //    // // Note: The current state is fall through state. If the device certificate
//    //    // // & device provisioning RCB successfully written into NVM3, it should go
//    //    // // to next state.
//    //    // case eDPS_WAIT_FOR_WRITING_DEVICE_CERT:
//    //    // {
//    //    //    // Read device provisioning record control block from NVM3
//    //    //    t_retVal = gt_ReadDeviceProvisioningRCB(&st_deviceProvisioningRCB);

//    //    //    // Check if reading device provisioning RCB was successful
//    //    //    if (SL_STATUS_OK == t_retVal)
//    //    //    {
//    //    //       // Device certificate tag for NVM3
//    //    //       u16_certTagNVM3 = \
//    //    //          (st_deviceProvisioningRCB.u8ar_positionNVM3[POS_DEVICE_CERTIFICATE] | \
//    //    //             CSR_GENERATOR_DEVICE_PROVISIONING_RCB_NVM3_TAG);

//    //    //       // Store the device certificate
//    //    //       t_retVal = gt_StoreCertificate(su8ar_receivedDeviceCertDER, su16_receivedCertDERLen, \
//    //    //          CSR_GENERATOR_NVM3_REGION, u16_certTagNVM3);

//    //    //       // Check if the device certificate has been written successfully
//    //    //       if (SL_STATUS_OK == t_retVal)
//    //    //       {
//    //    //          app_log_info("Device certificate written successfully." APP_LOG_NL);

//    //    //          // Update the device provisioning RCB
//    //    //          gv_UpdateDeviceProvisioningRCB(POS_DEVICE_CERTIFICATE, true, \
//    //    //             gst_CSRConfig.u8_certPositionOnNVM3);

//    //    //          // Store the provisioning record control block
//    //    //          t_retVal = st_WriteDeviceProvisioningRCB(&gst_deviceProvisioningRCB);
//    //    //          app_assert((t_retVal == SL_STATUS_OK), \
//    //    //             "Could not write the device provisioning record control block." APP_LOG_NL);

//    //    //          // Check if the device provisioning RCB has been written successfully
//    //    //          if (SL_STATUS_OK == t_retVal)
//    //    //          {
//    //    //             // Update the current state
//    //    //             se_currentState = eDPS_DEVICE_PROVISIONING_COMPLETE;
//    //    //          }
//    //    //       }
//    //    //    }
//    //    // }

//    //    // // Check if the current state is writing device certificate
//    //    // if (eDPS_WAIT_FOR_WRITING_DEVICE_CERT == se_currentState)
//    //    // {
//    //    //    break;
//    //    // }

//    //    // // Device provisioning complete
//    //    // case eDPS_DEVICE_PROVISIONING_COMPLETE:
//    //    // {
//    //    //    app_log_info("Device provisioning has been completed successfully." APP_LOG_NL);

//    //    //    t_retVal = SL_STATUS_OK;
//    //    // }
//    //    // break;

//    //    // default:
//    //    // {
//    //    //    app_log_info("Unhandled state." APP_LOG_NL);
//    //    // }
//    //    // break;
//    // }

//    return t_retVal;
// }

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
