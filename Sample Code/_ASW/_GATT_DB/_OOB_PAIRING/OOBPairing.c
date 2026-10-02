/**
 * @file          OOBPairing.c
 * @brief         Source file containing GATT database for OOB Pairing service.
 * @date          18/03/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "OOBPairing.h"

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

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           su8_isReadyToPair
 * @brief         Local data variable for the 'Is Ready To Pair' characteristic.
 */
static uint8_t su8_isReadyToPair = 0U;

/**
 * @var           su8_isPairingTriggered
 * @brief         Local data variable for the 'Is Pairing Triggered' characteristic.
 */
static uint8_t su8_isPairingTriggered = 0U;

/**
 * @var           su8ar_targetDeviceToPair
 * @brief         Local data variable for the 'Target Device To Pair' characteristic.
 *                Declared as a byte array sized to the characteristic's
 *                maximum length (6 bytes, per the GATT XML descriptor).
 */
static uint8_t su8ar_targetDeviceToPair[6U] = { 0U };

/**
 * @var           su8_pairingStatus
 * @brief         Local data variable for the 'Pairing Status' characteristic.
 */
static uint8_t su8_pairingStatus = 0U;

/**
 * @var           su8_ecuRole
 * @brief         Local data variable for the 'ECU Role' characteristic.
 */
static uint8_t su8_ecuRole = 0U;

/**
 * @var           su8ar_oobData
 * @brief         Local data variable for the 'OOB Data' characteristic.
 *                Declared as a byte array sized to the characteristic's
 *                maximum length (96 bytes, per the GATT XML descriptor).
 */
static uint8_t su8ar_oobData[96U] = { 0U };

/**
 * @var           sstar_OOBPairingSvc
 * @brief         OOB pairing service instance.
 */
BT_GATT_SERVICE_DEFINE(sstar_OOBPairingSvc,
   // Primary service declaration with OOB Pairing service UUID
   BT_GATT_PRIMARY_SERVICE(
      // UUID
      BT_UUID_OOB_PAIRING_SERVICE
   ),
   // Characteristic declaration for 'Is Ready To Pair'
   BT_GATT_CHARACTERISTIC(
      // UUID
      BT_UUID_IS_READY_TO_PAIR_CHAR,
      // Properties - Read
      BT_GATT_CHRC_READ,
      // Permissions - Read
      BT_GATT_PERM_READ,
      // Read callback - gt_GATT_GenericRead
      gt_GATT_GenericRead,
      // Write callback - NULL
      NULL,
      // User data - &gst_isReadyToPairDesc
      &gst_isReadyToPairDesc
   ),
   BT_GATT_CUD(
      "Is Ready To Pair",
      BT_GATT_PERM_READ
   ),
   // Characteristic declaration for 'Is Pairing Triggered'
   BT_GATT_CHARACTERISTIC(
      // UUID
      BT_UUID_IS_PAIRING_TRIGGERED_CHAR,
      // Properties - Write
      BT_GATT_CHRC_WRITE,
      // Permissions - Write
      BT_GATT_PERM_WRITE,
      // Read callback - NULL
      NULL,
      // Write callback - gt_GATT_GenericWrite
      gt_GATT_GenericWrite,
      // User data - &gst_isPairingTriggeredDesc
      &gst_isPairingTriggeredDesc
   ),
   BT_GATT_CUD(
      "Is Pairing Triggered",
      BT_GATT_PERM_READ
   ),
   // Characteristic declaration for 'Target Device To Pair'
   BT_GATT_CHARACTERISTIC(
      // UUID
      BT_UUID_TARGET_DEVICE_TO_PAIR_CHAR,
      // Properties - Write
      BT_GATT_CHRC_WRITE,
      // Permissions - Write
      BT_GATT_PERM_WRITE,
      // Read callback - NULL
      NULL,
      // Write callback - gt_GATT_GenericWrite
      gt_GATT_GenericWrite,
      // User data - &gst_targetDeviceToPairDesc
      &gst_targetDeviceToPairDesc
   ),
   BT_GATT_CUD(
      "Target Device To Pair",
      BT_GATT_PERM_READ
   ),
   // Characteristic declaration for 'Pairing Status'
   BT_GATT_CHARACTERISTIC(
      // UUID
      BT_UUID_PAIRING_STATUS_CHAR,
      // Properties - Read
      BT_GATT_CHRC_READ,
      // Permissions - Read
      BT_GATT_PERM_READ,
      // Read callback - gt_GATT_GenericRead
      gt_GATT_GenericRead,
      // Write callback - NULL
      NULL,
      // User data - &gst_pairingStatusDesc
      &gst_pairingStatusDesc
   ),
   BT_GATT_CUD(
      "Pairing Status",
      BT_GATT_PERM_READ
   ),
   // Characteristic declaration for 'ECU Role'
   BT_GATT_CHARACTERISTIC(
      // UUID
      BT_UUID_ECU_ROLE_CHAR,
      // Properties - Write
      BT_GATT_CHRC_WRITE,
      // Permissions - Write
      BT_GATT_PERM_WRITE,
      // Read callback - NULL
      NULL,
      // Write callback - gt_GATT_GenericWrite
      gt_GATT_GenericWrite,
      // User data - &gst_ECURoleDesc
      &gst_ECURoleDesc
   ),
   BT_GATT_CUD(
      "ECU Role",
      BT_GATT_PERM_READ
   ),
   // Characteristic declaration for 'OOB Data'
   BT_GATT_CHARACTERISTIC(
      // UUID
      BT_UUID_OOB_DATA_CHAR,
      // Properties - Read
      BT_GATT_CHRC_READ,
      // Permissions - Read
      BT_GATT_PERM_READ,
      // Read callback - gt_GATT_GenericRead
      gt_GATT_GenericRead,
      // Write callback - NULL
      NULL,
      // User data - &gst_OOBDataDesc
      &gst_OOBDataDesc
   ),
   BT_GATT_CUD(
      "OOB Data",
      BT_GATT_PERM_READ
   )
);

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/

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
 * @var           gst_isReadyToPairDesc
 * @brief         Descriptor for the 'Is Ready To Pair' characteristic.
 *                Fixed-length (1 byte).
 */
