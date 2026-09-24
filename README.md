# nRF54 BLE Bulk Transfer

Zephyr / nRF Connect SDK firmware for a BLE peripheral that receives bulk data over GATT, targeting the **nRF54L15 DK**.

## Status

- **Application (`_ASW`)** — advertises as `BLE Bulk Transfer` and exposes GAP, Device Information and the BulkXfer service. It receives an Intel HEX upload one contiguous segment at a time, buffers each segment in RAM and prints it on the serial terminal as `0xADDRESS: xx xx …` lines. Nothing is written to flash yet.
- **BulkXfer library (`_LIB/BulkXfer`)** — the transfer protocol itself. The firmware runs it in the receiver (Server) role.
- **Upload client** — a PC GUI is planned. Until then, the `hex` command of the Python test client does the upload.

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
- The server answers with a RESULT message when the transfer ends, and a STORED message once the segment has been printed. The client waits for STORED before sending the next segment.

The full contract for client implementations is in [_DOC/HexUpload/PROTOCOL.md](_DOC/HexUpload/PROTOCOL.md).

## Repository layout

| Path | Contents |
|---|---|
| [`_ASW/`](_ASW) | Application: `main.c` plus modules for BLE init/advertising, the Device Information service, the BulkXfer GATT service (`_BLK_SVC`), the RAM segment store and serial dump (`_DATA_STORE`), logging and helpers |
| [`_DI/`](_DI) | Build configuration (`prj.conf`) and the board devicetree overlay (console UART at 921600 baud) |
| [`_LIB/GATT_CB/`](_LIB/GATT_CB) | Generic GATT read/write callbacks driven by per-characteristic descriptors |
| [`_LIB/BulkXfer/`](_LIB/BulkXfer) | Bulk-transfer library, with example client/server apps, host-side unit tests and a Python (`bleak`) test client |
| [`_DOC/`](_DOC) | Coding guidelines, library documentation and reference notes |

## Building

- **SDK:** nRF Connect SDK v3.4.1 (Zephyr 4.4.2)
- **Board:** `nrf54l15dk/nrf54l15/cpuapp`

Build with the nRF Connect extension for VS Code (sysbuild), or from an NCS shell:

```sh
west build -b nrf54l15dk/nrf54l15/cpuapp --sysbuild -d build .
```

After changing the layout or configuration, do a pristine build (`-p always`).

To upload a hex file from a PC, use [`_LIB/BulkXfer/tools/bulkxfer_client.py`](_LIB/BulkXfer/tools/bulkxfer_client.py). It needs `pip install bleak`.

```sh
python _LIB/BulkXfer/tools/bulkxfer_client.py hex app.hex --base 16a1-4812-af35-f3f29a92f6ca --name "BLE Bulk Transfer"
```

The received data appears on the board's serial terminal at **921600 baud with RTS/CTS flow control** (set in the board overlay).

## Documentation

- [BulkXfer design and integration](_DOC/BulkXfer/README.md)
- [BulkXfer API reference](_DOC/BulkXfer/API_REFERENCE.md)
- [GATT_CB API reference](_DOC/GATT_CB/API_REFERENCE.md)
- [Hex upload protocol](_DOC/HexUpload/PROTOCOL.md)
- [BATL coding guidelines](_DOC/BATL%20Coding%20Guidelines/)

## License

The repository is licensed under the GNU GPL v3; see [LICENSE](LICENSE). The `_LIB` libraries carry an MIT licence header (`SPDX-License-Identifier: MIT`).
