# BulkXfer — reliable bulk data transfer over BLE GATT (Zephyr / nRF54)

BulkXfer moves objects of any size (logs, files, images, firmware chunks) between two BLE
devices. The **sender is always the GATT client** and the **receiver always hosts the GATT
server**:

| Role | Does | API |
|---|---|---|
| **Client** (TX) | Writes frames into the peer's **DATA** characteristic with Write Without Response | [BulkXfer_Client.h](../../_LIB/BulkXfer/BulkXfer_Client.h) |
| **Server** (RX) | Hosts DATA + **CTRL**. Answers with ACK / NACK / END notifications on CTRL | [BulkXfer_Server.h](../../_LIB/BulkXfer/BulkXfer_Server.h) |

A device that must both send and receive runs both roles. The Server sits on top of the
[`GATT_CB`](../../_LIB/GATT_CB) generic callbacks without modifying them.

- **Frame format:** `len(1) + type(1) + payload(≤242)`, one frame per GATT write or notification.
- **Reliability:** windowed cumulative ACKs, Go-Back-N retransmit, CRC-32 over the whole object.
- **Throughput:** the Client keeps the controller queue full with Write Without Response. CTRL carries only about one small notification per half window.
- **Streaming:** data is pulled from a source callback and pushed to a sink callback, so objects never have to fit in RAM.

The wire protocol is specified in [PROTOCOL.md](PROTOCOL.md), and the exact API contract is in
[API_REFERENCE.md](API_REFERENCE.md).

## Why the sender is the GATT client

Protocol v1 sent device → central data as notifications. Version 2 does not, for these reasons:

- **Symmetry between devices.** Any two BulkXfer devices can exchange data in either
  direction: whichever side sends attaches as a client to the other side's service. There is
  no longer one special "central" side that has to receive notifications.
- **The receiver owns its buffer.** The data lands in the receiver's own GATT database, the
  write hook runs on the receiver's BLE RX thread, and the receiver decides how deep its
  queue is (`BLK_RX_POOL_DEPTH`).
- **Same air-time cost.** Write Without Response and notifications are both unacknowledged
  ATT PDUs with a 3-byte header, so throughput is unchanged. The credit scheme simply moves
  from `bt_gatt_notify_cb()` to `bt_gatt_write_without_response_cb()`, which has the same
  completion callback.
- **Small, rare control traffic.** Notifications are still the natural way for a server to
  talk back, but CTRL carries only ≤ 14-byte ACK / NACK / END frames (about one per 8 DATA
  frames at the default window) and short messages.

The cost is that the sender must be able to act as a GATT client: it needs
`CONFIG_BT_GATT_CLIENT` and discovers the service once per connection. A PC tool built on
`bleak` is a GATT client only, so to receive from a device the PC hosts a BulkXfer service
of its own and the device sends to it with its Client role, over the same connection. The
PC GUI does this for device provisioning: [`gatt_server.py`](../../_TOOLS/BleHostGUI/blehost/core/gatt_server.py)
(WinRT) with the receiver in [`bulkxfer_receiver.py`](../../_TOOLS/BleHostGUI/bulkxfer_receiver.py),
see [../Provisioning/README.md](../Provisioning/README.md).

## MTU: why 244?

| Quantity | Value | Why |
|---|---|---|
| LL payload (with Data Length Extension) | 251 B | Controller maximum |
| ATT_MTU | **247** | 251 − 4 (L2CAP header) |
| Write / notification payload = frame | **244** | 247 − 3 (ATT opcode + handle) |
| Short-message payload | 242 | 244 − `len` − `type` |
| DATA chunk per frame | 240 | 244 − `len` − `type` − `xferId` − `seq` |

ATT_MTU can be negotiated up to 517 (512-byte attribute values). Above 247, though, every
frame is split across several LL packets. That gains little throughput, uses more RAM, and
cannot be described by a 1-byte length field. **247 / 244 is the right target.**

