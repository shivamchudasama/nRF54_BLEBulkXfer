# FileSystemPoC

UART-driven FATFS file system test harness on `nrf54l15dk/nrf54l15/cpuapp` using external NOR flash (`FLASH_DISK`).

## Overview

This project mounts a FATFS volume and runs a File System Manager FSM in a dedicated thread.
`main.c` provides a UART console to send file-system commands, which are packed into `FileSysMessage_T` and pushed into a Zephyr message queue.

Core pieces:
- `ASW/main.c`: UART command parser and queue producer.
- `LIB/FileSysManager/FileSysManager.c`: mount/mkfs logic and FSM thread.
- `LIB/FileSysManager/FileSysManagerFSM.c`: state-machine logic for directory/file operations.
- `LIB/Common/TransferMsgTypes.h`: command/message contract between UART harness and FSM.
- `DOC`: Folder contains documentation associated with the project.

## Storage Setup

- FATFS mount point: `"/FLASH_DISK:"`
- Backing disk: `zephyr,flash-disk` (`disk-name = "FLASH_DISK"`)
- Partition configured in:
  - `boards/nrf54l15dk_nrf54l15_cpuapp_FATFS.overlay`

If mount fails at boot, the manager attempts `fs_mkfs(...)` and mounts again.

## Build

From repo root:

```sh
west build -b nrf54l15dk/nrf54l15/cpuapp
```

Select external flash interface at compile time:

```sh
# SPI NOR (default)
west build --build-dir d:/GIT/nrf54_Zephyr/build_spi d:/GIT/nrf54_Zephyr --pristine --board nrf54l15dk/nrf54l15/cpuapp -- -DCONFIG_DEBUG_THREAD_INFO=y -DCONF_FILE="_DI/prj.conf" -DEXTRA_CONF_FILE="_DI/conf/flash_spi.conf" -DDTC_OVERLAY_FILE=_DI/boards/nrf54l15dk_nrf54l15_cpuapp_FATFS.overlay

# sQSPI via FLPR + MSPI shim
west build --build-dir d:/GIT/nrf54_Zephyr/build_qspi d:/GIT/nrf54_Zephyr --pristine --board nrf54l15dk/nrf54l15/cpuapp -- -DCONFIG_DEBUG_THREAD_INFO=y -DCONF_FILE="_DI/prj.conf" -DEXTRA_CONF_FILE="_DI/conf/flash_sqspi.conf" -DDTC_OVERLAY_FILE=_DI/boards/nrf54l15dk_nrf54l15_cpuapp_FATFS_sqspi.overlay
```



```sh
west flash
```

## UART Commands

Type `help` on UART console to print available commands.

- `mkdir <path>`: create directory (or keep if already exists) and set it as current directory.
- `cd <path>`: open existing directory only and set as current directory.
- `openr <path>`: open file in read mode and transition to READ state.
- `openw <path>`: open/create file in write mode and transition to WRITE state.
- `write <text>`: write payload to currently opened file (WRITE state only).
- `read [bytes]`: read from currently opened file (READ state only).
  Default read size is `FS_MAX_CHUNK_SIZE` (240), max is also 240.
- `ls`: debug listing of mount root and one level under each root directory.
- `delfile <path>`: delete file (IDLE state only).
- `deldir <path>`: delete directory (IDLE state only, directory must be empty).
- `close`: close current file.
- `abort`: close file and clear FSM saved context.

## FSM Behavior Notes

- File open mode is explicit:
  - `openr` -> `FS_O_READ` -> READ state
  - `openw` -> `FS_O_CREATE | FS_O_WRITE` -> WRITE state
- Delete operations are accepted only from IDLE (via dedicated DELETE state).
- Debug `ls` command is executed only in IDLE.
- Path handling supports:
  - absolute FATFS path (example: `/FLASH_DISK:/TLA/a.txt`)
  - relative paths using current directory context.

## Known Limits

- Debug listing is intentionally non-recursive beyond one level:
  - root entries are listed
  - for each root directory, only direct children are listed.
- `deldir` does not recursively delete contents. Non-empty deletion will be allowed, but only upto one sub-directory depth.

## Configuration Highlights

Important options in `prj.conf`:
- `CONFIG_FILE_SYSTEM=y`
- `CONFIG_FILE_SYSTEM_MKFS=y`
- `CONFIG_FAT_FILESYSTEM_ELM=y`
- `CONFIG_FS_FATFS_MOUNT_MKFS=y`
- `CONFIG_FS_FATFS_CUSTOM_MOUNT_POINTS="FLASH_DISK"`
- `CONFIG_SMF=y`

## License / Ownership

See source-file headers for project ownership and copyright notices.
