"""The PC's own SETU GATT service, so the device can send to the PC.

A SETU transfer goes from the GATT client to the side that hosts DATA and
CTRL (_DOC/SETU/PROTOCOL.md). bleak is a GATT client only, so for the
device -> PC direction (the CSR during provisioning) the PC hosts the service
itself, and the device attaches its SETU Client role to it over the same
connection. The service uses the same UUIDs as the device's (base from the
settings): DATA (Write Without Response), CTRL (Notify) and CAPS (Read).

PcGattServer is the SETUReceiver (setu_receiver.py) plus a backend
that hosts the attributes. WinRtBackend, the only code in the tool that uses
the WinRT GATT *server* API, needs Windows 10/11 and an adapter that supports
the peripheral role; it uses the winrt packages bleak already installs.
(bless, the usual Python GATT server library, cannot be installed next to
bleak >= 1 on Python 3.12: its winrt pins conflict.)

Threading: WinRT raises its events on its own threads. The backend hands each
written value to the asyncio loop in arrival order; the receiver runs on the
loop only, as BleLink's code does.
"""

import asyncio
import sys
import uuid

import setu_client
from setu_receiver import SETUReceiver

CAPS = bytes([2, setu_client.MAX_FRAME, setu_client.WINDOW, 0])    # as the device's gv_SETUS_GetCaps()


class GattServerUnavailable(RuntimeError):
    """This PC cannot host the service (OS, adapter or driver)."""


class WinRtBackend:
    """Hosts the SETU service with WinRT's GattServiceProvider.

    on_write(data: bytes) and on_subscribed(count: int) are called on `loop`.
    """

    def __init__(self, base: str, loop, on_write, on_subscribed, log):
        self.base = base
        self.loop = loop
        self.on_write = on_write
        self.on_subscribed = on_subscribed
        self.log = log
        self._provider = None
        self._ctrl = None
        self._tokens = []
        self._writes: asyncio.Queue = asyncio.Queue()
        self._pump = None

    def _uuid(self, char_id: int):
        return uuid.UUID(setu_client.char_uuid(char_id, self.base))

    async def start(self):
        if sys.platform != "win32":
            raise GattServerUnavailable("hosting a GATT service needs Windows (WinRT)")
        try:
            from winrt.windows.devices.bluetooth import BluetoothAdapter, BluetoothError
            from winrt.windows.devices.bluetooth.genericattributeprofile import (
                GattCharacteristicProperties as P, GattLocalCharacteristicParameters,
                GattProtectionLevel, GattServiceProvider, GattServiceProviderAdvertisingParameters)
            from winrt.windows.storage.streams import DataWriter
        except ImportError as e:
            raise GattServerUnavailable(f"WinRT Bluetooth packages missing ({e})") from None

        adapter = await BluetoothAdapter.get_default_async()
        if adapter is None or not adapter.is_peripheral_role_supported:
            raise GattServerUnavailable("the Bluetooth adapter does not support the peripheral role")

        res = await GattServiceProvider.create_async(self._uuid(0))
        if res.error != BluetoothError.SUCCESS:
            raise GattServerUnavailable(f"GattServiceProvider: {BluetoothError(res.error).name}")
        self._provider = res.service_provider
        service = self._provider.service

        async def add(char_id, props, value=None):
            p = GattLocalCharacteristicParameters()
            p.characteristic_properties = props
            p.read_protection_level = GattProtectionLevel.PLAIN
            p.write_protection_level = GattProtectionLevel.PLAIN
            if value is not None:
                w = DataWriter()
                w.write_bytes(value)
                p.static_value = w.detach_buffer()
            r = await service.create_characteristic_async(self._uuid(char_id), p)
            if r.error != BluetoothError.SUCCESS:
                raise GattServerUnavailable(f"characteristic {char_id}: {BluetoothError(r.error).name}")
            return r.characteristic

        data = await add(1, P.WRITE_WITHOUT_RESPONSE | P.WRITE)
        self._ctrl = await add(2, P.NOTIFY)
        await add(3, P.READ, CAPS)
        self._tokens.append((data, data.add_write_requested(self._write_requested)))
        self._tokens.append((self._ctrl, self._ctrl.add_subscribed_clients_changed(self._subscribed_changed)))
        self._pump = asyncio.ensure_future(self._pump_writes())

        # Starting the advertisement is what publishes the service in Windows'
        # GATT database, where the connected device discovers it. The device
        # never scans for it; discoverable + connectable is the configuration
        # verified on Windows 11.
        ap = GattServiceProviderAdvertisingParameters()
        ap.is_connectable = True
        ap.is_discoverable = True
        self._provider.start_advertising_with_parameters(ap)

    async def stop(self):
        if self._pump is not None:
            self._pump.cancel()
            self._pump = None
        for obj, token in self._tokens:
            try:
                if obj is self._ctrl:
                    obj.remove_subscribed_clients_changed(token)
                else:
                    obj.remove_write_requested(token)
            except Exception:
                pass
        self._tokens = []
        if self._provider is not None:
            try:
                self._provider.stop_advertising()
            except Exception:
                pass
            self._provider = None

    # ---- WinRT threads -> loop ------------------------------------------------
    def _write_requested(self, _sender, args):
        deferral = args.get_deferral()
        self.loop.call_soon_threadsafe(self._writes.put_nowait, (args, deferral))

    def _subscribed_changed(self, sender, _args):
        n = len(list(sender.subscribed_clients))
        self.loop.call_soon_threadsafe(self.on_subscribed, n)

    async def _pump_writes(self):
        """One write at a time, in arrival order (DATA frames must not overtake)."""
        from winrt.windows.devices.bluetooth.genericattributeprofile import GattWriteOption
        from winrt.windows.storage.streams import DataReader

        while True:
            args, deferral = await self._writes.get()
            try:
                request = await args.get_request_async()
                reader = DataReader.from_buffer(request.value)
                data = bytes(reader.read_byte() for _ in range(reader.unconsumed_buffer_length))
                if request.option == GattWriteOption.WRITE_WITH_RESPONSE:
                    request.respond()
                self.on_write(data)
            except Exception as e:
                self.log(f"PC GATT server: write failed: {e}")
            finally:
                deferral.complete()

    # ---- loop -> WinRT ----------------------------------------------------------
    def notify(self, frame: bytes):
        from winrt.windows.storage.streams import DataWriter

        if self._ctrl is None:
            return
        w = DataWriter()
        w.write_bytes(frame)
        op = self._ctrl.notify_value_async(w.detach_buffer())   # starts now: order kept
        asyncio.ensure_future(self._await_notify(op))

    async def _await_notify(self, op):
        try:
            await op
        except Exception as e:
            self.log(f"PC GATT server: notification failed: {e}")


