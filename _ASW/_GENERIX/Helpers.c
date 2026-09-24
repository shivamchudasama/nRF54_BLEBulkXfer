/**
 * @file          Helpers.c
 * @brief         Source file containing all the helper functions and macros.
 * @date          20/02/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

/******************************************************************************/
/*                                                                            */
/*                                  INCLUDES                                  */
/*                                                                            */
/******************************************************************************/
#include "Helpers.h"

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
/*                       PRIVATE FUNCTION DECLARATIONS                        */
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

/******************************************************************************/
/*                                                                            */
/*                        PUBLIC FUNCTION DEFINITIONS                         */
/*                                                                            */
/******************************************************************************/
/**
 * @public        gv_ReverseByteOrder
 * @brief         Generic byte order swap (little endian <-> big endian) for byte arrays.
 * @param[out]    u8pt_dst Destination buffer.
 * @param[in]     u8pt_src Source buffer.
 * @param[in]     u32_numBytes Number of bytes to swap.
 * @return        None.
 */
inline void gv_ReverseByteOrder(uint8_t *u8pt_dst, const uint8_t *u8pt_src, uint32_t u32_numBytes)
{
   // Check if neither the source nor the destination is empty
   if ((u8pt_dst != 0) && (u8pt_src != 0))
   {
      // Check if the source and destination are the same
      if (u8pt_dst == u8pt_src)
      {
         // Handle in-place reversal when source and destination are the same buffer
         for (uint32_t u32_lpIdx = 0U; u32_lpIdx < (u32_numBytes / 2U); u32_lpIdx++)
         {
            uint8_t u8_tmp = u8pt_dst[u32_lpIdx];
            u8pt_dst[u32_lpIdx] = u8pt_dst[(u32_numBytes - 1U) - u32_lpIdx];
            u8pt_dst[(u32_numBytes - 1U) - u32_lpIdx] = u8_tmp;
         }
      }
      else
      {
         for (uint32_t u32_lpIdx = 0U; u32_lpIdx < u32_numBytes; u32_lpIdx++)
         {
            u8pt_dst[(u32_numBytes - 1U) - u32_lpIdx] = u8pt_src[u32_lpIdx];
         }
      }
   }
}

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Shivam Chudasama [SC]
 */
