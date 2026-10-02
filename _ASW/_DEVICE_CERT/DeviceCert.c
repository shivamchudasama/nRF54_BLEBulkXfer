/**
 * @file          DeviceCert.c
 * @brief         Source file containing the CA and device certificate buffers and
 *                their storage in PSA Internal Trusted Storage (ITS).
 *
 *                Both certificates are stored together, only once the device
 *                certificate is verified against the CA (Prov.c), and loaded
 *                together at boot. Each is one ITS entry holding its record
 *                ([u8 flag][u16 length][DER], packed) up to the DER length:
 *                - CA certificate:     DEVICE_CERT_STORAGE_UID_BASE | 3;
 *                - device certificate: DEVICE_CERT_STORAGE_UID_BASE | 1.
 * @date          05/09/2025
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdbool.h>
#include <string.h>
#include "DeviceCert.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           DEVICE_CERT_STORAGE_UID_BASE
 * @brief         Base UID for certificate storage. First 7 bytes are ASCII "DEVCERT",
 *                last 1 bytes are for specific entries.
 */
#define DEVICE_CERT_STORAGE_UID_BASE         ((psa_storage_uid_t)0x4445564345525400ULL)

/**
 * @def           DEVICE_CERT_RECORD_HEADER_LEN
 * @brief         Length of a stored record before the DER data (flag + length).
 *                Same for CACertData_T and DeviceCertData_T.
 */
#define DEVICE_CERT_RECORD_HEADER_LEN        (offsetof(DeviceCertData_T, u8ar_DeviceCert))

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        CertRecord_T
 * @brief         Where one certificate record lives in RAM and in ITS.
 */
typedef struct
{
   const char *cpt_name;                     /**< Name for the log                  */
   uint8_t u8_flag;                          /**< Record flag (1 = valid)            */
   uint16_t u16_len;                         /**< Record DER length                  */
   void *vpt_record;                         /**< Whole record                       */
   size_t t_recordSize;                      /**< sizeof() the record                */
} CertRecord_T;

/******************************************************************************/
/*                                                                            */
/*                                   UNIONS                                   */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/*                                                                            */
/******************************************************************************/
static psa_storage_uid_t st_GetUID(uint8_t u8_offset);
static void sv_GetRecord(bool b_isCA, CertRecord_T *stpt_record);
static psa_storage_uid_t st_UIDOf(bool b_isCA);
static psa_status_t st_StoreRecord(bool b_isCA);
static psa_status_t st_LoadRecord(bool b_isCA);
static psa_status_t st_RemoveRecord(bool b_isCA);

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
 * @var           gst_deviceCertData
 * @brief         Instance of DeviceCertData_T structure. The verified device
 *                certificate, received from the provisioner or loaded from ITS.
 */
DeviceCertData_T gst_deviceCertData = { 0 };

/**
 * @var           gst_CACertData
 * @brief         Instance of CACertData_T structure. The verified CA certificate,
 *                received from the provisioner or loaded from ITS.
 */
CACertData_T gst_CACertData = { 0 };

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/

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
 * @private       st_GetUID
 * @brief         Get the PSA storage UID of a certificate entry.
 * @param[in]     u8_offset Entry offset (gst_CSRConfig.u8_*PositionOnITS).
 * @return        PSA storage UID.
 */
static psa_storage_uid_t st_GetUID(uint8_t u8_offset)
{
   return (DEVICE_CERT_STORAGE_UID_BASE | (psa_storage_uid_t)u8_offset);
}

/**
 * @private       sv_GetRecord
 * @brief         Describe the CA or the device certificate record, with a copy of
 *                its current flag and length (the records are packed, so their
 *                fields are read by value, never through a pointer).
 * @param[in]     b_isCA true for the CA certificate, false for the device's.
 * @param[out]    stpt_record Record description.
 * @return        None.
 */
static void sv_GetRecord(bool b_isCA, CertRecord_T *stpt_record)
{
   // Check which certificate is described
   if (b_isCA)
   {
      stpt_record->cpt_name = "CA certificate";
      stpt_record->u8_flag = gst_CACertData.u8_isCACertReceived;
      stpt_record->u16_len = gst_CACertData.u16_CACertLen;
      stpt_record->vpt_record = &gst_CACertData;
      stpt_record->t_recordSize = sizeof(gst_CACertData);
   }
   else
   {
      stpt_record->cpt_name = "Device certificate";
      stpt_record->u8_flag = gst_deviceCertData.u8_isDeviceCertGenerated;
      stpt_record->u16_len = gst_deviceCertData.u16_deviceCertLen;
      stpt_record->vpt_record = &gst_deviceCertData;
      stpt_record->t_recordSize = sizeof(gst_deviceCertData);
   }
}