The frame size is chosen **per connection** as `min(ATT_MTU − 3, 244)`. iOS typically
negotiates MTU 185, which gives 182-byte frames; a link that never exchanges MTU gives
20-byte frames. The chunk size is fixed in the START frame, so both sides always agree. With
`b_autoTuneLink`, the Client exchanges MTU **before** discovery, so the chunk size is final
by the time it reports ready.

## Protocol

The complete wire contract, independent of this code, is in [PROTOCOL.md](PROTOCOL.md): GATT
service, byte layout of every frame, CRC-32 definition, sender and receiver state machines,
timing, error handling and worked examples. In short:

- Every frame is `[len][type][payload]`. Types `0x00–0xEF` are application short messages
  (no handshake, no ACK). `0xF0–0xF5` are START, DATA, ACK, NACK, END and ABORT.
- The client writes START and DATA to **DATA**. The server answers ACK / NACK / END on
  **CTRL**. Each characteristic carries frames in one direction only.

```
client (GATT client)                              server (GATT server)
  attach: [MTU exchange] → discover DATA/CTRL → subscribe CTRL
  START(id, type, len, chunk, W, crc)  ─WwR DATA─►  fpt_onRxStart() may reject → ABORT
                        ◄─CTRL ntf─ ACK(seq 0, W')   window = min(W, W')
  DATA 0 … DATA W-1                    ─WwR DATA─►  fpt_onRxData(offset, chunk) in order
                        ◄─CTRL ntf─ ACK(next)        every W/2 frames, or after 20 ms
  DATA …   (gap seen / RX pool full)   ─WwR DATA─►
                        ◄─CTRL ntf─ NACK(next)       client rewinds to `next` (Go-Back-N)
  DATA last                            ─WwR DATA─►  CRC check
                        ◄─CTRL ntf─ END(status)      both sides report the result
```

The design choices behind it:

- The BLE link layer already acknowledges and orders packets. A frame can only be lost when
  the receiver drops it, for example when its RX queue is full. So the protocol only needs a
  NACK for that case and an ACK timeout for a stalled peer, not per-frame acknowledgement.
- The client keeps **no retransmit buffer**. A resend reads the source again, which is what
  lets objects stream from flash or a file without fitting in RAM.
- The window is capped at 128, so an 8-bit sequence number stays unambiguous. The engine
  counts frames with 32-bit indices internally.

## Integration

### Server (receiving device)

1. **Declare the service** in the GATT Configurator (generic-callback mode) with the UUIDs from
   [BulkXfer_Uuid.h](../../_LIB/BulkXfer/BulkXfer_Uuid.h):
   - **DATA:** Write + Write Without Response, variable length, length **244**. Set the custom write hook to e.g. `st_OnBulkData`.
   - **CTRL:** Notify (with CCC).
   - **Caps (optional):** Read, 4 bytes.

2. **Forward the hook** in the generated service `.c`:
   ```c
   static ssize_t st_OnBulkData(struct bt_conn *c, const struct bt_gatt_attr *a,
      const void *b, uint16_t l, uint16_t o, uint8_t f)
   {
      return gt_BLKS_DataWriteHook(c, a, b, l, o, f);
   }
   ```
   `gt_GATT_GenericWrite` still copies each frame into the descriptor buffer. BulkXfer
   ignores that copy and `u16_actualLen`, and uses the hook's `vpt_buf` / `u16_length` instead.

3. **Initialise and wire the connection callbacks:**
   ```c
   BlkSrvCfg_T cfg = {
      .stpt_ctrlAttr  = bt_gatt_find_by_uuid(svc.attrs, svc.attr_count, BT_UUID_BLK_CTRL),
      .fpt_onRxStart  = on_rx_start,   // optional: accept / reject by type & size
      .fpt_onRxData   = on_rx_data,    // in-order chunks, e.g. straight to flash
      .fpt_onRxDone   = on_rx_done,
      .fpt_onRxShort  = on_rx_short,
      .b_autoTuneLink = true,          // request 2M PHY, DLE 251, MTU exchange
   };
   gi_BLKS_Init(&cfg);
   // bt_conn_cb: connected    -> gv_BLKS_OnConnected(conn)
   //             disconnected -> gv_BLK_OnDisconnected(conn)
   ```

