/**
 * @file          TransferMsgTypes.h
 * @brief         Header file containing details and data types of structures
 *                associated with file and data transfer
 * @date          18/02/26
 * @author        Yash Sunil Giramkar [YSG]
 * @copyright     Copyright(c) Yash Sunil Giramkar (YSG) as an unpublished work.
 */

#ifndef TRANSFER_MSG_H
#define TRANSFER_MSG_H

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/

#include <zephyr/kernel.h>

/******************************************************************************/
/*                                                                            */
/*                                  DEFINES                                   */
/*                                                                            */
/******************************************************************************/
/**
 * @def           FS_MAX_CHUNK_SIZE
 * @brief         Maximum chunk of data that can be received in one transfer
 *                between BT FSM and File Manager FSM
 */
#define FS_MAX_CHUNK_SIZE                    242

/**
 * @def MAX_FILENAME_LEN
 * @brief
 */
#define MAX_FILENAME_LEN                     64

/**
 * @def MAX_PATH_LEN
 * @brief
 */
#define MAX_PATH_LEN                         3




/******************************************************************************/
/*                                                                            */
/*                                   ENUMS                                    */
/*                                                                            */
/******************************************************************************/
/**
 * @enum          FileSysManagerEvents_E
 * @brief         Enum to hold status of each event of file system manager.
 */
typedef enum {
   eFSC_OPEN_DIR,
   eFSC_MAKE_DIR,
   eFSC_OPEN_FILE_READ,
   eFSC_OPEN_FILE_WRITE,
   eFSC_WRITE_DATA,
   eFSC_READ_FILE,
   eFSC_DEBUG_LIST_DRIVE,
   eFSC_DELETE_FILE,
   eFSC_DELETE_DIR,
   eFSC_CLOSE_FILE,
   eFSC_ABORT
} FileSysCommand_E;

/******************************************************************************/
/*                                                                            */
/*                                 STRUCTURES                                 */
/*                                                                            */
/******************************************************************************/
/**
 * @struct        FileSysMessage_T
 * @brief         Structure to hold details of message queue object passed
 *                between BT FSM and File System Manager FSM.
 */
typedef struct {
   FileSysCommand_E e_command;
   /* Used for Data message as well as File and Directory creation*/
   uint32_t u32_sizeOfData;
   // Data field can either contain Directory name, File name or data to write
   uint8_t u8_data[FS_MAX_CHUNK_SIZE];
} FileSysMessage_T;


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
extern uint8_t gu8_consumedBuff;

/******************************************************************************/
/*                                                                            */
/*                              EXTERN FUNCTIONS                              */
/*                                                                            */
/******************************************************************************/

#endif //!TRANSFER_MSG_H

/**
 * Copyright(c) Yash Sunil Giramkar (YSG) as an unpublished work.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION IS ALLOWED ONLY IN ACCORDANCE WITH
 * THE TERMS OF THE LICENSE
 *
 * @author:Yash Sunil Giramkar [YSG]
 */













