# BLE Host GUI

PC-side GUI (Tkinter + [bleak](https://github.com/hbldh/bleak)) for the nRF54 BLE Bulk Transfer firmware. The PC is the BLE central and the BulkXfer **Client**; the board is the GATT server and BulkXfer Server.

Stage 1 covers:

- **Device**: scan, connect, disconnect, and read the BulkXfer CAPS characteristic.
- **Hex Upload**: browse for an Intel HEX file, split it into contiguous segments, and send each segment as one transfer (`appType 0x10`, `[u32 LE address][data]`). The server must answer with STORED before the next segment is sent. Start and Abort buttons control the upload. The contract is in [_DOC/HexUpload/PROTOCOL.md](../../_DOC/HexUpload/PROTOCOL.md).
- **BLE traffic monitor**: every write, read, notification and link event, shown as hex and decoded BulkXfer frames. Turn it on or off with **View ▸ BLE Traffic**, the toolbar button or `Ctrl+T`. It captures nothing while it is off.

## Run

```sh
pip install -r requirements.txt
python ble_host_gui.py
```

Python 3.10 or later. The BulkXfer protocol code is imported from [`_LIB/BulkXfer/tools/bulkxfer_client.py`](../../_LIB/BulkXfer/tools/bulkxfer_client.py), so keep this folder inside the repository.

Typical session: **Scan**, select `BLE Bulk Transfer`, **Connect**, **Read Caps**, then on the Hex Upload tab **Browse…**, **Start Upload**. The board prints each segment on its serial terminal (921600 baud, RTS/CTS).

**Base UUID** must match the firmware's `BaseUUIDs.h`. The default is the project base, `16a1-4812-af35-f3f29a92f6ca`. **Max segment** must not exceed the server's `DS_BUF_SIZE` (65536).

## Layout

```
ble_host_gui.py          entry point; FEATURES lists the tabs
blehost/
  context.py             AppContext: settings, event bus, asyncio runner, link, services
  core/
    async_runner.py      asyncio loop on a worker thread (all bleak calls run here)
    event_bus.py         thread-safe worker -> Tk messaging (LINK_STATE, LOG, TRAFFIC)
    ble_link.py          BleLink: the only bleak user; scan/connect/GATT, traffic capture
    traffic.py           TrafficEvent + TrafficTap
    decoders.py          characteristic names and payload decoders for the monitor
  protocols/
    bulkxfer.py          imports the reference client, frame decoder, BulkXferService
  features/
    base.py              Feature base class (one tab each)
    hex_upload.py        Hex Upload tab
  ui/
    main_window.py       window, menus, panes, status bar
    connection_panel.py  scan / connect / caps
    traffic_view.py      BLE traffic monitor
```

### Threading

Tk runs on the main thread. BLE work runs on an asyncio loop in a worker thread. The Tk side starts work with `ctx.run(coro, on_done=…, on_error=…, on_cancel=…)`, and those callbacks run back on the Tk thread. The worker reports to Tk only through `ctx.bus` (`post` / `call`). Never touch a widget from a coroutine: use `ctx.bus.call(fn, …)`.

## Adding a feature

1. Create `blehost/features/<name>.py` with a `Feature` subclass. Set `title`, build the tab in `build(parent)`, and react to `on_connected` / `on_disconnected`. Report `busy` and implement `cancel()` if the feature runs long operations.
2. Talk to the device only through `ctx.link` (`gatt.write_gatt_char`, `gatt.read_gatt_char`, `start_notify`) or a service in `ctx.services`. Traffic then shows in the monitor automatically.
3. If the feature has its own wire format, put the codec in `blehost/protocols/` and register its characteristics with `ctx.decoders.register(uuid, name, decoder)`. BulkXfer appTypes are named with `bulkxfer.register_app_type()`.
4. Add the class to `FEATURES` in `ble_host_gui.py`.

A connection-wide service, such as a shared protocol endpoint, registers `link.add_connect_hook()` / `add_disconnect_hook()` and goes in `ctx.services` (see `BulkXferService`).

## Planned

OOB pairing, device provisioning (CSR → signed certificate over BLE), encrypted communication, and a PC-side BulkXfer Server. Known constraints:

- bleak cannot host a GATT server, so the Server role needs another backend (for example `bless`). It will be a second link class next to `BleLink`.
- bleak's `pair()` on Windows supports basic pairing only. OOB will likely need WinRT custom pairing inside `BleLink.pair()`, which is a stub for now.
