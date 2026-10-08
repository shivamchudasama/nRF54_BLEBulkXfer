/**
 * @file          FOTA_GATT.c
 * @brief         Source file containing GATT database for FOTA service
 * @date          10/02/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "FOTA_GATT.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           FOTA_CHUNK_SIZE
 * @brief         Maximum chunk size for FOTA.
 */
#define FOTA_CHUNK_SIZE 							(MAX_MTU_SIZE_WITH_DLE)

/**
 * @def           FOTA_SLABS_CNT
 * @brief         Number of FOTA slabs.
 */
#define FOTA_SLABS_CNT 								(5U)

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
// static void sv_PopulateFOTAEvent();
static ssize_t st_FOTACtrlWrite(struct bt_conn *stpt_connHandle,
	const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
	uint16_t u16_offset, uint8_t u8_flags);
static ssize_t st_FOTADataTransferWrite(struct bt_conn *stpt_connHandle,
	const struct bt_gatt_attr *stpt_attr, 	const void *vpt_buf, uint16_t u16_length,
	uint16_t u16_offset, uint8_t u8_flags);
static ssize_t st_FOTAStsRead(struct bt_conn *stpt_connHandle,
	const struct bt_gatt_attr *stpt_attr, void *vpt_buf, uint16_t u16_length,
	uint16_t u16_offset);
static ssize_t st_FOTAProgressRead(struct bt_conn *stpt_connHandle,
	const struct bt_gatt_attr *stpt_attr, void *vpt_buf, uint16_t u16_length,
	uint16_t u16_offset);

/******************************************************************************/
/*                                                                            */
/*                             PRIVATE VARIABLES                              */
/*                                                                            */
/******************************************************************************/
/**
 * @var           sst_FOTACtrlCharCPList
 * @brief         An array mapped with FOTA Control characteristic. All the
 * 					Control Packets received from the peer device will be stored in
 * 					this structure after parsing it in the write callback of FOTA
 *                Control characteristic.
 */
static CPList_T sst_FOTACtrlCharCPList = { 0 };

/**
 * @var           sst_FOTACtrlCharCtx
 * @brief         A structure mapped with FOTA Control characteristic. It contains
 * 					pointer to the data buffer and its maximum and current length.
 */
static CharCtx_T sst_FOTACtrlCharCtx = {
	.stpt_data = &sst_FOTACtrlCharCPList,
	.u16_maxLen = MAX_MTU_SIZE_WITH_DLE,
	.u16_curLen = 0U
};

/**
 * @var           sst_FOTADataXferCharCPList
 * @brief         An array mapped with FOTA Data Transfer characteristic. All the
 * 					Data Packets received from the peer device will be stored in
 * 					this structure after parsing it in the write callback of FOTA
 *                Data Transfer characteristic.
 */
static CPList_T sst_FOTADataXferCharCPList = { 0 };

/**
 * @var           sst_FOTADataXferCharCtx
 * @brief         A structure mapped with FOTA Data Transfer characteristic. It contains
 * 					pointer to the data buffer and its maximum and current length.
 */
static CharCtx_T sst_FOTADataXferCharCtx = {
	.stpt_data = &sst_FOTADataXferCharCPList,
	.u16_maxLen = MAX_MTU_SIZE_WITH_DLE,
	.u16_curLen = 0U
};

/**
 * @var           sst_FOTAStatusCharCPList
 * @brief         An array mapped with FOTA Status characteristic. All the
 * 					Data Packets received from the peer device will be stored in
 * 					this structure after parsing it in the write callback of FOTA
 *                Status characteristic.
 */
static CPList_T sst_FOTAStatusCharCPList = { 0 };

/**
 * @var           sst_FOTAStatusCharCtx
 * @brief         A structure mapped with FOTA Status characteristic. It contains
 * 					pointer to the data buffer and its maximum and current length.
 */
