# nRF54 BLE Bulk Transfer

Zephyr / nRF Connect SDK firmware for a BLE device on the **nRF54L15 DK**. It receives bulk data over GATT, can be provisioned with a device certificate, and can pair with another provisioned device through certificate-based OOB pairing. A PC GUI uploads to the device, provisions it and orchestrates the pairing.

## Status

- **Application (`_ASW`)**: advertises as `BLE Bulk Transfer` and exposes GAP, Device Information, the BulkXfer service and the Pairing service. An appType router splits BulkXfer between three users:
  - **Hex upload** (`0x10`): receives an Intel HEX file one contiguous segment at a time, buffers each segment in RAM and logs it on the serial terminal. By default it logs one summary line with the segment's CRC-32; built with `CONFIG_DS_HEX_DUMP=y` (for debugging, much slower) it logs every byte as `0xADDRESS: xx xx …` lines. Nothing is written to flash yet.
  - **Device provisioning** (`0x20`–`0x2F`): phase 1 of certificate-based authentication and pairing (Silicon Labs AN1396/AN1268). The device keeps a persistent P-256 key and a CSR. The PC acts as the Certificate Authority: it fetches the CSR and returns a CA certificate and a device certificate. The device verifies both, stores both in PSA Internal Trusted Storage, deletes its CSR and logs both certificates as PEM. Stored certificates are re-verified at boot. Provisioning is one-time. A wipe (the DEPROVISION message, or holding DK Button 0 for 5 s) destroys the key, the CSR and the certificates, and the device makes a fresh key and CSR.
  - **Device pairing** (`0x30`–`0x3F`, between two devices): phase 2 (AN1396 §4.2). The PC tells two provisioned devices their roles and each other's address. The devices connect to each other and exchange and verify their device certificates against their CA. Each then signs fresh LE Secure Connections OOB data with its device key. Each verifies the other's signature, and they pair with the OOB method at security level 4, bonded. The central proves the link by writing a characteristic that only an encrypted, authenticated link may write, and the peripheral lights LED0. The PC follows both devices step by step. From then on the pair reconnects by itself, after a reset or a lost link, and encrypts with the stored keys; while that reconnected link is up, both devices blink LED0. A device keeps one link to the PC and one to its peer.
- **BulkXfer library (`_LIB/BulkXfer`)**: the transfer protocol itself. The firmware runs both roles: the Server receives uploads, certificates and a peer's pairing data, and the Client sends the CSR to the PC and its own pairing data to the peer. The roles move between live links (`gi_BLKS_Rebind()`, `gi_BLKC_Detach()`).
- **PC GUI** ([`_TOOLS/BleHostGUI`](_TOOLS/BleHostGUI/README.md)): a navigation rail with Device (scan, connect, read CAPS), Hex upload, Provisioning (the PC is the CA), Pairing (choose two devices and their roles, follow both live), Log and a switchable BLE traffic monitor. It hosts the PC's own BulkXfer service so that the device can send its CSR. The command-line client `bulkxfer_client.py` does the same uploads, provisioning and pairing without the GUI.
- **Memory report** ([`_TOOLS/MemReport`](_TOOLS/MemReport/README.md)): every firmware build prints FLASH and RAM use per module of `_ASW` and `_LIB` below the linker's summary, read from the map file.

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

## Pairing in brief

- A provisioned device advertises the **Pairing service** `B1C10000-…`: CONTROL (`…0001`, Write: START `[01][role][peer address]`, CANCEL, UNPAIR), STATUS (`…0002`, Read/Notify: state, error, detail, provisioning state, role, own and peer address) and SECURED (`…0003`, writable only over an LE Secure Connections link).
- The PC connects to both devices over plain links and sends START to the peripheral, then to the central. The peripheral advertises directed to the central, and the central scans for it and connects.
- Over BulkXfer on that link: each device sends its certificate (`0x30`) and verifies the peer's against its CA. Each then sends `[r][c][signature]` (`0x31`), where `r` and `c` are fresh OOB data and the signature is ECDSA P-256 over `r ‖ c ‖ sender address ‖ receiver address` with the device key. Only the pairing appTypes are reachable from the peer.
- SMP pairs with OOB data from both devices: level 4, bonded. The central writes SECURED, and the peripheral lights LED0. Each saves its role and peer next to the bond.
- A bonded pair reconnects by itself whenever its link is down: the peripheral advertises (also with a PC connected), the central scans for it, connects and encrypts with the stored keys. At level 4 both blink LED0 until the link drops; a link that does not reach level 4 within 10 s is dropped and retried. UNPAIR, a new START or a wipe stops it. STATUS states: IDLE, ARMED, CONNECTED, CERT_EXCHANGE, CERT_VERIFIED, OOB_EXCHANGE, PAIRING, PAIRED, FAILED (with an error such as PEER_CERT, OOB_SIG, SMP, TIMEOUT).

