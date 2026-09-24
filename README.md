# nRF54 BLE Bulk Transfer

Zephyr / nRF Connect SDK firmware for a BLE peripheral that receives bulk data over GATT, targeting the **nRF54L15 DK**.

## Status

- **Application (`_ASW`)** — brings up the BLE stack, advertises as `BLE Bulk Transfer`, and exposes the GAP and Device Information services. It does not use the bulk-transfer library yet.
- **BulkXfer library (`_LIB/BulkXfer`)** — the transfer protocol itself. It is complete and built into the firmware in the receiver (Server) role.

## BulkXfer in brief

- The **sender is the GATT client**: it writes frames to the receiver's **DATA** characteristic with Write Without Response.
- The **receiver is the GATT server**: it hosts DATA and **CTRL**, and replies on CTRL with ACK / NACK / END notifications.
- Transfers use windowed cumulative ACKs, Go-Back-N retransmission and a CRC-32 over each object.
- Data streams through source and sink callbacks, so an object never has to fit in RAM.
- Frames are up to 244 bytes (ATT_MTU 247 with Data Length Extension).

The design rationale and protocol walkthrough are in [_DOC/BulkXfer/README.md](_DOC/BulkXfer/README.md).

## Repository layout

| Path | Contents |
|---|---|
| [`_ASW/`](_ASW) | Application: `main.c` plus modules for BLE init/advertising, GAP/DIS services, logging and helpers |
| [`_DI/`](_DI) | Build configuration (`prj.conf`) |
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

To send test data from a PC, use [`_LIB/BulkXfer/tools/bulkxfer_client.py`](_LIB/BulkXfer/tools/bulkxfer_client.py). It needs `pip install bleak`.

## Documentation

- [BulkXfer design and integration](_DOC/BulkXfer/README.md)
- [BulkXfer API reference](_DOC/BulkXfer/API_REFERENCE.md)
- [GATT_CB API reference](_DOC/GATT_CB/API_REFERENCE.md)
- [BATL coding guidelines](_DOC/BATL%20Coding%20Guidelines/)

## License

The repository is licensed under the GNU GPL v3; see [LICENSE](LICENSE). The `_LIB` libraries carry an MIT licence header (`SPDX-License-Identifier: MIT`).
