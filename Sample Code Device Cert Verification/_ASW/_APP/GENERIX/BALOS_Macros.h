/**
 * @file          BALOS_Macros.h
 * @brief         Header file containing macros for BALOS
 * @date          10/11/2016
 * @author        Aniket Bhattacharya [ABB], Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#ifndef _BALOS_MACROS_H
#define _BALOS_MACROS_H

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
 * @def           <Define name>
 * @brief         <Define details>.
 */
#define US_PER_MS                            (1000UL)
#define REF_MILLIVOLTS                       (5000)


/*! Set desired bit in byte/ word/ double word
 *  Argument a: byte/ word/ double word in which bit is to be set
 *  Argument b: Bit index to be set (starting from LSB = 0)
 *  It will only set the desired bit and keep all the other bits intect.
 */

#define SetBit32(a, b)                       (a) |= (((uint32_t)(1)) << (b))
#define SetBit16(a, b)                       (a) |= (((uint16_t)(1)) << (b))
#define SetBit(a, b)                         (a) |= (((uint8_t)(1)) << (b))

/*! Set desired bit in byte/ word/ double word
 *  Argument a: byte/ word/ double word in which bit is to be set
 *  Argument b: Bit index to be set (starting from LSB = 0)
 *  It will forcefully set the desired bit and erase all the other bits.
 */
#define ForcedSetBit32(a, b)                 (a) = (((uint32_t)(1)) << (b))
#define ForcedSetBit16(a, b)                 (a) = (((uint16_t)(1)) << (b))
#define ForcedSetBit(a, b)                   (a) = (((uint8_t)(1)) << (b))

/*! Clear desired bit in byte/ word/ double word
 *  Argument a: byte/ word/ double word in which bit is to be cleared
 *  Argument b: Bit index to be cleared (starting from LSB = 0)
 */
#define ClrBit32(a, b)                       (a) &= (~(((uint32_t)(1)) << (b)))
#define ClrBit16(a, b)                       (a) &= (~(((uint16_t)(1)) << (b)))
#define ClrBit(a, b)                         (a) &= (~(((uint8_t)(1)) << (b)))

/*! Get the desired bit in byte/ word/ double word
 *  Argument a: byte/ word/ double word from which which bit is to be gotten
 *  Argument b: Bit index to be read (starting from LSB = 0)
 */
#define ChkBit32(a, b)                       (((a) & (((uint32_t)(1)) << (b))) ? 1 : 0)
#define ChkBit16(a, b)                       (((a) & (((uint16_t)(1)) << (b))) ? 1 : 0)
#define ChkBit(a, b)                         (((a) & (((uint8_t)(1)) << (b))) ? 1 : 0)

/*! Invert the desired bit in byte/ word/ double word
 *  Argument a: byte/ word/ double word in which which bit is to be inverted
 *  Argument b: Bit index to be inverted (starting from LSB = 0)
 */
#define InvBit32(a, b)                       SHIFT_BITS_32(b);\
                                             (a) ^= (DWordAlias.u32_doubleWordElems)
#define InvBit16(a, b)                       (a) ^= (((uint16_t)(1)) << (b))
#define InvBit(a, b)                         (a) ^= (((uint8_t)(1)) << (b))

/*! Write the desired value at the desired bit in byte/ word/ double word
 *  Argument a: byte/ word/ double word to which a desired value is to be written
 *  Argument b: Bit index to be read (starting from LSB = 0)
 *  Argument c: Value to be written at the desired bit
 */
#define WriteBit32(a, b, c)                  (((a) &= ~(((uint32_t)(1)) << (b))) | (((uint32_t)(c)) << b))
#define WriteBit16(a, b, c)                  (((a) &= ~(((uint16_t)(1)) << (b))) | (((uint16_t)(c)) << b))
#define WriteBit(a, b, c)                    (((a) &= ~(((uint8_t)(1)) << (b))) | (((uint8_t)(c)) << b))