GATTCharDescriptor_T gst_isReadyToPairDesc = {
   .vpt_data          = &su8_isReadyToPair,
   .u16_dataLen       = sizeof(su8_isReadyToPair),
   .u16_actualLen     = sizeof(su8_isReadyToPair),
   .b_variableLength  = false,
   .stpt_mutex        = NULL,
   .fpt_customReadCb  = NULL,
   .fpt_customWriteCb = NULL,
};

/**
 * @var           gst_isPairingTriggeredDesc
 * @brief         Descriptor for the 'Is Pairing Triggered' characteristic.
 *                Fixed-length (1 byte).
 */
GATTCharDescriptor_T gst_isPairingTriggeredDesc = {
   .vpt_data          = &su8_isPairingTriggered,
   .u16_dataLen       = sizeof(su8_isPairingTriggered),
   .u16_actualLen     = sizeof(su8_isPairingTriggered),
   .b_variableLength  = false,
   .stpt_mutex        = NULL,
   .fpt_customReadCb  = NULL,
   .fpt_customWriteCb = NULL,
};

/**
 * @var           gst_targetDeviceToPairDesc
 * @brief         Descriptor for the 'Target Device To Pair' characteristic.
 *                Fixed-length array (6 bytes).
 */
GATTCharDescriptor_T gst_targetDeviceToPairDesc = {
   .vpt_data          = su8ar_targetDeviceToPair,
   .u16_dataLen       = sizeof(su8ar_targetDeviceToPair),
   .u16_actualLen     = 0U,
   .b_variableLength  = true,
   .stpt_mutex        = NULL,
   .fpt_customReadCb  = NULL,
   .fpt_customWriteCb = NULL,
};

/**
 * @var           gst_pairingStatusDesc
 * @brief         Descriptor for the 'Pairing Status' characteristic.
 *                Fixed-length (1 byte).
 */
GATTCharDescriptor_T gst_pairingStatusDesc = {
   .vpt_data          = &su8_pairingStatus,
   .u16_dataLen       = sizeof(su8_pairingStatus),
   .u16_actualLen     = sizeof(su8_pairingStatus),
   .b_variableLength  = false,
   .stpt_mutex        = NULL,
   .fpt_customReadCb  = NULL,
   .fpt_customWriteCb = NULL,
};

/**
 * @var           gst_ECURoleDesc
 * @brief         Descriptor for the 'ECU Role' characteristic.
 *                Fixed-length (1 byte).
 */
GATTCharDescriptor_T gst_ECURoleDesc = {
   .vpt_data          = &su8_ecuRole,
   .u16_dataLen       = sizeof(su8_ecuRole),
   .u16_actualLen     = sizeof(su8_ecuRole),
   .b_variableLength  = false,
   .stpt_mutex        = NULL,
   .fpt_customReadCb  = NULL,
   .fpt_customWriteCb = NULL,
};

/**
 * @var           gst_OOBDataDesc
 * @brief         Descriptor for the 'OOB Data' characteristic.
 *                Fixed-length array (96 bytes).
 */
GATTCharDescriptor_T gst_OOBDataDesc = {
   .vpt_data          = su8ar_oobData,
   .u16_dataLen       = sizeof(su8ar_oobData),
   .u16_actualLen     = 0U,
   .b_variableLength  = true,
   .stpt_mutex        = NULL,
   .fpt_customReadCb  = NULL,
   .fpt_customWriteCb = NULL,
};

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/

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
