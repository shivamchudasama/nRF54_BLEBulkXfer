/* SPDX-License-Identifier: MIT */
/* Host-test stand-in for zephyr/settings/settings.h: declared only; a test
   that builds with CONFIG_TRUSTED_STORAGE_STORAGE_BACKEND_SETTINGS defines it. */
#ifndef _SHIM_SETTINGS_H
#define _SHIM_SETTINGS_H
#include "zephyr_shim.h"
extern int settings_subsys_init(void);
#endif //!_SHIM_SETTINGS_H