/**
 * @private       st_UIDOf
 * @brief         UID of the CA or the device certificate entry.
 * @param[in]     b_isCA true for the CA certificate.
 * @return        PSA storage UID.
 */
static psa_storage_uid_t st_UIDOf(bool b_isCA)
{
   return st_GetUID(b_isCA ? gst_CSRConfig.u8_CACertPositionOnITS :
      gst_CSRConfig.u8_certPositionOnITS);
}

/**
 * @private       st_StoreRecord
 * @brief         Persist one in-RAM certificate record into trusted storage.
 * @param[in]     b_isCA true for the CA certificate.
 * @return        PSA_SUCCESS, PSA_ERROR_INVALID_ARGUMENT if the RAM record is not
 *                valid, or the psa_its_set() error.
 */
static psa_status_t st_StoreRecord(bool b_isCA)
{
   CertRecord_T st_rec;
   psa_status_t t_status;

   sv_GetRecord(b_isCA, &st_rec);

   // Check if the in-RAM record is valid for storage
   if ((st_rec.u8_flag != 1u) || (st_rec.u16_len == 0u) ||
      (st_rec.u16_len > DEVICE_CERT_MAX_DER_LEN))
   {
      APP_LOG_ERR("%s is not valid for persistent storage", st_rec.cpt_name);
      return PSA_ERROR_INVALID_ARGUMENT;
   }

   t_status = psa_its_set(st_UIDOf(b_isCA),
      DEVICE_CERT_RECORD_HEADER_LEN + (size_t)st_rec.u16_len, st_rec.vpt_record,
      PSA_STORAGE_FLAG_NONE);

   // Check if the record was stored
   if (t_status == PSA_SUCCESS)
   {
      APP_LOG_INF("%s stored in trusted storage (%u bytes)", st_rec.cpt_name,
         st_rec.u16_len);
   }
   else
   {
      APP_LOG_ERR("Failed to store %s: %d", st_rec.cpt_name, t_status);
   }

   return t_status;
}

/**
 * @private       st_LoadRecord
 * @brief         Load one certificate record from trusted storage into RAM. The
 *                RAM record is cleared first and stays cleared on any failure.
 * @param[in]     b_isCA true for the CA certificate.
 * @return        PSA_SUCCESS, PSA_ERROR_DOES_NOT_EXIST if not stored,
 *                PSA_ERROR_DATA_CORRUPT if the stored record is not valid, or the
 *                psa_its_get_info()/psa_its_get() error.
 */
static psa_status_t st_LoadRecord(bool b_isCA)
{
   CertRecord_T st_rec;
   struct psa_storage_info_t st_info = { 0 };
   size_t t_bytesRead = 0;
   psa_status_t t_status;

   sv_GetRecord(b_isCA, &st_rec);
   memset(st_rec.vpt_record, 0, st_rec.t_recordSize);

   t_status = psa_its_get_info(st_UIDOf(b_isCA), &st_info);

   // Check if the record is stored
   if (t_status != PSA_SUCCESS)
   {
      return t_status;
   }

   // Check if the stored size can hold a record
   if ((st_info.size <= DEVICE_CERT_RECORD_HEADER_LEN) || (st_info.size > st_rec.t_recordSize))
   {
      APP_LOG_ERR("Stored %s has invalid size: %u", st_rec.cpt_name,
         (unsigned int)st_info.size);
      return PSA_ERROR_DATA_CORRUPT;
   }

   t_status = psa_its_get(st_UIDOf(b_isCA), 0u, st_info.size, st_rec.vpt_record,
      &t_bytesRead);

   // Take the flag and the length just read
   sv_GetRecord(b_isCA, &st_rec);

   // Check if the record was read whole and is consistent
   if ((t_status == PSA_SUCCESS) &&
      ((t_bytesRead != st_info.size) || (st_rec.u8_flag != 1u) ||
      (st_rec.u16_len == 0u) || (st_rec.u16_len > DEVICE_CERT_MAX_DER_LEN) ||
      (st_info.size != (DEVICE_CERT_RECORD_HEADER_LEN + (size_t)st_rec.u16_len))))
   {
      t_status = PSA_ERROR_DATA_CORRUPT;
   }

   // Check if loading failed
   if (t_status != PSA_SUCCESS)
   {
      APP_LOG_ERR("Stored %s could not be loaded: %d", st_rec.cpt_name, t_status);
      memset(st_rec.vpt_record, 0, st_rec.t_recordSize);
      return t_status;
   }

   APP_LOG_INF("%s restored from trusted storage (%u bytes)", st_rec.cpt_name,
      st_rec.u16_len);

   return PSA_SUCCESS;
}

