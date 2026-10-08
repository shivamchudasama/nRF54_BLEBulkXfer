## File System Manager Proposed Architecture

### 1. Scope
This document describes the core architecture of:
- `FileSysManager` (`LIB/FileSysManager/FileSysManager.c`)
- `FileSysManagerFSM` (`LIB/FileSysManager/FileSysManagerFSM.c/.h`)

It also documents the UART testing harness as a separate integration layer:
- `ASW/main.c`

Primary goals:
- Keep file-system operations serialized in one thread.
- Decouple command producer(s) from FS execution using queue messaging.
- Represent behavior explicitly using a state machine.

### 2. High-Level Design
The design is a producer-consumer model with FSM-based execution:
- Producer: UART harness parses console commands and builds `FileSysMessage_T`.
- Transport: Zephyr message queue (`k_msgq`) buffers command messages.
- Consumer: File System Manager thread blocks on queue and dispatches messages into FSM.
- Execution engine: FSM performs validated FS operations using Zephyr FS APIs.

Data flow:
1. User types command on UART.
2. `main.c` parses and fills `FileSysMessage_T`.
3. Message is enqueued into `FSMGR_MSG_Q`.
4. `sv_FSMGR_Thread` dequeues message into `sst_FSMGRContext.st_currentMsg`.
5. `gv_FileSysManagerFSMRun()` advances FSM and performs the requested operation.

### 3. Core Modules

#### 3.1 FileSysManager (`FileSysManager.c`)
Responsibilities:
- Defines and owns `FSMGR_MSG_Q`.
- Owns FS mount information and mount lifecycle.
- Creates dedicated manager thread using `K_THREAD_DEFINE`.
- Initializes FSM context and feeds messages into FSM.

Important objects:
- `K_MSGQ_DEFINE(FSMGR_MSG_Q, sizeof(FileSysMessage_T), 16, 4)`
- `static FATFS st_FatFs`
- `static struct fs_mount_t sst_mountInfo`
- `static FileSysManagerCTX_T sst_FSMGRContext`

Mount behavior:
- Attempts `fs_mount`.
- On failure, calls `fs_mkfs` and retries mount.

#### 3.2 FileSysManager FSM (`FileSysManagerFSM.c`)
Responsibilities:
- Implements operation sequencing via SMF states.
- Validates command/state compatibility.
- Normalizes/constructs paths.
- Executes Zephyr FS APIs for create/open/read/write/list/delete/close.

Current FSM states:
- `STATE_IDLE`
- `STATE_CREATE_DIR`
- `STATE_CREATE_FILE`
- `STATE_DELETE`
- `STATE_WRITE_FILE`
- `STATE_READ_FILE`
- `STATE_CLOSE`
- `STATE_FAILED`

### 4. Data Types and Context

#### 4.1 Command and Message Contract (`TransferMsgTypes.h`)
`FileSysCommand_E` currently includes:
- `FSC_OPEN_DIR`
- `FSC_MAKE_DIR`
- `FSC_OPEN_FILE_READ`
- `FSC_OPEN_FILE_WRITE`
- `FSC_WRITE_DATA`
- `FSC_READ_FILE`
- `FSC_DEBUG_LIST_DRIVE`
- `FSC_DELETE_FILE`
- `FSC_DELETE_DIR`
- `FSC_CLOSE_FILE`
- `FSC_ABORT`

Message payload type:
```c
typedef struct {
   FileSysCommand_E e_command;
   uint32_t u32_sizeOfData;
   uint8_t u8_data[FS_MAX_CHUNK_SIZE];
} FileSysMessage_T;
```

Constants:
- `FS_MAX_CHUNK_SIZE = 240`
- `MAX_FILENAME_LEN = 64`
- `MAX_PATH_LEN = 3` (currently not driving logic)

#### 4.2 FSM Context (`FileSysManagerFSM.h`)
Primary execution context:
```c
typedef struct{
   struct smf_ctx smf;
   struct fs_file_t file;
   FileSysMessage_T st_currentMsg;
   uint32_t u32_byteWritten;
   uint32_t u32_totalExpectedBytes;
   bool b_fileOpenStatus;
   char as8_currentDir[FS_MAX_CHUNK_SIZE];
   char as8_activeFile[FS_MAX_CHUNK_SIZE];
} FileSysManagerCTX_T;
```

Meaning of key fields:
- `smf`: Zephyr SMF internal state carrier.
- `file`: active file handle (`struct fs_file_t`) used for read/write/close.
- `st_currentMsg`: current command dequeued from `k_msgq`.
- `b_fileOpenStatus`: gate to prevent read/write without valid file.
- `as8_currentDir`: current directory context for relative paths.
- `as8_activeFile`: last opened file path.

#### 4.3 File/Directory Info Structures Used
- File handle: `struct fs_file_t`
  - initialized via `fs_file_t_init()`
  - used with `fs_open`, `fs_read`, `fs_write`, `fs_close`
