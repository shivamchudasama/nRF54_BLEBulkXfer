# GATT_CB — API Reference

A small local GATT database for Zephyr / NCS. Every characteristic in the project registers
the **same two** Zephyr callbacks, `gt_GATT_GenericRead` and `gt_GATT_GenericWrite`, and passes a
per-characteristic `GATTCharDescriptor_T` as `user_data`. The descriptor names the local
variable that stores the value, how long it may be, an optional mutex, and optional hooks for
logic beyond a plain copy. Application threads read and write the same variable through
`gt_GATT_LocalRead` / `gv_GATT_LocalWrite`.

| Header | Contents |
|---|---|
| [GATT_CB_Types.h](../../_LIB/GATT_CB/GATT_CB_Types.h) | `GATTCharDescriptor_T`, hook types `GATTCustomReadCb_F` / `GATTCustomWriteCb_F` |
| [GATT_GenericCallbacks.h](../../_LIB/GATT_CB/GATT_GenericCallbacks.h) | Stack callbacks and application-side access API. Includes `GATT_CB_Types.h` |
| [GATT_GenericCallbacks.c](../../_LIB/GATT_CB/GATT_GenericCallbacks.c) | Implementation |

Applications include only `GATT_GenericCallbacks.h`.

**Dependencies:** Zephyr kernel (`k_mutex`), `zephyr/bluetooth/gatt.h`, and
[`AppLog`](../../_ASW/_APP_LOG) (`AppLog.h`) for error logging. No Kconfig of its own.

**Used by:** the BulkXfer Server — its DATA characteristic uses `gt_GATT_GenericWrite` with
`gt_BLKS_DataWriteHook` as `fpt_customWriteCb`, and its Caps characteristic uses
`gt_GATT_GenericRead`. See [BulkXfer_Service.c](../../_LIB/BulkXfer/examples/bulk_server/src/BulkXfer_Service.c)
for a complete service built on this library.

**Build:** [`_LIB/CMakeLists.txt`](../../_LIB/CMakeLists.txt) adds `GATT_CB` before `BulkXfer`.
Its own `CMakeLists.txt` globs `*.c` into `app` and puts the folder on the include path.

---

## 1. Quick start

```c
#include "GATT_GenericCallbacks.h"

/* Fixed-length, read/write, stack-only access: no mutex, no hooks */
static uint8_t su8_mode = 0U;
static GATTCharDescriptor_T sst_modeDesc = {
   .vpt_data          = &su8_mode,
   .u16_dataLen       = sizeof(su8_mode),
   .u16_actualLen     = sizeof(su8_mode),     /* fixed-length: must equal u16_dataLen */
   .b_variableLength  = false,
   .stpt_mutex        = NULL,
   .fpt_customReadCb  = NULL,
   .fpt_customWriteCb = NULL,
};

/* Variable-length, shared with an application thread, with a write hook */
static uint8_t su8ar_name[32U];
static K_MUTEX_DEFINE(sst_nameMutex);
static ssize_t st_OnNameWrite(struct bt_conn *stpt_conn, const struct bt_gatt_attr *stpt_attr,
   const void *vpt_buf, uint16_t u16_len, uint16_t u16_off, uint8_t u8_flags);

static GATTCharDescriptor_T sst_nameDesc = {
   .vpt_data          = su8ar_name,
   .u16_dataLen       = sizeof(su8ar_name),   /* buffer capacity */
   .u16_actualLen     = 0U,                   /* empty until written */
   .b_variableLength  = true,
   .stpt_mutex        = &sst_nameMutex,
   .fpt_customReadCb  = NULL,
   .fpt_customWriteCb = st_OnNameWrite,
};

BT_GATT_SERVICE_DEFINE(my_svc,
   BT_GATT_PRIMARY_SERVICE(BT_UUID_MY_SVC),
   BT_GATT_CHARACTERISTIC(BT_UUID_MY_MODE,
      BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
      gt_GATT_GenericRead, gt_GATT_GenericWrite, &sst_modeDesc),
   BT_GATT_CHARACTERISTIC(BT_UUID_MY_NAME,
      BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
      gt_GATT_GenericRead, gt_GATT_GenericWrite, &sst_nameDesc),
);

/* From an application thread */
void app_update(void)
{
   uint8_t u8ar_buf[32U];
   uint16_t u16_n = 0U;

   gv_GATT_LocalWrite(&sst_modeDesc, &(uint8_t){ 2U }, 1U);
   gt_GATT_LocalRead(&sst_nameDesc, u8ar_buf, sizeof(u8ar_buf), &u16_n);
}
```

