/* SPDX-License-Identifier: MIT */
/* Host-test stand-in for zephyr/settings/settings.h: declared only, with
   Zephyr's names and contracts; a test that reaches a function defines it
   (CSR: settings_subsys_init(); _PAIR: save, delete, and the static handler,
   which a test calls as settings_load() would). */
#ifndef _SHIM_SETTINGS_H
#define _SHIM_SETTINGS_H
#include "zephyr_shim.h"
typedef ssize_t (*settings_read_cb)(void *cb_arg, void *data, size_t len);
struct settings_handler_static
{
   const char *name;
   int (*h_get)(const char *key, char *val, int val_len_max);
   int (*h_set)(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg);
   int (*h_commit)(void);
   int (*h_export)(int (*export_func)(const char *name, const void *val, size_t val_len));
};
#define SETTINGS_STATIC_HANDLER_DEFINE(_hname, _tree, _get, _set, _commit, _export) \
   const struct settings_handler_static settings_handler_##_hname = \
      { (_tree), (_get), (_set), (_commit), (_export) }
extern int settings_subsys_init(void);
extern int settings_save_one(const char *name, const void *value, size_t val_len);
extern int settings_delete(const char *name);
#endif //!_SHIM_SETTINGS_H