/*! Write the desired value at the desired bits (of custom width) in byte/ word/ double word
 *  Argument a: byte/ word/ double word to which a desired value is to be written
 *  Argument b: Bit index to be read (starting from LSB = 0)
 *  Argument c: Number of bits to be written
 *  Argument d: Value to be written at the desired bit
 */
#define WriteBits32(a, b, c, d)              (((a) &= ~((((uint32_t)1 << (c)) - 1) << (b))) | (((uint32_t)(d) & ((1U << (c)) - 1)) << (b)))
#define WriteBits16(a, b, c, d)              (((a) &= ~((((uint16_t)1 << (c)) - 1) << (b))) | (((uint16_t)(d) & ((1U << (c)) - 1)) << (b)))
#define WriteBits(a, b, c, d)                (((a) &= ~((((uint8_t)1 << (c)) - 1) << (b))) | (((uint8_t)(d) & ((1U << (c)) - 1)) << (b)))

/*! Read the desired value at the desired bits (of custom width) in byte/ word/ double word
 *  Argument a: byte/ word/ double word to which a desired value is to be read
 *  Argument b: Bit index to be read (starting from LSB = 0)
 *  Argument c: Number of bits to be read
 */
#define ReadBits32(a, b, c)                  ((uint32_t)(((a) >> (b)) & ((1U << (c)) - 1)))
#define ReadBits16(a, b, c)                  ((uint16_t)(((a) >> (b)) & ((1U << (c)) - 1)))
#define ReadBits(a, b, c)                    ((uint8_t)(((a) >> (b)) & ((1U << (c)) - 1)))

/*! The maximum value that can be represented with given number of bits
 *  Argument a: number of bits
 */
#define MaxNum(a)                            ((1<<(a))-1)

/*! The absolute value of difference between 2 passed values
 *  Argument a: value 1
 *  Argument b: value 2
 */
#define DELTA(a, b)                          (((a)>(b))?((a)-(b)):((b)-(a)))

/*! Limit value of the passed argument within passed limits
 *  Argument a: Upper Limit
 *  Argument b: Lower limit
 *  Argument c: Value to be limited
 */
#define LIMIT_VAL(a, b, c)                   c=((c)>(a)?(a):((c)<(b)?(b):(c)))

/*! Decrement given quantity by given amount. Do no decrement if the result
 *  of the decrement will be negative.
 *  Argument a: Quantity to decrement
 *  Argument b: Amount by which to decrement
 */
#define DEC_TILL_ZERO(a, b)                  (((a)>=(b))?((a)-=(b)):((a)=0))

/*! Increment given quantity by given amount. Roll over if the result exceeds
 *  a given limit. If the incremental amount is 1, then this is like a modulo
 *  limit counter.
 *  Argument a: Quantity to increment
 *  Argument b: Amount by which to increment
 *  Argument b: given limit beyond which incremented amount will roll over
 *  @see COUNT_UP_MODULO
 */
#define INC_N_RESET(a, b, c)                 (a)+=(b); (a)%=(c)

/*! Absolute value of difference between passed parameters
 *  Argument a: Passed parameter 1
 *  Argument b: Passed parameter 2
 */
#define DIFF(a, b)                           ((a)>=(b)?((a)-(b)):((b)-(a)))

#define CLR(a)                               ((a)=0)

/*! Increment given quantity by 1. Reset to 0 if the result exceeds a
 *  given limit. Effectively  modulo limit counter.
 *  Argument a: Quantity to increment
 *  Argument b: Limit
 *  @see INC_N_RESET
 */
#define COUNT_UP_MODULO(a, b)                (a)+=1;\
                                             (a)%=(b)

/*! Minimum value of passed parameters
 *  Argument a: Passed parameter 1
 *  Argument b: Passed parameter 2
 */
#define MIN(a,b)                             ((a)<(b)?(a):(b))
/*! Maximum value of passed parameters
 *  Argument a: Passed parameter 1
 *  Argument b: Passed parameter 2
 */
#define MAX(a,b)                             ((a)<(b)?(b):(a))