For a read-only characteristic pass `NULL` as the write callback (and vice versa). The
descriptor is `user_data`, so it must have static storage duration and must not be `const`
(`gt_GATT_GenericWrite` updates `u16_actualLen`).

---

## 2. Types — `GATT_CB_Types.h`

### `GATTCharDescriptor_T`

One instance per characteristic, file-scope `static` in the service's `.c` file.

| Field | Type | Meaning |
|---|---|---|
| `vpt_data` | `void *` | Local variable holding the value. Any type. **Must not be NULL** (stack callbacks return `BT_ATT_ERR_UNLIKELY`; local API asserts). |
| `u16_dataLen` | `uint16_t` | Capacity of `vpt_data` in bytes. Fixed-length: `sizeof` the value. Variable-length: buffer size, the maximum accepted write end. |
| `u16_actualLen` | `uint16_t` | Bytes of valid data; this is what reads return. Fixed-length: **initialise to `u16_dataLen`** — the library never sets it. Variable-length: initialise to `0` (empty) or the pre-populated length; updated by writes (§3, §4). |
| `b_variableLength` | `bool` | `false`: writes must cover exactly `u16_dataLen` bytes. `true`: any write ending at or before `u16_dataLen` is accepted. |
| `stpt_mutex` | `struct k_mutex *` | Optional. When non-NULL, every library function holds it (`K_FOREVER`) around the copy and the `u16_actualLen` access. Use `NULL` when only the BT stack touches the value. |
| `fpt_customReadCb` | `GATTCustomReadCb_F` | Optional post-read hook, `NULL` for a plain read. |
| `fpt_customWriteCb` | `GATTCustomWriteCb_F` | Optional post-write hook, `NULL` for a plain write. |

### `GATTCustomReadCb_F`

```c
typedef ssize_t (*GATTCustomReadCb_F)(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset);
```

Called by `gt_GATT_GenericRead` **after** `vpt_buf` has been filled and the mutex released.
The hook may inspect or overwrite `vpt_buf`.

- Return `0` → the generic callback's byte count is kept.
- Return non-zero (a byte count or `BT_GATT_ERR(...)`) → it replaces the result.

### `GATTCustomWriteCb_F`

```c
typedef ssize_t (*GATTCustomWriteCb_F)(struct bt_conn *stpt_connHandle,
   const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length,
   uint16_t u16_offset, uint8_t u8_flags);
```

Called by `gt_GATT_GenericWrite` **after** the length check passed, the data was copied into
`vpt_data`, `u16_actualLen` was updated, and the mutex released. `vpt_buf` is the stack's
incoming buffer (same bytes that are now in `vpt_data`). Intended for side effects: feed a
state machine, trigger a notification, or reject a semantically invalid value.

- Return `0` → the generic result (`u16_length`) is kept.
- Return non-zero → it replaces the result. `BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED)` is the
  way to reject a value.
- **No rollback:** `vpt_data` is already updated when the hook runs. If a rejected value must
  not persist, the hook restores it itself (taking the mutex if one is configured).

---

## 3. Stack callbacks

Register these in `BT_GATT_CHARACTERISTIC`; never call them directly. Both `__ASSERT` that
`stpt_attr` and `stpt_attr->user_data` are non-NULL.

#### `ssize_t gt_GATT_GenericRead(struct bt_conn *stpt_connHandle, const struct bt_gatt_attr *stpt_attr, void *vpt_buf, uint16_t u16_length, uint16_t u16_offset)`

1. `vpt_data == NULL` → `BT_GATT_ERR(BT_ATT_ERR_UNLIKELY)`.
2. Lock mutex (if any).
3. `bt_gatt_attr_read(..., vpt_data, u16_actualLen)` — handles the offset
   (`BT_ATT_ERR_INVALID_OFFSET` if `u16_offset > u16_actualLen`) and clips to `u16_length`.
4. Unlock mutex.
5. Run `fpt_customReadCb` (if any); a non-zero return replaces the result.

**Returns** bytes placed in `vpt_buf`, or `BT_GATT_ERR(...)`.

#### `ssize_t gt_GATT_GenericWrite(struct bt_conn *stpt_connHandle, const struct bt_gatt_attr *stpt_attr, const void *vpt_buf, uint16_t u16_length, uint16_t u16_offset, uint8_t u8_flags)`

