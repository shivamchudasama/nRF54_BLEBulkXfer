/**
 * @file          fs_sim.c
 * @brief         In-memory stand-in for Zephyr's file system API
 *                (shim/zephyr/fs/fs.h) on one FAT-like volume, for host tests of
 *                the File System Manager and its users.
 *
 *                Behaves as Zephyr's FAT backend does where the File System
 *                Manager can tell: names compare without case; an unformatted
 *                volume does not mount until fs_mkfs(); FS_O_CREATE does not
 *                truncate; a full volume makes fs_write() return fewer bytes;
 *                fs_unlink() of a non-empty directory fails with -EACCES;
 *                fs_rename() replaces an existing target; fs_readdir() ends
 *                with an empty name. Any call can be made to fail once
 *                (gv_SimFsFailOnce). Handles come from a fixed pool that
 *                gv_SimFsReset() clears, so a test may end with a file still
 *                open without leaking memory.
 *
 * @date          07/10/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#include <ctype.h>
#include "zephyr/fs/fs.h"

#define SIM_FS_MAX_NODES      (128U)
#define SIM_FS_PATH_LEN       (300U)
#define SIM_FS_MAX_HANDLES    (16U)

typedef struct
{
   bool b_used;
   bool b_dir;
   char car_path[SIM_FS_PATH_LEN];
   uint8_t *u8pt_data;
   uint32_t u32_size;
} SimFsNode_T;

typedef struct
{
   bool b_used;
   int i_node;
   uint32_t u32_pos;                        /* file: position; dir: next node index */
   fs_mode_t t_flags;
} SimFsHandle_T;

static SimFsNode_T sstar_nodes[SIM_FS_MAX_NODES];
static SimFsHandle_T sstar_handles[SIM_FS_MAX_HANDLES];
static bool sb_formatted;
static bool sb_mounted;
static char scar_mount[SIM_FS_PATH_LEN];
static uint32_t su32_free;
static uint32_t su32_openHandles;
static uint32_t su32ar_calls[eSFS_COUNT];
static int32_t si32ar_failAfter[eSFS_COUNT];
static int siar_failErr[eSFS_COUNT];

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/
static bool sb_SameName(const char *a, const char *b)
{
   while ((*a != '\0') && (*b != '\0'))
   {
      if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) { return false; }
      a++;
      b++;
   }
   return *a == *b;
}

/** Count a call of op; returns the injected error, or 0. */
static int si_Enter(SimFsOp_E e_op)
{
   su32ar_calls[e_op]++;
   if (si32ar_failAfter[e_op] >= 0)
   {
      if (si32ar_failAfter[e_op] == 0)
      {
         si32ar_failAfter[e_op] = -1;
         return siar_failErr[e_op];
      }
      si32ar_failAfter[e_op]--;
   }
   return 0;
}

static bool sb_IsRoot(const char *cpt_path)
{
   return sb_mounted && sb_SameName(cpt_path, scar_mount);
}

/** Whether cpt_path is on the mounted volume (the root or below it). */
static bool sb_OnVolume(const char *cpt_path)
{
   size_t s_len = strlen(scar_mount);
   char car_head[SIM_FS_PATH_LEN];

   if (!sb_mounted || (strlen(cpt_path) < s_len) || (strlen(cpt_path) >= SIM_FS_PATH_LEN))
   {
      return false;
   }
   (void)memcpy(car_head, cpt_path, s_len);
   car_head[s_len] = '\0';
   return sb_SameName(car_head, scar_mount) && ((cpt_path[s_len] == '\0') || (cpt_path[s_len] == '/'));
}

static int si_Find(const char *cpt_path)
{
   uint32_t i;

   for (i = 0U; i < SIM_FS_MAX_NODES; i++)
   {
      if (sstar_nodes[i].b_used && sb_SameName(sstar_nodes[i].car_path, cpt_path)) { return (int)i; }
   }
   return -1;
}

/** Parent directory of cpt_path into car_parent; false for the root. */
static bool sb_Parent(const char *cpt_path, char *car_parent)
{
   const char *cpt_slash = strrchr(cpt_path, '/');

   if ((cpt_slash == NULL) || (cpt_slash == cpt_path)) { return false; }
   (void)memcpy(car_parent, cpt_path, (size_t)(cpt_slash - cpt_path));
   car_parent[cpt_slash - cpt_path] = '\0';
   return true;
}

