# SPDX-License-Identifier: MIT
"""The Hex Upload page's "Store as" (_TOOLS/BleHostGUI/blehost/features/hex_upload.py):
the name check, and an upload stored as a file - BEGIN, the segments, COMMIT and
the size / CRC check - on the BLE loop's coroutine, against conftest's scripted
BulkXfer server with a FakeFsDevice behind it."""

import asyncio
import struct
import types
import zlib

import pytest

import bulkxfer_client as bx
from blehost.features import hex_upload as hu
from blehost.protocols import bulkxfer as bxproto
from conftest import FakeServer


class StoringServer(FakeServer):
    """FakeServer that answers BEGIN / COMMIT and STORED like the device."""

    def __init__(self, device):
        super().__init__()
        self.device = device

    def _on_frame(self, f):
        t, p = f[1], f[2:]
        if t == bx.APP_TYPE_BEGIN:
            self._notify(bx.frame(*self.device.begin(p)))
        elif t == bx.APP_TYPE_COMMIT:
            self._notify(bx.frame(*self.device.commit()))
        else:
            super()._on_frame(f)

    def _end(self):
        n = len(self.objects)
        super()._end()
        if len(self.objects) > n:
            obj = self.objects[-1][1]
            self.device.segment(obj)
            self._notify(bx.frame(bx.APP_TYPE_STORED,
                                  struct.pack("<BII", 0, struct.unpack("<I", obj[:4])[0], len(obj) - 4)))


class Ctx:
    def __init__(self, server):
        self.logs = []
        self.bus = types.SimpleNamespace(call=lambda fn, *a: None)
        self.link = types.SimpleNamespace(connected=True, mtu_size=247)

        def new_client():
            c = bx.BulkXferClient(server, log=lambda *_: None)
            server.client = c
            return c
        self.services = {bxproto.BulkXferService.NAME: types.SimpleNamespace(new_client=new_client)}

    def log(self, text, level="info"):
        self.logs.append((level, text))


SEGMENTS = [(0x0000, bytes(range(32))), (0x1000, bytes(range(100, 116)))]


@pytest.fixture
def page(fsdev):
    server = StoringServer(fsdev)
    return hu.HexUploadFeature(Ctx(server))


def test_store_name(page):
    page.store_var = types.SimpleNamespace(get=lambda: "  ")
    assert page._store_name() is None
    page.store_var = types.SimpleNamespace(get=lambda: " APP1.BIN ")
    assert page._store_name() == "APP1.BIN"
    for bad in ("a/b", "x" * 33, "upload.tmp"):
        page.store_var = types.SimpleNamespace(get=lambda b=bad: b)
        with pytest.raises(ValueError, match="not a file name"):
            page._store_name()
    del page.store_var
    assert page._store_name() is None, "before the page is built"


def test_upload_stored_as_file(page, fsdev):
    assert asyncio.run(page._upload(list(SEGMENTS), "APP1.BIN")) == 2
    image = bx.file_image(SEGMENTS)
    assert fsdev.files["/FLASH_DISK:/FW/APP1.BIN"] == image
    assert ("info", f"hex: stored as /FLASH_DISK:/FW/APP1.BIN, {len(image)} B, "
                    f"crc 0x{zlib.crc32(image):08X}") in page.ctx.logs


def test_upload_without_store_name_sends_no_begin(page, fsdev):
    assert asyncio.run(page._upload(list(SEGMENTS))) == 2
    assert fsdev.files == {} and fsdev.upload is None


def test_store_refused(page, fsdev):
    fsdev.busy = True
    with pytest.raises(RuntimeError, match="storing as APP refused: BUSY"):
        asyncio.run(page._upload(list(SEGMENTS), "APP"))


def test_commit_failure_and_mismatch(page, fsdev):
    fsdev.commit = lambda: (0x14, struct.pack("<BBII", 0x13, 4, 0, 0))
    with pytest.raises(RuntimeError, match="file APP not stored: NO_SPACE"):
        asyncio.run(page._upload(list(SEGMENTS), "APP"))
    fsdev.commit = lambda: (0x14, struct.pack("<BBII", 0x13, 0, 1, 2))
    with pytest.raises(RuntimeError, match="device has 1 B crc 0x00000002"):
        asyncio.run(page._upload(list(SEGMENTS), "APP"))


def test_monitor_names_begin_commit_file(vectors):
    def shown(name):
        v = next(s for s in vectors["hex_file"]["shorts"] if s["name"] == name)
        return bxproto.decode_frame(bytes.fromhex(v["hex"]))[1]
    assert shown("begin_fw1") == "BEGIN FW1"
    assert shown("commit") == "COMMIT "
    rec = vectors["hex_file"]["record"]
    assert shown("file_commit_ok") == f"FILE COMMIT OK size={rec['file_size']} crc=0x{rec['file_crc32']:08x}"
    assert shown("file_begin_busy") == "FILE BEGIN BUSY size=0 crc=0x00000000"
    assert bxproto.decode_frame(bx.frame(bx.APP_TYPE_FILE, b"\x12"))[1] == "FILE 12"
