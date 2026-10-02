/**
 * @file          AppLog.h
 * @brief         Header file containing application log module related definitions
 *                and declarations.
 * @date          16/02/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _APP_LOG_H
#define _APP_LOG_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include <zephyr/logging/log.h>

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           APP_LOG
 * @brief         Declare - already registered - the application log module.
 *                Module name is APP_LOG. Log level is set to INFO.
 */
LOG_MODULE_DECLARE(APP_LOG, LOG_LEVEL_INF);

/**
 * @def           APP_LOG_ERR
 * @brief         Declaring application level error logging macro that prepend the
 *                calling function name to the message.
 */
#define APP_LOG_ERR(fmt, ...)                LOG_ERR("%s: " fmt, __func__, ##__VA_ARGS__)

/**
 * @def           APP_LOG_WRN
 * @brief         Declaring application level warning logging macro that prepend the
 *                calling function name to the message.
 */
#define APP_LOG_WRN(fmt, ...)                LOG_WRN("%s: " fmt, __func__, ##__VA_ARGS__)

/**
 * @def           APP_LOG_INF
 * @brief         Declaring application level information logging macro that prepend the
 *                calling function name to the message.
 */
#define APP_LOG_INF(fmt, ...)                LOG_INF("%s: " fmt, __func__, ##__VA_ARGS__)

/**
 * @def           APP_LOG_DBG
 * @brief         Declaring application level debug logging macro that prepend the
 *                calling function name to the message.
 */
#define APP_LOG_DBG(fmt, ...)                LOG_INF("%s: " fmt, __func__, ##__VA_ARGS__)

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

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/

#endif //!_APP_LOG_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