1. `vpt_data == NULL` → `BT_GATT_ERR(BT_ATT_ERR_UNLIKELY)`.
2. Length check on `end = u16_offset + u16_length` (computed in 32 bits, so no wrap):
   - `end > u16_dataLen` → `BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN)`.
   - Fixed-length and `end != u16_dataLen` → `BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN)`.
3. Lock mutex (if any).
4. `memcpy(vpt_data + u16_offset, vpt_buf, u16_length)`.
5. Variable-length only: if `end > u16_actualLen`, set `u16_actualLen = end`. It **only grows** (see §6).
6. Unlock mutex.
7. Run `fpt_customWriteCb` (if any); a non-zero return replaces the result.

**Returns** `u16_length`, or `BT_GATT_ERR(...)`. Failures in step 1–2 are logged with `APP_LOG_ERR`
and leave `vpt_data` untouched. `u8_flags` is not inspected, only forwarded to the hook.

---

## 4. Application-side API

For application threads. All three `__ASSERT` their pointer arguments (and `vpt_data`) and
take the mutex if one is configured. None of them calls the custom hooks.

#### `void gt_GATT_LocalRead(const GATTCharDescriptor_T *stpt_desc, void *vpt_buf, uint16_t u16_bufLen, uint16_t *u16pt_bytesRead)`

Copies `min(u16_bufLen, u16_actualLen)` bytes from `vpt_data` into `vpt_buf` and stores that
count in `*u16pt_bytesRead`. Truncation is silent; compare with `gt_GATT_GetActualLen` to
detect it.

#### `void gv_GATT_LocalWrite(GATTCharDescriptor_T *stpt_desc, const void *vpt_buf, uint16_t u16_length)`

Copies `u16_length` bytes into `vpt_data` at offset 0. Length rules match the remote write:
fixed-length needs `u16_length == u16_dataLen`; variable-length needs `u16_length <= u16_dataLen`
and then sets `u16_actualLen = u16_length` (it **can shrink**, unlike a remote write).

On a length violation it logs `APP_LOG_ERR` and returns without writing — **the caller gets no
error indication**. Size the call correctly; it does not notify the peer either.

#### `void gt_GATT_GetActualLen(const GATTCharDescriptor_T *stpt_desc, uint16_t *u16pt_actualLen)`

Stores the current `u16_actualLen` in `*u16pt_actualLen`.

---

## 5. Threading contract

- The stack callbacks run in the BT RX context. With a mutex configured they block on it with
  `K_FOREVER`, so application code must hold the mutex only briefly and never while waiting on
  the BT stack.
- Hooks run **without** the mutex. A hook that touches `vpt_data` beyond the bytes in `vpt_buf`
  must lock it itself; the mutex is a `k_mutex`, so re-locking from the same thread is safe.
- With `stpt_mutex = NULL`, only the BT stack may touch the value. Using the local API from an
  application thread then races the stack.

---

## 6. Behaviour notes and known limitations

For anyone using or modifying the library:

- **Remote variable-length writes never shrink `u16_actualLen`.** Writing 10 bytes and then
  4 bytes at offset 0 leaves `u16_actualLen = 10`, so a read returns the 4 new bytes followed
  by 6 stale ones. Where a shorter value must replace a longer one, reset the length in the
  write hook (e.g. set `u16_actualLen = u16_offset + u16_length` under the mutex) or have the
  application rewrite it with `gv_GATT_LocalWrite`.
- **Offset gaps are not zero-filled.** A variable-length write at an offset past
  `u16_actualLen` extends the length over whatever bytes were already in the buffer.
- **Fixed-length values cannot be written in pieces.** Every write must end exactly at
  `u16_dataLen`, so a long write (Prepare / Execute) split into several offsets is rejected.
  Make such a characteristic variable-length.
- **Prepare-write flag is ignored.** If the attribute has `BT_GATT_PERM_PREPARE_WRITE`, Zephyr
  also calls the write callback during the Prepare phase (with `BT_GATT_WRITE_FLAG_PREPARE`),
  and this callback copies the data then. Leave that permission off.
- **A read hook cannot force a zero-byte result**, because `0` means "keep the generic result".
- **Write hook rejection has no rollback** (§2).
- **`gv_GATT_LocalWrite` fails silently** as seen by the caller (§4).
- **Naming:** `gt_GATT_LocalRead` and `gt_GATT_GetActualLen` return `void` despite the `gt_`
  (ssize_t) prefix. Renaming them to `gv_` means updating every caller.
- **Notifications and indications are not part of this library.** Send them with
  `bt_gatt_notify` / `bt_gatt_indicate` from the service code or a write hook.
