# nRF54 BLE Bulk Transfer

Zephyr / nRF Connect SDK firmware for a BLE peripheral that receives bulk data over GATT, targeting the **nRF54L15 DK**, plus a PC GUI that uploads to it.

## Status

- **Application (`_ASW`)** — advertises as `BLE Bulk Transfer` and exposes GAP, Device Information and the BulkXfer service. It receives an Intel HEX upload one contiguous segment at a time, buffers each segment in RAM and logs it on the serial terminal: one summary line with its CRC-32, or every byte as `0xADDRESS: xx xx …` lines when built with `CONFIG_DS_HEX_DUMP=y` (for debugging; much slower). Nothing is written to flash yet.
- **BulkXfer library (`_LIB/BulkXfer`)** — the transfer protocol itself. The firmware runs it in the receiver (Server) role.
- **Upload client** — the PC GUI in [`_TOOLS/BleHostGUI`](_TOOLS/BleHostGUI/README.md) scans, connects, reads CAPS and uploads a hex file, with a switchable BLE traffic monitor. The `hex` command of the Python test client does the same from the command line.

## BulkXfer in brief

- The **sender is the GATT client**: it writes frames to the receiver's **DATA** characteristic with Write Without Response.
- The **receiver is the GATT server**: it hosts DATA and **CTRL**, and replies on CTRL with ACK / NACK / END notifications.
- Transfers use windowed cumulative ACKs, Go-Back-N retransmission and a CRC-32 over each object.
- Data streams through source and sink callbacks, so an object never has to fit in RAM.
- Frames are up to 244 bytes (ATT_MTU 247 with Data Length Extension).

The design rationale and protocol walkthrough are in [_DOC/BulkXfer/README.md](_DOC/BulkXfer/README.md).

## Hex upload in brief

- Service `B1C00000-16A1-4812-AF35-F3F29A92F6CA`, with DATA (`…0001`, Write Without Response), CTRL (`…0002`, Notify) and CAPS (`…0003`, Read).
- Each contiguous segment is one BulkXfer transfer, appType `0x10`, carrying `[u32 little-endian start address][data]`. Segments are at most 64 KiB.
- The server answers with a RESULT message when the transfer ends, and a STORED message once the segment has been logged. The client waits for STORED before sending the next segment.

The full contract for client implementations is in [_DOC/HexUpload/PROTOCOL.md](_DOC/HexUpload/PROTOCOL.md).

## Repository layout

| Path | Contents |
|---|---|
| [`_ASW/`](_ASW) | Application: `main.c` plus modules for BLE init/advertising, the Device Information service, the BulkXfer GATT service (`_BLK_SVC`), the RAM segment store and its serial log (`_DATA_STORE`), logging and helpers |
| [`_DI/`](_DI) | Build configuration: `prj.conf`, application Kconfig options (`Kconfig`) and the board devicetree overlay (console UART at 921600 baud) |
| [`_LIB/GATT_CB/`](_LIB/GATT_CB) | Generic GATT read/write callbacks driven by per-characteristic descriptors |
| [`_LIB/BulkXfer/`](_LIB/BulkXfer) | Bulk-transfer library |
| [`_TOOLS/BleHostGUI/`](_TOOLS/BleHostGUI) | PC GUI (Tkinter + `bleak`): scan/connect, hex upload, BLE traffic monitor |
| [`_TEST/`](_TEST) | Host tests of the libraries, the application modules and the PC tools, with a merged test report |
| [`.github/workflows/`](.github/workflows) | CI: host tests, firmware build and the test report on every push |
| [`_DOC/`](_DOC) | Coding guidelines, library documentation and reference notes |

## Building

- **SDK:** nRF Connect SDK v3.4.1 (Zephyr 4.4.2)
- **Board:** `nrf54l15dk/nrf54l15/cpuapp`

Build with the nRF Connect extension for VS Code (sysbuild), or from an NCS shell:

```sh
west build -b nrf54l15dk/nrf54l15/cpuapp --sysbuild -d build .
```

After changing the layout or configuration, do a pristine build (`-p always`).

To upload a hex file from a PC, run the GUI (`pip install -r _TOOLS/BleHostGUI/requirements.txt`, then `python _TOOLS/BleHostGUI/ble_host_gui.py`), or use the command-line client [`_TOOLS/BleHostGUI/bulkxfer_client.py`](_TOOLS/BleHostGUI/bulkxfer_client.py), which needs `pip install bleak`:

```sh
python _TOOLS/BleHostGUI/bulkxfer_client.py hex app.hex --name "BLE Bulk Transfer"
```

The received segments appear on the board's serial terminal at **921600 baud with RTS/CTS flow control** (set in the board overlay). To see every byte, set `CONFIG_DS_HEX_DUMP=y` in `_DI/prj.conf`; each 64 KiB segment then holds the upload for several seconds while it prints.

## Testing

Host tests cover `_LIB/BulkXfer`, `_LIB/GATT_CB`, the segment store in `_ASW/_DATA_STORE`, the helpers, and the PC client and decoders in `_TOOLS`. One end-to-end test runs a hex upload through the real BulkXfer Server and data store over a simulated link. Expected bytes come from a real upload captured in `_LOG/`.

```powershell
powershell -ExecutionPolicy Bypass -File _TEST\run_tests.ps1   # add -Coverage for coverage
```

This needs a host `gcc` and Python 3.8+. The report is written to `build_test/report/report.html`. On GitHub, [CI](.github/workflows/ci.yml) runs the same tests (plus ASan/UBSan), builds the firmware, and publishes the results on each push and pull request. Details are in [_TEST/README.md](_TEST/README.md).

## Documentation

- [BulkXfer design and integration](_DOC/BulkXfer/README.md)
- [BulkXfer API reference](_DOC/BulkXfer/API_REFERENCE.md)
- [GATT_CB API reference](_DOC/GATT_CB/API_REFERENCE.md)
- [Hex upload protocol](_DOC/HexUpload/PROTOCOL.md)
- [BLE Host GUI](_TOOLS/BleHostGUI/README.md)
- [Tests and CI](_TEST/README.md)
- [BATL coding guidelines](_DOC/BATL%20Coding%20Guidelines/)

## License

The repository is licensed under the GNU GPL v3; see [LICENSE](LICENSE). The `_LIB` libraries carry an MIT licence header (`SPDX-License-Identifier: MIT`).