class PcGattServer:
    """The PC's SETU service and its receiver.

    tap: TrafficTap (or None) to show the device's writes and the PC's
    notifications in the traffic monitor. backend_factory(server) -> backend
    is for tests; the default is WinRtBackend.
    """

    NAME = "pc_server"

    def __init__(self, base: str, log=print, tap=None, backend_factory=None):
        self.base = base
        self.log = log
        self.tap = tap
        self.backend = None
        self.available = False
        self.error = None
        self.subscribers = 0
        self.receiver = SETUReceiver(self._notify, log=lambda t: self.log(t))
        self.receiver.subscribed = False
        self._backend_factory = backend_factory

    async def start(self):
        """Publish the service (on the loop that will run the receiver)."""
        loop = asyncio.get_running_loop()
        factory = self._backend_factory or (lambda s: WinRtBackend(
            s.base, loop, s._on_write, s._on_subscribed, s.log))
        backend = factory(self)
        try:
            await backend.start()
        except Exception as e:
            self.available, self.error = False, str(e)
            raise
        self.backend = backend
        self.available, self.error = True, None

    async def stop(self):
        if self.backend is not None:
            await self.backend.stop()
            self.backend = None
        self.available = False

    def link_lost(self):
        """The BLE connection is gone: a running transfer fails as DISCONNECTED."""
        self.receiver.disconnected()
        self._on_subscribed(0)

    # ---- backend callbacks (on the loop) -----------------------------------------
    def _on_write(self, data: bytes):
        if self.tap is not None:
            self.tap.rx("WNR", setu_client.char_uuid(1, self.base), data, "PC service")
        self.receiver.on_write(data)

    def _on_subscribed(self, count: int):
        self.subscribers = count
        self.receiver.subscribed = count > 0       # PROTOCOL.md §7.1 step 1

    def _notify(self, frame: bytes):
        if self.tap is not None:
            self.tap.tx("NOTIFY", setu_client.char_uuid(2, self.base), frame, "PC service")
        if self.backend is not None:
            self.backend.notify(frame)
