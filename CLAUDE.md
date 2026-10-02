# CLAUDE.md

## Routine

Every change updates, in the same change, each Markdown file that describes what it touched: the unit's `README.md`, `API_REFERENCE.md` and `PROTOCOL.md`, `_TEST/README.md`, `_TOOLS/BleHostGUI/README.md`, and this file. A change is not done while any of them is stale. When this file is updated, say which sections changed.

Do not touch docs on turns that change nothing they describe (questions, inspection). Keep this file lean — only what's needed to understand the repo.

## Repository nature

**Zephyr / nRF Connect SDK firmware** for a BLE peripheral that receives an Intel HEX upload over BLE and can be provisioned with a device certificate. It exposes GAP, Device Information and the BulkXfer service, and runs both `_LIB/BulkXfer` roles. Two users share the Server through an appType router:

- **Hex upload.** A PC client (the GUI in `_TOOLS/BleHostGUI`, or `bulkxfer_client.py hex`) sends each contiguous segment as one transfer, `[u32 LE address][data]` with appType `0x10`. The segment is buffered in RAM and logged on the UART, which is the only data store for now — nothing is written to flash. Contract: `_DOC/HexUpload/PROTOCOL.md`.
- **Device provisioning** (phase 1 of certificate-based authentication and pairing, `_DOC/CBAP`), appTypes `0x20`–`0x2F`. The device keeps a persistent P-256 key and CSR. The PC acts as the CA: it fetches the CSR (the device sends it with its BulkXfer Client to a BulkXfer service the PC hosts) and returns the CA and device certificates. The device holds the verified CA in RAM; once the device certificate verifies against it, it stores both in PSA ITS, deletes the CSR and logs both as PEM. Stored certificates are re-verified at boot. Provisioning is one-time: a wipe (DEPROVISION `0x27`, or DK Button 0 held 5 s) destroys key, CSR and certificates and makes a fresh key and CSR. Contract: `_DOC/Provisioning/PROTOCOL.md`; design and limitations: `_DOC/Provisioning/README.md`.

- SDK: **NCS v3.4.1** (Zephyr 4.4.2), toolchain `C:\ncs\toolchains\4f5b6ad6dd`.
- Board: **`nrf54l15dk/nrf54l15/cpuapp`**.
- Built from VS Code's nRF Connect extension (sysbuild) into `build/`. After a layout or config change, do a **pristine** build — a stale `build/` keeps cached paths.
- Command-line build: use PowerShell with the toolchain's `environment.json` variables (PATH, PYTHONPATH, `ZEPHYR_TOOLCHAIN_VARIANT`, `ZEPHYR_SDK_INSTALL_DIR`) plus `ZEPHYR_BASE=C:\ncs\v3.4.1\zephyr`, and run `west build` **from the repo directory**. Git Bash breaks the toolchain's Python (`ctypes` error), and running from `C:\ncs` fails because the source is on another drive. Use a separate `-d build_<name>` so VS Code's `build/` is left alone.
- Tests: host tests in `_TEST/` (C with CMake + CTest + Unity against a Zephyr shim, Python with pytest), run locally with `_TEST/run_tests.ps1` (report in `build_test/report/`) and in CI by `.github/workflows/ci.yml`, which also builds the firmware in Nordic's toolchain container. "Done" means the tests pass and the board above builds cleanly. Local host gcc is Strawberry's 4.4, so test code stays gnu99 and avoids newer GCC features. It has no sanitizers and misses warnings that CI's GCC 13 (64-bit Linux, `-Werror`, ASan/UBSan) reports, such as `%d` for a `ssize_t` or a one-byte overread, so a change is done only once CI passes too.

Source files follow the BATL coding guidelines in `_DOC/BATL Coding Guidelines/` (file banner, section headers, Hungarian-style prefixes such as `gv_`, `su8ar_`, `scar_`). `Sample_Format.c`/`.h` there are the templates for new files. `_LIB` sources (and `AppLog.h`) use the same layout but carry an MIT licence header (`SPDX-License-Identifier: MIT`) instead of the BATL copyright. The Python tools in `_TOOLS` are not covered by BATL; they follow ordinary PEP 8 style.

