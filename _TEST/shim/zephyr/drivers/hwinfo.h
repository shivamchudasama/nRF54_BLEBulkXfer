/* SPDX-License-Identifier: MIT */
/* Host-test stand-in for zephyr/drivers/hwinfo.h: declared only; a test that
   reaches it defines it. */
#ifndef _SHIM_HWINFO_H
#define _SHIM_HWINFO_H
#include "zephyr_shim.h"
extern ssize_t hwinfo_get_device_id(uint8_t *buffer, size_t length);
#endif //!_SHIM_HWINFO_H