/**
 * @private       st_RemoveRecord
 * @brief         Delete one certificate record from trusted storage (the RAM
 *                record is left as it is).
 * @param[in]     b_isCA true for the CA certificate.
 * @return        PSA_SUCCESS if removed or not stored, else the psa_its_remove() error.
 */
static psa_status_t st_RemoveRecord(bool b_isCA)
{
   CertRecord_T st_rec;
   psa_status_t t_status;

   sv_GetRecord(b_isCA, &st_rec);

   t_status = psa_its_remove(st_UIDOf(b_isCA));

   // Check if the record was not stored
   if (t_status == PSA_ERROR_DOES_NOT_EXIST)
   {
      t_status = PSA_SUCCESS;
   }
   // Check if the record could not be removed
   else if (t_status != PSA_SUCCESS)
   {
      APP_LOG_ERR("Failed to remove stored %s: %d", st_rec.cpt_name, t_status);
   }

   return t_status;
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gt_StoreCACert
 * @brief         Persist gst_CACertData (flag set, 1..DEVICE_CERT_MAX_DER_LEN bytes)
 *                into trusted storage.
 * @return        PSA_SUCCESS, PSA_ERROR_INVALID_ARGUMENT if the RAM record is not
 *                valid, or the psa_its_set() error.
 */
psa_status_t gt_StoreCACert(void)
{
   return st_StoreRecord(true);
}

/**
 * @public        gt_StoreDeviceCert
 * @brief         Persist gst_deviceCertData (flag set, 1..DEVICE_CERT_MAX_DER_LEN
 *                bytes) into trusted storage.
 * @return        PSA_SUCCESS, PSA_ERROR_INVALID_ARGUMENT if the RAM record is not
 *                valid, or the psa_its_set() error.
 */
psa_status_t gt_StoreDeviceCert(void)
{
   return st_StoreRecord(false);
}

/**
 * @public        gt_LoadStoredCerts
 * @brief         Load the CA and the device certificate from trusted storage into
 *                gst_CACertData and gst_deviceCertData. Only a complete pair is
 *                kept; on any other outcome both RAM records are cleared. Nothing
 *                is removed from storage: the caller decides (Prov.c wipes).
 * @return        PSA_SUCCESS (both loaded and well-formed), PSA_ERROR_DOES_NOT_EXIST
 *                (neither stored), PSA_ERROR_DATA_CORRUPT (only one stored, or a
 *                record is malformed), or another PSA storage error.
 */
psa_status_t gt_LoadStoredCerts(void)
{
   psa_status_t t_caStatus;
   psa_status_t t_devStatus;
   psa_status_t t_status;

   t_caStatus = st_LoadRecord(true);
   t_devStatus = st_LoadRecord(false);

   // Check if both certificates were loaded
   if ((t_caStatus == PSA_SUCCESS) && (t_devStatus == PSA_SUCCESS))
   {
      return PSA_SUCCESS;
   }

   // Check if nothing is stored (not provisioned)
   if ((t_caStatus == PSA_ERROR_DOES_NOT_EXIST) && (t_devStatus == PSA_ERROR_DOES_NOT_EXIST))
   {
      t_status = PSA_ERROR_DOES_NOT_EXIST;
   }
   // Check if exactly one of them is stored (an incomplete pair)
   else if ((t_caStatus == PSA_ERROR_DOES_NOT_EXIST) || (t_devStatus == PSA_ERROR_DOES_NOT_EXIST))
   {
      APP_LOG_ERR("Only one certificate of the pair is stored");
      t_status = PSA_ERROR_DATA_CORRUPT;
   }
   else
   {
      t_status = (t_caStatus != PSA_SUCCESS) ? t_caStatus : t_devStatus;
   }

   gv_ClearCertData();

   return t_status;
}

/**
 * @public        gt_RemoveStoredCerts
 * @brief         Delete both certificates from trusted storage. The RAM records
 *                are left as they are (see gv_ClearCertData()).
 * @return        PSA_SUCCESS if both are gone (also when not stored), otherwise
 *                the first psa_its_remove() error.
 */
psa_status_t gt_RemoveStoredCerts(void)
{
   psa_status_t t_caStatus;
   psa_status_t t_devStatus;

   t_devStatus = st_RemoveRecord(false);
   t_caStatus = st_RemoveRecord(true);

   return (t_devStatus != PSA_SUCCESS) ? t_devStatus : t_caStatus;
}

/**
 * @public        gv_ClearCertData
 * @brief         Clear the RAM records of both certificates.
 * @return        None.
 */
void gv_ClearCertData(void)
{
   memset(&gst_CACertData, 0, sizeof(gst_CACertData));
   memset(&gst_deviceCertData, 0, sizeof(gst_deviceCertData));
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