## Layout

- `CMakeLists.txt` — sets `KCONFIG_ROOT` to `_DI/Kconfig` and defaults `CONF_FILE` to `_DI/prj.conf` and `DTC_OVERLAY_FILE` to `_DI/boards/<board>.overlay`, then pulls in `_ASW` and `_LIB`. Never set `APPLICATION_CONFIG_DIR`: Zephyr resolves a relative `CONF_FILE` against it, so the extension's `_DI/prj.conf` would become `_DI/_DI/prj.conf`.
- `_ASW/` — application software. `main.c` plus one folder per module:
  - `_BLE/` — stack init, advertising, connection callbacks, and PHY/DLE/MTU negotiation (so BulkXfer's `b_autoTuneLink` is off), then a 7.5–15 ms connection-interval request (Zephyr sends it 5 s after connecting; `CONFIG_BT_GAP_AUTO_UPDATE_CONN_PARAMS` stays `n`). The interval is what sets upload throughput; Windows ignores the peripheral's request and settles on ~45 ms, so the GUI requests 15 ms itself. Forwards connect/disconnect to BulkXfer, and advertises the BulkXfer service UUID (the name is in the scan response).
  - `_BLK_SVC/` — the BulkXfer GATT service (DATA, CTRL + CCC, CAPS) built on `GATT_CB`. `gstpt_BulkSvc_Init()` returns the CTRL attribute. `BulkRouter.c` owns `gi_BLKS_Init()`: modules register an appType range with `gi_BulkRouter_Register()`, then `gi_BulkRouter_Start()` starts the Server and dispatches each callback by appType. Unregistered types are rejected.
  - `_DATA_STORE/` — hex upload (`0x10`): receive callbacks, the 64 KiB segment buffer (`DS_BUF_SIZE`) and a low-priority dump thread that logs each verified segment: with `CONFIG_DS_HEX_DUMP=n` (currently set in `prj.conf`), one `SEG addr= len= crc=` line; with `CONFIG_DS_HEX_DUMP=y` (debugging; several seconds per 64 KiB), every byte as `0xADDR: xx …` lines paced by `log_buffered_cnt()` so the deferred log never drops lines. Sends RESULT/STORED short messages. `gi_DataStore_Init()` registers its range.
  - `_CSR/` — from the reference `Sample Code/_ASW`, ported to NCS 3.4. `gt_InitCryptoStorage()` (HUK, settings, PSA, once); `gv_GenerateOrLoadCSR()` generates (first boot or after a wipe) or reuses the persistent PSA key `CSR_DEVICE_SIGNING_KEY_ID` (`0x0001`) and builds or restores the CSR (`gst_CSRData`, kept in PSA ITS until provisioned); `gt_RemoveStoredCSR()`, `gt_DestroyDeviceCredentials()` (key + CSR), `gt_ProbeDeviceKey()`. `DER.c` is the minimal CSR encoder (Zlib notice kept). CN is an RFC 4122 UUIDv5 of the hardware ID; the other subject fields come from Kconfig `CSR_SUBJ_*`.
  - `_DEVICE_CERT/` — certificate buffers (`gst_CACertData`, `gst_deviceCertData`) and their ITS storage (`DeviceCert.c`: one entry each, UID `"DEVCERT"`‖3 for the CA and ‖1 for the device; `gt_StoreCACert/DeviceCert()`, `gt_LoadStoredCerts()` returns the pair or nothing, `gt_RemoveStoredCerts()`, `gv_ClearCertData()`). `DeviceCert_Verify.c` holds the mbedTLS X.509 + PSA checks: `ge_VerifyCACertificate()`, `ge_VerifyOwnDeviceCertificate()` (with the CSR subject), `ge_VerifyStoredDeviceCertificate()` (boot, without it), and `ge_VerifyRemoteDeviceCertificate()` for phase 2. They follow the BG22/BG24 reference in `Sample Code Device Cert Verification`, with its defects fixed. Links `mbedtls_external`.
  - `_PROV/` — the provisioning protocol (`0x20`–`0x2F`): router callbacks (check, copy to a staging buffer, post), a low-priority thread (8 KiB stack) for verification, storage and the wipe, the BulkXfer Client attach + CSR send, STATUS/RESULT and PEM logging. It owns no cryptography. `gi_Prov_Init()` restores a stored pair (re-verified; wiped if invalid) or calls `gv_GenerateOrLoadCSR()`. `ProvButton.c` (NCS DK library) turns a held Button 0 into `gv_Prov_RequestWipe()`; it is a separate file with its own test (`prov_button`, DK-library shim).
  - `main()` order: `gi_DataStore_Init()`, `gi_Prov_Init()`, `gi_ProvButton_Init()`, `gi_BulkRouter_Start()`, `gv_BLEInitStartAdv()`.
  - `_GAP/` — Device Information GATT service. GAP itself is Zephyr's built-in service (`CONFIG_BT_GAP_SVC`); device name and appearance are set by `CONFIG_BT_DEVICE_NAME`/`CONFIG_BT_DEVICE_APPEARANCE` in `prj.conf`. Never define a second GAP service — the host allows exactly one, and `bt_enable()` fails with `-EINVAL`.
  - `_BLE_GENERIX/` — Bluetooth SIG UUID and appearance tables (headers only).
  - `_APP_LOG/` — `APP_LOG` logging module; `AppLog.h` has `APP_LOG_ERR/WRN/INF/DBG`, which prefix `__func__`.
  - `_GENERIX/` — generic helpers.
- `_DI/` — build configuration. `prj.conf` lives here, not at the root. Its "Device certificate" section enables:
  - PSA on CRACEN (ECDSA P-256, SHA-256/SHA-1) and persistent keys;
  - Zephyr secure storage as the ITS: settings on ZMS, AES-GCM with the hardware unique key, 64-bit UIDs, 1100-byte entries;
  - Zephyr's Mbed TLS for X.509 (`CONFIG_MBEDTLS`; the X.509 library itself has no prompt and comes with PSA ECDH);
  - an 8 KiB mbedTLS heap, `BASE64`, `HWINFO`, and an 8 KiB main stack;
  - the DK library (`CONFIG_DK_LIBRARY`) for the wipe button.

  A chip erase gives the device a new key.
  - `Kconfig` — application options (menu "Application": `CONFIG_DS_HEX_DUMP`, `CONFIG_PROV_WIPE_HOLD_MS`, and the CSR subject `CONFIG_CSR_SUBJ_C/ST/L/O/OU`), then `source "Kconfig.zephyr"`. New app-level switches go here and get their value in `prj.conf`.
  - `boards/nrf54l15dk_nrf54l15_cpuapp.overlay` — devicetree overlay, picked up automatically for the board. Sets the console/log UART (`uart20`) to 921600 baud with RTS/CTS flow control, so the serial terminal must match.
- `_LIB/` — reusable libraries. Its `CMakeLists.txt` sets the BulkXfer roles for the whole build: both roles, with `BLK_CLI_WRITE_INFLIGHT_MAX=3` so that Server + Client credits stay below `CONFIG_BT_ATT_TX_COUNT=6` (a `BUILD_ASSERT`). It then adds `GATT_CB` before `BulkXfer`.
  - `GATT_CB/` — generic GATT read/write callbacks driven by a per-characteristic descriptor (`GATT_CB_Types.h`), plus a local read/write API for application threads. Its docs live in `_DOC/GATT_CB/`.
  - `BulkXfer/` — bulk transfer over GATT: sender is the GATT client (Write Without Response to DATA), receiver hosts DATA + CTRL and answers with ACK/NACK/END notifications. Windowed ACKs, Go-Back-N, CRC-32 per object (needs `CONFIG_CRC`). `BulkXfer.h` is the umbrella header; tunables in `BulkXfer_Config.h`. Its docs live in `_DOC/BulkXfer/`, its tests in `_TEST/unit/BulkXfer/`.
- `_TOOLS/` — PC-side tools, not built into the firmware.
  - `BleHostGUI/` — Tkinter + `bleak` GUI, the BulkXfer Client, with its own flat light/dark ttk theme (on `clam`) and Pillow-drawn icons. A navigation rail shows one page at a time: Device (scan/connect, read CAPS), hex upload, device provisioning (the PC is the CA), Log and BLE traffic (a Capture switch). Not sv-ttk: its image-drawn elements made every redraw take hundreds of ms on Windows; don't reintroduce stretched image elements.
    - `ble_host_gui.py` is the entry point; its `FEATURES` list names the feature pages (rail order), and it publishes the PC's GATT service at start-up.
    - `bulkxfer_client.py` beside it is the BulkXfer reference client: protocol code plus a command line with `send`/`ping`/`caps`/`hex`/`provision [--negative]`/`deprovision`.
    - `bulkxfer_receiver.py` is the receiver role.
    - Package `blehost/`:
      - `core/`: asyncio runner thread, event bus, and `BleLink`, the only bleak user. `BleLink` also captures traffic and, on Windows 11, holds a throughput-optimized (15 ms) connection-parameter request for the life of the link through bleak's private `_backend._requester`. `gatt_server.py` (`PcGattServer`) hosts the PC's own BulkXfer service through WinRT's `GattServiceProvider` (not `bless`: its winrt pins conflict with bleak ≥ 1), so the device can send to the PC.
      - `protocols/`: codecs; `bulkxfer.py` imports `bulkxfer_client.py` rather than duplicating it; `provisioning.py` holds `ProvisioningSession`.
      - `pki/`: the CA (`authority.py`) and certificates the device must reject (`negative.py`).
      - `features/`: one `Feature` subclass per page (`title`, rail `icon`). A page is built when first shown; `on_connected`/`on_disconnected` reach only built ones. Feature messages go to `ctx.log()` (Log page and status line), not a log of their own.
      - `ui/`: `main_window.py` (rail, pages, header, status line, Log page), `connection_panel.py` (Device page), `traffic_view.py` (Traffic page); `theme.py` (panels get it with `theme.of(ctx)`; classic Tk widgets recolour through `on_change`; pixel sizes go through `px()`/`sp()`), `widgets.py` (cards, pills, tiles, step list, `Disclosure`, `MoreMenu`, tooltips, `set_icon`, `set_var`) and `icons.py` (Pillow: smooth shapes, check box and switch, Segoe Fluent Icons glyphs, the window icon; text-only without them). Pills, tiles and the step list only display what the feature logic's variables already hold, and redraw only when it changes: a timer writes through `set_var`.
    - New capabilities (pairing, encryption) are added as new features; the recipe is in its `README.md`. Never touch Tk from a coroutine — use `ctx.bus.call()`.
- `_TEST/` — host tests, not built into the firmware; `README.md` explains running, the report and adding tests.
  - `CMakeLists.txt` — fetches Unity and Mbed TLS 4.1.1 (both pinned by hash) and registers each test with `add_unit_test()`. `SANITIZE`/`COVERAGE` options. Mbed TLS is linked only by `devicecert_verify`.
  - `shim/` — single-threaded stand-in for the Zephyr kernel, logging and BT APIs (`zephyr_shim.h`, `zephyr_sim.c`: simulated time, log capture, `SIM_EXPECT_ASSERT()`, `gv_SimRunThread()`, `k_msgq`, `k_work_delayable`, base64), plus declarations-only NCS headers (DK buttons, HUK, settings). `shim/psa/` declares the PSA ITS API. `shim/psa_stub/` declares PSA Crypto for tests that script it (add `${PSA_STUB_DIR}` to their includes); `shim/tfpsa/` is the host platform (user config, in-memory ITS, test RNG) for the real TF-PSA-Crypto. Extend the shim with the real names and contracts when a new unit needs more.
  - `unit/<Unit>/` — Unity tests. They `#include` the `.c` under test to reach static state. `unit/BulkXfer/sim_link.h` is the simulated link and scripted peer, shared by the engine test and the hex-upload and provisioning end-to-end tests. Tests that `#include` several `.c` files need distinct names for static functions.
  - `python/` — pytest suite for `_TOOLS`; `conftest.py` has a fake GATT link, a scripted server and `tk_root`, one Tk root shared by the run (several Tk interpreters in one process fail now and then on Windows). GUI pages are tested against stand-in Tk variables plus one real-Tk test (CI runs pytest under `xvfb-run`); bleak and winrt are replaced by fake modules.
  - `tools/gen_cert_vectors.py` — runs on each fresh build: the PC's CA (`blehost/pki`) issues the certificates and `negative.py` the rejected cases that `devicecert_verify` checks on the real Mbed TLS, so the device is tested against what the PC sends.
  - `vectors/wire.json` — golden wire bytes (from `_LOG/`) checked by **both** the C and Python tests; change it first when the wire format changes. `provisioning.csr` is a CSR from the device's `DER.c` with a real signature; after changing `DER.c`, regenerate it with `tools/gen_csr_vector.py`.
  - Known open bugs are documented as tests: strict `xfail` (Python), `known_header_issues.txt` (header check), `TEST_IGNORE_MESSAGE("KNOWN BUG: …")` after a check that fails once fixed (Unity). They show in the report and fail once fixed.
- `.github/workflows/ci.yml` — jobs `host-tests` (ASan/UBSan + gcovr), `python-tests` (under `xvfb-run`), `firmware-build`, `report` (merged JUnit summary), and a manual `hil-tests` placeholder for a self-hosted runner with a DK (hex upload, then `provision --negative`, the negative set `devicecert_verify` also runs on the host, here on the device's own crypto, then a refused second provisioning, `deprovision` and a new provisioning).
- `_LOG/` — sample logs of one upload: `BulkXfer_GUI_Client.txt` (the GUI's traffic monitor) and `BulkXfer_Device_Server.txt` (the board's UART). `AA00000100.hex` at the root is the test file they were made with.
- `_DOC/` — coding guidelines, SIG UUID YAML sources, Zephyr notes, and one folder per `_LIB` library (see *Library, tool and module requirements*). Not built.
  - `BulkXfer/` — `PROTOCOL.md` (the wire contract, independent of the code: GATT service, frame layouts, CRC, state machines, timing, errors, examples), `README.md` (design rationale, integration steps) and `API_REFERENCE.md` (exact API contract). Links point back into `_LIB/BulkXfer/`. Update `PROTOCOL.md` whenever the frame format or either role's on-wire behaviour changes.
  - `GATT_CB/` — `README.md` (structure, design, integration) and `API_REFERENCE.md` (descriptor fields, callback and hook contract, threading, known limitations).
  - `HexUpload/` — `PROTOCOL.md`, the contract for the upload client: GATT table, appTypes (and the device's whole appType registry), segment format, sequence, rejection rules. Update it whenever `_BLK_SVC` or `_DATA_STORE` changes behaviour.
  - `Provisioning/` — `PROTOCOL.md` (roles, both BulkXfer services, appTypes `0x20`–`0x2F`, STATUS/RESULT, states, sequence, rejections, certificate profile, serial output) and `README.md` (design, provenance of the sample code and its fixes, integration, testing, limitations, storage and phase-2 hooks). Update them whenever `_PROV`, `_CSR`, `_DEVICE_CERT` or the PC provisioning code changes behaviour.
  - `CBAP/` — Silicon Labs application notes AN1396 (certificate-based authentication and pairing) and AN1268 (device certificates), the source of the design.

- `Supporting Scripts/` — third-party Silicon Labs reference scripts (Zlib licence) for certificate-based provisioning: `create_authority_certificate.py` (creates the root CA), `production_line_tool.py` (signs a device's CSR via Simplicity Commander), a Jinja template for the root-cert header, and `requirements.txt`. The PC CA (`blehost/pki`) mirrors them without Commander/NVM3. Not built, not tested, not BATL.
- `Sample Code/`, `Sample Code Device Cert Verification/` — the user's reference projects (Zephyr sample; Silicon Labs BG22/BG24 project). `_CSR` and `_DEVICE_CERT` are ported from them; `_DOC/Provisioning/README.md` lists what changed. Reference only: not built, not tested.

**Adding a module or library:** create `_ASW/_NAME/` (or `_LIB/<LIB>/`) with a `CMakeLists.txt` copied from a sibling (it globs `*.c` into `app` and adds its folder to the include path), then `add_subdirectory(_NAME)` in `_ASW/CMakeLists.txt` (or `_LIB/CMakeLists.txt`). Every module folder is on the include path, so headers are included by bare name. A module is not done until it has the tests below, and a library or tool until it also has the docs.

## Library, tool and module requirements

Every new unit ships with tests, added in the same change and kept current with it: each `_LIB` library, `_TOOLS` tool and `_ASW` module, and every new file in an existing one. Only code that does nothing but call the Zephyr BT stack (`_BLE`, `_GAP`, `BulkSvc.c`, `main.c`) is left to the firmware build and `hil-tests`. A library or tool also ships with its docs; an `_ASW` module needs none of its own, but the protocol documents it touches (`_DOC/HexUpload`, `_DOC/Provisioning`) are kept current.

- **Docs** (libraries and tools) in `_DOC/<Name>/` (for a `_TOOLS` tool, beside it, as `_TOOLS/BleHostGUI/README.md`):
  - `README.md` — purpose, overall structure (each file and its role), design rationale, how it fits with the other units, integration steps. Model: `_DOC/BulkXfer/README.md`.
  - `API_REFERENCE.md` — the exact contract of its public headers: types, functions, return/error values, threading, known limitations, so it can be used and modified without reading the source. Models: `_DOC/BulkXfer/API_REFERENCE.md`, `_DOC/GATT_CB/API_REFERENCE.md`.
  - `PROTOCOL.md` — when it defines anything exchanged with a peer: GATT table, message formats, sequences, rejection rules. Model: `_DOC/HexUpload/PROTOCOL.md`.
  - Other documents as the unit needs them (state machines, timing, memory budget).
- **Tests** (every unit), written against the API reference or protocol document and run both locally by `_TEST/run_tests.ps1` and on GitHub by `.github/workflows/ci.yml`:
  - C: Unity tests in `_TEST/unit/<Name>/`, registered with `add_unit_test()`; public headers added to `CHECKED_HEADERS` in `_TEST/CMakeLists.txt`. Cover the normal path, every documented error return, and boundaries.
  - Python: pytest in `_TEST/python/test_<name>.py`, GUI pages included.
  - Code that calls Zephyr/NCS APIs, PSA or a DK library is still tested: extend the shim, stub PSA with `shim/psa_stub`, record the calls. Code whose correctness is the cryptography itself (X.509, signatures) runs on the real Mbed TLS (`mbedx509`). Only behaviour that needs the radio, CRACEN or flash goes in `hil-tests`.
  - A defect found while writing a test is reported, and documented as a known-bug test until fixed.
  - Wire format changes go into `vectors/wire.json` first.
  - A test that needs a new tool, dependency or job is added to `run_tests.ps1` and `ci.yml` together, so local and GitHub runs stay the same.

## Git

Remote `github.com/shivamchudasama/nRF54_BLEBulkXfer`, default branch `main`.

- **Git LFS** via `.gitattributes`: `*.pdf`, `*.docx`. A new binary extension needs an entry **before** its first commit.
- `.gitignore` excludes `build*/`, `/.vscode/`, `workspace.code-workspace` and `.codex/config.toml` (local, machine-specific), plus `__pycache__/` and `.coverage`, and `_CA/` (a local CA folder: it holds the CA private key, never commit it). The test build (`build_test/`, including its Python venv) falls under `build*/`.
