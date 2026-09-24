# CLAUDE.md

## Routine

After a **significant change** — one that makes something in this file wrong or incomplete (files added/renamed/reorganized, a new convention adopted, git/LFS setup changed) — say which section is affected and ask:

> Would you like to modify `CLAUDE.md` accordingly?

Do not ask on turns that change nothing here (questions, inspection, small edits that fit existing conventions). Keep this file lean — only what's needed to understand the repo.

## Repository nature

**Zephyr / nRF Connect SDK firmware** for a BLE peripheral intended for bulk data transfer. At present it brings up the BLE stack, advertises, and exposes GAP and Device Information GATT services; the bulk-transfer service itself is not written yet.

- SDK: **NCS v3.4.1** (Zephyr 4.4.2), toolchain `C:\ncs\toolchains\4f5b6ad6dd`.
- Board: **`nrf54l15dk/nrf54l15/cpuapp`**.
- Built from VS Code's nRF Connect extension (sysbuild) into `build/`. After a layout or config change, do a **pristine** build — a stale `build/` keeps cached paths.
- No tests, no CI. "It compiles" means a clean build for the board above.

Source files follow the BATL coding guidelines in `_DOC/BATL Coding Guidelines/` (file banner, section headers, Hungarian-style prefixes such as `gv_`, `su8ar_`, `scar_`). `Sample_Format.c`/`.h` there are the templates for new files.

## Layout

- `CMakeLists.txt` — sets `APPLICATION_CONFIG_DIR` to `_DI`, then pulls in `_ASW` and `_LIB`.
- `_ASW/` — application software. `main.c` plus one folder per module:
  - `_BLE/` — stack init, advertising, connection callbacks.
  - `_GAP/` — GAP and Device Information GATT services.
  - `_BLE_GENERIX/` — Bluetooth SIG UUID and appearance tables (headers only).
  - `_APP_LOG/` — `APP_LOG` logging module.
  - `_GENERIX/` — generic helpers.
- `_DI/` — build configuration. `prj.conf` lives here, not at the root.
- `_LIB/` — placeholder for libraries; its `CMakeLists.txt` is empty.
- `_DOC/` — coding guidelines, SIG UUID YAML sources, Zephyr notes. Not built.

**Adding a module:** create `_ASW/_NAME/` with a `CMakeLists.txt` copied from a sibling (it globs `*.c` into `app` and adds its folder to the include path), then `add_subdirectory(_NAME)` in `_ASW/CMakeLists.txt`. Every module folder is on the include path, so headers are included by bare name.

## Git

Remote `github.com/shivamchudasama/nRF54_BLEBulkXfer`, default branch `main`.

- **Git LFS** via `.gitattributes`: `*.pdf`, `*.docx`. A new binary extension needs an entry **before** its first commit.
- `.gitignore` excludes `build*/`, `/.vscode/`, `workspace.code-workspace` and `.codex/config.toml` (local, machine-specific).
