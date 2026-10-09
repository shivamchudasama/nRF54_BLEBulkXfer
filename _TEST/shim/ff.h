/* SPDX-License-Identifier: MIT */
/* Host-test stand-in for ELM FatFs's ff.h: only the FATFS work area type the
   mount description points to. */
#ifndef _SHIM_FF_H
#define _SHIM_FF_H
typedef struct
{
   int i_unused;
} FATFS;
#endif //!_SHIM_FF_H