/*! Absolute value of passed parameter
 *  Argument a: Passed parameter
 */
#define ABS_VAL(a)                           (((a)<0)?(-(a)):((a)))

/*! Used to round up given value to desired precision
 *  eg: Round 56 to nearest 10 will give 60 while round
 *  52 to nearest 10 will give 50.
 *  Argument a: Amount to be rounded (52)
 *  Argument b: Precision - rounded to (10)
 */
#define ROUND_TO_NEAREST_X(a, X)             (((a + ((X)>>1)) / (X)) * (X))

/*! Used to divide fixed point numbers and round the
 *  result to the nearest integer
 *  Argument a: Dividend (can be positive or negative)
 *  Argument b: Divisor (always positive)
 */
#define ROUNDED_DIVIDE(a, b)                 ((a)>0)?(((a)+((b)>>1))/(b)):(((a)-((b)>>1))/(b))

/*! Used to give array length in bytes.
 *  Argument a: Array name whose length in bytes is desired
 */
#define ARRAY_LENGTH(x)                      (sizeof(x)/sizeof(x[0]))

/*! Used to give size in bits.
 *  Argument a: Entity whose size in bits is desired
 */
#define SIZEINBITS(a)                        ((sizeof(a))<< 3)

/*! Used to derive millivoltage from ADC count.
 *  Argument a: ADC count
 */
#define GET_MILLIVOLTS_FRM_COUNT(a)          {\
                                                uint32_t temp;\
                                                temp = a;\
                                                temp *= REF_MILLIVOLTS;\
                                                temp += 2048;\
                                                temp /= 4095;\
                                                a = (uint16_t)temp;\
                                             }
/*! The maximum value that can be represented with given number of bits
 *  Argument a: number of bits
 */
#define MAXNUM(a)                            ((1<<(a))-1)

/*! This macro is used to increment upto a ceiling
 *  Argument x: value to be incremented
 *  Argument y: Ceiling
 */
#define INC_TILL_LIMIT(x, y)                 (x)++;\
                                             if ((x)>(y)) (x) = (y)

/**
 * @def           BIG_END_2_LITTLE_END
 * @brief         It will conver the given 32-bit big endiened variable to little
 *                endian format.
 */
#define BIG_END_2_LITTLE_END(word)           (((word & 0x000000FF) << 24 ) | \
                                             ((word & 0x0000FF00) << 8 ) | \
                                             ((word & 0x00FF0000) >> 8) | \
                                             ((word & 0xFF000000) >> 24))

/*! Following definitions added to improve time required to handle incoming and
 *  outgoing messages by using pre-calculated values.
 */
#define MASK1BIT                             (0x00000001)
#define MASK2BIT                             (0x00000003)
#define MASK3BIT                             (0x00000007)
#define MASK4BIT                             (0x0000000F)
#define MASK5BIT                             (0x0000001F)
#define MASK6BIT                             (0x0000003F)
#define MASK7BIT                             (0x0000007F)
#define MASK8BIT                             (0x000000FF)
#define MASK9BIT                             (0x000001FF)
#define MASK10BIT                            (0x000003FF)
#define MASK11BIT                            (0x000007FF)
#define MASK12BIT                            (0x00000FFF)
#define MASK13BIT                            (0x00001FFF)
#define MASK14BIT                            (0x00003FFF)
#define MASK15BIT                            (0x00007FFF)
#define MASK16BIT                            (0x0000FFFF)
#define MASK17BIT                            (0x0001FFFF)
#define MASK18BIT                            (0x0003FFFF)
#define MASK19BIT                            (0x0007FFFF)
#define MASK20BIT                            (0x000FFFFF)
#define MASK21BIT                            (0x001FFFFF)
#define MASK22BIT                            (0x003FFFFF)
#define MASK23BIT                            (0x007FFFFF)
#define MASK24BIT                            (0x00FFFFFF)
#define MASK25BIT                            (0x01FFFFFF)
#define MASK26BIT                            (0x03FFFFFF)
#define MASK27BIT                            (0x07FFFFFF)
#define MASK28BIT                            (0x0FFFFFFF)
#define MASK29BIT                            (0x1FFFFFFF)
#define MASK30BIT                            (0x3FFFFFFF)
#define MASK31BIT                            (0x7FFFFFFF)
#define MASK32BIT                            (0xFFFFFFFF)

