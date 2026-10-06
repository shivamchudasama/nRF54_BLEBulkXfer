# SPDX-License-Identifier: MIT
"""BleLink (_TOOLS/BleHostGUI/blehost/core/ble_link.py) with a fake `bleak`:
the main link drives the window (LINK_STATE), a feature's own links
(primary=False, as the Pairing page makes) do not, and their traffic is tagged
with their label."""

import asyncio
import sys
import types

import pytest

from blehost.core.ble_link import BleLink, LinkState
from blehost.core.event_bus import LINK_STATE, TRAFFIC
from blehost.core.traffic import TrafficTap


class FakeClient:
    def __init__(self, target, disconnected_callback=None, timeout=15.0):
        self.target = target
        self.mtu_size = 247
        self.writes = []

    async def connect(self):
        pass

    async def disconnect(self):
        pass

    async def write_gatt_char(self, uuid, data, response=False):
        self.writes.append((uuid, bytes(data), response))

    async def read_gatt_char(self, uuid):
        return b"\x07"

    async def start_notify(self, uuid, cb):
        self.cb = cb


class Bus:
    def __init__(self):
        self.posts = []

    def post(self, topic, payload=None):
        self.posts.append((topic, payload))


@pytest.fixture
def bleak(monkeypatch):
    mod = types.ModuleType("bleak")
    mod.BleakClient = FakeClient
    monkeypatch.setitem(sys.modules, "bleak", mod)
    monkeypatch.setattr(sys, "platform", "linux")    # no WinRT connection parameters
    return mod


def _session(link):
    async def go():
        await link.connect("AA:BB", "dev")
        await link.gatt.write_gatt_char("u-1", b"\x01", response=True)
        await link.gatt.read_gatt_char("u-2")
        await link.start_notify("u-2", lambda s, d: None)
        link._client.cb(None, b"\x08")
        await link.disconnect()
    asyncio.run(go())


def test_main_link_drives_the_window(bleak):
    bus = Bus()
    tap = TrafficTap(bus)
    tap.enabled = True
    link = BleLink(bus, tap)
    _session(link)
    states = [p[0] for t, p in bus.posts if t == LINK_STATE]
    assert states == [LinkState.CONNECTING, LinkState.CONNECTED, LinkState.DISCONNECTING, LinkState.IDLE]
    notes = [p.note for t, p in bus.posts if t == TRAFFIC]
    assert not any(n.startswith("[") for n in notes)


def test_a_features_link_is_its_own(bleak):
    bus = Bus()
    tap = TrafficTap(bus)
    tap.enabled = True
    link = BleLink(bus, tap, label="central", primary=False)
    _session(link)
    assert not [p for t, p in bus.posts if t == LINK_STATE], "no LINK_STATE from a feature's link"
    assert link.state == LinkState.IDLE, "its own state still follows"
    events = [p for t, p in bus.posts if t == TRAFFIC]
    infos = [e.note for e in events if e.dir == "--"]
    assert "[central] connecting to dev [AA:BB]" in infos
    assert "[central] connected, ATT MTU 247" in infos and "[central] disconnected" in infos
    tagged = {(e.op, e.note) for e in events if e.dir != "--"}
    assert {("WRITE", "central"), ("READ", "central"), ("NOTIFY", "central"),
            ("CCCD", "[central] subscribe")} <= tagged
