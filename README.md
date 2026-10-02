# nRF54 BLE Bulk Transfer

Zephyr / nRF Connect SDK firmware for a BLE peripheral on the **nRF54L15 DK**. It receives bulk data over GATT and can be provisioned with a device certificate. A PC GUI uploads to the device and provisions it.

## Status

- **Application (`_ASW`)**: advertises as `BLE Bulk Transfer` and exposes GAP, Device Information and the BulkXfer service. An appType router splits the service between two users:
  - **Hex upload** (`0x10`): receives an Intel HEX file one contiguous segment at a time, buffers each segment in RAM and logs it on the serial terminal. By default it logs one summary line with the segment's CRC-32; built with `CONFIG_DS_HEX_DUMP=y` (for debugging, much slower) it logs every byte as `0xADDRESS: xx xx …` lines. Nothing is written to flash yet.
  - **Device provisioning** (`0x20`–`0x2F`): phase 1 of certificate-based authentication and pairing (Silicon Labs AN1396/AN1268). The device keeps a persistent P-256 key and a CSR. The PC acts as the Certificate Authority: it fetches the CSR and returns a CA certificate and a device certificate. The device verifies both, stores both in PSA Internal Trusted Storage, deletes its CSR and logs both certificates as PEM. Stored certificates are re-verified at boot. Provisioning is one-time. A wipe (the DEPROVISION message, or holding DK Button 0 for 5 s) destroys the key, the CSR and the certificates, and the device makes a fresh key and CSR.
- **BulkXfer library (`_LIB/BulkXfer`)**: the transfer protocol itself. The firmware runs both roles: the Server receives uploads and certificates, and the Client sends the CSR to the PC.
- **PC GUI** ([`_TOOLS/BleHostGUI`](_TOOLS/BleHostGUI/README.md)): a navigation rail with Device (scan, connect, read CAPS), Hex upload, Provisioning (the PC is the CA), Log and a switchable BLE traffic monitor. It hosts the PC's own BulkXfer service so that the device can send its CSR. The command-line client `bulkxfer_client.py` does the same uploads and provisioning without the GUI.

## BulkXfer in brief

- The **sender is the GATT client**: it writes frames to the receiver's **DATA** characteristic with Write Without Response.
- The **receiver is the GATT server**: it hosts DATA and **CTRL**, and replies on CTRL with ACK / NACK / END notifications.
- Transfers use windowed cumulative ACKs, Go-Back-N retransmission and a CRC-32 over each object.
- Data streams through source and sink callbacks, so an object never has to fit in RAM.
- Frames are up to 244 bytes (ATT_MTU 247 with Data Length Extension).

The wire protocol is specified in [_DOC/BulkXfer/PROTOCOL.md](_DOC/BulkXfer/PROTOCOL.md), and the design rationale in [_DOC/BulkXfer/README.md](_DOC/BulkXfer/README.md).

## Hex upload in brief

- Service `B1C00000-16A1-4812-AF35-F3F29A92F6CA`, with DATA (`…0001`, Write Without Response), CTRL (`…0002`, Notify) and CAPS (`…0003`, Read).
- Each contiguous segment is one BulkXfer transfer with appType `0x10`, carrying `[u32 little-endian start address][data]`. Segments are at most 64 KiB.
- The server answers with a RESULT message when the transfer ends, and with a STORED message once the segment has been logged. The client waits for STORED before it sends the next segment.

The full contract for client implementations, including the device's whole appType registry, is in [_DOC/HexUpload/PROTOCOL.md](_DOC/HexUpload/PROTOCOL.md).

## Provisioning in brief

