# SPDX-License-Identifier: MIT
"""Shared fixtures for the PC-tool tests.

Puts _TOOLS/BleHostGUI on sys.path (bulkxfer_client.py and the blehost
package live there) and provides the golden wire vectors shared with the C
tests, plus a fake GATT transport and a scripted BulkXfer server so that
BulkXferClient can be tested without a BLE adapter.
"""

import asyncio
import json
import os
import struct
import sys
import zlib

import pytest

REPO = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
GUI_DIR = os.path.join(REPO, "_TOOLS", "BleHostGUI")
if GUI_DIR not in sys.path:
    sys.path.insert(0, GUI_DIR)

import bulkxfer_client as bx  # noqa: E402


@pytest.fixture(scope="session")
def repo():
    return REPO


@pytest.fixture(scope="session")
def vectors():
    with open(os.path.join(REPO, "_TEST", "vectors", "wire.json")) as f:
        return json.load(f)


class FakeServer:
    """Scripted BulkXfer Server on the far side of a fake GATT link.

    Implements the receiver half of the protocol (ACK every window/2 frames,
    NACK on a gap, END with a CRC check) closely enough to exercise the client,
    plus knobs to misbehave. `writes` records every frame the client wrote.
    """

    def __init__(self, mtu=247, window=16):
        self.mtu_size = mtu
        self.window = window
        self.writes = []
        self.client = None                      # BulkXferClient, set by the test
        self.caps = bytes([2, 244, 16, 0])
        # knobs
        self.silent = False                     # never answer
        self.reject = None                      # status: answer START with ABORT(by receiver)
        self.drop_once = None                   # absolute frame index to lose once
        self.after_end = []                     # short frames (type, payload) to notify after END OK
        self.corrupt = False                    # END with CRC_ERROR
        # receiver state
        self.xid = None
        self.buf = bytearray()
        self.total = 0
        self.chunk = 0
        self.nxt = 0
        self.since_ack = 0
        self.nack_sent = False
        self.objects = []                       # (app_type, bytes) received OK
        self.aborted = None                     # (xid, reason, dir) of the client's ABORT

    # --- the three things BulkXferClient needs from bleak's BleakClient ----
    async def write_gatt_char(self, uuid, data, response=False):
        assert uuid == self.client.data_uuid, "client must write DATA"
        assert response is False, "client must use Write Without Response"
        data = bytes(data)
        assert len(data) <= self.mtu_size - 3
        self.writes.append(data)
        self._on_frame(data)
        await asyncio.sleep(0)

    async def read_gatt_char(self, uuid):
        assert uuid == self.client.caps_uuid
        return bytearray(self.caps)

    # --- receiver --------------------------------------------------------------
    def _notify(self, payload: bytes):
        self.client.on_notify(0, bytearray(payload))

    def _on_frame(self, f: bytes):
        if self.silent:
            return
        ftype, p = f[1], f[2:]
        if ftype == bx.T_START:
            xid, app, total, chunk, win, crc = struct.unpack("<BBIBBI", p)
            self.xid, self.app, self.total, self.chunk, self.crc = xid, app, total, chunk, crc
            self.buf, self.nxt, self.since_ack, self.nack_sent = bytearray(), 0, 0, False
            if self.reject is not None:
                self._notify(bx.frame(bx.T_ABORT, bytes([xid, self.reject, bx.ABORT_BY_RECEIVER])))
                return
            if total == 0:
                self._end()
                return
            self._notify(bx.frame(bx.T_ACK, bytes([xid, 0, min(win, self.window)])))
        elif ftype == bx.T_DATA and p[0] == self.xid:
            seq, data = p[1], p[2:]
            if seq == (self.nxt & 0xFF):
                if self.drop_once == self.nxt:
                    self.drop_once = None
                    return
                self.buf += data
                self.nxt += 1
                self.since_ack += 1
                self.nack_sent = False
                if len(self.buf) >= self.total:
                    self._end()
                elif self.since_ack >= self.window // 2:
                    self.since_ack = 0
                    self._notify(bx.frame(bx.T_ACK, bytes([self.xid, self.nxt & 0xFF, self.window])))
            elif not self.nack_sent:
                self.nack_sent = True
                self._notify(bx.frame(bx.T_NACK, bytes([self.xid, self.nxt & 0xFF, 11])))
        elif ftype == bx.T_ABORT:
            self.aborted = (p[0], p[1], p[2])

    def _end(self):
        ok = zlib.crc32(bytes(self.buf)) == self.crc and not self.corrupt
        self._notify(bx.frame(bx.T_END, bytes([self.xid, 0 if ok else 1])))
        if ok:
            self.objects.append((self.app, bytes(self.buf)))
            for t, payload in self.after_end:
                self._notify(bx.frame(t, payload))


@pytest.fixture
def server():
    return FakeServer()


@pytest.fixture
def client(server):
    c = bx.BulkXferClient(server, log=lambda *_: None)
    server.client = c
    return c


@pytest.fixture
def fast_timeouts(monkeypatch):
    """Shrink the client's 1 s ACK timeout so timeout paths run quickly."""
    monkeypatch.setattr(bx, "ACK_TIMEOUT", 0.02)
