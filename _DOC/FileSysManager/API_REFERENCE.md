# FileSysManager — API Reference

The File System Manager runs FAT file system commands on the external flash, one at a time,
on its own thread. A user fills a `FileSysMessage_T`, submits it, and receives the results
through a callback on the manager's thread: zero or more listing entries, then **exactly one
final result**.

| Header | Contents |
|---|---|
| [FileSysManager.h](../../_LIB/FileSysManager/FileSysManager.h) | The public API. Includes the two below; the only header users include |
| [FileSysManager_Types.h](../../_LIB/FileSysManager/FileSysManager_Types.h) | Commands, status codes, message, result, constants |
| [FileSysManager_Config.h](../../_LIB/FileSysManager/FileSysManager_Config.h) | Tunables |
| [FileSysManagerFSM.h](../../_LIB/FileSysManager/FileSysManagerFSM.h) | The state machine's context and functions, used by `FileSysManager.c` (and the tests) only |

**Dependencies:** Zephyr kernel (`k_msgq`, `k_sem`, threads), SMF (`CONFIG_SMF`), the file
system API (`CONFIG_FILE_SYSTEM`, `CONFIG_FAT_FILESYSTEM_ELM`, `CONFIG_FILE_SYSTEM_MKFS`), a
`zephyr,flash-disk` named `FLASH_DISK` and the custom mount point `FLASH_DISK`
(`CONFIG_FS_FATFS_CUSTOM_MOUNT_POINTS`), and [`AppLog`](../../_ASW/_APP_LOG) for logging.

---

## 1. Quick start

```c
#include "FileSysManager.h"

/* main(): mount the volume (on the manager's thread) */
(void)gi_FSMGR_Start();

/* From a thread that may wait: create a directory and write a file */
static FileSysMessage_T sst_msg;            /* large: keep it off small stacks */

static int si_Fs(FileSysCommand_E e_cmd, const void *vpt_data, uint32_t u32_len, uint8_t u8_flags)
{
   sst_msg.e_command = e_cmd;
   sst_msg.u8_flags = u8_flags;
   sst_msg.u32_sizeOfData = u32_len;
   memcpy(sst_msg.u8_data, vpt_data, u32_len);
   return gi_FSMGR_Call(&sst_msg);           /* waits for the final result */
}

int i_ret = si_Fs(eFSC_MAKE_DIR, "/FLASH_DISK:/LOG", 16U, FSMGR_MSG_KEEP_DIR);
i_ret = (i_ret == 0) ? si_Fs(eFSC_OPEN_FILE_WRITE, "/FLASH_DISK:/LOG/a.txt", 22U, 0U) : i_ret;
i_ret = (i_ret == 0) ? si_Fs(eFSC_WRITE_DATA, "hello", 5U, 0U) : i_ret;
i_ret = (i_ret == 0) ? si_Fs(eFSC_CLOSE_FILE, NULL, 0U, 0U) : i_ret;

/* From a thread that must not wait (e.g. a BulkXfer callback): results by callback */
static void sv_OnResult(const FsmgrResult_T *r, void *vpt_user)
{
   if (!r->b_final) { /* a listing entry: r->u8pt_data is its path */ return; }
   /* r->i_status, r->u8pt_data / r->u32_len, r->u32_total */
}

FileSysMessage_T st_ls = { .e_command = eFSC_DEBUG_LIST_DRIVE };
(void)gi_FSMGR_Submit(&st_ls, sv_OnResult, NULL, K_NO_WAIT);
```

---

## 2. Constants and configuration

| Name | Value | Meaning |
|---|---|---|
| `FS_MAX_CHUNK_SIZE` | 242 | Largest payload of a message (a path, a rename pair, a write); also the largest read |
| `FSMGR_DRIVE_NAME` | `"FLASH_DISK"` | Disk name of the volume |
| `FSMGR_MOUNT_POINT` | `"/FLASH_DISK:"` | Mount point; absolute paths start with it |
| `FSMGR_MSG_KEEP_DIR` | `0x01` | Message flag: `eFSC_MAKE_DIR` does not make the new directory current |
| `FSMGR_ENTRY_FILE` / `FSMGR_ENTRY_DIR` | 0 / 1 | `FsmgrResult_T.u8_entryType` |