### Client (sending device)

```c
BlkCliCfg_T cfg = {                    // NULL UUIDs: BulkXfer_Uuid.h defaults
   .fpt_onReady    = on_ready,         // attach result
   .fpt_onTxDone   = on_tx_done,
   .fpt_onRxShort  = on_server_short,
   .b_autoTuneLink = true,
};
gi_BLKC_Init(&cfg);
// bt_conn_cb: connected    -> gi_BLKC_Attach(conn)   (either link role)
//             disconnected -> gv_BLK_OnDisconnected(conn)

// once on_ready(conn, 0) has run:
gi_BLKC_SendBuffer(MY_TYPE_LOG, buf, len);                   // RAM object
gi_BLKC_Send(MY_TYPE_FILE, &(BlkSource_T){ read_fn, ctx }, len);  // streamed
gi_BLKC_SendShort(MY_TYPE_CMD, data, n, K_MSEC(50));         // single frame
```

`gi_BLKC_Send()` reads the whole source once to compute the CRC, so start large transfers
from a work item rather than from a BulkXfer callback.

All callbacks run on the BulkXfer engine thread, never in BLE stack context. They may call
the API, for example to start the next transfer from `fpt_onTxDone`.

### Choosing roles at build time

`BLK_ENABLE_SERVER` (default 1) and `BLK_ENABLE_CLIENT` (default: `CONFIG_BT_GATT_CLIENT`)
select what is compiled. A receive-only device sets
`zephyr_compile_definitions(BLK_ENABLE_CLIENT=0)`, and a send-only device sets
`BLK_ENABLE_SERVER=0` and does not need `GATT_CB`.

This project's firmware builds both roles (`_LIB/CMakeLists.txt`): the Server receives hex
segments and certificates, the Client sends the CSR. It keeps its RX-priority
`CONFIG_BT_ATT_TX_COUNT=6` and lowers `BLK_CLI_WRITE_INFLIGHT_MAX` to 3 instead (2 + 3 < 6);
the CSR is under 1 KiB, so the smaller Client window costs nothing. The Server's single
callback set is shared by appType range through `_ASW/_BLK_SVC/BulkRouter.c`. For device
pairing both roles also carry certificates and OOB data to and from a peer device.

### Several links: moving a role

Each role binds one connection at a time, and a binding stays until that link drops. A
device that keeps a host link up while it talks to a peer on a second link moves the roles
explicitly, between transfers:

- **Server:** `gv_BLKS_OnConnected()` binds only the first connection. `gi_BLKS_Rebind(peer)`
  moves the binding to the peer, and `gi_BLKS_Rebind(host)` (or `NULL`) moves it back. The
  link it leaves is not disconnected: its writes to DATA are simply dropped from then on.
- **Client:** `gi_BLKC_Detach()` releases the bound link and unsubscribes its CTRL, then
  `gi_BLKC_Attach(peer)` discovers the peer's service. The Client keeps two subscription
  parameter sets, used in turn, because the stack holds on to the old one until the
  unsubscribe's CCC write has completed.

Both calls refuse to move a role in the middle of a transfer (`-EBUSY`). This project's
pairing module moves both roles to the peer device for the certificate and OOB exchange
([_DOC/Pairing/README.md](../Pairing/README.md)), and limits what that peer can reach with the
router's appType filter (`gv_BulkRouter_SetFilter()`).

### Threading model

