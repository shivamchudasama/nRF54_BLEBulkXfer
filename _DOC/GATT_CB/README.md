# GATT_CB — generic GATT read/write callbacks (Zephyr / nRF54)

GATT_CB lets a GATT service be written as a table of data instead of a set of callbacks.
Every characteristic registers the **same two** Zephyr callbacks, `gt_GATT_GenericRead` and
`gt_GATT_GenericWrite`, and passes a per-characteristic **descriptor** as `user_data`. The
descriptor says where the value lives, how long it may be, which mutex guards it, and which
hooks (if any) run after a read or a write. Application threads reach the same value through
a small local API, so the BT stack and the application share one copy.

- **One implementation of the ATT rules:** offset handling, length checks and error codes are
  written once and tested once, not per characteristic.
- **Values are plain variables:** a `uint8_t`, a struct or a buffer; the library copies bytes
  and never interprets them.
- **Hooks only where needed:** a characteristic with behaviour (validate, feed a state
  machine, notify) adds a write or read hook; everything else is a descriptor with `NULL` hooks.
- **Optional locking:** a `k_mutex` per descriptor when an application thread shares the
  value, none when only the stack touches it.

The exact API contract is in [API_REFERENCE.md](API_REFERENCE.md).

## Structure

| File | Role |
|---|---|
| [GATT_CB_Types.h](../../_LIB/GATT_CB/GATT_CB_Types.h) | `GATTCharDescriptor_T` and the hook types `GATTCustomReadCb_F` / `GATTCustomWriteCb_F` |
| [GATT_GenericCallbacks.h](../../_LIB/GATT_CB/GATT_GenericCallbacks.h) | Public API; includes `GATT_CB_Types.h`, so it is the only header applications include |
| [GATT_GenericCallbacks.c](../../_LIB/GATT_CB/GATT_GenericCallbacks.c) | Stack callbacks and the local API |
| [CMakeLists.txt](../../_LIB/GATT_CB/CMakeLists.txt) | Globs `*.c` into `app` and puts the folder on the include path |

The public API has two halves:

| Caller | Functions | Hooks run? |
|---|---|---|
| BT stack (registered in `BT_GATT_CHARACTERISTIC`) | `gt_GATT_GenericRead`, `gt_GATT_GenericWrite` | Yes, after the copy |
| Application threads | `gt_GATT_LocalRead`, `gv_GATT_LocalWrite`, `gt_GATT_GetActualLen` | No |

```
 peer ──ATT read/write──▶ Zephyr GATT ──▶ gt_GATT_Generic{Read,Write}(attr, …)
                                              │  attr->user_data
                                              ▼
                                   GATTCharDescriptor_T ──▶ vpt_data (the value)
                                              ▲                    ▲
 app thread ──gt_GATT_LocalRead / gv_GATT_LocalWrite ──────────────┘ (under stpt_mutex)
```

## Design

**Why a descriptor and not per-characteristic callbacks.** Most characteristics are a value
the peer reads or writes. Hand-written callbacks repeat the same offset and length handling
each time, and small differences between them become bugs. With a descriptor the service file
is a list of variables and the rules live in one tested place.

**Fixed versus variable length.** A fixed-length value (`b_variableLength = false`) must be
written whole: every write must end exactly at `u16_dataLen`. A variable-length value accepts
any write that fits and tracks the valid length in `u16_actualLen`, which is what reads
return. Pick variable length for buffers, frames and anything written in pieces.

**Hooks run after the copy, without the mutex.** A write hook sees the value already stored,
so it can hand it straight to other code and may take its own locks without deadlocking the
library. The price is that a hook that rejects a value must restore the old one itself: there
is no rollback. The local API never calls hooks, so the application can change a value
without triggering the behaviour meant for the peer.

**What it does not do.** Notifications and indications, CCC descriptors and service
definition stay with the service code (`bt_gatt_notify`, `BT_GATT_CCC`,
`BT_GATT_SERVICE_DEFINE`). The library has no Kconfig and no state of its own besides the
descriptors the application owns.

The behaviour details that matter when using or changing the library (remote writes never
shrink the length, offset gaps are not zero-filled, the prepare-write flag is ignored,
`gv_GATT_LocalWrite` fails silently) are listed in
[API_REFERENCE.md §6](API_REFERENCE.md#6-behaviour-notes-and-known-limitations).

## Integration

1. Add the library to the build. [`_LIB/CMakeLists.txt`](../../_LIB/CMakeLists.txt) adds
   `GATT_CB` before `BulkXfer`, which builds on it.
2. In the service `.c` file, define one `static` (non-`const`) `GATTCharDescriptor_T` per
   characteristic next to its value. For a fixed-length value set
   `u16_actualLen = u16_dataLen`; for a variable-length one set it to `0` or the preloaded length.
3. Register `gt_GATT_GenericRead` and/or `gt_GATT_GenericWrite` with the descriptor as
   `user_data`; pass `NULL` for the unused direction.
4. Add a hook only for behaviour beyond storing the value, and a mutex only if an application
   thread uses the local API on that value.

[API_REFERENCE.md §1](API_REFERENCE.md#1-quick-start) has a complete example. In this project,
[BulkSvc.c](../../_ASW/_BLK_SVC/BulkSvc.c) uses it for the BulkXfer service:

| Characteristic | Descriptor | Why |
|---|---|---|
| DATA (Write / Write Without Response) | variable length up to one frame, no mutex, write hook `st_OnBulkDataWrite` | Each written frame goes to the BulkXfer Server engine in the BLE RX context |
| Caps (Read) | fixed length `BlkCaps_T`, no mutex, no hooks | Filled once with `gv_GATT_LocalWrite` before advertising, then served by `gt_GATT_GenericRead` |

CTRL is notify-only, so it does not use the library.

## Dependencies

- Zephyr kernel (`k_mutex`, `__ASSERT`) and `zephyr/bluetooth/gatt.h`.
- [`AppLog`](../../_ASW/_APP_LOG) (`AppLog.h`) for error logging.

## Tests

| Path | What |
|---|---|
| [`_TEST/unit/GATT_CB/test_gatt_cb.c`](../../_TEST/unit/GATT_CB/test_gatt_cb.c) | Unity tests written against [API_REFERENCE.md](API_REFERENCE.md): reads with offsets and hooks, fixed and variable-length writes and their bounds, hook ordering and veto without rollback, the local API, and asserts on missing descriptors |

The public headers are also compiled on their own by the header check. Both run locally and
in CI (`host-tests`, with ASan + UBSan); see [`_TEST/README.md`](../../_TEST/README.md).

```bash
powershell -ExecutionPolicy Bypass -File _TEST/run_tests.ps1
```
