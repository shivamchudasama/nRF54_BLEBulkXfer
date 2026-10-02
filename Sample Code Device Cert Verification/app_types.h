/**
 * @file          app_types.h
 * @brief         Header file containing types for application.
 * @date          10/09/25
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _APP_TYPES_H
#define _APP_TYPES_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <stdint.h>

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           APP_MAJOR_VER
 * @brief         Major version of the application.
 */
#define APP_MAJOR_VER                        (0)

/**
 * @def           APP_MINOR_VER
 * @brief         Minor version of the application.
 */
#define APP_MINOR_VER                        (0)

/**
 * @def           APP_REV_VER
 * @brief         Revision version of the application.
 */
#define APP_REV_VER                          (1)

// /**
//  * @def           MTU_SIZE
//  * @brief         Length of MTU.
//  */
// #define MTU_SIZE                             (CHAIN_LINK_DATA_LEN)

/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          AppState_E
 * @brief         Enums of different application states.
 */
typedef enum
{
   eAS_UNDEFINED = 0,                        /**< Undefined state. */
   eAS_CSR_AVAILABLE,                        /**< CSR generated. */
   eAS_DEVICE_CERT_VERIFIED,                 /**< Device certificate verified. */
   eAS_DEVICE_PAIRED,                        /**< Device paired. */
} AppState_E;

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

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/

#endif //!_APP_TYPES_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