- Directory entry info: `struct fs_dirent`
  - used for `fs_stat` type checks and debug listing
  - `type` identifies `FS_DIR_ENTRY_DIR` vs `FS_DIR_ENTRY_FILE`
  - `size` used for file-size logs
- Directory iterator: `struct fs_dir_t`
  - used with `fs_opendir`, `fs_readdir`, `fs_closedir` for listing

### 5. Command-to-State Behavior

IDLE-accepted operational commands:
- Directory commands: `FSC_OPEN_DIR`, `FSC_MAKE_DIR` -> `STATE_CREATE_DIR`
- File-open commands: `FSC_OPEN_FILE_READ`, `FSC_OPEN_FILE_WRITE` -> `STATE_CREATE_FILE`
- Delete commands: `FSC_DELETE_FILE`, `FSC_DELETE_DIR` -> `STATE_DELETE`
- Debug list: `FSC_DEBUG_LIST_DRIVE` executes in IDLE only

Mode-specific behavior:
- `FSC_OPEN_FILE_READ` opens with `FS_O_READ` and transitions to READ state.
- `FSC_OPEN_FILE_WRITE` opens with `FS_O_CREATE | FS_O_WRITE` and transitions to WRITE state.

Safety constraints:
- Delete is intentionally initiated from IDLE.
- Debug list command is only executed in IDLE.
- Non-compatible commands in READ/WRITE are rejected and logged.

### 6. Path Resolution Model
Path build helper:
- `si_BuildPathFromMsg(...)`

Rules:
- Absolute FAT paths (`/FLASH_DISK:/...`) are used as-is.
- Paths beginning with `/` but without drive prefix are anchored to `FAT_MOUNT_POINT`.
- Relative paths are anchored to:
  - `FAT_MOUNT_POINT` for directory-create/open logic.
  - `as8_currentDir` for file operations.

### 7. Zephyr Internals Used

#### 7.1 `k_msgq` (Message Queue)
Purpose:
- Thread-safe decoupling between command source and FS executor.
- Avoids direct FS calls from UART context.

How used here:
- Queue is statically allocated: `K_MSGQ_DEFINE`.
- Producer side (`main.c`) calls `k_msgq_put(...)` with timeout.
- Consumer side (`sv_FSMGR_Thread`) calls `k_msgq_get(..., K_FOREVER)` and blocks.

Why this is useful:
- Natural back-pressure when queue is full.
- Deterministic single-owner FS execution in one thread.
- Easy extension: any future producer can send same `FileSysMessage_T`.

#### 7.2 SMF (State Machine Framework)
Purpose:
- Explicitly model valid command handling by state.
- Keep operation transitions and error paths predictable.

How used here:
- Context embeds `struct smf_ctx`.
- State table built with `SMF_CREATE_STATE(...)`.
- Initialization with `smf_set_initial(...)`.
- Runtime dispatch via `smf_run_state(...)`.
- Transitions via `smf_set_state(...)`.

Why this is useful:
- Prevents invalid action execution in wrong state.
- Centralized place for transition policy.
- Easier debugging via state-scoped logs.

### 8. UART Test Harness (Separate Integration Layer)

Implemented in `ASW/main.c`.

Responsibilities:
- Poll UART (`uart_poll_in`) and parse command line input.
- Build `FileSysMessage_T`.
- Push message to manager queue (`gstpt_FSMGR_GetMsgQ()` + `k_msgq_put`).

Current UART commands:
- `mkdir <path>`
- `cd <path>`
- `openr <path>`
- `openw <path>`
- `write <text payload>`
- `read [bytes]`
- `ls`
- `delfile <path>`
- `deldir <path>`
- `close`
- `abort`

Role boundary:
- UART layer is intentionally “dumb”: parse and send.
- FS correctness and sequencing stay inside FSM.

### 9. Debug Listing Design
Debug helper currently lists:
- Root level entries under mount point.
- One additional level under each root directory.

Current behavior:
- Non-recursive beyond one depth.
- If deeper subdirectory is detected at depth 1, it is logged as ignored.

### 10. Deletion Semantics
Delete implementation uses:
- Path build
- `fs_stat` type check
- `fs_unlink`

Important behavior:
- File delete requires `FS_DIR_ENTRY_FILE`.
- Directory delete requires `FS_DIR_ENTRY_DIR`.
- Directory delete is not recursive. Non-empty directory deletion can fail.

### 11. Error Handling Strategy
- Invalid path/message data -> transition to `STATE_FAILED`.
- FS API failures -> log + `STATE_FAILED`.
- `STATE_FAILED` performs cleanup and returns to IDLE.
- `FSC_ABORT` from IDLE/active states closes file and clears saved context.

### 12. Suggested Next Refinements
- Add a compact operation result struct for command acknowledgements.
- Add optional response queue for UART-side success/failure reporting.
- Add compile-time flag for debug-only commands (`ls`).
- Add path-length and filename validation centralization.
- Add focused unit tests for path build and command/state compatibility.
