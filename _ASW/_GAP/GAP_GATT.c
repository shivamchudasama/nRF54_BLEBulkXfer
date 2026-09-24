/**
 * @file          GAP_GATT.c
 * @brief         Source file containing GATT database for GAP service
 * @date          12/02/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "GAP_GATT.h"

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
/*                       PRIVATE FUNCTION DECLARATIONS                        */
/*                                                                            */
/******************************************************************************/
static ssize_t st_DeviceNameRead(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	void *buf, uint16_t len,
	uint16_t offset);
static ssize_t st_AppearanceRead(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	void *buf, uint16_t len,
	uint16_t offset);
static ssize_t st_ManufacturerNameRead(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	void *buf, uint16_t len,
	uint16_t offset);
static ssize_t st_HWRevisionRead(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	void *buf, uint16_t len,
	uint16_t offset);
static ssize_t st_FWRevisionRead(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	void *buf, uint16_t len,
	uint16_t offset);

/******************************************************************************/
/*                                                                            */
/*                              PUBLIC VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           su8ar_deviceName
 * @brief         Name of the device.
 */
static const uint8_t su8ar_deviceName[] = "BLE Bulk Transfer";

/**
 * @var           su16_appearance
 * @brief         Appearance of the device.
 */
static const uint16_t su16_appearance = BLE_APPEARANCE_MOTORIZED_VEHICLE_2_WHEELED_VEHICLE;

/**
 * @var           gstar_GAPSvc
 * @brief         GAP service instance. Creates a structure of bt_gatt_attr type.
 *                It statically define and register this GATT service.
 */
BT_GATT_SERVICE_DEFINE(gstar_GAPSvc,
	// Primary service declaration with GAP service UUID
	BT_GATT_PRIMARY_SERVICE(
		// UUID
		BT_UUID_DECLARE_16(BLE_SERVICE_GAP_UUID)
	),
	// Characteristic declaration for device name
	BT_GATT_CHARACTERISTIC(
		// UUID
		BT_UUID_DECLARE_16(BLE_CHAR_DEVICE_NAME_UUID),
		// Properties - Read
		BT_GATT_CHRC_READ,
		// Permissions - Read
		BT_GATT_PERM_READ,
		// Read callback - st_DeviceNameRead
		st_DeviceNameRead,
		// Write callback - NULL
		NULL,
		// User data - su8ar_deviceName
		su8ar_deviceName
	),
	BT_GATT_CUD(
		"Device Name",
		BT_GATT_PERM_READ
	),
	// Characteristic declaration for appearance
	BT_GATT_CHARACTERISTIC(
		// UUID
		BT_UUID_DECLARE_16(BLE_CHAR_APPEARANCE_UUID),
		// Properties - Read
		BT_GATT_CHRC_READ,
		// Permissions - Read
		BT_GATT_PERM_READ,
		// Read callback - st_AppearanceRead
		st_AppearanceRead,
		// Write callback - NULL
		NULL,
		// User data - su16_appearance
		&su16_appearance
	),
	BT_GATT_CUD(
		"Appearance",
		BT_GATT_PERM_READ
	),
);

/**
 * @var           scar_manufacturerName
 * @brief         Name of the manufacturer.
 */
static const char scar_manufacturerName[] = "Bajaj Auto Technology Limited";

/**
 * @var           scar_HWRevision
 * @brief         Revision of the hardware.
 */
static const char scar_HWRevision[] = "0.1.1";

/**
 * @var           scar_FWRevision
 * @brief         Revision of the firmware.
 */
static const char scar_FWRevision[] = "0.1.2";

/**
 * @var           gstar_deviceInfoSvc
 * @brief         GAP service instance. Creates a structure of bt_gatt_attr type.
 *                It statically define and register this GATT service.
 */
