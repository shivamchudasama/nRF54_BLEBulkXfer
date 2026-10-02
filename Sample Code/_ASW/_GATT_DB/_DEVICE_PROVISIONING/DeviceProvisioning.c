/**
 * @file          DeviceProvisioning.c
 * @brief         Source file containing GATT database for Device Provisioning service.
 * @date          09/03/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "DeviceProvisioning.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          DeviceProvisioningStep_E
 * @brief         Ordered steps for the device provisioning protocol.
 */
typedef enum
{
   eDPS_WAIT_CSR_GENERATION_STATUS_READ = 0, /**< CSR generation status must be read first */
   eDPS_WAIT_CSR_DATA_READ,                  /**< CSR data read may start */
   eDPS_CSR_DATA_READ_IN_PROGRESS,           /**< CSR data is being read in-order */
   eDPS_WAIT_DEVICE_CERT_DATA_WRITE,         /**< Device certificate write may start */
   eDPS_DEVICE_CERT_DATA_WRITE_IN_PROGRESS,  /**< Device certificate is being written in-order */
   eDPS_COMPLETE,                            /**< Provisioning flow completed for this connection */
} DeviceProvisioningStep_E;

/**
 * @enum          DeviceProvisioningChar_E
 * @brief         Internal identifiers for device provisioning characteristics.
 */
typedef enum
{
   eDPC_CSR_DATA = 0,                        /**< CSR data characteristic */
   eDPC_DEVICE_CERT_DATA,                    /**< Device certificate data characteristic */
   eDPC_CSR_GENERATION_STATUS,               /**< CSR generation status characteristic */
   eDPC_DEVICE_CERT_RECEIVED_STATUS,         /**< Device certificate received status characteristic */
} DeviceProvisioningChar_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        DeviceProvisioningFlowCtx_T
 * @brief         Runtime context owned by the device provisioning flow.
 */
typedef struct
{
   uint16_t u16_nextCSRReadOffset;           /**< Next CSR offset expected from client */
   uint16_t u16_nextDeviceCertWriteOffset;   /**< Next certificate write offset expected from client */
   uint16_t u16_receivedDeviceCertLength;    /**< Total certificate bytes received so far */
} DeviceProvisioningFlowCtx_T;

/******************************************************************************/
/*                                                                            */
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/*                                                                            */
/******************************************************************************/
static void sv_DeviceProvisioningConnected(struct bt_conn *stpt_connHandle, uint8_t u8_err);
static void sv_DeviceProvisioningDisconnected(struct bt_conn *stpt_connHandle, uint8_t u8_reason);
static ssize_t st_DeviceCertDataWrite(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset, uint8_t u8_flags);
static ssize_t st_DeviceCertReceivedStatusWrite(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset, uint8_t u8_flags);

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           su8ar_csrData
 * @brief         Local GATT database buffer for 'CSR Data' characteristic.
 *                Application pushes generated CSR data into this buffer via
 *                gv_GATT_LocalWrite(&gst_CSRDataDesc, ...).
 */
static uint8_t su8ar_csrData[CSR_MAX_DER_LEN] = { 0 };

/**
 * @var           su8ar_deviceCertData
 * @brief         Local GATT database buffer for 'Device Cert Data' characteristic.
 *                Remote client writes certificate chunks into this buffer.
 *                Application retrieves data via
 *                gv_GATT_LocalRead(&gst_deviceCertDataDesc, ...).
 */
static uint8_t su8ar_deviceCertData[DEVICE_CERT_MAX_DER_LEN] = { 0 };

/**
 * @var           su8_csrGenerationStatus
 * @brief         Local GATT database buffer for 'CSR Generation Status' characteristic.
 *                Application pushes the CSR generation flag via
 *                gv_GATT_LocalWrite(&gst_CSRGenerationStatusDesc, ...).
 */
static uint8_t su8_csrGenerationStatus = 0;

/**
 * @var           su8_deviceCertReceivedStatus
 * @brief         Local GATT database buffer for 'Device Cert Received Status' characteristic.
 *                Remote client writes the status byte into this buffer.
 */
static uint8_t su8_deviceCertReceivedStatus = 0;