/** Whether a directory exists: the root or a directory node. */
static bool sb_DirExists(const char *cpt_path)
{
   int i_node;

   if (sb_IsRoot(cpt_path)) { return true; }
   i_node = si_Find(cpt_path);
   return (i_node >= 0) && sstar_nodes[i_node].b_dir;
}

/** Whether cpt_child is directly inside cpt_dir. */
static bool sb_IsChildOf(const char *cpt_child, const char *cpt_dir)
{
   char car_parent[SIM_FS_PATH_LEN];

   return sb_Parent(cpt_child, car_parent) && sb_SameName(car_parent, cpt_dir);
}

static int si_NewNode(const char *cpt_path, bool b_dir)
{
   char car_parent[SIM_FS_PATH_LEN];
   uint32_t i;

   if (!sb_Parent(cpt_path, car_parent) || !sb_DirExists(car_parent)) { return -ENOENT; }
   for (i = 0U; i < SIM_FS_MAX_NODES; i++)
   {
      if (!sstar_nodes[i].b_used)
      {
         (void)memset(&sstar_nodes[i], 0, sizeof(sstar_nodes[i]));
         sstar_nodes[i].b_used = true;
         sstar_nodes[i].b_dir = b_dir;
         (void)snprintf(sstar_nodes[i].car_path, SIM_FS_PATH_LEN, "%s", cpt_path);
         return (int)i;
      }
   }
   return -ENOSPC;
}

static void sv_FreeNode(int i_node)
{
   su32_free += (su32_free == UINT32_MAX) ? 0U : sstar_nodes[i_node].u32_size;
   free(sstar_nodes[i_node].u8pt_data);
   (void)memset(&sstar_nodes[i_node], 0, sizeof(sstar_nodes[i_node]));
}

/** A free handle from the pool, cleared and marked used; NULL when all are open. */
static SimFsHandle_T *sstpt_NewHandle(void)
{
   uint32_t i;

   for (i = 0U; i < SIM_FS_MAX_HANDLES; i++)
   {
      if (!sstar_handles[i].b_used)
      {
         (void)memset(&sstar_handles[i], 0, sizeof(sstar_handles[i]));
         sstar_handles[i].b_used = true;
         su32_openHandles++;
         return &sstar_handles[i];
      }
   }
   return NULL;
}

static void sv_FreeHandle(SimFsHandle_T *stpt_h)
{
   stpt_h->b_used = false;
   su32_openHandles--;
}

static bool sb_HasChildren(const char *cpt_dir)
{
   uint32_t i;

   for (i = 0U; i < SIM_FS_MAX_NODES; i++)
   {
      if (sstar_nodes[i].b_used && sb_IsChildOf(sstar_nodes[i].car_path, cpt_dir)) { return true; }
   }
   return false;
}

/******************************************************************************/
/*  Simulation control                                                        */
/******************************************************************************/
void gv_SimFsReset(void)
{
   uint32_t i;

   for (i = 0U; i < SIM_FS_MAX_NODES; i++)
   {
      if (sstar_nodes[i].b_used) { free(sstar_nodes[i].u8pt_data); }
   }
   (void)memset(sstar_nodes, 0, sizeof(sstar_nodes));
   (void)memset(sstar_handles, 0, sizeof(sstar_handles));
   sb_formatted = false;
   sb_mounted = false;
   scar_mount[0] = '\0';
   su32_free = UINT32_MAX;
   su32_openHandles = 0U;
   for (i = 0U; i < (uint32_t)eSFS_COUNT; i++)
   {
      su32ar_calls[i] = 0U;
      si32ar_failAfter[i] = -1;
      siar_failErr[i] = 0;
   }
}

void gv_SimFsFormat(bool b_mount)
{
   uint32_t u32_free = su32_free;
   uint32_t i;

   for (i = 0U; i < SIM_FS_MAX_NODES; i++)
   {
      if (sstar_nodes[i].b_used) { free(sstar_nodes[i].u8pt_data); }
   }
   (void)memset(sstar_nodes, 0, sizeof(sstar_nodes));
   su32_free = u32_free;
   sb_formatted = true;
   if (b_mount)
   {
      sb_mounted = true;
      (void)snprintf(scar_mount, sizeof(scar_mount), "%s", "/FLASH_DISK:");
   }
}

void gv_SimFsFailOnce(SimFsOp_E e_op, uint32_t u32_after, int i_err)
{
   si32ar_failAfter[e_op] = (int32_t)u32_after;
   siar_failErr[e_op] = i_err;
}