static CharCtx_T sst_FOTAStatusCharCtx = {
	.stpt_data = &sst_FOTAStatusCharCPList,
	.u16_maxLen = 4,
	.u16_curLen = 0U
};

/**
 * @var           sst_FOTAProgressCharCPList
 * @brief         An array mapped with FOTA Progress characteristic. All the
 * 					Data Packets received from the peer device will be stored in
 * 					this structure after parsing it in the write callback of FOTA
 *                Progress characteristic.
 */
static CPList_T sst_FOTAProgressCharCPList = { 0 };

/**
 * @var           sst_FOTAProgressCharCtx
 * @brief         A structure mapped with FOTA Progress characteristic. It contains
 * 					pointer to the data buffer and its maximum and current length.
 */
static CharCtx_T sst_FOTAProgressCharCtx = {
	.stpt_data = &sst_FOTAProgressCharCPList,
	.u16_maxLen = 4,
	.u16_curLen = 0U
};

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @var           gstar_FOTASvc
 * @brief         FOTA service instance. Creates a structure of bt_gatt_attr type.
 *                It statically define and register this GATT service.
 */
BT_GATT_SERVICE_DEFINE(gstar_FOTASvc,
	// Primary service declaration with FOTA service UUID
	BT_GATT_PRIMARY_SERVICE(
		// UUID
		BT_UUID_FOTA_SERVICE
	),
   // FOTA control characteristic
	// Characteristic declaration for FOTA control
	BT_GATT_CHARACTERISTIC(
		// UUID
		BT_UUID_FOTA_CONTROL_CHAR,
		// Properties - Write without response
		BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		// BT_GATT_CHRC_WRITE,
		// Permissions - Write
		BT_GATT_PERM_WRITE,
		// Read callback - NULL
		NULL,
		// Write callback - st_FOTACtrlWrite
		st_FOTACtrlWrite,
		// User data - sst_FOTACtrlCharCtx
		&sst_FOTACtrlCharCtx
	),
	BT_GATT_CUD(
		"FOTA Control",
		BT_GATT_PERM_READ
	),
   // FOTA data transfer characteristic
	// Characteristic declaration for FOTA data transfer
	BT_GATT_CHARACTERISTIC(
		// UUID
		BT_UUID_FOTA_DATA_TRANSFER_CHAR,
		// Properties - Write without response
		BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		// BT_GATT_CHRC_WRITE,
		// Permissions - Write
		BT_GATT_PERM_WRITE,
		// Read callback - NULL
		NULL,
		// Write callback - st_FOTADataTransferWrite
		st_FOTADataTransferWrite,
		// User data - sst_FOTADataXferCharCtx
		&sst_FOTADataXferCharCtx
	),
	BT_GATT_CUD(
		"FOTA Data Transfer",
		BT_GATT_PERM_READ
	),
   // FOTA status characteristic
	// Characteristic declaration for FOTA Status
	BT_GATT_CHARACTERISTIC(
		// UUID
		BT_UUID_FOTA_STATUS_CHAR,
		// Properties - Notify or Indicate
		BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_INDICATE,
		// Permissions - None
		BT_GATT_PERM_NONE,
		// Read callback - NULL
		NULL,
		// Write callback - NULL
		NULL,
		// User data - sst_FOTAStatusCharCtx
		&sst_FOTAStatusCharCtx
	),
	BT_GATT_CUD(
		"FOTA Status",
		BT_GATT_PERM_READ
	),
	// Client Characteristic Configuration Descriptor for FOTA Status characteristic
	BT_GATT_CCC(
		// Configuration changed callback - NULL
		NULL,
		// Permissions - Read
		BT_GATT_PERM_READ
	),
   // FOTA progress characteristic
	// Characteristic declaration for FOTA Progress
	BT_GATT_CHARACTERISTIC(
		// UUID
		BT_UUID_FOTA_PROGRESS_CHAR,
		// Properties - Notify or Indicate
		BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_INDICATE,
		// Permissions - None
		BT_GATT_PERM_NONE,
		// Read callback - NULL
		NULL,
		// Write callback - NULL
		NULL,
		// User data - sst_FOTAProgressCharCtx
		&sst_FOTAProgressCharCtx
	),
	BT_GATT_CUD(
		"FOTA Progress",
		BT_GATT_PERM_READ
	),
	BT_GATT_CCC(
		// Configuration changed callback - NULL
		NULL,
		// Permissions - Read
		BT_GATT_PERM_READ
	),
);

