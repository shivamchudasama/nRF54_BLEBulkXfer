# SPDX-License-Identifier: MIT
"""Shared fixtures for the PC-tool tests.

Puts _TOOLS/BleHostGUI on sys.path (bulkxfer_client.py and the blehost
package live there) and provides the golden wire vectors shared with the C
tests, plus a fake GATT transport and a scripted BulkXfer server so that
BulkXferClient can be tested without a BLE adapter, and one shared Tk root
for the tests that build widgets.
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


@pytest.fixture(scope="session")
def _tk_session():
    """One Tk interpreter for the whole run: on Windows, creating several in
    one process now and then fails to find tk.tcl. None without a display."""
    try:
        import tkinter as tk
        root = tk.Tk()
    except Exception as e:                       # ImportError, TclError
        yield e
        return
    root.withdraw()
    yield root
    root.destroy()


@pytest.fixture
def tk_root(_tk_session):
    """The shared Tk root (skips without a display); widgets a test creates
    on it are destroyed after the test."""
    if isinstance(_tk_session, Exception):
        pytest.skip(f"no display for Tk ({_tk_session})")
    yield _tk_session
    for w in _tk_session.winfo_children():
        w.destroy()


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


class FakeFsDevice:
    """The device's file commands (_DOC/FileSysManager/PROTOCOL.md) and the
    hex upload's BEGIN / COMMIT, scripted in Python with the firmware's rules:
    paths ("/FLASH_DISK:/x" as is, "/x" from the root, a relative directory
    from the root and a relative file from the current directory), one open
    file, opening for writing does not truncate, LS lists the root and one
    level, DELDIR deletes files but refuses subdirectories, an ABORT without an
    open file resets the current directory. handle() returns the notifications
    as (appType, payload). Knobs: busy, silent, fail (op name -> status)."""

    ROOT = "/FLASH_DISK:"
    OPS = {"MKDIR": 1, "CD": 2, "OPENR": 3, "OPENW": 4, "WRITE": 5, "READ": 6, "LS": 7,
           "DELFILE": 8, "DELDIR": 9, "CLOSE": 10, "ABORT": 11}
    OK, NOT_FOUND, BAD_ARG, BAD_STATE, BUSY, NOT_SUPPORTED, WRONG_TYPE = 0, 1, 5, 6, 7, 10, 11

    def __init__(self):
        self.files = {}                       # path -> bytearray
        self.dirs = set()
        self.cwd = self.ROOT
        self.open = None                      # [path, mode, position, written]
        self.busy = False
        self.silent = False
        self.fail = {}
        self.commands = []                    # (seq, op, arg) received
        self.upload = None                    # bytearray between BEGIN and COMMIT
        self.upload_name = None

    def _path(self, arg: bytes, is_dir: bool) -> str:
        name = arg.decode()
        if name.startswith("/") and ":" in name:
            return name
        if name.startswith("/"):
            return f"{self.ROOT}/{name[1:]}"
        return f"{self.ROOT if is_dir else self.cwd}/{name}"

    def _parent_ok(self, path: str) -> bool:
        parent = path.rsplit("/", 1)[0]
        return parent == self.ROOT or parent in self.dirs

    def _children(self, d: str):
        for p in sorted(self.dirs | set(self.files)):
            if p.rsplit("/", 1)[0] == d:
                yield p

    def handle(self, payload: bytes) -> list:
        seq, op, arg = payload[0], payload[1], bytes(payload[2:])
        self.commands.append((seq, op, arg))

        def reply(st, data=b""):
            return [(0x41, bytes([seq, op, st]) + data)]

        if self.silent:
            return []
        if self.busy:
            return reply(self.BUSY)
        name = next((k for k, v in self.OPS.items() if v == op), None)
        if name is None:
            return reply(self.BAD_ARG)
        if name in self.fail:
            if name in ("WRITE", "READ", "CLOSE"):
                self.open = None
            return reply(self.fail[name])
        o = self.open
        if name in ("WRITE", "READ") and o is None:
            return reply(self.BAD_STATE)
        if o is not None and name not in ("WRITE", "READ", "CLOSE", "ABORT"):
            return reply(self.BAD_STATE)
        if name in ("MKDIR", "CD", "OPENR", "OPENW", "DELFILE", "DELDIR") and arg.lstrip(b"/") == b"":
            return reply(self.BAD_ARG)
        if name in ("MKDIR", "CD"):
            p = self._path(arg, True)
            if name == "MKDIR":
                if not self._parent_ok(p):
                    return reply(self.NOT_FOUND)
                self.dirs.add(p)
            elif p not in self.dirs and p != self.ROOT:
                return reply(self.NOT_FOUND if p not in self.files else self.WRONG_TYPE)
            self.cwd = p
            return reply(self.OK, p.encode())
        if name in ("OPENR", "OPENW"):
            p = self._path(arg, False)
            if name == "OPENR" and p not in self.files:
                return reply(self.NOT_FOUND)
            if name == "OPENW":
                if not self._parent_ok(p):
                    return reply(self.NOT_FOUND)
                self.files.setdefault(p, bytearray())
            self.open = [p, name, 0, 0]
            return reply(self.OK, p.encode())
        if name == "WRITE":
            if o[1] != "OPENW":
                return reply(self.BAD_STATE)
            if not arg:
                return reply(self.BAD_ARG)
            f = self.files[o[0]]
            f[o[2]:o[2] + len(arg)] = arg
            o[2] += len(arg)
            o[3] += len(arg)
            return reply(self.OK, struct.pack("<I", o[3]))
        if name == "READ":
            if o[1] != "OPENR":
                return reply(self.BAD_STATE)
            n = struct.unpack("<H", arg)[0] if len(arg) == 2 else 239
            n = 239 if n == 0 or n > 239 else n
            data = bytes(self.files[o[0]][o[2]:o[2] + n])
            o[2] += len(data)
            return reply(self.OK, data)
        if name == "LS":
            out, n = [], 0
            for p in self._children(self.ROOT):
                for q in [p] + (list(self._children(p)) if p in self.dirs else []):
                    is_dir = q in self.dirs
                    size = 0 if is_dir else len(self.files[q])
                    out.append((0x42, struct.pack("<BBI", seq, 1 if is_dir else 0, size) + q.encode()))
                    n += 1
            return out + reply(self.OK, struct.pack("<H", n))
        if name in ("DELFILE", "DELDIR"):
            p = self._path(arg, name == "DELDIR")
            if name == "DELFILE":
                if p in self.dirs:
                    return reply(self.WRONG_TYPE)
                if p not in self.files:
                    return reply(self.NOT_FOUND)
                del self.files[p]
                return reply(self.OK)
            if p not in self.dirs:
                return reply(self.NOT_FOUND if p not in self.files else self.WRONG_TYPE)
            kids = list(self._children(p))
            if any(k in self.dirs for k in kids):
                return reply(self.NOT_SUPPORTED)
            for k in kids:
                del self.files[k]
            self.dirs.discard(p)
            return reply(self.OK)
        if name == "CLOSE":
            total = o[3] if o is not None else 0
            self.open = None
            return reply(self.OK, struct.pack("<I", total))
        # ABORT
        if o is None:
            self.cwd = self.ROOT
        self.open = None
        return reply(self.OK)

    # ---- hex upload stored as a file ------------------------------------------------
    def begin(self, name: bytes) -> tuple:
        if self.busy:
            return (0x14, struct.pack("<BBII", 0x12, self.BUSY, 0, 0))
        self.upload, self.upload_name = bytearray(), name.decode()
        return (0x14, struct.pack("<BBII", 0x12, self.OK, 0, 0))

    def segment(self, obj: bytes):
        if self.upload is not None:
            self.upload += struct.pack("<II", struct.unpack("<I", obj[:4])[0], len(obj) - 4) + obj[4:]

    def commit(self) -> tuple:
        if self.upload is None:
            return (0x14, struct.pack("<BBII", 0x13, self.BAD_STATE, 0, 0))
        data, self.upload = bytes(self.upload), None
        self.files[f"{self.ROOT}/FW/{self.upload_name}"] = bytearray(data)
        return (0x14, struct.pack("<BBII", 0x13, self.OK, len(data), zlib.crc32(data)))


class FakeBlk:
    """What FileSystemSession needs from a BulkXferClient: send_short and
    short_q, wired to a FakeFsDevice."""

    def __init__(self, device: FakeFsDevice):
        self.device = device
        self.short_q = asyncio.Queue()
        self.sent = []

    async def send_short(self, app_type: int, payload: bytes):
        self.sent.append((app_type, bytes(payload)))
        if app_type == 0x40:
            for t, p in self.device.handle(bytes(payload)):
                self.short_q.put_nowait((t, p))
        await asyncio.sleep(0)


@pytest.fixture
def fsdev():
    return FakeFsDevice()
