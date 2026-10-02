# Tests

Host tests for the libraries in `_LIB`, the testable application modules in `_ASW`, and the PC tools in `_TOOLS`. They run on every push through [`.github/workflows/ci.yml`](../.github/workflows/ci.yml), and locally with one command. Each run produces a report that lists every test case, every failure, and the **known bugs**.

## Running

```powershell
powershell -ExecutionPolicy Bypass -File _TEST\run_tests.ps1            # all tests
powershell -ExecutionPolicy Bypass -File _TEST\run_tests.ps1 -Coverage  # + C and Python coverage
```

This needs a host `gcc` on PATH (MinGW-w64 or Strawberry Perl; gcc 4.4 and newer work) and Python 3.8+. CMake, CTest and Ninja are taken from PATH, or else from `C:\ncs\toolchains`. Python dependencies go into a venv in `build_test/venv`. The first configure downloads Unity and Mbed TLS 4.1.1 (both pinned by hash) and builds Mbed TLS once.

A local pass is not the whole check. gcc 4.4 has no sanitizers and is 32-bit, so out-of-bounds reads and printf formats that are wrong only on 64-bit hosts (`size_t`, `ssize_t`) show up only in CI's `host-tests` job (GCC 13, ASan + UBSan, `-Werror`). When it fails there, the sanitizer report is in the job log and in the `report-c` artifact (`junit/<test>.xml`, as the failing test's output).

The output goes to `build_test/report/`:

| File | Content |
|---|---|
| `report.html` | Every suite and test case. Failures are expanded, with their messages |
| `summary.md` | The same as a Markdown table (also the GitHub job summary) |
| `junit/*.xml` | JUnit XML per suite, one `<testcase>` per Unity test / pytest item |
| `coverage/c/index.html`, `coverage/python/` | With `-Coverage` |

## What runs where