/**
 * @var           gu8ar_FOTAData
 * @brief         Data buffer for a FOTA chunk.
 */
uint8_t gu8ar_FOTAData[FOTA_CHUNK_SIZE];

/**
 * @var           gst_FOTASlab
 * @brief         Define a memory slab for FOTA chunks.
 */
K_MEM_SLAB_DEFINE(gst_FOTASlab,					/**< Name of the structure of FOTA slab. */
                  FOTA_CHUNK_SIZE,				/**< Size of each FOTA chunk. */
                  FOTA_SLABS_CNT,				/**< Number of FOTA chunks in the slab. */
                  4);								/**< Alignment of the slab. */

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
#if 0
/**
 * @private       sv_PopulateFOTAEvent
 * @brief         Callback function for write operation on FOTA Control characteristic.
 *                It will parse the incoming data as Control Packets and populate a
 *                FOTA event structure with the parsed information. This event can
 *                then be used as an input to FOTA state machine.
 * @param[inout]  stpt_connHandle Connection handle.
 * @param[in]    	stpt_attr The attribute being written to.
 * @param[in]    	vpt_buf The buffer containing the data being written.
 * @param[in]    	u16_length The length of the data being written.
 * @param[in]    	u16_offset The offset at which the data is being written.
 * @param[in]    	u8_flags Write flags, see @ref bt_gatt_attr_write_flag.
 * @return        Either BT_GATT_ERR() or the number of bytes written.
 */
static void sv_PopulateFOTAEvent(CharCtx_T *stpt_charCtx, FOTAEvent_T *stpt_FOTAEvent)
{
   // Populate the event
   // Check if the CP type of the single CP block in the parsed CP list is FOTA Start
   switch (stpt_charCtx->stpt_data->star_CPBlocks[0].e_CPType)
   {
      case eCPT_FOTA_START:
      {

      }
      break;

      case eCPT_MANIFEST:
      {

      }
      break;

      case eCPT_METADATA:
      {

      }
      break;

      case eCPT_FOTA_DATA:
      {

      }
      break;

      default:
      break;
   }
   if (stpt_charCtx->stpt_data->star_CPBlocks[0].e_CPType == eCPT_FOTA_START)
   {
      // Populate the event
      stpt_FOTAEvent->e_evt = eFE_FOTA_START;
      memcpy(&stpt_FOTAEvent->u_FOTAEventsPayload.st_FOTAStart, \
         stpt_charCtx->stpt_data->star_CPBlocks[0].u8ar_CPData, \
         stpt_charCtx->stpt_data->star_CPBlocks[0].u8_CPBlockLength);
      stpt_FOTAEvent->u16_payloadLength = stpt_charCtx->u16_curLen;
   }
   // Check if the CP type of the single CP block in the parsed CP list is Manifest
   else if (stpt_charCtx->stpt_data->star_CPBlocks[0].e_CPType == eCPT_MANIFEST)
   {
      // Populate the event
      stpt_FOTAEvent->e_evt = eFE_MANIFEST;
      memcpy(&stpt_FOTAEvent->u_FOTAEventsPayload.st_metadata, \
         stpt_charCtx->stpt_data->star_CPBlocks[0].u8ar_CPData, \
         stpt_charCtx->stpt_data->star_CPBlocks[0].u8_CPBlockLength);
      stpt_FOTAEvent->u16_payloadLength = stpt_charCtx->u16_curLen;
   }
   // Check if the CP type of the single CP block in the parsed CP list is Metadata
   else if (stpt_charCtx->stpt_data->star_CPBlocks[0].e_CPType == eCPT_METADATA)
   {
      // Populate the event
      stpt_FOTAEvent->e_evt = eFE_METADATA;
      memcpy(&stpt_FOTAEvent->u_FOTAEventsPayload.st_manifest, \
         stpt_charCtx->stpt_data->star_CPBlocks[0].u8ar_CPData, \
         stpt_charCtx->stpt_data->star_CPBlocks[0].u8_CPBlockLength);
      stpt_FOTAEvent->u16_payloadLength = stpt_charCtx->u16_curLen;
   }
   // Check if the CP type of the single CP block in the parsed CP list is FOTA Data
   else if (stpt_charCtx->stpt_data->star_CPBlocks[0].e_CPType == eCPT_FOTA_DATA)
   {
      // Populate the event
      stpt_FOTAEvent->e_evt = eFE_FOTA_DATA;
      memcpy(&stpt_FOTAEvent->u_FOTAEventsPayload.st_FOTAData, \
         stpt_charCtx->stpt_data->star_CPBlocks[0].u8ar_CPData, \
         stpt_charCtx->stpt_data->star_CPBlocks[0].u8_CPBlockLength);
      stpt_FOTAEvent->u16_payloadLength = stpt_charCtx->u16_curLen;
   }
   else
   {
      LOG_INF("Unsupported Control Packet type received in FOTA Control characteristic data. "
         "Received CP type: %u", st_CPList.star_CPBlocks[0].e_CPType);
   }
}
#endif // 0

