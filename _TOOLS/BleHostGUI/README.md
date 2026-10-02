# BLE Host GUI

PC-side GUI (Tkinter + [bleak](https://github.com/hbldh/bleak), themed with [sv-ttk](https://github.com/rdbende/Sun-Valley-ttk-theme)) for the nRF54 BLE Bulk Transfer firmware. The PC is the BLE central and the BulkXfer **Client**; the board is the GATT server and BulkXfer Server. For device provisioning the PC also hosts a BulkXfer service of its own, so the board can send its CSR to it.

The window has the device in a sidebar on the left (link state, ATT MTU and CAPS above the scan list) and, on the right, one tab per feature, the application log and the BLE traffic pane. **View ▸ Theme** or the toolbar button switches between light and dark; it starts in Windows' app mode. File and View are toolbar buttons (`Alt+F`, `Alt+V`), because Windows draws a native menu bar light whatever the theme.

It covers:

- **Device** (sidebar): scan, connect, disconnect, and read the BulkXfer CAPS characteristic. The base UUID is under *Advanced*. On Windows 11, connecting also asks Windows for its throughput-optimized connection parameters (15 ms interval) for as long as the link is up. Otherwise Windows settles on about 45 ms after service discovery, which caps an upload at about 10 KB/s. The result goes to the log, and each interval change shows in the traffic monitor as `CONN`.
- **Hex Upload**: browse for an Intel HEX file, split it into contiguous segments, and send each segment as one transfer (`appType 0x10`, `[u32 LE address][data]`). The server must answer with STORED before the next segment is sent. Start and Abort buttons control the upload. Tiles show segments, size, bytes sent and the rate; a pill shows the upload's state, and each segment's status is coloured. The contract is in [_DOC/HexUpload/PROTOCOL.md](../../_DOC/HexUpload/PROTOCOL.md).
- **Provisioning**: the PC is the Certificate Authority. Create or load a CA folder, read the device's STATUS, then **Provision**: the device sends its CSR, the PC checks it and issues a device certificate, and sends it with the CA certificate; the device verifies both, stores both, deletes its CSR and logs them as PEM. **Save device certificate…** writes the result. The tab shows the CA and the device side by side, with pills for the PC service, the CA and the device's state, and a step list that follows the sequence. Provisioning is one-time: a provisioned device is refused. **Remove provisioning…** (after a confirmation) sends DEPROVISION, which wipes the device's key, CSR and certificates; the device makes a new key and CSR and can be provisioned again. The contract is in [_DOC/Provisioning/PROTOCOL.md](../../_DOC/Provisioning/PROTOCOL.md), the design in [_DOC/Provisioning/README.md](../../_DOC/Provisioning/README.md).
- **BLE traffic monitor**: every write, read, notification and link event, shown as hex and decoded BulkXfer frames (provisioning's STATUS and RESULT decoded too). Traffic of the PC's own service is marked `PC service`. Turn it on or off with **View ▸ BLE Traffic**, the toolbar button or `Ctrl+T`. It captures nothing while it is off. It stays a dark terminal in both themes; TX, RX, Events and the hide / full-payload filters are toggle buttons.

## Run

```sh
pip install -r requirements.txt
python ble_host_gui.py
```

Python 3.10 or later; `requirements.txt` installs bleak (≥ 1, for the winrt 3.x packages), cryptography and sv-ttk. Without sv-ttk (or if its theme file cannot be loaded) the GUI runs on the platform's ttk theme, light only. The BulkXfer protocol code lives in [`bulkxfer_client.py`](bulkxfer_client.py), which is also a command-line client (`python bulkxfer_client.py --help`), and [`bulkxfer_receiver.py`](bulkxfer_receiver.py) (the receiver role); keep both next to `ble_host_gui.py`.

Typical session: in the sidebar **Scan**, select `BLE Bulk Transfer`, **Connect** (or double-click it), **Read Caps**, then on the Hex Upload tab **Browse…**, **Start Upload**. The board prints each segment on its serial terminal (921600 baud, RTS/CTS).

Provisioning session: on the Provisioning tab **Create CA** once (or **Load** a folder; the default `~/.blehost/ca` holds the CA private key, keep it private), connect, **Get Status**, **Provision**. Headless: `python bulkxfer_client.py provision --name "BLE Bulk Transfer" [--ca DIR] [--out device.pem] [--negative]` (`--negative` wipes a device that is not fresh, sends the certificates it must reject and checks each answer, then provisions it) and `python bulkxfer_client.py deprovision --name "BLE Bulk Transfer"` (wipe).

The PC's own BulkXfer service is published at start-up through WinRT's `GattServiceProvider` (Windows 10/11, adapter with the peripheral role; the tab says whether it is up). `bless` is not used: it cannot be installed next to bleak ≥ 1 on Python 3.12.

**Base UUID** must match the firmware's `BaseUUIDs.h`. The default is the project base, `16a1-4812-af35-f3f29a92f6ca`. **Max segment** must not exceed the server's `DS_BUF_SIZE` (65536).

## Layout

```
ble_host_gui.py          entry point; FEATURES lists the tabs; starts the PC GATT service
bulkxfer_client.py       BulkXfer reference client (protocol + command line, incl. provision/deprovision); imported by protocols/bulkxfer.py
bulkxfer_receiver.py     BulkXfer receiver role (protocol §7), transport independent
blehost/
  context.py             AppContext: settings, event bus, asyncio runner, link, services
  core/
    async_runner.py      asyncio loop on a worker thread (all bleak calls run here)
    event_bus.py         thread-safe worker -> Tk messaging (LINK_STATE, LOG, TRAFFIC)
    ble_link.py          BleLink: the only bleak user; scan/connect/GATT, traffic capture
    gatt_server.py       PcGattServer: the PC's BulkXfer service (WinRT) + receiver; the only WinRT GATT-server user
    traffic.py           TrafficEvent + TrafficTap
    decoders.py          characteristic names and payload decoders for the monitor
  protocols/
    bulkxfer.py          imports the reference client, frame decoder, BulkXferService
    provisioning.py      provisioning appTypes, STATUS/RESULT codecs, ProvisioningSession (incl. deprovision)
  pki/
    authority.py         the CA: create/load, check a CSR, issue a device certificate
    negative.py          certificates the device must reject (host test devicecert_verify, provision --negative)
  features/
    base.py              Feature base class (one tab each)
    hex_upload.py        Hex Upload tab
    provisioning.py      Provisioning tab
  ui/
    main_window.py       window, toolbar (File / View, theme, traffic), sidebar, panes, status bar
    connection_panel.py  the sidebar: scan / connect / caps
    traffic_view.py      BLE traffic monitor
    theme.py             Theme: sv-ttk light / dark, shared styles, palettes, on_change; of(ctx)
    widgets.py           display widgets: card(), Pill, StatTile, Stepper, Disclosure
```

### Threading

Tk runs on the main thread. BLE work runs on an asyncio loop in a worker thread. The Tk side starts work with `ctx.run(coro, on_done=…, on_error=…, on_cancel=…)`, and those callbacks run back on the Tk thread. The worker reports to Tk only through `ctx.bus` (`post` / `call`). Never touch a widget from a coroutine: use `ctx.bus.call(fn, …)`.

### Look and feel

`MainWindow` creates the `Theme` (`ui/theme.py`) and puts it in `ctx.theme`; a panel gets it with `theme.of(ctx)`, which returns a passive light theme when the panel is built on its own (a test). ttk widgets follow the theme by themselves; use the shared styles (`Card.TFrame`, `Accent.TButton`, `Danger.TButton`, `Toggle.TButton`, `Caption.TLabel`, `Strong.TLabel`, `Title.TLabel`, `Value.TLabel`, `Mono.TLabel`) rather than colours or fonts of your own. Classic Tk widgets (`Text`, `Canvas`) and Treeview row tags do not follow it: register `theme.on_change(fn)`, which calls `fn(palette)` at once and after every switch (`theme.style_text()` colours a log `Text`). Status colours are the tones `ok`, `info`, `warn`, `err` and `idle`, each a (foreground, background) pair readable in both modes.

The display widgets (pills, tiles, the step list) only show state: they follow the `StringVar`s and calls the feature logic already makes (for example through `trace_add`), so the logic and its tests do not depend on them.

## Adding a feature

1. Create `blehost/features/<name>.py` with a `Feature` subclass. Set `title`, build the tab in `build(parent)` from `ui/widgets.card()` sections and the shared styles (see *Look and feel*), and react to `on_connected` / `on_disconnected`. Report `busy` and implement `cancel()` if the feature runs long operations.
2. Talk to the device only through `ctx.link` (`gatt.write_gatt_char`, `gatt.read_gatt_char`, `start_notify`) or a service in `ctx.services`. Traffic then shows in the monitor automatically.
3. If the feature has its own wire format, put the codec in `blehost/protocols/` and register its characteristics with `ctx.decoders.register(uuid, name, decoder)`. BulkXfer appTypes are named with `bulkxfer.register_app_type()`.
4. Add the class to `FEATURES` in `ble_host_gui.py`.
5. Add its tests to `_TEST/python/` with the feature: the codec and protocol logic, and the tab itself. The `conftest.py` there provides a fake GATT link and a scripted BulkXfer server, so no adapter is needed. Test the tab's logic against stand-in Tk variables and a fake context, plus one test that builds the real tab, as `test_feature_provisioning.py` does (see [_TEST/README.md](../../_TEST/README.md)).

A connection-wide service, such as a shared protocol endpoint, registers `link.add_connect_hook()` / `add_disconnect_hook()` and goes in `ctx.services` (see `BulkXferService`, and `PcGattServer` for a service the PC hosts: `ctx.services["pc_server"].receiver` receives what the device sends).

## Planned

OOB pairing (phase 2 of certificate-based authentication) and encrypted communication. Known constraint: bleak's `pair()` on Windows supports basic pairing only. OOB will likely need WinRT custom pairing inside `BleLink.pair()`, which is a stub for now.
