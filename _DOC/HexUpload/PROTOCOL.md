# Hex Upload — Protocol

This is the contract between the firmware in `_ASW` (the BLE peripheral and GATT server that receives data) and a client that uploads an Intel HEX file. Everything runs on top of the BulkXfer library, so the client implements the BulkXfer **Client** role. The BulkXfer protocol itself (GATT service, frame formats, sequences, timing, errors) is specified in [../BulkXfer/PROTOCOL.md](../BulkXfer/PROTOCOL.md). There are two clients: the PC GUI in [_TOOLS/BleHostGUI](../../_TOOLS/BleHostGUI/README.md) (Hex Upload tab), and the `hex` command in the reference client [bulkxfer_client.py](../../_TOOLS/BleHostGUI/bulkxfer_client.py), whose protocol code the GUI reuses.

For now the server does not program flash. It buffers each segment in RAM and logs it on the serial terminal (UART, 921600 baud, RTS/CTS flow control): by default one summary line per segment, or every byte as `0xAAAAAAAA: xx xx …` lines when the firmware is built with `CONFIG_DS_HEX_DUMP=y` (see §6).

## 1. Discovery

| Item | Value |
|---|---|
| Device name (scan response) | `BLE Bulk Transfer` (`CONFIG_BT_DEVICE_NAME`) |
| Advertised service (adv data, 128-bit list) | `B1C00000-16A1-4812-AF35-F3F29A92F6CA` |
| Connections | One at a time |
| Security | None (open link) |

## 2. GATT table

The UUIDs are `B1C0xxxx` followed by the project base `-16A1-4812-AF35-F3F29A92F6CA` (`_ASW/_BLE_GENERIX/BaseUUIDs.h`).

| Attribute | UUID | Properties | Permissions | Value |
|---|---|---|---|---|
| Primary service | `B1C00000-…` | — | — | — |
| DATA | `B1C00001-…` | Write Without Response, Write | Write | One BulkXfer frame per write, 2…244 B. **Use Write Without Response.** Long (prepared) writes are refused |
| CTRL | `B1C00002-…` | Notify | — | Server → client frames (ACK / NACK / END / ABORT and short messages) |
| CTRL CCCD | `0x2902` | — | Read, Write | Write `01 00` to subscribe. The server ignores START until the client is subscribed |
| CAPS | `B1C00003-…` | Read | Read | 4 B: protocol version `2`, max frame `244`, window `16`, reserved `0` |

The device also exposes Zephyr's GAP service and the Device Information service.

Link setup: the server itself requests 2M PHY, data length 251 and MTU 247 about 2 s after connecting, then a connection interval of 7.5–15 ms (latency 0, supervision timeout 4 s), which Zephyr sends 5 s after connecting (`CONFIG_BT_CONN_PARAM_UPDATE_TIMEOUT`). The interval sets the throughput, so a client that uploads right after connecting runs at the central's default interval until then. The central may refuse the request. A client should also request MTU 247. BulkXfer's chunk size is `min(ATT_MTU − 3, 244) − 4`, fixed when each transfer starts.

## 3. Application messages

| appType | Direction | Kind | Payload |
|---|---|---|---|
| `0x10` SEGMENT | client → server | BulkXfer transfer (START / DATA …) | `[u32 LE start address][data]` |
| `0x01` RESULT | server → client | short message on CTRL | `[u8 status][u32 LE address][u32 LE data length]` |
| `0x11` STORED | server → client | short message on CTRL | `[u8 0][u32 LE address][u32 LE data length]` |

`status` is a `BlkStatus_E` value (`0` = OK). On failure, RESULT carries address and length `0`.

The START frame's length and CRC-32 cover the whole object, **address header included**.

The device's BulkXfer Server is shared by appType range ([_ASW/_BLK_SVC/BulkRouter.c](../../_ASW/_BLK_SVC/BulkRouter.c)). This is the whole registry:

| appTypes | Owner | Contract |
|---|---|---|
| `0x01`, `0x10`, `0x11` | Hex upload (`_DATA_STORE`) | This document |
| `0x20`–`0x2F` | Device provisioning (`_PROV`) | [../Provisioning/PROTOCOL.md](../Provisioning/PROTOCOL.md) |
| others up to `0xEF` | Free | A transfer is refused (§5), a short message is logged and ignored |

A new user of the Server registers a free range with `gi_BulkRouter_Register()` before `gi_BulkRouter_Start()` and adds it here.

## 4. Upload sequence

1. Parse the hex file. Apply record types `02`/`04` (extended address) to data records (`00`), ignore `03`/`05`, and stop at `01`.
2. Merge the bytes into contiguous runs. Split any run longer than **65536** data bytes (the server's `DS_BUF_SIZE`).
3. Connect, subscribe to CTRL, and optionally read CAPS.
4. For each segment:
   1. Send a BulkXfer transfer with appType `0x10` and object `pack('<I', address) + data`.
   2. Wait for END. Only status `OK` means the segment arrived intact.
   3. RESULT follows right away.
   4. **Wait for STORED** before sending the next segment. The server keeps the segment in its only buffer until it has been logged. By default that is one line and STORED follows almost at once. With `CONFIG_DS_HEX_DUMP=y` the full dump takes several seconds per 64 KiB segment (10–15 s has been observed at 921600 baud), and a slower terminal or UART setting stretches it further, so use a generous timeout.

## 5. Rejections

The server rejects a START by answering it with **ABORT** (direction *by receiver*, reason `REJECTED` = `0x05`) instead of the first ACK. No END follows (`bulkxfer_client.py` reports this as `ABORTED_BY_SERVER:REJECTED`). This happens when:

| Cause | Fix |
|---|---|
| appType is not `0x10` (no other range takes segments; provisioning refuses non-certificates and also answers RESULT `0x26`, see its protocol) | Use `0x10` |
| Object ≤ 4 bytes (no data) | Send at least one data byte |
| Object > 4 + 65536 bytes | Split the segment |
| The previous segment is still being dumped | Wait for STORED |

A transfer that fails (link lost, timeout, CRC error, abort) is discarded whole, and the client resends the segment.

## 6. Serial output

Default (`CONFIG_DS_HEX_DUMP=n`), one line per segment:

```
<inf> APP_LOG: SEG addr=0x00010000 len=1148 crc=0x1a2b3c4d
```

With `CONFIG_DS_HEX_DUMP=y` in `_DI/prj.conf` (debugging), every byte:

```
<inf> APP_LOG: SEG start addr=0x00010000 len=1148 crc=0x1a2b3c4d
<inf> APP_LOG: 0x00010000: 00 10 00 20 d5 1c 00 00 ...
...
<inf> APP_LOG: SEG end addr=0x00010000 len=1148
```

`crc` equals the CRC-32 in the client's START frame, so a segment on the terminal can be matched to the transfer that carried it.