| Tier | Local | CI job | What it tests |
|---|---|---|---|
| C unit tests (CTest + [Unity](https://github.com/ThrowTheSwitch/Unity)) | yes | `host-tests`, with ASan + UBSan | `_LIB/BulkXfer`, `_LIB/GATT_CB`, `_ASW/_DATA_STORE`, `_ASW/_BLK_SVC` (router), `_ASW/_PROV` (flow and wipe button), `_ASW/_DEVICE_CERT` (storage, and X.509 verification on the real Mbed TLS), `_ASW/_CSR` (key and CSR life cycle, DER encoder), `_ASW/_GENERIX`, header hygiene |
| Python tests (pytest) | yes | `python-tests`, under `xvfb-run` | `bulkxfer_client.py` (protocol and command line), `bulkxfer_receiver.py`, `blehost/protocols`, `blehost/pki`, `blehost/core/gatt_server.py` (fake backend, and the WinRT backend against fake `winrt` modules), `blehost/core/decoders.py`, `blehost/features/provisioning.py` (the page's logic, and building the real Tk page), `blehost/ui` (the flat light/dark theme and its drawn check box and switch, one-pass switching, the display widgets and that they do not redraw unchanged state, the More menu, the hex upload's coalesced progress and segment count, the status mappings the pages colour by, the main window: pages built on first show, one mapped at a time, the status line, rail badges, traffic capture, and the window without Pillow or an icon font), `blehost/ui/icons.py` (the Pillow-drawn shapes and indicators, icon glyphs with and without a badge, the window icon, and their fallbacks without Pillow or an icon font) |
| Firmware build | no (VS Code) | `firmware-build`, in `ghcr.io/nrfconnect/sdk-nrf-toolchain` | The whole app for `nrf54l15dk/nrf54l15/cpuapp`. The ROM/RAM use goes in the job summary |
| Hardware in the loop | no | `hil-tests` (placeholder, manual) | Flash, upload `AA00000100.hex` over BLE, provision the board with `--negative` (the device's certificate verification), check the certificate with `openssl verify`, check that a second provisioning is refused, `deprovision` and provision again |

The BLE glue (`_BLE`, `_GAP`, `BulkSvc.c`, `main.c`) only calls the Zephyr stack, so it is covered by the firmware build rather than by unit tests. Everything else in `_ASW` has a host test:

- `DeviceCert_Verify.c` runs on the **real** Mbed TLS 4 / TF-PSA-Crypto (the major version NCS v3.4.1 ships), built for the host by `CMakeLists.txt`, against certificates made by the PC tool's own CA and negative set (see *Ideas* below). Nothing cryptographic is stubbed there.
- `CSR_Generator.c`, `DeviceCert.c` and `Prov.c` use the PSA stub (`shim/psa_stub`) and an in-memory ITS, so every PSA result can be scripted.
- `ProvButton.c` uses the DK-library shim and simulated time.

What only the board can show (CRACEN, the flash-backed ITS, the radio) is left to `hil-tests`.

## Layout

```
CMakeLists.txt         host test project; add_unit_test() registers one Unity executable
shim/                  single-threaded stand-in for the Zephyr kernel, logging and BT APIs
  zephyr_shim.h          simulated time, k_sem/k_fifo/k_msgq/k_timer/k_mutex, k_work_delayable,
                         atomics, base64, LOG_* / LOG_HEXDUMP_* capture, __ASSERT trap
  zephyr_sim.c           log capture, SIM_EXPECT_ASSERT(), gv_SimRunThread()
  zephyr/settings/, hw_unique_key.h, dk_buttons_and_leds.h
                         declarations only (tests define the functions)
  psa/                   PSA ITS types and declarations (Zephyr's size_t signatures)
  psa_stub/psa/          PSA Crypto types, constants and declarations only, with recording
                         key-attribute setters; on the include path of the tests that stub PSA
  tfpsa/                 host platform for the real TF-PSA-Crypto: tf_psa_crypto_host_config.h
                         (user config), tfpsa_host_platform.c (in-memory ITS, test RNG,
                         zeroize), include/psa/ (the ITS headers its key storage expects)
unit/BulkXfer/         test_frame.c, test_engine.c (built 3x: both / server / client roles)
  sim_link.h             simulated BLE link + scripted peer (other side of both roles)
unit/GATT_CB/          test_gatt_cb.c, against _DOC/GATT_CB/API_REFERENCE.md
unit/DataStore/        test_datastore.c (built with and without CONFIG_DS_HEX_DUMP),
                       test_upload_e2e.c: real Server engine + router + DataStore + simulated client
unit/BlkSvc/           test_router.c: appType router registration and dispatch
unit/Prov/             test_prov.c: provisioning flow, storage order, boot restore and the wipe,
                       everything around it stubbed (storage/key calls recorded in order);
                       test_prov_e2e.c: real BulkXfer (both roles) + router + Prov.c, peer = provisioner;
                       test_prov_button.c: the 5 s wipe button (DK library stubbed, simulated time)
unit/DeviceCert/       test_devicecert_store.c: certificate storage in DeviceCert.c, in-memory ITS;
                       test_devicecert_verify.c: DeviceCert_Verify.c on the real Mbed TLS, with
                       gen/cert_vectors.h (see "Ideas" below)
unit/CSR/              test_der.c: the CSR DER encoder against the real-signature CSR vector;
                       test_csr_generator.c: key and CSR life cycle (init, first boot, reuse,
                       repair, failures, wipe); its init failures run as four more CTest
                       entries (--init=...), since the init result is kept per process
unit/Helpers/          test_helpers.c
python/                pytest suite; conftest.py has a fake GATT link + scripted server and a shared Tk root
vectors/wire.json      golden wire vectors, checked by BOTH the C and the Python tests
known_header_issues.txt  headers with a known, reported problem (see below)
tools/                 gen_vectors.py, run_unity.py (Unity -> JUnit), check_headers.py, report.py,
                       gen_csr_vector.py (regenerates the CSR vector, below),
                       gen_cert_vectors.py (certificates for devicecert_verify, every fresh build)
```

## Ideas the tests are built on

- **Written from the spec, not from the code.** DataStore tests follow `_DOC/HexUpload/PROTOCOL.md`, and GATT_CB tests follow `_DOC/GATT_CB/API_REFERENCE.md`. Comments name the section. When code and document disagree, the test makes that visible.
- **Real captures as golden data.** `vectors/wire.json` holds frames from `_LOG/`: the START of the real upload of `AA00000100.hex`, the END, RESULT and STORED frames, and the device's `SEG … crc=0xe2e72827` line. The C encoder, the DataStore, the end-to-end upload, `parse_ihex` and the Python client must all reproduce them byte for byte. The GUI traffic capture is replayed through the monitor's decoders.
- **One wire format, two implementations.** The C firmware and the Python client are checked against the same vectors, so a change on one side without the other fails CI.
- **The device's CSR, checked with the CA's library.** `wire.json` `provisioning.csr` is the CSR the device's `DER.c` builds, with a real ECDSA signature (the C test's signing stub returns it). `test_der.c` requires `DER.c` to produce it byte for byte; the Python tests require `cryptography` to parse and verify it, and the CA to issue a certificate whose subject and key bytes are what the device compares.
- **The device verifies what the PC sends.** `tools/gen_cert_vectors.py` runs on each fresh build: it makes a device key and CSR, has the PC tool's CA (`blehost/pki/authority.py`) issue the device certificate, and takes every certificate the device must reject, with its expected RESULT status, from `blehost/pki/negative.py`. `devicecert_verify` imports that key as the persistent device key and runs the device's verification on all of them, so a change on either side that breaks the profile fails the host tests, not only a hardware run.
- **The real library where it matters.** Certificate parsing and verification are the security boundary, so their test links Mbed TLS itself. `shim/tfpsa/tf_psa_crypto_host_config.h` removes only what a host cannot provide (AES-NI and asm, the OS entropy source, the file ITS, calendar time, the OS zeroize call). Under ASan/LSan in CI, a certificate context left unfreed on any path fails the test.
- **Private state stays reachable.** C tests `#include` the `.c` under test, as `test_engine.c` always did, so static state can be driven and inspected without changing production code.

## Known bugs

A test that documents a defect that is still open does not turn the pipeline red, but it is never hidden either:

- **Python:** `@pytest.mark.xfail(strict=True, reason="KNOWN BUG: …")`. Strict means the test fails the run once the bug is fixed, so the marker gets removed with the fix.
- **Headers:** an entry in `known_header_issues.txt`, with the same strict behaviour.
- **C (Unity):** `TEST_IGNORE_MESSAGE("KNOWN BUG: …")`, after a check that fails the test once the bug is fixed (strict, as above).

The report lists all of them under *Known bugs*.

## Adding tests

- **A C module or library:**
  1. Create `unit/<Name>/test_<name>.c` with `setUp`/`tearDown`, `RUN_TEST(...)` in `main()` and `setvbuf(stdout, NULL, _IONBF, 0)` first.
  2. Register it in `CMakeLists.txt` with `add_unit_test(<name> SOURCES … INCLUDES …)`. Code that calls PSA Crypto adds `${PSA_STUB_DIR}` to `INCLUDES` and defines the PSA functions it reaches; code whose correctness *is* the cryptography links `mbedx509` instead, as `devicecert_verify` does.
  3. If the code needs Zephyr or NCS APIs that the shim lacks, add them to `shim/zephyr_shim.h` (plus a forwarding header under `shim/zephyr/`) or as a declarations-only header in `shim/`, keeping the real names and contracts.
  4. Add the module's public headers to `CHECKED_HEADERS`.
- **Python:** add `python/test_<topic>.py`. A GUI feature is tested like `test_feature_provisioning.py`: its logic against stand-in Tk variables and a fake context, plus one test that builds the real page (CI gives it a display through `xvfb-run`). A test that creates widgets takes the `tk_root` fixture, one Tk root shared by the whole run (on Windows, creating several Tk interpreters in one process now and then fails to find `tk.tcl`); it skips without a display and destroys the test's widgets afterwards. The fixtures `server`, `client`, `vectors`, `repo`, `fast_timeouts` and `tk_root` come from `conftest.py`. Python dependencies go in `requirements-test.txt`; `run_tests.ps1` installs it on every run, so a new one reaches an existing venv.
- **A wire-format change:** update `vectors/wire.json` first. Both suites then show where the C and Python sides are wrong.
- **A change to `_ASW/_CSR/DER.c`:** `csr_der` fails until the CSR vector is regenerated. After a test build (`run_tests.ps1`), run `python _TEST/tools/gen_csr_vector.py build_test` with cmake on PATH (or `CMAKE=<path>`): it makes a new key, rebuilds `csr_der`, signs the TBS it prints, and writes key, SKI, TBS, signature, CSR and the STATUS key hashes into `wire.json`.
