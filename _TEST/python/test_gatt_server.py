# SPDX-License-Identifier: MIT
"""PcGattServer (_TOOLS/BleHostGUI/blehost/core/gatt_server.py): the PC's
BulkXfer service, with a fake backend in place of WinRT. The WinRT backend
itself needs Windows and a Bluetooth adapter (hil-tests)."""

import asyncio

import pytest

import bulkxfer_client as bx
from blehost.core import gatt_server as gs

BASE = "16a1-4812-af35-f3f29a92f6ca"


class FakeBackend:
    def __init__(self, server, fail=None):
        self.server = server
        self.fail = fail
        self.started = False
        self.notified = []
        self.client = None                  # a BulkXferClient on the device side

    async def start(self):
        if self.fail:
            raise gs.GattServerUnavailable(self.fail)
        self.started = True

    async def stop(self):
        self.started = False

    def notify(self, frame):
        self.notified.append(bytes(frame))
        if self.client is not None:
            self.client.on_notify(0, bytearray(frame))


class Tap:
    def __init__(self):
        self.events = []

    def rx(self, op, uuid, data, note=""):
        self.events.append(("RX", op, uuid, bytes(data), note))

    def tx(self, op, uuid, data, note=""):
        self.events.append(("TX", op, uuid, bytes(data), note))


class DeviceLink:
    """The device's BulkXfer Client writes into the PC's DATA."""

    def __init__(self, server):
        self.server = server
        self.mtu_size = 247

    async def write_gatt_char(self, uuid, data, response=False):
        assert uuid == bx.char_uuid(1, BASE)
        self.server._on_write(bytes(data))
        await asyncio.sleep(0)


def make(tap=None, fail=None):
    backends = []

    def factory(server):
        backends.append(FakeBackend(server, fail))
        return backends[-1]
    return gs.PcGattServer(BASE, log=lambda *_: None, tap=tap, backend_factory=factory), backends


def test_start_and_stop():
    async def go():
        srv, b = make()
        await srv.start()
        up = srv.available and b[0].started
        await srv.stop()
        return up, srv.available, b[0].started
    assert run(go()) == (True, False, False)


def test_start_failure_is_reported():
    async def go():
        srv, _ = make(fail="no peripheral role")
        with pytest.raises(gs.GattServerUnavailable):
            await srv.start()
        return srv
    srv = run(go())
    assert not srv.available and srv.error == "no peripheral role"


def test_device_sends_an_object_to_the_pc():
    tap = Tap()

    async def go():
        srv, b = make(tap)
        await srv.start()
        dev = bx.BulkXferClient(DeviceLink(srv), BASE, log=lambda *_: None)
        b[0].client = dev
        srv._on_subscribed(1)                       # the device subscribed to CTRL
        status = await dev.send(0x23, bytes(range(256)) * 2)
        return status, await srv.receiver.receive(0x23, timeout=1.0), b[0]
    status, r, backend = run(go())
    assert status == "OK" and r.status == "OK" and r.data == bytes(range(256)) * 2
    assert backend.notified[-1][1] == bx.T_END
    assert ("RX", "WNR", bx.char_uuid(1, BASE)) == tap.events[0][:3]
    assert any(e[0] == "TX" and e[2] == bx.char_uuid(2, BASE) and e[4] == "PC service"
               for e in tap.events)


def test_start_is_ignored_until_the_device_subscribes():
    async def go():
        srv, b = make()
        await srv.start()
        start = bx.frame(bx.T_START, bytes([1, 0x23, 10, 0, 0, 0, 240, 16, 0, 0, 0, 0]))
        srv._on_write(start)
        before = list(b[0].notified)
        srv._on_subscribed(1)
        srv._on_write(start)
        return before, b[0].notified
    before, after = run(go())
    assert before == [] and after[0][1] == bx.T_ACK


def test_link_lost_fails_the_transfer():
    async def go():
        srv, _ = make()
        await srv.start()
        srv._on_subscribed(1)
        srv._on_write(bx.frame(bx.T_START, bytes([1, 0x23, 0, 1, 0, 0, 240, 16, 0, 0, 0, 0])))
        srv.link_lost()
        return srv, await srv.receiver.receive(timeout=1.0)
    srv, r = run(go())
    assert r.status == "DISCONNECTED" and not srv.receiver.subscribed and srv.subscribers == 0


def test_caps_value_matches_the_device():
    assert gs.CAPS == bytes([2, 244, 16, 0])


def test_winrt_backend_needs_windows(monkeypatch):
    monkeypatch.setattr(gs.sys, "platform", "linux")
    backend = gs.WinRtBackend(BASE, None, None, None, print)
    with pytest.raises(gs.GattServerUnavailable, match="Windows"):
        run(backend.start())


def run(coro):
    return asyncio.run(coro)
