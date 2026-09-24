# CLAUDE.md

## Routine

After a **significant change** — one that makes something in this file wrong or incomplete (files added/renamed/reorganized, a new convention adopted, git/LFS setup changed) — say which section is affected and ask:

> Would you like to modify `CLAUDE.md` accordingly?

Do not ask on turns that change nothing here (questions, inspection, small edits that fit existing conventions). Keep this file lean — only what's needed to understand the repo.

## Repository nature

**Zephyr / nRF Connect SDK firmware** for a BLE peripheral intended for bulk data transfer. At present it brings up the BLE stack, advertises, and exposes the GAP and Device Information GATT services. The bulk-transfer protocol is the `_LIB/BulkXfer` library; `_ASW` does not call it yet.

- SDK: **NCS v3.4.1** (Zephyr 4.4.2), toolchain `C:\ncs\toolchains\4f5b6ad6dd`.
- Board: **`nrf54l15dk/nrf54l15/cpuapp`**.
- Built from VS Code's nRF Connect extension (sysbuild) into `build/`. After a layout or config change, do a **pristine** build — a stale `build/` keeps cached paths.
- No tests, no CI. "It compiles" means a clean build for the board above.

Source files follow the BATL coding guidelines in `_DOC/BATL Coding Guidelines/` (file banner, section headers, Hungarian-style prefixes such as `gv_`, `su8ar_`, `scar_`). `Sample_Format.c`/`.h` there are the templates for new files. `_LIB` sources (and `AppLog.h`) use the same layout but carry an MIT licence header (`SPDX-License-Identifier: MIT`) instead of the BATL copyright.

## Layout

- `CMakeLists.txt` — sets `APPLICATION_CONFIG_DIR` to `_DI`, then pulls in `_ASW` and `_LIB`.
- `_ASW/` — application software. `main.c` plus one folder per module:
  - `_BLE/` — stack init, advertising, connection callbacks.
  - `_GAP/` — Device Information GATT service. GAP itself is Zephyr's built-in service (`CONFIG_BT_GAP_SVC`); device name and appearance are set by `CONFIG_BT_DEVICE_NAME`/`CONFIG_BT_DEVICE_APPEARANCE` in `prj.conf`. Never define a second GAP service — the host allows exactly one, and `bt_enable()` fails with `-EINVAL`.
  - `_BLE_GENERIX/` — Bluetooth SIG UUID and appearance tables (headers only).
  - `_APP_LOG/` — `APP_LOG` logging module; `AppLog.h` has `APP_LOG_ERR/WRN/INF/DBG`, which prefix `__func__`.
  - `_GENERIX/` — generic helpers.
- `_DI/` — build configuration. `prj.conf` lives here, not at the root.
- `_LIB/` — reusable libraries. Its `CMakeLists.txt` sets the BulkXfer roles for the whole build (`BLK_ENABLE_SERVER=1`, `BLK_ENABLE_CLIENT=0` — this device only receives), then adds `GATT_CB` before `BulkXfer`.
  - `GATT_CB/` — generic GATT read/write callbacks driven by a per-characteristic descriptor (`GATT_CB_Types.h`), plus a local read/write API for application threads. Its docs live in `_DOC/GATT_CB/`.
  - `BulkXfer/` — bulk transfer over GATT: sender is the GATT client (Write Without Response to DATA), receiver hosts DATA + CTRL and answers with ACK/NACK/END notifications. Windowed ACKs, Go-Back-N, CRC-32 per object (needs `CONFIG_CRC`). `BulkXfer.h` is the umbrella header; tunables in `BulkXfer_Config.h`. Its docs live in `_DOC/BulkXfer/`.
- `_DOC/` — coding guidelines, SIG UUID YAML sources, Zephyr notes. Not built.
  - `BulkXfer/` — `README.md` (design rationale, protocol walkthrough, integration steps) and `API_REFERENCE.md` (exact API contract). Links point back into `_LIB/BulkXfer/`.
  - `GATT_CB/` — `API_REFERENCE.md` (descriptor fields, callback and hook contract, threading, known limitations).

**Adding a module:** create `_ASW/_NAME/` with a `CMakeLists.txt` copied from a sibling (it globs `*.c` into `app` and adds its folder to the include path), then `add_subdirectory(_NAME)` in `_ASW/CMakeLists.txt`. Every module folder is on the include path, so headers are included by bare name. `_LIB` libraries use the same `CMakeLists.txt` pattern.

**Adding a library:** every `_LIB` library needs an API reference manual at `_DOC/<LIB>/API_REFERENCE.md` — the exact contract of its public headers (types, functions, return/error values, threading, known limitations), so it can be used and modified without reading the source. Add it with the library, and update it whenever the public API or behaviour changes. `_DOC/BulkXfer/API_REFERENCE.md` and `_DOC/GATT_CB/API_REFERENCE.md` are the models.

## Git

Remote `github.com/shivamchudasama/nRF54_BLEBulkXfer`, default branch `main`.

- **Git LFS** via `.gitattributes`: `*.pdf`, `*.docx`. A new binary extension needs an entry **before** its first commit.
- `.gitignore` excludes `build*/`, `/.vscode/`, `workspace.code-workspace` and `.codex/config.toml` (local, machine-specific).
