"""The BLE central link: the only module that talks to bleak.

Features and protocol modules never hold a BleakClient. They use:
  - BleLink.gatt: a GattFacade with write_gatt_char / read_gatt_char / mtu_size,
    which reports every access to the traffic monitor;
  - BleLink.start_notify(), which reports every notification;
  - connect/disconnect hooks, run on the asyncio loop.

All coroutines here run on the AsyncRunner loop. State changes are posted to
the EventBus (LINK_STATE) for the Tk side.
"""

import enum
from dataclasses import dataclass, field

from .event_bus import LINK_STATE, LOG

CCCD_NOTIFY = b"\x01\x00"


class LinkState(enum.Enum):
    IDLE = "Idle"
    SCANNING = "Scanning"
    CONNECTING = "Connecting"
    CONNECTED = "Connected"
    DISCONNECTING = "Disconnecting"


@dataclass
class ScanResult:
    address: str
    name: str
    rssi: int
    service_uuids: list = field(default_factory=list)


class GattFacade:
    """The subset of BleakClient that protocol code uses, with traffic capture."""

    def __init__(self, client, tap):
        self._client = client
        self._tap = tap

    @property
    def mtu_size(self) -> int:
        return self._client.mtu_size

    async def write_gatt_char(self, uuid, data, response: bool = False):
        self._tap.tx("WRITE" if response else "WNR", uuid, data)
        await self._client.write_gatt_char(uuid, data, response=response)

    async def read_gatt_char(self, uuid) -> bytes:
        data = bytes(await self._client.read_gatt_char(uuid))
        self._tap.rx("READ", uuid, data)
        return data

    def has_characteristic(self, uuid) -> bool:
        return self._client.services.get_characteristic(uuid) is not None


class BleLink:
    def __init__(self, bus, tap):
        self._bus = bus
        self._tap = tap
        self._devices = {}              # address -> BLEDevice from the last scan
        self._client = None
        self.gatt: GattFacade | None = None
        self.state = LinkState.IDLE
        self.info = {}
        self._connect_hooks = []
        self._disconnect_hooks = []

    # ---- hooks -------------------------------------------------------------
    def add_connect_hook(self, coro_fn):
        """await coro_fn(link) after connecting, before the state becomes CONNECTED.
        An exception is logged and does not drop the link."""
        self._connect_hooks.append(coro_fn)

    def add_disconnect_hook(self, fn):
        """fn(link, reason) on the asyncio loop once the link is gone."""
        self._disconnect_hooks.append(fn)

    # ---- state -------------------------------------------------------------
    @property
    def connected(self) -> bool:
        return self.state == LinkState.CONNECTED

    def _set_state(self, state: LinkState, **info):
        self.state = state
        self.info = info
        self._bus.post(LINK_STATE, (state, dict(info)))

    def _log(self, level, text):
        self._bus.post(LOG, (level, text))

    # ---- scan --------------------------------------------------------------
    async def scan(self, timeout: float = 5.0) -> list:
        from bleak import BleakScanner

        self._set_state(LinkState.SCANNING)
        self._tap.info("SCAN", f"scanning {timeout:.0f} s")
        try:
            found = await BleakScanner.discover(timeout=timeout, return_adv=True)
        finally:
            self._set_state(LinkState.IDLE)
        self._devices = {}
        results = []
        for addr, (dev, adv) in found.items():
            self._devices[addr] = dev
            results.append(ScanResult(addr, adv.local_name or dev.name or "", adv.rssi,
                                      [u.lower() for u in adv.service_uuids]))
        self._tap.info("SCAN", f"{len(results)} device(s) found")
        return results

    # ---- connect / disconnect ----------------------------------------------
    async def connect(self, address: str, name: str = "", timeout: float = 15.0):
        from bleak import BleakClient

        if self._client is not None:
            raise RuntimeError("already connected")
        target = self._devices.get(address, address)
        self._set_state(LinkState.CONNECTING, address=address, name=name)
        self._tap.info("CONNECT", f"connecting to {name or '?'} [{address}]")
        client = BleakClient(target, disconnected_callback=self._on_bleak_disconnect, timeout=timeout)
        try:
            await client.connect()
        except BaseException:
            self._set_state(LinkState.IDLE)
            raise
        self._client = client
        self.gatt = GattFacade(client, self._tap)
        self._tap.info("CONNECT", f"connected, ATT MTU {client.mtu_size}")
        for hook in self._connect_hooks:
            try:
                await hook(self)
            except Exception as e:
                self._log("warn", f"connect hook {getattr(hook, '__qualname__', hook)}: {e}")
        if self._client is client:          # the link can drop while the hooks run
            self._set_state(LinkState.CONNECTED, address=address, name=name, mtu=client.mtu_size)

    async def disconnect(self):
        client = self._client
        if client is None:
            return
        self._set_state(LinkState.DISCONNECTING, **self.info)
        try:
            await client.disconnect()
        finally:
            self._teardown(client, "disconnected")

    def _on_bleak_disconnect(self, client):
        # bleak calls this on the loop for both local and remote disconnects
        self._teardown(client, "link lost")

    def _teardown(self, client, reason: str):
        if self._client is not client:
            return                          # already torn down
        self._client = None
        self.gatt = None
        self._tap.info("DISCONNECT", reason)
        for hook in self._disconnect_hooks:
            try:
                hook(self, reason)
            except Exception as e:
                self._log("warn", f"disconnect hook: {e}")
        self._set_state(LinkState.IDLE, reason=reason)

    # ---- GATT --------------------------------------------------------------
    @property
    def mtu_size(self) -> int:
        return self._client.mtu_size if self._client else 0

    async def start_notify(self, uuid: str, callback):
        """Subscribe; callback(handle, data) runs on the loop after the notification
        is reported to the traffic monitor."""
        tap = self._tap

        def wrapped(sender, data):
            tap.rx("NOTIFY", uuid, data)
            callback(sender, data)

        tap.tx("CCCD", uuid, CCCD_NOTIFY, "subscribe")
        await self._client.start_notify(uuid, wrapped)

    async def stop_notify(self, uuid: str):
        self._tap.tx("CCCD", uuid, b"\x00\x00", "unsubscribe")
        await self._client.stop_notify(uuid)

    # ---- security (planned) ------------------------------------------------
    async def pair(self, **kwargs):
        """Reserved for the security stage (OOB pairing). bleak's pair() on Windows
        only covers basic pairing, so OOB will need WinRT custom pairing here."""
        raise NotImplementedError("pairing is not implemented yet")

    async def unpair(self):
        raise NotImplementedError("pairing is not implemented yet")