| Tunable ([FileSysManager_Config.h](../../_LIB/FileSysManager/FileSysManager_Config.h)) | Kconfig (`_DI/Kconfig`) | Default |
|---|---|---|
| `FSMGR_STACK_SIZE` | `CONFIG_FSMGR_STACK_SIZE` | 4096 |
| `FSMGR_PRIORITY` | `CONFIG_FSMGR_PRIORITY` | 10 (below the BulkXfer engine's 5) |
| `FSMGR_QUEUE_DEPTH` | `CONFIG_FSMGR_QUEUE_DEPTH` | 8 messages |
| `FSMGR_WRITE_BUFFER_SIZE` | `CONFIG_FSMGR_WRITE_BUFFER_SIZE` with `CONFIG_FSMGR_BUFFERED_WRITE` | 512 if buffered, else 0 (direct) |
| `FSMGR_MAX_PATH_LEN` | — | 256, terminator included |
| — | `CONFIG_FSMGR_FLASH_IF_SPI` / `_SQSPI` | Interface named in the mount log line (set by `_DI/conf/flash_<if>.conf`) |

---

## 3. Types

### `FileSysMessage_T`

| Field | Meaning |
|---|---|
| `e_command` | `FileSysCommand_E` |
| `u8_flags` | `FSMGR_MSG_*` |
| `u32_sizeOfData` | Payload length, at most `FS_MAX_CHUNK_SIZE`; for `eFSC_READ_FILE` the byte count wanted (any value) |
| `u8_data[FS_MAX_CHUNK_SIZE]` | Payload, not terminated |
| `fpt_onResult`, `vpt_user` | Set by `gi_FSMGR_Submit()`; ignored on input |

### `FileSysCommand_E`

| Command | Payload | Final result's data (on success) | `u32_total` |
|---|---|---|---|
| `eFSC_OPEN_DIR` | Directory path | The directory's absolute path; it becomes current | — |
| `eFSC_MAKE_DIR` | Directory path (an existing one is fine) | Its absolute path; current unless `FSMGR_MSG_KEEP_DIR` | — |
| `eFSC_OPEN_FILE_READ` | File path | Its absolute path | — |
| `eFSC_OPEN_FILE_WRITE` | File path; created if missing, **not truncated** | Its absolute path | 0 |
| `eFSC_WRITE_DATA` | 1..`FS_MAX_CHUNK_SIZE` bytes | — | Bytes written to the file so far (buffered ones included) |
| `eFSC_READ_FILE` | — (`u32_sizeOfData` = count; 0 or more than `FS_MAX_CHUNK_SIZE` reads `FS_MAX_CHUNK_SIZE`) | The bytes read; none at the end of the file | — |
| `eFSC_DEBUG_LIST_DRIVE` | — | One non-final result per entry (below), then the final one | Entries listed |
| `eFSC_DELETE_FILE` | File path | — | — |
| `eFSC_DELETE_DIR` | Directory path; its files are deleted first | — | — |
| `eFSC_CLOSE_FILE` | — | — | Bytes written (0 for a file read) |
| `eFSC_ABORT` | — | — | — |
| `eFSC_RENAME` | `"old\0new"`, both file paths; an existing `new` is replaced | — | — |

A listing entry has `b_final = false`, `u8_entryType`, `u32_entrySize` (files) and its absolute
path in `u8pt_data` / `u32_len` (not terminated). Entries come root first: each root entry,
and right after a directory its direct children; a child directory is listed, its content not.

### `FsmgrResult_T`

| Field | Meaning |
|---|---|
| `e_command` | The message's command |
| `i_status` | 0 or a negative errno (§6) |
| `b_final` | `false` for a listing entry, `true` for the one final result |
| `u8_entryType`, `u32_entrySize` | Listing entry only |
| `u8pt_data`, `u32_len` | Data (see the table above); valid only during the callback |
| `u32_total` | See the table above |

`FsmgrResult_F` is `void (*)(const FsmgrResult_T *stpt_result, void *vpt_user)`.

### `FsmgrStatus_E` and `gu8_FSMGR_StatusCode()`

| Code | Name | From |
|---|---|---|
| 0 | `eFSS_OK` | 0 |
| 1 | `eFSS_NOT_FOUND` | `-ENOENT` |
| 2 | `eFSS_EXISTS` | `-EEXIST` |
| 3 | `eFSS_NOT_EMPTY` | `-ENOTEMPTY` |
| 4 | `eFSS_NO_SPACE` | `-ENOSPC` |
| 5 | `eFSS_BAD_ARG` | `-EINVAL`, `-ENAMETOOLONG` |
| 6 | `eFSS_BAD_STATE` | `-EPERM`, `-EBADF` |
| 7 | `eFSS_BUSY` | `-EBUSY`, `-ENOMSG`, `-EAGAIN` |
| 8 | `eFSS_NOT_MOUNTED` | `-ENODEV` |
| 9 | `eFSS_IO` | any other |
| 10 | `eFSS_NOT_SUPPORTED` | `-ENOTSUP` |
| 11 | `eFSS_WRONG_TYPE` | `-ENOTDIR`, `-EISDIR` |
| 12 | `eFSS_DENIED` | `-EACCES` |

---

## 4. Functions

| Function | Returns | Notes |
|---|---|---|
| `int gi_FSMGR_Start(void)` | 0; `-EALREADY` if started before | Starts the thread, which mounts the volume (formatting it if mounting fails) and then runs messages. Call once from `main()`. Messages may be submitted before; they wait |
| `int gi_FSMGR_Submit(const FileSysMessage_T *msg, FsmgrResult_F cb, void *user, k_timeout_t t)` | 0; `-EINVAL` (no message, unknown command, payload longer than `FS_MAX_CHUNK_SIZE` for a command other than read); `k_msgq_put()`'s error when the queue stays full for `t` (`-ENOMSG` with `K_NO_WAIT`, `-EAGAIN` after a timeout) | Copies the message. `cb` may be `NULL`. Any thread; ISR only with `K_NO_WAIT` |
| `int gi_FSMGR_Call(const FileSysMessage_T *msg)` | The final result's status, or `gi_FSMGR_Submit()`'s error | Waits (`K_FOREVER`) for room in the queue and for the result. Not from the manager's thread (a result callback): it would wait for ever. Listing entries and data are not returned |
| `bool gb_FSMGR_IsMounted(void)` | Whether the volume is mounted | |
| `bool gb_FSMGR_IsFileOpen(void)` | Whether the FSM holds an open file | A snapshot; it changes on the manager's thread |
| `uint8_t gu8_FSMGR_StatusCode(int status)` | `FsmgrStatus_E` | For reporting to a peer |

Internal ([FileSysManagerFSM.h](../../_LIB/FileSysManager/FileSysManagerFSM.h)), manager's thread
only: `gv_FileSysManagerFSMInit()` (root current, no file, IDLE), `gv_FileSysManagerFSMRun()`
(executes `st_currentMsg`; reports `-EIO` if it would end without a result) and
`gv_FileSysManagerFSMReport()` (the final result).

---

## 5. Commands per state

| Command | IDLE | WRITE_FILE | READ_FILE |
|---|---|---|---|
| OPEN_DIR, MAKE_DIR | runs | `-EPERM` | `-EPERM` |
| OPEN_FILE_READ / _WRITE | runs → READ_FILE / WRITE_FILE | `-EPERM` | `-EPERM` |
| WRITE_DATA | `-EPERM` | runs | `-EPERM` |
| READ_FILE | `-EPERM` | `-EPERM` | runs |
| DEBUG_LIST_DRIVE, DELETE_FILE, DELETE_DIR, RENAME | runs | `-EPERM` | `-EPERM` |
| CLOSE_FILE | 0 (nothing open) | flush, sync, close → IDLE | close → IDLE |
| ABORT | 0; resets the current directory to the root | close, drop buffered data → IDLE | close → IDLE |

A refused command (`-EPERM`) changes nothing. Any other failure while a file is open (a
failed write, read, flush or sync, an empty write) closes it and returns to IDLE. A failure in IDLE leaves the current directory as it was.

---

## 6. Paths and errors

Paths: `"/FLASH_DISK:/x"` is used as is; `"/x"` is taken from the root; a relative name from the
root for `eFSC_MAKE_DIR`, `eFSC_OPEN_DIR` and `eFSC_DELETE_DIR`, and from the current directory
for files. An empty name, a lone `"/"`, or a `'\0'` inside gives `-EINVAL`; a path that does not
fit `FSMGR_MAX_PATH_LEN` gives `-ENAMETOOLONG`.

| Status | When |
|---|---|
| `-ENODEV` | The volume is not mounted (every command) |
| `-EPERM` | Not allowed in this state (§5) |
| `-EINVAL` | Bad path or payload: empty write, rename without `'\0'`, unknown command |
| `-ENAMETOOLONG` | Path too long |
| `-ENOENT` | No such file or directory, or its parent is missing |
| `-ENOTDIR` / `-EISDIR` | OPEN_DIR or DELETE_DIR on a file / DELETE_FILE on a directory |
| `-ENOTSUP` | DELETE_DIR of a directory with a subdirectory |
| `-ENOSPC` | The volume is full (a short write) |
| `-EIO` | A command ended without a result (a defect), or a flash error |
| other negative errno | From Zephyr's file system API |

---

## 7. Threading

- The manager's thread is the only one that touches the file system. Results and listing
  entries are delivered on it; a callback must not block on the manager (no `gi_FSMGR_Call()`).
- `gi_FSMGR_Submit()` copies the message, so the caller's buffer is free when it returns.
- The current directory and the open file are shared by all users. Use absolute paths, and
  agree on who may hold the open file (`gb_FSMGR_IsFileOpen()`).

---

## 8. Behaviour notes and known limitations

- With the write buffer, `eFSC_WRITE_DATA` succeeds once the data is buffered; a flash error
  shows on a later write or on CLOSE.
- Opening for writing does not truncate; delete the file first to replace it.
- The listing is one level deep below the root; DELETE_DIR is not recursive.
- Any mount failure formats the volume.
- FAT is not power-fail safe; write to a temporary file and rename it when a half-written file
  must never appear under its real name.