| Context | Work |
|---|---|
| BLE RX thread | Server DATA hook and Client CTRL notify callback: check the frame, copy it into a slab block, queue it (never block). Client discovery callbacks post their result through atomics |
| BLE TX completion | Returns a write / notify credit |
| Timer ISR | Sets an event bit |
| Engine thread | Everything else: both state machines, ACK/NACK, sending, application callbacks |

Each role has its own credit semaphore. The Client allows `BLK_CLI_WRITE_INFLIGHT_MAX`
(default 6) DATA writes in flight, and the Server `BLK_SRV_NOTIFY_INFLIGHT_MAX` (default 2)
CTRL notifications. Their sum must stay below `CONFIG_BT_ATT_TX_COUNT` (checked at build
time), so the host never blocks on buffer allocation. Credits are restored on disconnect,
because the host does not call completion callbacks for PDUs lost with the link.

## Configuration

`BulkXfer_Config.h` holds the defaults (window 16, RX pool 20, timeouts, stack size, role
selection). Override any of them with compile definitions.

A throughput baseline for NCS 3.4.1:

- `CONFIG_BT_L2CAP_TX_MTU=247`
- `BT_BUF_ACL_{RX,TX}_SIZE=251`, `BT_CTLR_DATA_LENGTH_MAX=251`
- `BT_USER_{PHY,DATA_LEN}_UPDATE=y`
- `BT_ATT_TX_COUNT=10`, `BT_CONN_TX_MAX=10`, `BT_BUF_ACL_TX_COUNT=10`
- `BT_BUF_EVT_RX_COUNT=11` (must exceed the ACL TX count)
- `BT_CTLR_SDC_MAX_CONN_EVENT_LEN_DEFAULT=4000000`
- a 7.5–15 ms connection interval
- `CONFIG_CRC=y`
- `CONFIG_BT_GATT_CLIENT=y` on the sender

## Tests and tools

| Path | What |
|---|---|
| [`_TOOLS/BleHostGUI/bulkxfer_client.py`](../../_TOOLS/BleHostGUI/bulkxfer_client.py) | PC GATT client (`bleak`): `hex`, `caps`, `provision`, and `send <bytes>` / `ping` for a server that accepts any appType. Also importable (`parse_ihex`, `BulkXferClient`) |
| [`_TOOLS/BleHostGUI/bulkxfer_receiver.py`](../../_TOOLS/BleHostGUI/bulkxfer_receiver.py) | PC receiver role (§7 of the protocol), transport independent; behind the PC's GATT service in `blehost/core/gatt_server.py` |
| [`_TEST/unit/BulkXfer/test_frame.c`](../../_TEST/unit/BulkXfer/test_frame.c) | Host unit tests of the frame codec, plus the golden wire vectors shared with the PC client (`_TEST/vectors/wire.json`) |
| [`_TEST/unit/BulkXfer/test_engine.c`](../../_TEST/unit/BulkXfer/test_engine.c) | Host tests of the **real engine** (both roles) against the simulated link and scripted peer in `sim_link.h` |

`test_engine.c` covers these scenarios:
- Client: sizes 0 B to 100 kB, frame loss (NACK), silent server (timeout), window stall, MTU 23 with sequence wrap, local abort, disconnect with writes in flight (credit restore), send before attach
- Client attach: missing service or CTRL, subscribe failure, disconnect during discovery, MTU exchange first
- Server: sizes, RX pool overflow (NACK), CRC error, reject, unsubscribed client, local abort, disconnect, stale frames after reconnect, malformed DATA, wrong-channel frames
- Both roles: short messages on both channels, and simultaneous transfers in both directions on one link

It is built three times: both roles, server only, and client only. The tests are part of the repo-wide host test suite; see [`_TEST/README.md`](../../_TEST/README.md) for how to run them and read the report.

```bash
# Host tests (all libraries, application modules and PC tools)
powershell -ExecutionPolicy Bypass -File _TEST/run_tests.ps1

# On hardware: the project firmware as the server, a PC as the client
python _TOOLS/BleHostGUI/bulkxfer_client.py hex app.hex
```