- The PC (the **provisioner**) hosts a BulkXfer service with the same UUIDs as the device's. The CSR travels from device to PC, so the device attaches its BulkXfer Client to the PC's service over the same connection.
- Short messages: GET_STATUS `0x20` → STATUS `0x21` (state, flags, CSR length, SHA-256 of the public key); CSR_REQ `0x22`; RESULT `0x26` (`[refAppType][status]`); DEPROVISION `0x27`.
- Transfers: CSR `0x23` (device → PC, DER PKCS#10), then CA_CERT `0x24` and DEV_CERT `0x25` (PC → device, DER X.509, at most 1024 bytes each).
- Device states: NO_KEY → KEY_READY → CA_OK → PROVISIONED. The device rejects a certificate that does not parse, does not chain, carries another key or subject than its CSR, or falls outside the profile (P-256, ecdsa-with-SHA256, X.509 v3). The RESULT status says which check failed.

The contract is in [_DOC/Provisioning/PROTOCOL.md](_DOC/Provisioning/PROTOCOL.md). Design, the origin of the ported sample code, the trust model and the limitations are in [_DOC/Provisioning/README.md](_DOC/Provisioning/README.md).

## Repository layout

| Path | Contents |
|---|---|
| [`_ASW/`](_ASW) | Application: `main.c` plus modules for BLE init/advertising (`_BLE`), the Device Information service (`_GAP`), the BulkXfer GATT service and appType router (`_BLK_SVC`), the RAM segment store and its serial log (`_DATA_STORE`), the device key and CSR (`_CSR`), certificate storage and X.509 verification (`_DEVICE_CERT`), the provisioning protocol and wipe button (`_PROV`), logging and helpers |
| [`_DI/`](_DI) | Build configuration: `prj.conf` (including PSA crypto on CRACEN, secure storage, Mbed TLS X.509 and the DK library), application Kconfig options (`Kconfig`) and the board devicetree overlay (console UART at 921600 baud) |
| [`_LIB/GATT_CB/`](_LIB/GATT_CB) | Generic GATT read/write callbacks driven by per-characteristic descriptors |
| [`_LIB/BulkXfer/`](_LIB/BulkXfer) | Bulk-transfer library (Server and Client roles) |
| [`_TOOLS/BleHostGUI/`](_TOOLS/BleHostGUI) | PC GUI (Tkinter + `bleak`, flat light/dark theme): Device, Hex upload, Provisioning, Log and Traffic pages; the PC's GATT server and CA; the command-line client |
| [`_TEST/`](_TEST) | Host tests of the libraries, the application modules and the PC tools, with a merged test report |
| [`.github/workflows/`](.github/workflows) | CI: host tests, Python tests, firmware build and the test report on every push; a manual hardware-in-the-loop placeholder |
| [`_DOC/`](_DOC) | Coding guidelines, library and protocol documentation, the CBAP application notes and reference notes |
| [`_LOG/`](_LOG) | Sample logs of one upload (GUI traffic monitor and the board's UART), made with `AA00000100.hex` at the root |
| [`Supporting Scripts/`](Supporting%20Scripts) | Silicon Labs reference scripts for certificate provisioning (third party, not built) |
| `Sample Code/`, `Sample Code Device Cert Verification/` | Reference projects that `_CSR` and `_DEVICE_CERT` were ported from (not built) |

## Building

- **SDK:** nRF Connect SDK v3.4.1 (Zephyr 4.4.2)
- **Board:** `nrf54l15dk/nrf54l15/cpuapp`

Build with the nRF Connect extension for VS Code (sysbuild), or from an NCS shell:

```sh
west build -b nrf54l15dk/nrf54l15/cpuapp --sysbuild -d build .
```

After changing the layout or configuration, do a pristine build (`-p always`). A chip erase also erases the device's key, so the device makes a new key and CSR and has to be provisioned again.

## Using it from a PC

Install the requirements (`pip install -r _TOOLS/BleHostGUI/requirements.txt`: bleak, cryptography, Pillow) and run the GUI with `python _TOOLS/BleHostGUI/ble_host_gui.py`. Provisioning needs Windows 10/11 and an adapter that supports the peripheral role, because the PC hosts its own GATT service.

The command-line client [`_TOOLS/BleHostGUI/bulkxfer_client.py`](_TOOLS/BleHostGUI/bulkxfer_client.py) does the same (`--help` lists all options):

```sh
python _TOOLS/BleHostGUI/bulkxfer_client.py hex app.hex --name "BLE Bulk Transfer"
python _TOOLS/BleHostGUI/bulkxfer_client.py provision --name "BLE Bulk Transfer"              # --ca <folder>, --out <pem>
python _TOOLS/BleHostGUI/bulkxfer_client.py provision --negative --name "BLE Bulk Transfer"   # certificates the device must reject first
python _TOOLS/BleHostGUI/bulkxfer_client.py deprovision --name "BLE Bulk Transfer"
```

The CA folder holds the CA's private key. A folder named `_CA/` at the repository root is ignored by git; keep the CA there or outside the repository, and never commit it.

The board's serial terminal runs at **921600 baud with RTS/CTS flow control** (set in the board overlay). It shows the received segments and, after provisioning, both certificates as PEM. To see every byte of an upload, set `CONFIG_DS_HEX_DUMP=y` in `_DI/prj.conf`; each 64 KiB segment then holds the upload for several seconds while it prints.

## Testing

Host tests cover `_LIB/BulkXfer`, `_LIB/GATT_CB`, every application module that is not only calls into the BT stack (data store, router, CSR, certificate storage, provisioning, wipe button, helpers), and the PC client, CA, decoders and GUI pages in `_TOOLS`. Certificate verification runs on the real Mbed TLS, against certificates that the PC's CA issues on each build, including the cases the device must reject. End-to-end tests run a hex upload and a full provisioning through the real BulkXfer engine over a simulated link. Expected wire bytes come from a real upload captured in `_LOG/` and are shared by the C and Python tests.

```powershell
powershell -ExecutionPolicy Bypass -File _TEST\run_tests.ps1   # add -Coverage for coverage
```

This needs a host `gcc` and Python 3.8+. The report is written to `build_test/report/report.html`. On GitHub, [CI](.github/workflows/ci.yml) runs the same tests (plus ASan/UBSan, and the GUI tests under `xvfb-run`), builds the firmware, and publishes the results on each push and pull request. A manual `hil-tests` job is a placeholder for a self-hosted runner with a DK. Details are in [_TEST/README.md](_TEST/README.md).

## Documentation

- [BulkXfer design and integration](_DOC/BulkXfer/README.md)
- [BulkXfer protocol](_DOC/BulkXfer/PROTOCOL.md)
- [BulkXfer API reference](_DOC/BulkXfer/API_REFERENCE.md)
- [GATT_CB design and integration](_DOC/GATT_CB/README.md)
- [GATT_CB API reference](_DOC/GATT_CB/API_REFERENCE.md)
- [Hex upload protocol](_DOC/HexUpload/PROTOCOL.md)
- [Device provisioning protocol](_DOC/Provisioning/PROTOCOL.md)
- [Device provisioning design](_DOC/Provisioning/README.md)
- [CBAP application notes (AN1396, AN1268)](_DOC/CBAP/)
- [BLE Host GUI](_TOOLS/BleHostGUI/README.md)
- [Tests and CI](_TEST/README.md)
- [BATL coding guidelines](_DOC/BATL%20Coding%20Guidelines/)

## License

The repository is licensed under the GNU GPL v3; see [LICENSE](LICENSE). The `_LIB` libraries carry an MIT licence header (`SPDX-License-Identifier: MIT`). `Supporting Scripts/` and the Silicon Labs-derived `DER.c` keep their Zlib notices.