The contract is in [_DOC/Pairing/PROTOCOL.md](_DOC/Pairing/PROTOCOL.md). Design, security notes and limitations are in [_DOC/Pairing/README.md](_DOC/Pairing/README.md).

## Repository layout

| Path | Contents |
|---|---|
| [`_ASW/`](_ASW) | Application: `main.c` plus modules for BLE init, the host and peer links and advertising (`_BLE`), the Device Information service (`_GAP`), the BulkXfer GATT service and appType router (`_BLK_SVC`), the RAM segment store and its serial log (`_DATA_STORE`), the device key and CSR (`_CSR`), certificate storage and X.509 verification (`_DEVICE_CERT`), the provisioning protocol and wipe button (`_PROV`), certificate-based pairing and the Pairing service (`_PAIR`), logging and helpers |
| [`_DI/`](_DI) | Build configuration: `prj.conf` (including PSA crypto on CRACEN, secure storage, Mbed TLS X.509, the DK library, the central role, SMP and bonds), application Kconfig options (`Kconfig`) and the board devicetree overlay (console UART at 921600 baud) |
| [`_LIB/GATT_CB/`](_LIB/GATT_CB) | Generic GATT read/write callbacks driven by per-characteristic descriptors |
| [`_LIB/BulkXfer/`](_LIB/BulkXfer) | Bulk-transfer library (Server and Client roles) |
| [`_TOOLS/BleHostGUI/`](_TOOLS/BleHostGUI) | PC GUI (Tkinter + `bleak`, flat light/dark theme): Device, Hex upload, Provisioning, Pairing, Log and Traffic pages; the PC's GATT server and CA; the pairing orchestrator; the command-line client |
| [`_TOOLS/MemReport/`](_TOOLS/MemReport) | Build-time script that splits the linker's FLASH/RAM figures by module of `_ASW` and `_LIB` |
| [`_TEST/`](_TEST) | Host tests of the libraries, the application modules and the PC tools, with a merged test report |
| [`.github/workflows/`](.github/workflows) | CI: host tests, Python tests, firmware build and the test report on every push; manual hardware-in-the-loop placeholders (one DK; two DKs for pairing) |
| [`_DOC/`](_DOC) | Coding guidelines, library and protocol documentation, the CBAP application notes and reference notes |
| [`_LOG/`](_LOG) | Sample logs of one upload (GUI traffic monitor and the board's UART), made with `AA00000100.hex` at the root |
| [`Supporting Scripts/`](Supporting%20Scripts) | Silicon Labs reference scripts for certificate provisioning (third party, not built) |
| `Sample Code/`, `Sample Code Device Cert Verification/` | Reference projects that `_CSR`, `_DEVICE_CERT` and the `_PAIR` flow were ported from (not built) |

## Building

- **SDK:** nRF Connect SDK v3.4.1 (Zephyr 4.4.2)
- **Board:** `nrf54l15dk/nrf54l15/cpuapp`

Build with the nRF Connect extension for VS Code (sysbuild), or from an NCS shell:

```sh
west build -b nrf54l15dk/nrf54l15/cpuapp --sysbuild -d build .
```

Below the linker's FLASH/RAM summary, the build prints the same figures per module of `_ASW` and `_LIB` (with Zephyr, NCS and libc as one "Other" row) and writes them to `zephyr/mem_report.txt` in the image's build folder; see [MemReport](_TOOLS/MemReport/README.md).

After changing the layout or configuration, do a pristine build (`-p always`). A chip erase also erases the device's key, so the device makes a new key and CSR and has to be provisioned again.

## Using it from a PC

Install the requirements (`pip install -r _TOOLS/BleHostGUI/requirements.txt`: bleak, cryptography, Pillow) and run the GUI with `python _TOOLS/BleHostGUI/ble_host_gui.py`. Provisioning needs Windows 10/11 and an adapter that supports the peripheral role, because the PC hosts its own GATT service.

The command-line client [`_TOOLS/BleHostGUI/bulkxfer_client.py`](_TOOLS/BleHostGUI/bulkxfer_client.py) does the same (`--help` lists all options):

```sh
python _TOOLS/BleHostGUI/bulkxfer_client.py hex app.hex --name "BLE Bulk Transfer"
python _TOOLS/BleHostGUI/bulkxfer_client.py provision --name "BLE Bulk Transfer"              # --ca <folder>, --out <pem>
python _TOOLS/BleHostGUI/bulkxfer_client.py provision --negative --name "BLE Bulk Transfer"   # certificates the device must reject first
python _TOOLS/BleHostGUI/bulkxfer_client.py deprovision --name "BLE Bulk Transfer"
python _TOOLS/BleHostGUI/bulkxfer_client.py pair --central <address> --peripheral <address>
python _TOOLS/BleHostGUI/bulkxfer_client.py pairstatus --address <address>                     # unpair --address <address>
```

Pairing needs two DKs provisioned by the same CA (a device certified by another CA is refused). In the GUI, disconnect the Device page first: the Pairing page makes its own link to each device. Two devices already paired with each other are not paired again until one of them is unpaired. Once paired, the PC can disconnect: the two boards reconnect to each other after every reset or lost link, and blink LED0 while connected.

The CA folder holds the CA's private key. A folder named `_CA/` at the repository root is ignored by git; keep the CA there or outside the repository, and never commit it.

The board's serial terminal runs at **921600 baud with RTS/CTS flow control** (set in the board overlay). It shows the received segments, after provisioning both certificates as PEM, and each step of a pairing. To see every byte of an upload, set `CONFIG_DS_HEX_DUMP=y` in `_DI/prj.conf`; each 64 KiB segment then holds the upload for several seconds while it prints.

## Testing

Host tests cover `_LIB/BulkXfer`, `_LIB/GATT_CB`, every application module that is not only calls into the BT stack (data store, router, CSR, certificate storage, provisioning, wipe button, pairing, helpers), and the PC client, CA, pairing orchestrator, decoders, GUI pages and memory report in `_TOOLS`. Certificate verification runs on the real Mbed TLS, against certificates that the PC's CA issues on each build, including the cases the device must reject; the signed OOB data runs on the real TF-PSA-Crypto against a frame signed by Python. End-to-end tests run a hex upload, a full provisioning and a pairing exchange through the real BulkXfer engine over a simulated link. Expected wire bytes come from a real upload captured in `_LOG/` and are shared by the C and Python tests.

```powershell
powershell -ExecutionPolicy Bypass -File _TEST\run_tests.ps1   # add -Coverage for coverage
```

This needs a host `gcc` and Python 3.8+. The report is written to `build_test/report/report.html`. On GitHub, [CI](.github/workflows/ci.yml) runs the same tests (plus ASan/UBSan, and the GUI tests under `xvfb-run`), builds the firmware (with the per-module memory table in the job summary), and publishes the results on each push and pull request. Manual `hil-tests` and `hil-pair-tests` jobs are placeholders for self-hosted runners with one DK and with two DKs. Details are in [_TEST/README.md](_TEST/README.md).

## Documentation

- [BulkXfer design and integration](_DOC/BulkXfer/README.md)
- [BulkXfer protocol](_DOC/BulkXfer/PROTOCOL.md)
- [BulkXfer API reference](_DOC/BulkXfer/API_REFERENCE.md)
- [GATT_CB design and integration](_DOC/GATT_CB/README.md)
- [GATT_CB API reference](_DOC/GATT_CB/API_REFERENCE.md)
- [Hex upload protocol](_DOC/HexUpload/PROTOCOL.md)
- [Device provisioning protocol](_DOC/Provisioning/PROTOCOL.md)
- [Device provisioning design](_DOC/Provisioning/README.md)
- [Device pairing protocol](_DOC/Pairing/PROTOCOL.md)
- [Device pairing design](_DOC/Pairing/README.md)
- [CBAP application notes (AN1396, AN1268)](_DOC/CBAP/)
- [BLE Host GUI](_TOOLS/BleHostGUI/README.md)
- [Memory report per module](_TOOLS/MemReport/README.md)
- [Tests and CI](_TEST/README.md)
- [BATL coding guidelines](_DOC/BATL%20Coding%20Guidelines/)

## License

The repository is licensed under the GNU GPL v3; see [LICENSE](LICENSE). The `_LIB` libraries carry an MIT licence header (`SPDX-License-Identifier: MIT`). `Supporting Scripts/` and the Silicon Labs-derived `DER.c` keep their Zlib notices.