/**
 * @var           su16_receivedDeviceCertLength
 * @brief         Running total of device certificate bytes received from the remote client.
 *                Updated by st_DeviceCertDataWrite on every incoming write to the
 *                'Device Cert Data' characteristic. Consumed by
 *                st_DeviceCertReceivedStatusWrite to validate that certificate data
 *                was actually transferred before finalising the provisioning flow.
 */
static uint16_t su16_receivedDeviceCertLength = 0u;

/**
 * @var           sst_deviceProvisioningConnCallbacks
 * @brief         Connection callbacks used to manage device provisioning flow state.
 */
BT_CONN_CB_DEFINE(sst_deviceProvisioningConnCallbacks) = {
   .connected = sv_DeviceProvisioningConnected,
   .disconnected = sv_DeviceProvisioningDisconnected,
};

/**
 * @var           sstar_deviceProvisioningSvc
 * @brief         Device provisioning service instance. Creates a structure of bt_gatt_attr type.
 *                It statically define and register this GATT service.
 */
BT_GATT_SERVICE_DEFINE(sstar_deviceProvisioningSvc,
   // Primary service declaration with Device Provisioning service UUID
   BT_GATT_PRIMARY_SERVICE(
      // UUID
      BT_UUID_DEVICE_PROVISIONING_SERVICE
   ),
   // Characteristic declaration for 'CSR Data'
   BT_GATT_CHARACTERISTIC(
      // UUID
      BT_UUID_CSR_DATA_CHAR,
      // Properties - Read
      BT_GATT_CHRC_READ,
      // Permissions - Read
      BT_GATT_PERM_READ,
      // Read callback - gt_GATT_GenericRead
      gt_GATT_GenericRead,
      // Write callback - NULL
      NULL,
      // User data - gst_CSRDataDesc
      &gst_CSRDataDesc
   ),
   BT_GATT_CUD(
      "CSR Data",
      BT_GATT_PERM_READ
   ),
   // Characteristic declaration for 'Device Cert Data'
   BT_GATT_CHARACTERISTIC(
      // UUID
      BT_UUID_DEVICE_CERT_DATA_CHAR,
      // Properties - Write
      BT_GATT_CHRC_WRITE,
      // Permissions - Write
      BT_GATT_PERM_WRITE,
      // Read callback - NULL
      NULL,
      // Write callback - gt_GATT_GenericWrite
      gt_GATT_GenericWrite,
      // User data - gst_deviceCertDataDesc
      &gst_deviceCertDataDesc
   ),
   BT_GATT_CUD(
      "Device Cert Data",
      BT_GATT_PERM_READ
   ),
   // Characteristic declaration for 'CSR Generation Status'
   BT_GATT_CHARACTERISTIC(
      // UUID
      BT_UUID_CSR_GENERATION_STATUS_CHAR,
      // Properties - Read
      BT_GATT_CHRC_READ,
      // Permissions - Read
      BT_GATT_PERM_READ,
      // Read callback - gt_GATT_GenericRead
      gt_GATT_GenericRead,
      // Write callback - NULL
      NULL,
      // User data - gst_CSRGenerationStatusDesc
      &gst_CSRGenerationStatusDesc
   ),
   BT_GATT_CUD(
      "CSR Generation Status",
      BT_GATT_PERM_READ
   ),
   // Characteristic declaration for 'Device Cert Received Status'
   BT_GATT_CHARACTERISTIC(
      // UUID
      BT_UUID_DEVICE_CERT_RECEIVED_STATUS_CHAR,
      // Properties - Write
      BT_GATT_CHRC_WRITE | BT_GATT_CHRC_READ,
      // Permissions - Write
      BT_GATT_PERM_WRITE | BT_GATT_PERM_READ,
      // Read callback - NULL
      gt_GATT_GenericRead,
      // Write callback - gt_GATT_GenericWrite
      gt_GATT_GenericWrite,
      // User data - gst_deviceCertReceivedStatusDesc
      &gst_deviceCertReceivedStatusDesc
   ),
   BT_GATT_CUD(
      "Device Cert Received Status",
      BT_GATT_PERM_READ
   ),
);

/******************************************************************************/
/*                                                                            */
/*                                   UNIONS                                   */
/*                                                                            */
/******************************************************************************/

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
 * @var           gst_CSRDataDesc
 * @brief         Descriptor for 'CSR Data' characteristic.
 *                Points to the local GATT database buffer su8ar_csrData.
 *                Variable-length, up to CSR_MAX_DER_LEN bytes.
 */
