# Tests

Host tests for the libraries in `_LIB`, the testable application modules in `_ASW`, and the PC tools in `_TOOLS`. They run on every push through [`.github/workflows/ci.yml`](../.github/workflows/ci.yml), and locally with one command. Each run produces a report that lists every test case, every failure, and the **known bugs**.

## Running

```powershell
powershell -ExecutionPolicy Bypass -File _TEST\run_tests.ps1            # all tests
powershell -ExecutionPolicy Bypass -File _TEST\run_tests.ps1 -Coverage  # + C and Python coverage
```

This needs a host `gcc` on PATH (MinGW-w64 or Strawberry Perl; gcc 4.4 and newer work) and Python 3.8+. CMake, CTest and Ninja are taken from PATH, or else from `C:\ncs\toolchains`. Python dependencies go into a venv in `build_test/venv`.

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
| C unit tests (CTest + [Unity](https://github.com/ThrowTheSwitch/Unity)) | yes | `host-tests`, with ASan + UBSan | `_LIB/BulkXfer`, `_LIB/GATT_CB`, `_ASW/_DATA_STORE`, `_ASW/_BLK_SVC` (router), `_ASW/_PROV`, `_ASW/_DEVICE_CERT` (certificate storage), `_ASW/_CSR` (DER encoder), `_ASW/_GENERIX`, header hygiene |
| Python tests (pytest) | yes | `python-tests` | `bulkxfer_client.py`, `bulkxfer_receiver.py`, `blehost/protocols`, `blehost/pki`, `blehost/core/gatt_server.py` (fake backend), `blehost/core/decoders.py` |
| Firmware build | no (VS Code) | `firmware-build`, in `ghcr.io/nrfconnect/sdk-nrf-toolchain` | The whole app for `nrf54l15dk/nrf54l15/cpuapp`. The ROM/RAM use goes in the job summary |
| Hardware in the loop | no | `hil-tests` (placeholder, manual) | Flash, upload `AA00000100.hex` over BLE, provision the board with `--negative` (the device's certificate verification), check the certificate with `openssl verify`, check that a second provisioning is refused, `deprovision` and provision again |

The BLE glue (`_BLE`, `_GAP`, `BulkSvc.c`, `main.c`) only calls the Zephyr stack, so it is covered by the firmware build rather than by unit tests. So is the code that needs PSA/mbedTLS on the device (`CSR_Generator.c`, `DeviceCert_Verify.c`) and the DK button (`ProvButton.c`): the shim has no cryptography or DK library, and `hil-tests` exercises them on the board. `DeviceCert.c` only calls the PSA ITS API, so `devicecert_store` tests it against an in-memory ITS.

## Layout

```
CMakeLists.txt         host test project; add_unit_test() registers one Unity executable
shim/                  single-threaded stand-in for the Zephyr kernel, logging and BT APIs
  zephyr_shim.h          simulated time, k_sem/k_fifo/k_msgq/k_timer/k_mutex, atomics, base64,
                         LOG_* capture, __ASSERT trap
  zephyr_sim.c           log capture, SIM_EXPECT_ASSERT(), gv_SimRunThread()
  psa/                   PSA Crypto / ITS types and declarations only (tests define the functions)
unit/BulkXfer/         test_frame.c, test_engine.c (built 3x: both / server / client roles)
  sim_link.h             simulated BLE link + scripted peer (other side of both roles)
unit/GATT_CB/          test_gatt_cb.c, against _DOC/GATT_CB/API_REFERENCE.md
unit/DataStore/        test_datastore.c (built with and without CONFIG_DS_HEX_DUMP),
                       test_upload_e2e.c: real Server engine + router + DataStore + simulated client
unit/BlkSvc/           test_router.c: appType router registration and dispatch
unit/Prov/             test_prov.c: provisioning flow, storage order, boot restore and the wipe,
                       everything around it stubbed (storage/key calls recorded in order);
                       test_prov_e2e.c: real BulkXfer (both roles) + router + Prov.c, peer = provisioner
unit/DeviceCert/       test_devicecert_store.c: certificate storage in DeviceCert.c, in-memory ITS
unit/CSR/              test_der.c: the CSR DER encoder against the real-signature CSR vector
unit/Helpers/          test_helpers.c
python/                pytest suite; conftest.py has a fake GATT link + scripted server
vectors/wire.json      golden wire vectors, checked by BOTH the C and the Python tests
known_header_issues.txt  headers with a known, reported problem (see below)
tools/                 gen_vectors.py, run_unity.py (Unity -> JUnit), check_headers.py, report.py,
                       gen_csr_vector.py (regenerates the CSR vector, below)
```

## Ideas the tests are built on

- **Written from the spec, not from the code.** DataStore tests follow `_DOC/HexUpload/PROTOCOL.md`, and GATT_CB tests follow `_DOC/GATT_CB/API_REFERENCE.md`. Comments name the section. When code and document disagree, the test makes that visible.
- **Real captures as golden data.** `vectors/wire.json` holds frames from `_LOG/`: the START of the real upload of `AA00000100.hex`, the END, RESULT and STORED frames, and the device's `SEG … crc=0xe2e72827` line. The C encoder, the DataStore, the end-to-end upload, `parse_ihex` and the Python client must all reproduce them byte for byte. The GUI traffic capture is replayed through the monitor's decoders.
- **One wire format, two implementations.** The C firmware and the Python client are checked against the same vectors, so a change on one side without the other fails CI.
- **The device's CSR, checked with the CA's library.** `wire.json` `provisioning.csr` is the CSR the device's `DER.c` builds, with a real ECDSA signature (the C test's signing stub returns it). `test_der.c` requires `DER.c` to produce it byte for byte; the Python tests require `cryptography` to parse and verify it, and the CA to issue a certificate whose subject and key bytes are what the device compares.
- **Private state stays reachable.** C tests `#include` the `.c` under test, as `test_engine.c` always did, so static state can be driven and inspected without changing production code.

## Known bugs

A test that documents a defect that is still open does not turn the pipeline red, but it is never hidden either:

- **Python:** `@pytest.mark.xfail(strict=True, reason="KNOWN BUG: …")`. Strict means the test fails the run once the bug is fixed, so the marker gets removed with the fix.
- **Headers:** an entry in `known_header_issues.txt`, with the same strict behaviour.
- **C (Unity):** if needed, `TEST_IGNORE_MESSAGE("KNOWN BUG: …")` at the top of the test.

The report lists all of them under *Known bugs*.

## Adding tests

- **A C module or library:**
  1. Create `unit/<Name>/test_<name>.c` with `setUp`/`tearDown`, `RUN_TEST(...)` in `main()` and `setvbuf(stdout, NULL, _IONBF, 0)` first.
  2. Register it in `CMakeLists.txt` with `add_unit_test(<name> SOURCES … INCLUDES …)`.
  3. If the code needs Zephyr APIs that the shim lacks, add them to `shim/zephyr_shim.h` (plus a forwarding header under `shim/zephyr/`), keeping Zephyr's names and contracts.
  4. Add the module's public headers to `CHECKED_HEADERS`.
- **Python:** add `python/test_<topic>.py`. The fixtures `server`, `client`, `vectors`, `repo` and `fast_timeouts` come from `conftest.py`. Python dependencies go in `requirements-test.txt`; `run_tests.ps1` installs it on every run, so a new one reaches an existing venv.
- **A wire-format change:** update `vectors/wire.json` first. Both suites then show where the C and Python sides are wrong.
- **A change to `_ASW/_CSR/DER.c`:** `csr_der` fails until the CSR vector is regenerated. After a test build (`run_tests.ps1`), run `python _TEST/tools/gen_csr_vector.py build_test` with cmake on PATH (or `CMAKE=<path>`): it makes a new key, rebuilds `csr_der`, signs the TBS it prints, and writes key, SKI, TBS, signature, CSR and the STATUS key hashes into `wire.json`.
