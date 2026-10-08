/* SPDX-License-Identifier: MIT */
/* Host-test stand-in for zephyr/fs/fs.h: Zephyr's names, layouts and
   contracts for the calls the File System Manager makes. fs_sim.c implements
   them on an in-memory volume (one mount point, files and directories) with
   error injection; see the gv_SimFs* functions below. */
#ifndef _SHIM_FS_H
#define _SHIM_FS_H
#include "zephyr_shim.h"

#ifndef ENOTEMPTY
#define ENOTEMPTY             41
#endif // ENOTEMPTY

#define MAX_FILE_NAME         255

enum fs_dir_entry_type
{
   FS_DIR_ENTRY_FILE = 0,
   FS_DIR_ENTRY_DIR,
};

enum
{
   FS_FATFS = 0,
   FS_LITTLEFS,
   FS_EXT2,
   FS_VIRTIOFS,
   FS_TYPE_EXTERNAL_BASE,
};

typedef uint8_t fs_mode_t;
#define FS_O_READ             0x01
#define FS_O_WRITE            0x02
#define FS_O_RDWR             (FS_O_READ | FS_O_WRITE)
#define FS_O_CREATE           0x10
#define FS_O_APPEND           0x20
#define FS_O_TRUNC            0x40

struct fs_mount_t
{
   int type;
   const char *mnt_point;
   void *fs_data;
   void *storage_dev;
   size_t mountp_len;
   const void *fs;
   uint8_t flags;
};

struct fs_dirent
{
   enum fs_dir_entry_type type;
   char name[MAX_FILE_NAME + 1];
   size_t size;
};

struct fs_file_t
{
   void *filep;
   const struct fs_mount_t *mp;
   fs_mode_t flags;
};

struct fs_dir_t
{
   void *dirp;
   const struct fs_mount_t *mp;
};

static inline void fs_file_t_init(struct fs_file_t *zfp)
{
   zfp->filep = NULL;
   zfp->mp = NULL;
   zfp->flags = 0;
}

static inline void fs_dir_t_init(struct fs_dir_t *zdp)
{
   zdp->dirp = NULL;
   zdp->mp = NULL;
}

extern int fs_mount(struct fs_mount_t *mp);
extern int fs_mkfs(int fs_type, uintptr_t dev_id, void *cfg, int flags);
extern int fs_open(struct fs_file_t *zfp, const char *file_name, fs_mode_t flags);
extern int fs_close(struct fs_file_t *zfp);
extern ssize_t fs_read(struct fs_file_t *zfp, void *ptr, size_t size);
extern ssize_t fs_write(struct fs_file_t *zfp, const void *ptr, size_t size);
extern int fs_sync(struct fs_file_t *zfp);
extern int fs_mkdir(const char *path);
extern int fs_unlink(const char *path);
extern int fs_rename(const char *from, const char *to);
extern int fs_stat(const char *path, struct fs_dirent *entry);
extern int fs_opendir(struct fs_dir_t *zdp, const char *path);
extern int fs_readdir(struct fs_dir_t *zdp, struct fs_dirent *entry);
extern int fs_closedir(struct fs_dir_t *zdp);

/* ---- Simulation control (fs_sim.c) ---------------------------------------- */
typedef enum
{
   eSFS_MOUNT, eSFS_MKFS, eSFS_OPEN, eSFS_CLOSE, eSFS_READ, eSFS_WRITE, eSFS_SYNC,
   eSFS_MKDIR, eSFS_UNLINK, eSFS_RENAME, eSFS_STAT, eSFS_OPENDIR, eSFS_READDIR,
   eSFS_COUNT
} SimFsOp_E;

/** Empty, unformatted volume; no injected errors; counters cleared. */
extern void gv_SimFsReset(void);
/** Format (and optionally mount) the volume, as after a successful mkfs. */
extern void gv_SimFsFormat(bool b_mount);
/** The u32_after+1-th call of op from now fails with i_err (one shot). */
extern void gv_SimFsFailOnce(SimFsOp_E e_op, uint32_t u32_after, int i_err);
/** Bytes the volume can still hold (default: unlimited). */
extern void gv_SimFsSetFree(uint32_t u32_bytes);
/** Calls of op since the last reset. */
extern uint32_t gu32_SimFsCalls(SimFsOp_E e_op);
/** Open handles (files and directories) not closed yet. */
extern uint32_t gu32_SimFsOpenHandles(void);
/** Whether a path exists, and the content of a file (NULL if none). */
extern bool gb_SimFsExists(const char *cpt_path);
extern const uint8_t *gu8pt_SimFsData(const char *cpt_path, uint32_t *u32pt_size);
/** Create a file or directory directly (parents must exist). */
extern int gi_SimFsPut(const char *cpt_path, const void *vpt_data, uint32_t u32_size);
extern int gi_SimFsPutDir(const char *cpt_path);

#endif //!_SHIM_FS_H