void gv_SimFsSetFree(uint32_t u32_bytes)
{
   su32_free = u32_bytes;
}

uint32_t gu32_SimFsCalls(SimFsOp_E e_op)
{
   return su32ar_calls[e_op];
}

uint32_t gu32_SimFsOpenHandles(void)
{
   return su32_openHandles;
}

bool gb_SimFsExists(const char *cpt_path)
{
   return sb_IsRoot(cpt_path) || (si_Find(cpt_path) >= 0);
}

const uint8_t *gu8pt_SimFsData(const char *cpt_path, uint32_t *u32pt_size)
{
   int i_node = si_Find(cpt_path);

   if ((i_node < 0) || sstar_nodes[i_node].b_dir) { return NULL; }
   *u32pt_size = sstar_nodes[i_node].u32_size;
   return (sstar_nodes[i_node].u8pt_data != NULL) ? sstar_nodes[i_node].u8pt_data : (const uint8_t *)"";
}

int gi_SimFsPut(const char *cpt_path, const void *vpt_data, uint32_t u32_size)
{
   int i_node = si_Find(cpt_path);

   if (i_node < 0) { i_node = si_NewNode(cpt_path, false); }
   if (i_node < 0) { return i_node; }
   free(sstar_nodes[i_node].u8pt_data);
   sstar_nodes[i_node].u8pt_data = malloc((u32_size > 0U) ? u32_size : 1U);
   (void)memcpy(sstar_nodes[i_node].u8pt_data, vpt_data, u32_size);
   sstar_nodes[i_node].u32_size = u32_size;
   return 0;
}

int gi_SimFsPutDir(const char *cpt_path)
{
   int i_node = si_NewNode(cpt_path, true);
   return (i_node < 0) ? i_node : 0;
}

/******************************************************************************/
/*  Zephyr file system API                                                    */
/******************************************************************************/
int fs_mount(struct fs_mount_t *mp)
{
   int i_err = si_Enter(eSFS_MOUNT);

   if (i_err != 0) { return i_err; }
   if (sb_mounted) { return -EBUSY; }
   if (!sb_formatted) { return -ENODEV; }
   sb_mounted = true;
   (void)snprintf(scar_mount, sizeof(scar_mount), "%s", mp->mnt_point);
   return 0;
}

int fs_mkfs(int fs_type, uintptr_t dev_id, void *cfg, int flags)
{
   int i_err = si_Enter(eSFS_MKFS);

   (void)dev_id; (void)cfg; (void)flags;
   if (i_err != 0) { return i_err; }
   if (fs_type != FS_FATFS) { return -EINVAL; }
   gv_SimFsFormat(false);
   return 0;
}

int fs_open(struct fs_file_t *zfp, const char *file_name, fs_mode_t flags)
{
   SimFsHandle_T *stpt_h;
   int i_err = si_Enter(eSFS_OPEN);
   int i_node;

   if (i_err != 0) { return i_err; }
   if (!sb_OnVolume(file_name) || sb_IsRoot(file_name)) { return -ENOENT; }
   i_node = si_Find(file_name);
   if ((i_node >= 0) && sstar_nodes[i_node].b_dir) { return -ENOENT; }
   if (i_node < 0)
   {
      if ((flags & FS_O_CREATE) == 0U) { return -ENOENT; }
      i_node = si_NewNode(file_name, false);
      if (i_node < 0) { return i_node; }
   }
   if ((flags & FS_O_TRUNC) != 0U)
   {
      su32_free += (su32_free == UINT32_MAX) ? 0U : sstar_nodes[i_node].u32_size;
      sstar_nodes[i_node].u32_size = 0U;
   }

   stpt_h = sstpt_NewHandle();
   if (stpt_h == NULL) { return -ENFILE; }
   stpt_h->i_node = i_node;
   stpt_h->u32_pos = ((flags & FS_O_APPEND) != 0U) ? sstar_nodes[i_node].u32_size : 0U;
   stpt_h->t_flags = flags;
   zfp->filep = stpt_h;
   zfp->flags = flags;
   return 0;
}

int fs_close(struct fs_file_t *zfp)
{
   int i_err = si_Enter(eSFS_CLOSE);

   if (zfp->filep != NULL)
   {
      sv_FreeHandle((SimFsHandle_T *)zfp->filep);
      zfp->filep = NULL;
   }
   return i_err;
}