/**
 * @private       st_FOTACtrlWrite
 * @brief         Callback function for write operation on FOTA Control characteristic.
 *                It will parse the incoming data as Control Packets and populate a
 *                FOTA event structure with the parsed information. This event can
 *                then be used as an input to FOTA state machine.
 * @param[inout]  stpt_connHandle Connection handle.
 * @param[in]    	stpt_attr The attribute being written to.
 * @param[in]    	vpt_buf The buffer containing the data being written.
 * @param[in]    	u16_length The length of the data being written.
 * @param[in]    	u16_offset The offset at which the data is being written.
 * @param[in]    	u8_flags Write flags, see @ref bt_gatt_attr_write_flag.
 * @return        Either BT_GATT_ERR() or the number of bytes written.
 */
static ssize_t st_FOTACtrlWrite(struct bt_conn *stpt_connHandle,
	const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
	uint16_t u16_offset, uint8_t u8_flags)
{
	ssize_t t_retVal = 0;
	// Take a pointer to the user data of this attribute and consider it as a byte array,
	//	as we are expecting byte array input for this characteristic.
	CharCtx_T *stpt_charCtx = stpt_attr->user_data;
	// Create a temporary CP list structure to hold the parsed Control Packets from the incoming data.
	CPList_T st_CPList;
	FOTAEvent_T st_FOTAEvent = { 0 };
	bool b_isOk2Proceed = false;

	// Perform all the basic checks for the incoming write request parameters before processing the data.
	// Check if the connection handle, attribute pointer, input buffer pointer or
	// characteristic context pointer is NULL.
	if (!stpt_connHandle || !stpt_attr || !vpt_buf || !stpt_charCtx)
	{
		t_retVal = BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		LOG_INF("Invalid parameters in FOTA Control characteristic write. "
			"Connection Handle: %p, Attribute: %p, Buffer: %p", stpt_connHandle, stpt_attr, vpt_buf);
	}
	// Check if the write operation is not starting at offset 0, as we do not support
	//	offset writes for this characteristic.
	else if (u16_offset != 0U)
	{
		t_retVal = BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
		LOG_INF("Offset write not supported for FOTA Control characteristic. "
			"Received u16_offset: %u", u16_offset);
	}
	// We want to keep writing to this characteristic with variable length data,
	// so we will only check if the length of incoming data is more than the buffer size.
	else if (u16_length > stpt_charCtx->u16_maxLen)
	{
		t_retVal = BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		LOG_INF("FOTA Control characteristic write length exceeds buffer size. "
			"Received length: %u", u16_length);
	}

	// Check if there is not any error in parameters
	if (0 == t_retVal)
	{
		// Writing to this characteristic will considered as an input to FOTA state machine.

		LOG_INF("Received data from FOTA Control characteristic.");

      /************************************************************************/
      // Note: We are not copying the incoming data to the mapped data buffer of
      // this characteristic, as we are directly parsing the incoming data buffer
      // and place this parsed information in the mapped data buffer of this
      // characteristic. This is done to avoid unnecessary copying of data and
      // optimize memory usage. Later, we should also use mutexes to protect the static
		//	structure which has been registered while defining the GATT database.
		//	Only the pointer of this mutex is passed to the event over ZBUS to the
		//	subscriber. The subscriber shall use mutex to read the actual value of
		//	received data over BLE.
      /************************************************************************/

		// Update the current length of the data in the buffer.
		stpt_charCtx->u16_curLen = u16_length;

		// SDC: For testing, we should send 0x05, 0x01, 0xAA, 0xBB, 0xCC, 0xDD over BLE

		// Check if parsing the Control Packets from the incoming data and
		// populating the temporary CP list structure completed successfully.
      if (eTP_OK == ge_TP_ParseCPList((uint8_t *)vpt_buf, stpt_charCtx->u16_curLen, stpt_charCtx->stpt_data))
		{
			// Check if there is only 1 CP block in the parsed CP list
         if (1 == stpt_charCtx->stpt_data->u8_totCPBlock)
			{
				// Populate the event
				// Check if the CP type of the single CP block in the parsed CP list is FOTA Start
				if (stpt_charCtx->stpt_data->star_CPBlocks[0].e_CPType == eCPT_FOTA_START)
				{
               // Populate the event
					st_FOTAEvent.e_evt = eFE_FOTA_START;
               memcpy(&st_FOTAEvent.u8ar_Payload[0], \
						&stpt_charCtx->stpt_data->star_CPBlocks[0].u8ar_CPData[0], \
						stpt_charCtx->stpt_data->star_CPBlocks[0].u8_CPBlockLength);
					st_FOTAEvent.u16_payloadLength = stpt_charCtx->stpt_data->star_CPBlocks[0].u8_CPBlockLength;
					b_isOk2Proceed = true;
				}
				// Check if the CP type of the single CP block in the parsed CP list is Manifest
				else if (stpt_charCtx->stpt_data->star_CPBlocks[0].e_CPType == eCPT_MANIFEST)
				{
               // Populate the event
					st_FOTAEvent.e_evt = eFE_MANIFEST;
               memcpy(&st_FOTAEvent.u8ar_Payload[0], \
						&stpt_charCtx->stpt_data->star_CPBlocks[0].u8ar_CPData[0], \
						stpt_charCtx->stpt_data->star_CPBlocks[0].u8_CPBlockLength);
               st_FOTAEvent.u16_payloadLength = stpt_charCtx->stpt_data->star_CPBlocks[0].u8_CPBlockLength;
					b_isOk2Proceed = true;
				}
				// Check if the CP type of the single CP block in the parsed CP list is Metadata
				else if (stpt_charCtx->stpt_data->star_CPBlocks[0].e_CPType == eCPT_METADATA)
				{
               // Populate the event
					st_FOTAEvent.e_evt = eFE_METADATA;
               memcpy(&st_FOTAEvent.u8ar_Payload[0], \
						&stpt_charCtx->stpt_data->star_CPBlocks[0].u8ar_CPData[0], \
						stpt_charCtx->stpt_data->star_CPBlocks[0].u8_CPBlockLength);
               st_FOTAEvent.u16_payloadLength = stpt_charCtx->stpt_data->star_CPBlocks[0].u8_CPBlockLength;
					b_isOk2Proceed = true;
				}
				else
				{
					LOG_ERR("Unsupported Control Packet type received in FOTA Control characteristic data. "
						"Received CP type: %u", st_CPList.star_CPBlocks[0].e_CPType);
				}

				// Check if the CP block parsed and event populated
				if (b_isOk2Proceed = true)
				{
					// Publish this event
					if (0 == zbus_chan_pub(&FOTAEventChannel, &st_FOTAEvent, K_NO_WAIT))
					{
						// LOG_INF("Published FOTA Event to ZBUS channel successfully.");
					}
					else
					{
						LOG_ERR("Failed to publish FOTA Event to ZBUS channel.");
					}
				}
			}
			else
			{
				LOG_ERR("Invalid number of CP blocks in FOTA Control characteristic data. "
					"Parsed CP block count: %u", st_CPList.u8_totCPBlock);
			}
		}
		else
		{
			LOG_ERR("Failed to parse Control Packets from FOTA Control characteristic data.");
		}

		// Finally, after successful completion,  update the buffer with incoming data
		// and return the length of data written.
		t_retVal = u16_length;
	}

	return t_retVal;
}

