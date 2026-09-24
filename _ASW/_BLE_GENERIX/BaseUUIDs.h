/**
 * @file          BaseUUIDs.h
 * @brief         Header file containing base UUIDs for the current project.
 * @date          19/02/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _BASE_UUIDS_H
#define _BASE_UUIDS_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
// Note: To keep the UUIDs of all the services and characteristics v4 compliant
// (RFC 4122 compliant), we've decided the base UUIDs of last 96-bits. Only the
// first 32-bits would be changed throughout the project.
/**
 * @def           UUID_FIRST_PART_32BIT
 * @brief         First part of UUID (32-bits) formulation.
 *                | 8-bit Domain | 8-bit Service ID | 16-bit Characteristic ID |
 */
#define UUID_FIRST_PART_32BIT(domain, svc, char) \
                                             (((uint32_t)(domain) << 24) | \
                                             ((uint32_t)(svc) << 16) | \
                                             ((uint32_t)(char)))
                                             
/**
 * @def           BASE_UUID_SECOND_PART_16BIT
 * @brief         Second part of base UUID (16-bits).
 */
#define BASE_UUID_SECOND_PART_16BIT          (0X16A1)

/**
 * @def           BASE_UUID_THIRD_PART_16BIT
 * @brief         Third part of base UUID (16-bits).
 */
#define BASE_UUID_THIRD_PART_16BIT           (0X4812)

/**
 * @def           BASE_UUID_FOURTH_PART_16BIT
 * @brief         Fourth part of base UUID (16-bits).
 */
#define BASE_UUID_FOURTH_PART_16BIT          (0XAF35)

/**
 * @def           BASE_UUID_FIFTH_PART_48BIT
 * @brief         Fifth part of base UUID (48-bits).
 */
#define BASE_UUID_FIFTH_PART_48BIT           (0XF3F29A92F6CA)

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
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/

#endif //!_BASE_UUIDS_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