GATTCharDescriptor_T gst_CSRDataDesc = {
   .vpt_data = su8ar_csrData,
   .u16_dataLen = sizeof(su8ar_csrData),
   .u16_actualLen = 0,
   .b_variableLength = true,
   .stpt_mutex = NULL,
   .fpt_customReadCb = NULL,
   .fpt_customWriteCb = NULL,
};

/**
 * @var           gst_deviceCertDataDesc
 * @brief         Descriptor for 'Device Cert Data' characteristic.
 *                Points to the local GATT database buffer su8ar_deviceCertData.
 *                Variable-length, up to DEVICE_CERT_MAX_DER_LEN bytes.
 */
GATTCharDescriptor_T gst_deviceCertDataDesc = {
   .vpt_data = su8ar_deviceCertData,
   .u16_dataLen = sizeof(su8ar_deviceCertData),
   .u16_actualLen = 0,
   .b_variableLength = true,
   .stpt_mutex = NULL,
   .fpt_customReadCb = NULL,
   .fpt_customWriteCb = st_DeviceCertDataWrite,
};

/**
 * @var           gst_CSRGenerationStatusDesc
 * @brief         Descriptor for 'CSR Generation Status' characteristic.
 *                Points to the local GATT database buffer su8_csrGenerationStatus.
 *                Fixed-length, 1 byte.
 */
GATTCharDescriptor_T gst_CSRGenerationStatusDesc = {
   .vpt_data = &su8_csrGenerationStatus,
   .u16_dataLen = sizeof(su8_csrGenerationStatus),
   .u16_actualLen = sizeof(su8_csrGenerationStatus),
   .b_variableLength = false,
   .stpt_mutex = NULL,
   .fpt_customReadCb = NULL,
   .fpt_customWriteCb = NULL,
};

/**
 * @var           gst_deviceCertReceivedStatusDesc
 * @brief         Descriptor for 'Device Cert Received Status' characteristic.
 *                Points to the local GATT database buffer
 *                su8_deviceCertReceivedStatus. Fixed-length, 1 byte.
 */
GATTCharDescriptor_T gst_deviceCertReceivedStatusDesc = {
   .vpt_data = &su8_deviceCertReceivedStatus,
   .u16_dataLen = sizeof(su8_deviceCertReceivedStatus),
   .u16_actualLen = sizeof(su8_deviceCertReceivedStatus),
   .b_variableLength = false,
   .stpt_mutex = NULL,
   .fpt_customReadCb = NULL,
   .fpt_customWriteCb = st_DeviceCertReceivedStatusWrite,
};

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
 * @private       sv_DeviceProvisioningConnected
 * @brief         Reset device provisioning flow for a new BLE connection.
 * @param[in]     stpt_connHandle Connection handle.
 * @param[in]     u8_err Connection error code.
 * @return        None.
 */
static void sv_DeviceProvisioningConnected(struct bt_conn *stpt_connHandle, uint8_t u8_err)
{
   ARG_UNUSED(stpt_connHandle);
   ARG_UNUSED(u8_err);
}

/**
 * @private       sv_DeviceProvisioningDisconnected
 * @brief         Release device provisioning flow for a disconnected BLE connection.
 * @param[in]     stpt_connHandle Connection handle.
 * @param[in]     u8_reason Disconnection reason.
 * @return        None.
 */
static void sv_DeviceProvisioningDisconnected(struct bt_conn *stpt_connHandle, uint8_t u8_reason)
{
   ARG_UNUSED(stpt_connHandle);
   ARG_UNUSED(u8_reason);
}

/**
 * @private       st_DeviceCertDataWrite
 * @brief         Post-write hook for Device Cert Data characteristic.
 *                Called after gt_GATT_GenericWrite has written data into the local
 *                GATT database buffer. It accumulates the total bytes received.
 * @param[inout]  stpt_connHandle Connection handle.
 * @param[in]     stpt_attr The attribute being written to.
 * @param[in]     vpt_buf The buffer containing the data being written.
 * @param[in]     u16_length The length of the data being written.
 * @param[in]     u16_offset The offset at which the data is being written.
 * @param[in]     u8_flags Write flags, see @ref bt_gatt_attr_write_flag.
 * @return        0 to keep the generic callback's return value, or
 *                BT_GATT_ERR() to override it.
 */