#define BITSTOSHIFT1                         (31)
#define BITSTOSHIFT2                         (30)
#define BITSTOSHIFT3                         (29)
#define BITSTOSHIFT4                         (28)
#define BITSTOSHIFT5                         (27)
#define BITSTOSHIFT6                         (26)
#define BITSTOSHIFT7                         (25)
#define BITSTOSHIFT8                         (24)
#define BITSTOSHIFT9                         (23)
#define BITSTOSHIFT10                        (22)
#define BITSTOSHIFT11                        (21)
#define BITSTOSHIFT12                        (20)
#define BITSTOSHIFT13                        (19)
#define BITSTOSHIFT14                        (18)
#define BITSTOSHIFT15                        (17)
#define BITSTOSHIFT16                        (16)
#define BITSTOSHIFT17                        (15)
#define BITSTOSHIFT18                        (14)
#define BITSTOSHIFT19                        (13)
#define BITSTOSHIFT20                        (12)
#define BITSTOSHIFT21                        (11)
#define BITSTOSHIFT22                        (10)
#define BITSTOSHIFT23                        (9)
#define BITSTOSHIFT24                        (8)
#define BITSTOSHIFT25                        (7)
#define BITSTOSHIFT26                        (6)
#define BITSTOSHIFT27                        (5)
#define BITSTOSHIFT28                        (4)
#define BITSTOSHIFT29                        (3)
#define BITSTOSHIFT30                        (2)
#define BITSTOSHIFT31                        (1)
#define BITSTOSHIFT32                        (0)


#define TEN_TO_PWR_OF(a)                     ((a)==0?1:\
                                                ((a)==1?(uint32_t)10:\
                                                ((a)==2?(uint32_t)100:\
                                                ((a)==3?(uint32_t)1000:\
                                                ((a)==4?(uint32_t)10000:\
                                                ((a)==5?(uint32_t)100000:\
                                                ((a)==6?(uint32_t)1000000:\
                                                ((a)==7?(uint32_t)10000000:\
                                                ((a)==8?(uint32_t)100000000:\
                                                0)))))))))

#define LOG_BASE2(a)                         ((a)==1?0:\
                                             ((a)==2?(uint8_t)1:\
                                             ((a)==4?(uint8_t)2:\
                                             ((a)==8?(uint8_t)3:\
                                             ((a)==16?(uint8_t)4:\
                                             ((a)==32?(uint8_t)5:\
                                             ((a)==64?(uint8_t)6:\
                                             ((a)==128?(uint8_t)7:\
                                             ((a)==256?(uint8_t)8:\
                                             0xFF)))))))))

#define Q(x)                                 #x
#define QUOTE(x)                             Q(x)

#define ROTATE_LFSR_16(a,b)                  if (a)\
                                             {\
                                                uint16_t wLSB = (a) & 0x001;\
                                                (a) >>= 1;\
                                                (a) ^= ((-1)*wLSB) & 0xB400;\
                                             }else\
                                                (a) = (b)

#define ROTATE_LFSR_32(a,b)                  if (a)\
                                             {\
                                                uint32_t dwLSB = (a) & 0x0000001UL;\
                                                (a) >>= 1;\
                                                (a) ^= (uint32_t)(((-1)*dwLSB) & 0xA3000000UL);\
                                             }else\
                                                (a) = (b)

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

#endif //!_BALOS_MACROS_H

/**
 * Copyright(c) Bajaj Auto Technology Limited (BATL) as an unpublished work.
 * THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF BATL.
 * ALL USE, DISCLOSURE, AND/OR REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
 * BATL IS PROHIBITED.
 *
 * @author:Aniket Bhattacharya [ABB], Shivam Chudasama [SC]
 */