/**
 * @private       st_FOTADataTransferWrite
 * @brief         <Function details>.
 * @param[inout]  stpt_connHandle Connection handle.
 * @param[in]    	stpt_attr The attribute being written to.
 * @param[in]    	vpt_buf The buffer containing the data being written.
 * @param[in]    	u16_length The length of the data being written.
 * @param[in]    	u16_offset The offset at which the data is being written.
 * @param[in]    	u8_flags Write flags, see @ref bt_gatt_attr_write_flag.
 * @return        Either BT_GATT_ERR() or the number of bytes written.
 */
static ssize_t st_FOTADataTransferWrite(struct bt_conn *stpt_connHandle,
	const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
	uint16_t u16_offset, uint8_t u8_flags)
{
#if 1
	ssize_t t_retVal = 0;
	// Take a pointer to the user data of this attribute and consider it as a byte array,
	//	as we are expecting byte array input for this characteristic.
	CharCtx_T *stpt_charCtx = stpt_attr->user_data;
	// Create a temporary CP list structure to hold the parsed Control Packets from the incoming data.
	CPList_T st_CPList;
	FOTAEvent_T st_FOTAEvent = { 0 };
   int64_t u64_delta = 0;
	uint8_t *u8pt_FOTAData;
   static int64_t slu64_msgRef = 0;
   static bool slb_msgRefInit = false;

	// Perform all the basic checks for the incoming write request parameters before processing the data.
	// Check if the connection handle, attribute pointer, input buffer pointer or
	// characteristic context pointer is NULL.
	if (!stpt_connHandle || !stpt_attr || !vpt_buf || !stpt_charCtx)
	{
		t_retVal = BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		LOG_INF("Invalid parameters in FOTA Data Transfer characteristic write. "
			"Connection Handle: %p, Attribute: %p, Buffer: %p", stpt_connHandle, stpt_attr, vpt_buf);
	}
	// Check if the write operation is not starting at offset 0, as we do not support
	//	offset writes for this characteristic.
	else if (u16_offset != 0U)
	{
		t_retVal = BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
		LOG_INF("Offset write not supported for FOTA Data Transfer characteristic. "
			"Received u16_offset: %u", u16_offset);
	}
	// We want to keep writing to this characteristic with variable length data,
	// so we will only check if the length of incoming data is more than the buffer size.
	else if (u16_length > stpt_charCtx->u16_maxLen)
	{
		t_retVal = BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		LOG_INF("FOTA Data Transfer characteristic write length exceeds buffer size. "
			"Received length: %u", u16_length);
	}

	// Check if there is not any error in parameters
	if (0 == t_retVal)
	{
		// Writing to this characteristic will considered as an input to FOTA state machine.

		LOG_INF("Received data from FOTA Data Transfer characteristic.");

      // Check if the message reference timestamp isn't initialized.
      if (!slb_msgRefInit)
      {
         slu64_msgRef = k_uptime_get();
         slb_msgRefInit = true;
         LOG_INF("First message at %lld ms", (long long)slu64_msgRef);
      }

      u64_delta = k_uptime_delta(&slu64_msgRef);
      LOG_INF("Delta=%lld ms (now=%lld ms)", (long long)u64_delta, (long long)slu64_msgRef);

      // Update the current length of the data in the buffer.
		stpt_charCtx->u16_curLen = u16_length;

      // Check if parsing the Control Packets from the incoming data and
      // populating the temporary CP list structure completed successfully.
      if (eTP_OK == ge_TP_ParseCPList((uint8_t *)vpt_buf, stpt_charCtx->u16_curLen, stpt_charCtx->stpt_data))
		{
			// Check if there is only 1 CP block in the parsed CP list
         if (1 == stpt_charCtx->stpt_data->u8_totCPBlock)
         {
            // Populate the event
				// Check if the CP type of the single CP block in the parsed CP list is FOTA Data
				if (stpt_charCtx->stpt_data->star_CPBlocks[0].e_CPType == eCPT_FOTA_DATA)
				{
					// Check if allocation of a FOTA chunk from the slab is successful or not
					if (k_mem_slab_alloc(&gst_FOTASlab, (void **)&u8pt_FOTAData, K_NO_WAIT) != 0)
					{
						LOG_ERR("Failed to allocate memory for FOTA chunk from slab.");

						// Terminate the connection as we've encountered an error in processing the FOTA data
						bt_conn_disconnect(gstpt_currentConn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
					}
					else
					{
						// Copy the FOTA data from the parsed CP block to the allocated FOTA chunk slab.
						memcpy(u8pt_FOTAData, &stpt_charCtx->stpt_data->star_CPBlocks[0].u8ar_CPData[0],
							stpt_charCtx->stpt_data->star_CPBlocks[0].u8_CPBlockLength);

						// Populate the event
						st_FOTAEvent.e_evt = eFE_FOTA_DATA;
						st_FOTAEvent.u8pt_payload = u8pt_FOTAData;
						st_FOTAEvent.u16_payloadLength = stpt_charCtx->stpt_data->star_CPBlocks[0].u8_CPBlockLength;
					}
				}
            else
				{
					LOG_ERR("Unsupported Control Packet type received in FOTA data transfer characteristic data. "
						"Received CP type: %u", st_CPList.star_CPBlocks[0].e_CPType);
				}

            // Publish this event
				if (0 == zbus_chan_pub(&FOTAEventChannel, &st_FOTAEvent, K_NO_WAIT))
				{
					// LOG_INF("Published FOTA Event to ZBUS channel successfully.");
				}
				else
				{
					LOG_ERR("Failed to publish FOTA Event to ZBUS channel.");
				}
         }
         else
			{
				LOG_ERR("Invalid number of CP blocks in FOTA data transfer characteristic data. "
					"Parsed CP block count: %u", st_CPList.u8_totCPBlock);
			}
      }
      else
		{
			LOG_ERR("Failed to parse Control Packets from FOTA data transfer characteristic data.");
		}

      // Finally, after successful completion,  update the buffer with incoming data
		// and return the length of data written.
		t_retVal = u16_length;
	}

	return t_retVal;
#endif // 0
}

/* ================= Read Handlers ================= */
/**
 * @private       st_FOTAStsRead
 * @brief         <Function details>.
 * @param[inout]  stpt_connHandle Connection handle.
 * @param[in]    	stpt_attr The attribute being read from.
 * @param[in]    	vpt_buf The buffer to be filled with the data being read.
 * @param[in]    	u16_length The length of the buffer.
 * @param[in]    	u16_offset The offset at which the data is being read.
 * @return        Either BT_GATT_ERR() or the number of bytes written.
 */
static ssize_t st_FOTAStsRead(struct bt_conn *stpt_connHandle,
	const struct bt_gatt_attr *stpt_attr, void *vpt_buf, uint16_t u16_length,
	uint16_t u16_offset)
{
#if 0
	ssize_t t_retVal = 0;
	// Take a pointer to the user data of this attribute and consider it as a byte array,
	//	as we are expecting byte array input for this characteristic.
	CharCtx_T *stpt_charCtx = stpt_attr->user_data;

	// Perform all the basic checks for the incoming read request parameters before processing the data.
	// Check if the connection handle, attribute pointer, input buffer pointer or
	// characteristic context pointer is NULL.
	if (!stpt_connHandle || !stpt_attr || !vpt_buf || !stpt_charCtx)
	{
		t_retVal = BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		LOG_INF("Invalid parameters in FOTA Status characteristic read. "
			"Connection Handle: %p, Attribute: %p, Buffer: %p", stpt_connHandle, stpt_attr, vpt_buf);
	}
	// Check if the read operation is not starting at offset 0, as we do not support
	//	offset reads for this characteristic.
	else if (u16_offset != 0U)
	{
		t_retVal = BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
		LOG_INF("Offset read not supported for FOTA Status characteristic. "
			"Received u16_offset: %u", u16_offset);
	}

	// Check if there is not any error in parameters
	if (0 == t_retVal)
	{
		// Read the local attribute value from the local database.
		t_retVal = bt_gatt_attr_read(stpt_connHandle, stpt_attr, vpt_buf, u16_length,
			u16_offset, stpt_charCtx->u8pt_data, stpt_charCtx->u16_maxLen);
	}

	return t_retVal;
#endif // 0
}

/**
 * @private       st_FOTAProgressRead
 * @brief         <Function details>.
 * @param[inout]  stpt_connHandle Connection handle.
 * @param[in]    	stpt_attr The attribute being read from.
 * @param[in]    	vpt_buf The buffer to be filled with the data being read.
 * @param[in]    	u16_length The length of the buffer.
 * @param[in]    	u16_offset The offset at which the data is being read.
 * @return        Either BT_GATT_ERR() or the number of bytes written.
 */
static ssize_t st_FOTAProgressRead(struct bt_conn *stpt_connHandle,
	const struct bt_gatt_attr *stpt_attr, void *vpt_buf, uint16_t u16_length,
	uint16_t u16_offset)
{
#if 0
	ssize_t t_retVal = 0;
	// Take a pointer to the user data of this attribute and consider it as a byte array,
	//	as we are expecting byte array input for this characteristic.
	CharCtx_T *stpt_charCtx = stpt_attr->user_data;

	// Perform all the basic checks for the incoming read request parameters before processing the data.
	// Check if the connection handle, attribute pointer, input buffer pointer or
	// characteristic context pointer is NULL.
	if (!stpt_connHandle || !stpt_attr || !vpt_buf || !stpt_charCtx)
	{
		t_retVal = BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		LOG_INF("Invalid parameters in FOTA Progress characteristic read. "
			"Connection Handle: %p, Attribute: %p, Buffer: %p", stpt_connHandle, stpt_attr, vpt_buf);
	}
	// Check if the read operation is not starting at offset 0, as we do not support
	//	offset reads for this characteristic.
	else if (u16_offset != 0U)
	{
		t_retVal = BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
		LOG_INF("Offset read not supported for FOTA Progress characteristic. "
			"Received u16_offset: %u", u16_offset);
	}

	// Check if there is not any error in parameters
	if (0 == t_retVal)
	{
		// Read the local attribute value from the local database.
		t_retVal = bt_gatt_attr_read(stpt_connHandle, stpt_attr, vpt_buf, u16_length,
			u16_offset, stpt_charCtx->u8pt_data, stpt_charCtx->u16_maxLen);
	}

	return t_retVal;
#endif // 0
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
