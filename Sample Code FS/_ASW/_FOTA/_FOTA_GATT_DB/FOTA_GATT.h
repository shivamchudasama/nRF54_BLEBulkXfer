/**
 * @file          FOTA_GATT.h
 * @brief         Header file containing GATT database for FOTA service.
 * @date          10/02/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _FOTA_GATT_H
#define _FOTA_GATT_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/gatt.h>
#include <string.h>
#include "BaseUUIDs.h"
#include "FOTA_GATT_Types.h"
#include "FOTA_TP.h"
#include "FOTAEvent.h"
#include "AppLog.h"
#include "ConnectionHandling.h"

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/* ================= FOTA SERVICE UUID ================= */
/**
 * @def           FOTA_SERVICE_UUID_FIRST_PART_32BIT
 * @brief         First part of FOTA Service UUID (32-bits).
 */
#define FOTA_SERVICE_UUID_FIRST_PART_32BIT   (0x00000001)

/**
 * @def           BT_UUID_FOTA_SERVICE_VAL
 * @brief         128-bit value encoding for FOTA service UUID.
 */
#define BT_UUID_FOTA_SERVICE_VAL            	BT_UUID_128_ENCODE( \
                                                FOTA_SERVICE_UUID_FIRST_PART_32BIT, \
                                                BASE_UUID_SECOND_PART_16BIT, \
                                                BASE_UUID_THIRD_PART_16BIT, \
                                                BASE_UUID_FOURTH_PART_16BIT, \
                                                BASE_UUID_FIFTH_PART_48BIT)

/**
 * @def           BT_UUID_FOTA_SERVICE
 * @brief         Creating UUID structure pointer (const struct bt_uuid *) for
 * 					FOTA service.
 */
#define BT_UUID_FOTA_SERVICE                	BT_UUID_DECLARE_128(BT_UUID_FOTA_SERVICE_VAL)

/* ================= CHARACTERISTIC UUIDs ============== */
/**
 * @def           FOTA_CONTROL_CHAR_UUID_FIRST_PART_32BIT
 * @brief         First part of FOTA Control UUID (32-bits).
 */
#define FOTA_CONTROL_CHAR_UUID_FIRST_PART_32BIT \
                                             (0x00000002)

/**
 * @def           BT_UUID_FOTA_CONTROL_CHAR_VAL
 * @brief         128-bit value encoding for FOTA Control characteristic UUID.
 */
#define BT_UUID_FOTA_CONTROL_CHAR_VAL        BT_UUID_128_ENCODE( \
                                                FOTA_CONTROL_CHAR_UUID_FIRST_PART_32BIT, \
                                                BASE_UUID_SECOND_PART_16BIT, \
                                                BASE_UUID_THIRD_PART_16BIT, \
                                                BASE_UUID_FOURTH_PART_16BIT, \
                                                BASE_UUID_FIFTH_PART_48BIT)

/**
 * @def           BT_UUID_FOTA_CONTROL
 * @brief         Creating UUID structure pointer (const struct bt_uuid *) for
 * 					FOTA Control characteristic.
 */
#define BT_UUID_FOTA_CONTROL_CHAR            BT_UUID_DECLARE_128(BT_UUID_FOTA_CONTROL_CHAR_VAL)

/**
 * @def           FOTA_DATA_TRANSFER_UUID_FIRST_PART_32BIT
 * @brief         First part of FOTA Data Transfer UUID (32-bits).
 */
#define FOTA_DATA_TRANSFER_UUID_FIRST_PART_32BIT \
                                             (0x00000003)

/**
 * @def           BT_UUID_FOTA_DATA_TRANSFER_CHAR_VAL
 * @brief         128-bit value encoding for FOTA Data Transfer characteristic UUID.
 */
#define BT_UUID_FOTA_DATA_TRANSFER_CHAR_VAL  BT_UUID_128_ENCODE( \
                                                FOTA_DATA_TRANSFER_UUID_FIRST_PART_32BIT, \
                                                BASE_UUID_SECOND_PART_16BIT, \
                                                BASE_UUID_THIRD_PART_16BIT, \
                                                BASE_UUID_FOURTH_PART_16BIT, \
                                                BASE_UUID_FIFTH_PART_48BIT)

/**
 * @def           BT_UUID_FOTA_DATA_TRANSFER_CHAR
 * @brief         Creating UUID structure pointer (const struct bt_uuid *) for
 * 					FOTA Data Transfer characteristic.
 */
#define BT_UUID_FOTA_DATA_TRANSFER_CHAR      BT_UUID_DECLARE_128(BT_UUID_FOTA_DATA_TRANSFER_CHAR_VAL)

/**
 * @def           FOTA_STATUS_UUID_FIRST_PART_32BIT
 * @brief         First part of FOTA Status UUID (32-bits).
 */
#define FOTA_STATUS_UUID_FIRST_PART_32BIT    (0x00000004)

/**
 * @def           BT_UUID_FOTA_STATUS_CHAR_VAL
 * @brief         128-bit value encoding for FOTA Status characteristic UUID.
 */
#define BT_UUID_FOTA_STATUS_CHAR_VAL         BT_UUID_128_ENCODE( \
                                                FOTA_STATUS_UUID_FIRST_PART_32BIT, \
                                                BASE_UUID_SECOND_PART_16BIT, \
                                                BASE_UUID_THIRD_PART_16BIT, \
                                                BASE_UUID_FOURTH_PART_16BIT, \
                                                BASE_UUID_FIFTH_PART_48BIT)

/**
 * @def           BT_UUID_FOTA_STATUS_CHAR
 * @brief         Creating UUID structure pointer (const struct bt_uuid *) for
 * 					FOTA status characteristic.
 */
#define BT_UUID_FOTA_STATUS_CHAR             BT_UUID_DECLARE_128(BT_UUID_FOTA_STATUS_CHAR_VAL)

/**
 * @def           FOTA_PROGRESS_UUID_FIRST_PART_32BIT
 * @brief         First part of FOTA Progress UUID (32-bits).
 */
#define FOTA_PROGRESS_UUID_FIRST_PART_32BIT  (0x00000005)

/**
 * @def           BT_UUID_FOTA_PROGRESS_CHAR_VAL
 * @brief         128-bit value encoding for FOTA progress characteristic UUID.
 */
#define BT_UUID_FOTA_PROGRESS_CHAR_VAL       BT_UUID_128_ENCODE( \
                                                FOTA_PROGRESS_UUID_FIRST_PART_32BIT, \
                                                BASE_UUID_SECOND_PART_16BIT, \
                                                BASE_UUID_THIRD_PART_16BIT, \
                                                BASE_UUID_FOURTH_PART_16BIT, \
                                                BASE_UUID_FIFTH_PART_48BIT)

/**
 * @def           BT_UUID_FOTA_PROGRESS_CHAR
 * @brief         Creating UUID structure pointer (const struct bt_uuid *) for
 * 					FOTA progress characteristic.
 */
#define BT_UUID_FOTA_PROGRESS_CHAR           BT_UUID_DECLARE_128(BT_UUID_FOTA_PROGRESS_CHAR_VAL)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          <Enum name>
 * @brief         <Enum details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        <Structure name>
 * @brief         <Structure details>.
 */

/******************************************************************************/
/*                                                                            */
/*                                   UNIONS                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @union         <Union name>
 * @brief         <Union details>.
 */

/******************************************************************************/
/*                                                                            */
/*                              EXTERN VARIABLES                              */
/*                                                                            */
/******************************************************************************/
extern struct k_mem_slab gst_FOTASlab;

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/

#endif //!_FOTA_GATT_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