static ssize_t st_DeviceCertDataWrite(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset, uint8_t u8_flags)
{
   ssize_t t_retVal = 0;

   ARG_UNUSED(stpt_connHandle);
   ARG_UNUSED(stpt_attr);
   ARG_UNUSED(vpt_buf);
   ARG_UNUSED(u16_offset);
   ARG_UNUSED(u8_flags);

   // Accumulate the total bytes received
   su16_receivedDeviceCertLength += u16_length;

   return t_retVal;
}

/**
 * @private       st_DeviceCertReceivedStatusWrite
 * @brief         Post-write hook for Device Cert Received Status characteristic.
 *                Called after gt_GATT_GenericWrite has written the status byte into
 *                the local GATT database buffer. Validates the written value,
 *                copies certificate data from the local GATT database to the
 *                application structure, stores it to ITS, and advances the flow.
 * @param[inout]  stpt_connHandle Connection handle.
 * @param[in]     stpt_attr The attribute being written to.
 * @param[in]     vpt_buf The buffer containing the data being written.
 * @param[in]     u16_length The length of the data being written.
 * @param[in]     u16_offset The offset at which the data is being written.
 * @param[in]     u8_flags Write flags, see @ref bt_gatt_attr_write_flag.
 * @return        0 to keep the generic callback's return value, or
 *                BT_GATT_ERR() to override it.
 */
static ssize_t st_DeviceCertReceivedStatusWrite(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset, uint8_t u8_flags)
{
   ARG_UNUSED(stpt_connHandle);
   ARG_UNUSED(stpt_attr);
   ARG_UNUSED(vpt_buf);
   ARG_UNUSED(u16_length);
   ARG_UNUSED(u16_offset);
   ARG_UNUSED(u8_flags);

   ssize_t t_retVal = 0;
   uint8_t u8_receivedStatus = 0u;
   uint16_t u16_deviceCertReceivedStatusBytesRead = 0u;
   uint16_t u16_certBytesRead = 0u;
   psa_status_t t_storeStatus;

   // Read the local GATT database for the current characteristic
   gv_GATT_LocalRead(&gst_deviceCertReceivedStatusDesc, (uint8_t *)&u8_receivedStatus,
      sizeof(u8_receivedStatus), &u16_deviceCertReceivedStatusBytesRead);

   // Check if the device certificate has been received
   if (u8_receivedStatus == 1)
   {
      gv_GATT_LocalRead(&gst_deviceCertDataDesc, (uint8_t *)&gst_deviceCertData.u8ar_DeviceCert[0],
         DEVICE_CERT_MAX_DER_LEN, &u16_certBytesRead);

      // Check if the total bytes read matches the currnet length of the certificate
      if ((u16_certBytesRead == su16_receivedDeviceCertLength) && (u16_certBytesRead != 0))
      {
         gst_deviceCertData.u16_deviceCertLen = u16_certBytesRead;
         gst_deviceCertData.u8_isDeviceCertGenerated = u8_receivedStatus;

         // Store the received device certificate to ITS
         t_storeStatus = gt_StoreDeviceCert();

         // Check if the earlier ITS store operation completed successfully
         if (t_storeStatus != PSA_SUCCESS)
         {
            t_retVal = BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
            APP_LOG_ERR("Failed to store received device certificate: %d", t_storeStatus);
         }
      }
      else
      {
         t_retVal = BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
         APP_LOG_ERR("Device certificate is not available.");
      }
   }
   else
   {
      t_retVal = BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
      APP_LOG_WRN("Rejected device certificate received status. Currently received status value: %u",
         u8_receivedStatus);
   }

   // Check if there is any error in the return value
   if (t_retVal != 0)
   {
      // Update the local GATT database with the status value as 0
      u8_receivedStatus = 0;

      gv_GATT_LocalWrite(&gst_deviceCertReceivedStatusDesc, (uint8_t *)&u8_receivedStatus,
         sizeof(u8_receivedStatus));
   }


   return t_retVal;
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF Bajaj Auto Technology Limited (BATL).
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * Bajaj Auto Technology Limited (BATL) IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