ssize_t fs_read(struct fs_file_t *zfp, void *ptr, size_t size)
{
   SimFsHandle_T *stpt_h = (SimFsHandle_T *)zfp->filep;
   SimFsNode_T *stpt_n;
   int i_err = si_Enter(eSFS_READ);
   size_t s_n;

   if (i_err != 0) { return i_err; }
   if (stpt_h == NULL) { return -EBADF; }
   if ((stpt_h->t_flags & FS_O_READ) == 0U) { return -EACCES; }
   stpt_n = &sstar_nodes[stpt_h->i_node];
   s_n = (stpt_h->u32_pos < stpt_n->u32_size) ? MIN(size, (size_t)(stpt_n->u32_size - stpt_h->u32_pos)) : 0U;
   if (s_n > 0U) { (void)memcpy(ptr, &stpt_n->u8pt_data[stpt_h->u32_pos], s_n); }
   stpt_h->u32_pos += (uint32_t)s_n;
   return (ssize_t)s_n;
}

ssize_t fs_write(struct fs_file_t *zfp, const void *ptr, size_t size)
{
   SimFsHandle_T *stpt_h = (SimFsHandle_T *)zfp->filep;
   SimFsNode_T *stpt_n;
   int i_err = si_Enter(eSFS_WRITE);
   uint32_t u32_end;
   uint32_t u32_grow;
   size_t s_n = size;

   if (i_err != 0) { return i_err; }
   if (stpt_h == NULL) { return -EBADF; }
   if ((stpt_h->t_flags & FS_O_WRITE) == 0U) { return -EACCES; }
   stpt_n = &sstar_nodes[stpt_h->i_node];

   // A full volume takes only what fits (a short write)
   u32_end = stpt_h->u32_pos + (uint32_t)size;
   u32_grow = (u32_end > stpt_n->u32_size) ? (u32_end - stpt_n->u32_size) : 0U;
   if ((su32_free != UINT32_MAX) && (u32_grow > su32_free))
   {
      s_n -= (u32_grow - su32_free);
      u32_end = stpt_h->u32_pos + (uint32_t)s_n;
      u32_grow = su32_free;
   }
   if (u32_end > stpt_n->u32_size)
   {
      stpt_n->u8pt_data = realloc(stpt_n->u8pt_data, u32_end);
      stpt_n->u32_size = u32_end;
   }
   if (s_n > 0U) { (void)memcpy(&stpt_n->u8pt_data[stpt_h->u32_pos], ptr, s_n); }
   stpt_h->u32_pos += (uint32_t)s_n;
   if (su32_free != UINT32_MAX) { su32_free -= u32_grow; }
   return (ssize_t)s_n;
}

int fs_sync(struct fs_file_t *zfp)
{
   int i_err = si_Enter(eSFS_SYNC);

   if (i_err != 0) { return i_err; }
   return (zfp->filep == NULL) ? -EBADF : 0;
}

int fs_mkdir(const char *path)
{
   int i_err = si_Enter(eSFS_MKDIR);
   int i_node;

   if (i_err != 0) { return i_err; }
   if (!sb_OnVolume(path)) { return -ENOENT; }
   if (gb_SimFsExists(path)) { return -EEXIST; }
   i_node = si_NewNode(path, true);
   return (i_node < 0) ? i_node : 0;
}

int fs_unlink(const char *path)
{
   int i_err = si_Enter(eSFS_UNLINK);
   int i_node;

   if (i_err != 0) { return i_err; }
   if (!sb_OnVolume(path)) { return -ENOENT; }
   if (sb_IsRoot(path)) { return -EINVAL; }
   i_node = si_Find(path);
   if (i_node < 0) { return -ENOENT; }
   if (sstar_nodes[i_node].b_dir && sb_HasChildren(path)) { return -EACCES; }
   sv_FreeNode(i_node);
   return 0;
}