BT_GATT_SERVICE_DEFINE(gstar_deviceInfoSvc,
	// Primary service declaration with device information service UUID
	BT_GATT_PRIMARY_SERVICE(
		// UUID
		BT_UUID_DECLARE_16(BLE_SERVICE_DEVICE_INFORMATION_UUID)
	),
	// Characteristic declaration for manufacturer name
	BT_GATT_CHARACTERISTIC(
		// UUID
		BT_UUID_DECLARE_16(BLE_CHAR_MANUFACTURER_NAME_STRING_UUID),
		// Properties - Read
		BT_GATT_CHRC_READ,
		// Permissions - Read
		BT_GATT_PERM_READ,
		// Read callback - st_ManufacturerNameRead
		st_ManufacturerNameRead,
		// Write callback - NULL
		NULL,
		// User data - scar_manufacturerName
		scar_manufacturerName
	),
	BT_GATT_CUD(
		"Manufacturer Name",
		BT_GATT_PERM_READ
	),
	// Characteristic declaration for HW revision
	BT_GATT_CHARACTERISTIC(
		// UUID
		BT_UUID_DECLARE_16(BLE_CHAR_HARDWARE_REVISION_STRING_UUID),
		// Properties - Read
		BT_GATT_CHRC_READ,
		// Permissions - Read
		BT_GATT_PERM_READ,
		// Read callback - st_HWRevisionRead
		st_HWRevisionRead,
		// Write callback - NULL
		NULL,
		// User data - scar_HWRevision
		scar_HWRevision
	),
	BT_GATT_CUD(
		"Hardware Revision",
		BT_GATT_PERM_READ
	),
	// Characteristic declaration for FW revision
	BT_GATT_CHARACTERISTIC(
		// UUID
		BT_UUID_DECLARE_16(BLE_CHAR_FIRMWARE_REVISION_STRING_UUID),
		// Properties - Read
		BT_GATT_CHRC_READ,
		// Permissions - Read
		BT_GATT_PERM_READ,
		// Read callback - st_FWRevisionRead
		st_FWRevisionRead,
		// Write callback - NULL
		NULL,
		// User data - scar_FWRevision
		scar_FWRevision
	),
	BT_GATT_CUD(
		"Firmware Revision",
		BT_GATT_PERM_READ
	),
);

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
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                              PUBLIC VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           <Variable name>
 * @brief         <Variable details>.
 */

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
/* ================= Read Handlers ================= */
/**
 * @private       st_DeviceNameRead
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
static ssize_t st_DeviceNameRead(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	void *buf, uint16_t len,
	uint16_t offset)
{
	const char *cpt_value = attr->user_data;
	uint16_t u16_valueLen = sizeof(su8ar_deviceName);

	LOG_INF("Device Name read requested, UUID is: %x", (BT_UUID_16(attr->uuid))->val);
	LOG_INF("Device Name read requested, Handle is: %u", bt_gatt_attr_get_handle(attr));

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
	                         cpt_value, u16_valueLen);
}

/**
 * @private       st_AppearanceRead
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
static ssize_t st_AppearanceRead(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	void *buf, uint16_t len,
	uint16_t offset)
{
	const char *cpt_value = attr->user_data;
	uint16_t u16_valueLen = sizeof(su16_appearance);

	LOG_INF("Appearance read requested, UUID is: %x", (BT_UUID_16(attr->uuid))->val);
	LOG_INF("Appearance read requested, Handle is: %u", bt_gatt_attr_get_handle(attr));

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
	                         cpt_value, u16_valueLen);
}

/**
 * @private       st_ManufacturerNameRead
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
static ssize_t st_ManufacturerNameRead(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	void *buf, uint16_t len,
	uint16_t offset)
{
	const char *cpt_value = attr->user_data;
	uint16_t u16_valueLen = (uint16_t)strlen(cpt_value);

	LOG_INF("Manufacturer Name read requested, UUID is: %x", (BT_UUID_16(attr->uuid))->val);
	LOG_INF("Manufacturer Name read requested, Handle is: %u", bt_gatt_attr_get_handle(attr));

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
	                         cpt_value, u16_valueLen);
}

/**
 * @private       st_HWRevisionRead
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
static ssize_t st_HWRevisionRead(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	void *buf, uint16_t len,
	uint16_t offset)
{
	const char *cpt_value = attr->user_data;
	uint16_t u16_valueLen = (uint16_t)strlen(cpt_value);

	LOG_INF("Hardware Revision read requested, UUID is: %x", (BT_UUID_16(attr->uuid))->val);
	LOG_INF("Hardware Revision read requested, Handle is: %u", bt_gatt_attr_get_handle(attr));

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
	                         cpt_value, u16_valueLen);
}

/**
 * @private       st_FWRevisionRead
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */
static ssize_t st_FWRevisionRead(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	void *buf, uint16_t len,
	uint16_t offset)
{
	const char *cpt_value = attr->user_data;
	uint16_t u16_valueLen = (uint16_t)strlen(cpt_value);

	LOG_INF("Firmware Revision read requested, UUID is: %x", (BT_UUID_16(attr->uuid))->val);
	LOG_INF("Firmware Revision read requested, Handle is: %u", bt_gatt_attr_get_handle(attr));

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
	                         cpt_value, u16_valueLen);
}

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        <Function name>
 * @brief         <Function details>.
 * @param[in]     <Input parameter details>.
 * @param[out]    <Output parameter details>.
 * @param[inout]  <Input-Output parameter details>.
 * @return        <Return details>.
 */

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