int fs_rename(const char *from, const char *to)
{
   char car_old[SIM_FS_PATH_LEN];
   int i_err = si_Enter(eSFS_RENAME);
   int i_node;
   int i_target;
   uint32_t i;
   size_t s_len;

   if (i_err != 0) { return i_err; }
   if (!sb_OnVolume(from) || !sb_OnVolume(to)) { return -ENOENT; }
   i_node = si_Find(from);
   if (i_node < 0) { return -ENOENT; }
   {
      char car_parent[SIM_FS_PATH_LEN];
      if (!sb_Parent(to, car_parent) || !sb_DirExists(car_parent)) { return -ENOENT; }
   }

   // As Zephyr's FAT backend: an existing target is deleted first
   i_target = si_Find(to);
   if ((i_target >= 0) && (i_target != i_node))
   {
      if (sstar_nodes[i_target].b_dir && sb_HasChildren(to)) { return -EACCES; }
      sv_FreeNode(i_target);
   }

   (void)snprintf(car_old, sizeof(car_old), "%s", sstar_nodes[i_node].car_path);
   s_len = strlen(car_old);
   for (i = 0U; i < SIM_FS_MAX_NODES; i++)
   {
      SimFsNode_T *n = &sstar_nodes[i];
      if (n->b_used && (strlen(n->car_path) > s_len) && (n->car_path[s_len] == '/')
         && (strncmp(n->car_path, car_old, s_len) == 0))
      {
         char car_new[SIM_FS_PATH_LEN];
         (void)snprintf(car_new, sizeof(car_new), "%s%s", to, &n->car_path[s_len]);
         (void)snprintf(n->car_path, sizeof(n->car_path), "%s", car_new);
      }
   }
   (void)snprintf(sstar_nodes[i_node].car_path, SIM_FS_PATH_LEN, "%s", to);
   return 0;
}

int fs_stat(const char *path, struct fs_dirent *entry)
{
   const char *cpt_name;
   int i_err = si_Enter(eSFS_STAT);
   int i_node;

   if (i_err != 0) { return i_err; }
   if (!sb_OnVolume(path)) { return -ENOENT; }
   (void)memset(entry, 0, sizeof(*entry));
   if (sb_IsRoot(path))
   {
      entry->type = FS_DIR_ENTRY_DIR;
      return 0;
   }
   i_node = si_Find(path);
   if (i_node < 0) { return -ENOENT; }
   cpt_name = strrchr(sstar_nodes[i_node].car_path, '/') + 1;
   entry->type = sstar_nodes[i_node].b_dir ? FS_DIR_ENTRY_DIR : FS_DIR_ENTRY_FILE;
   (void)snprintf(entry->name, sizeof(entry->name), "%s", cpt_name);
   entry->size = sstar_nodes[i_node].u32_size;
   return 0;
}

int fs_opendir(struct fs_dir_t *zdp, const char *path)
{
   SimFsHandle_T *stpt_h;
   int i_err = si_Enter(eSFS_OPENDIR);

   if (i_err != 0) { return i_err; }
   if (!sb_OnVolume(path) || !sb_DirExists(path)) { return -ENOENT; }
   stpt_h = sstpt_NewHandle();
   if (stpt_h == NULL) { return -ENFILE; }
   stpt_h->i_node = sb_IsRoot(path) ? -1 : si_Find(path);
   stpt_h->u32_pos = 0U;
   zdp->dirp = stpt_h;
   return 0;
}

int fs_readdir(struct fs_dir_t *zdp, struct fs_dirent *entry)
{
   SimFsHandle_T *stpt_h = (SimFsHandle_T *)zdp->dirp;
   const char *cpt_dir;
   int i_err = si_Enter(eSFS_READDIR);

   if (i_err != 0) { return i_err; }
   if (stpt_h == NULL) { return -EBADF; }
   cpt_dir = (stpt_h->i_node < 0) ? scar_mount : sstar_nodes[stpt_h->i_node].car_path;
   (void)memset(entry, 0, sizeof(*entry));
   while (stpt_h->u32_pos < SIM_FS_MAX_NODES)
   {
      SimFsNode_T *n = &sstar_nodes[stpt_h->u32_pos++];
      if (n->b_used && sb_IsChildOf(n->car_path, cpt_dir))
      {
         entry->type = n->b_dir ? FS_DIR_ENTRY_DIR : FS_DIR_ENTRY_FILE;
         (void)snprintf(entry->name, sizeof(entry->name), "%s", strrchr(n->car_path, '/') + 1);
         entry->size = n->u32_size;
         return 0;
      }
   }
   return 0;                                 /* end: empty name */
}

int fs_closedir(struct fs_dir_t *zdp)
{
   if (zdp->dirp != NULL)
   {
      sv_FreeHandle((SimFsHandle_T *)zdp->dirp);
      zdp->dirp = NULL;
   }
   return 0;
}
