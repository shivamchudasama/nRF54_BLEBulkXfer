# SPDX-License-Identifier: MIT
"""BulkXferReceiver (the PC-side receiver, bulkxfer_receiver.py) against the
protocol: _DOC/BulkXfer/PROTOCOL.md §3.5 (frame checks), §5 and §7 (receiver
behaviour), §8 (timers). Most tests run the PC's own BulkXferClient into the
receiver over a loopback; a few write hand-made frames."""

import asyncio
import os
import struct
import zlib

import pytest

import bulkxfer_client as bx
import bulkxfer_receiver as bxr


class Loopback:
    """Fake GATT link: client writes go into the receiver, receiver notifications
    come back to the client. Knobs: drop one DATA frame index, duplicate one,
    rewrite START."""

    def __init__(self, mtu=247, accept=None, window=16):
        self.mtu_size = mtu
        self.notified = []
        self.rx = bxr.BulkXferReceiver(self._notify, accept=accept, window=window,
                                       log=lambda *_: None)
        self.client = bx.BulkXferClient(self, log=lambda *_: None)
        self.drop_index = None
        self.dup_index = None
        self.start_patch = None          # fn(bytes) -> bytes
        self.data_frames = 0

    def _notify(self, frame):
        self.notified.append(bytes(frame))
        self.client.on_notify(0, bytearray(frame))

    async def write_gatt_char(self, uuid, data, response=False):
        assert uuid == self.client.data_uuid and response is False
        data = bytes(data)
        if data[1] == bx.T_START and self.start_patch:
            data = self.start_patch(data)
        if data[1] == bx.T_DATA:
            idx = self.data_frames
            self.data_frames += 1
            if idx == self.drop_index:
                return
            if idx == self.dup_index:
                self.rx.on_write(data)
        self.rx.on_write(data)
        await asyncio.sleep(0)

    async def read_gatt_char(self, uuid):
        return bytearray(b"\x02\xf4\x10\x00")

    def sent(self, ftype):
        return [f for f in self.notified if f[1] == ftype]


def run(coro):
    return asyncio.run(coro)


async def transfer(lb, app_type, data):
    status = await lb.client.send(app_type, data)
    result = None
    if not lb.rx.results.empty():
        result = lb.rx.results.get_nowait()
    return status, result


# ---- normal transfers ------------------------------------------------------
@pytest.mark.parametrize("size", [1, 239, 240, 241, 421, 1024, 5000, 70000])
def test_object_arrives_intact(size):
    lb = Loopback()
    data = os.urandom(size)
    status, r = run(transfer(lb, 0x23, data))
    assert status == "OK"
    assert r == bxr.Received(0x23, "OK", data)


def test_small_mtu_chunks():
    lb = Loopback(mtu=23)
    data = os.urandom(500)
    status, r = run(transfer(lb, 0x23, data))
    assert status == "OK" and r.data == data


def test_empty_object():
    lb = Loopback()
    status, r = run(transfer(lb, 0x23, b""))
    assert status == "OK" and r == bxr.Received(0x23, "OK", b"")
    assert lb.sent(bx.T_ACK) == [], "an empty object is answered with END only"


def test_acks_every_half_window():
    lb = Loopback(window=16)
    run(transfer(lb, 0x23, bytes(240 * 40)))
    acks = lb.sent(bx.T_ACK)
    # ACK(0) for START, then every 8 frames up to the last (frame 40 gets END)
    assert [a[3] for a in acks] == [0, 8, 16, 24, 32]
    assert all(a[4] == 16 for a in acks)


def test_window_is_the_smaller_of_both():
    lb = Loopback(window=4)
    run(transfer(lb, 0x23, bytes(240 * 9)))
    assert lb.sent(bx.T_ACK)[0][4] == 4


# ---- losses ----------------------------------------------------------------
def test_lost_frame_is_nacked_once_and_resent(fast_timeouts):
    lb = Loopback()
    lb.drop_index = 3
    data = os.urandom(240 * 20)
    status, r = run(transfer(lb, 0x23, data))
    assert status == "OK" and r.data == data
    nacks = lb.sent(bx.T_NACK)
    assert len(nacks) == 1, "one NACK per gap"
    assert nacks[0][3:] == bytes([3, bxr.ST_OUT_OF_ORDER])


def test_duplicate_frame_is_dropped():
    lb = Loopback()
    lb.dup_index = 2
    data = os.urandom(240 * 6)
    status, r = run(transfer(lb, 0x23, data))
    assert status == "OK" and r.data == data


def test_crc_mismatch_ends_with_crc_error():
    lb = Loopback()
    lb.start_patch = lambda f: f[:-4] + struct.pack("<I", struct.unpack("<I", f[-4:])[0] ^ 1)
    status, r = run(transfer(lb, 0x23, bytes(1000)))
    assert status == "CRC_ERROR"
    assert r == bxr.Received(0x23, "CRC_ERROR", b""), "a failed object carries no data"


# ---- START checks (§7.1) ---------------------------------------------------
def test_rejected_by_application():
    lb = Loopback(accept=lambda t, n: t == 0x23 and n <= 1024)
    assert run(transfer(lb, 0x23, bytes(1025)))[0] == "ABORTED_BY_SERVER:REJECTED"
    assert run(transfer(lb, 0x24, bytes(10)))[0] == "ABORTED_BY_SERVER:REJECTED"
    assert lb.rx.results.empty(), "a rejected START produces no result"
    assert run(transfer(lb, 0x23, bytes(1024)))[0] == "OK"


def start_frame(xid=1, app=0x23, total=100, chunk=240, win=16, crc=None, data=b""):
    crc = zlib.crc32(data) if crc is None else crc
    return bx.frame(bx.T_START, struct.pack("<BBIBBI", xid, app, total, chunk, win, crc))


class Probe:
    def __init__(self, **kw):
        self.frames = []
        self.rx = bxr.BulkXferReceiver(self.frames.append, log=lambda *_: None, **kw)


@pytest.mark.parametrize("chunk, win", [(0, 16), (241, 16), (240, 0), (240, 129)])
def test_bad_start_parameters(chunk, win):
    async def go():
        p = Probe()
        p.rx.on_write(start_frame(chunk=chunk, win=win))
        return p
    p = run(go())
    assert p.frames == [bx.frame(bx.T_ABORT, bytes([1, bxr.ST_PROTOCOL_ERROR, bx.ABORT_BY_RECEIVER]))]
    assert not p.rx.active


def test_start_repeat_before_data_is_reacked():
    async def go():
        p = Probe()
        p.rx.on_write(start_frame(xid=7))
        p.rx.on_write(start_frame(xid=7))
        return p
    p = run(go())
    ack = bx.frame(bx.T_ACK, bytes([7, 0, 16]))
    assert p.frames == [ack, ack]
    assert p.rx.results.empty()


def test_new_start_replaces_active_transfer():
    async def go():
        p = Probe()
        p.rx.on_write(start_frame(xid=1, total=500))
        p.rx.on_write(bx.frame(bx.T_DATA, bytes([1, 0]) + bytes(240)))
        p.rx.on_write(start_frame(xid=2, total=10))
        return p
    p = run(go())
    assert p.rx.results.get_nowait() == bxr.Received(0x23, "REMOTE_ABORTED", b"")
    assert p.rx.active and p.frames[-1] == bx.frame(bx.T_ACK, bytes([2, 0, 16]))


def test_not_subscribed_ignores_start():
    async def go():
        p = Probe()
        p.rx.subscribed = False
        p.rx.on_write(start_frame())
        return p
    p = run(go())
    assert p.frames == [] and not p.rx.active


# ---- DATA checks (§7.2) ----------------------------------------------------
def test_wrong_data_length_is_protocol_error():
    async def go():
        p = Probe()
        p.rx.on_write(start_frame(total=500))
        p.rx.on_write(bx.frame(bx.T_DATA, bytes([1, 0]) + bytes(100)))
        return p
    p = run(go())
    assert p.frames[-1] == bx.frame(bx.T_ABORT, bytes([1, bxr.ST_PROTOCOL_ERROR, bx.ABORT_BY_RECEIVER]))
    assert p.rx.results.get_nowait().status == "PROTOCOL_ERROR"


def test_stale_data_is_ignored():
    async def go():
        p = Probe()
        p.rx.on_write(bx.frame(bx.T_DATA, bytes([1, 0]) + bytes(10)))   # no transfer
        p.rx.on_write(start_frame(xid=4, total=500))
        p.rx.on_write(bx.frame(bx.T_DATA, bytes([3, 0]) + bytes(240)))  # other xferId
        return p
    p = run(go())
    assert p.frames == [bx.frame(bx.T_ACK, bytes([4, 0, 16]))]


def test_trickle_is_acked_after_ack_delay():
    async def go():
        p = Probe()
        p.rx.on_write(start_frame(total=240 * 10))
        p.rx.on_write(bx.frame(bx.T_DATA, bytes([1, 0]) + bytes(240)))
        n = len(p.frames)
        await asyncio.sleep(bxr.ACK_DELAY * 3)
        return p, n
    p, n = run(go())
    assert n == 1, "no immediate ACK for one frame"
    assert p.frames[1] == bx.frame(bx.T_ACK, bytes([1, 1, 16]))


def test_idle_timeout_aborts(monkeypatch):
    monkeypatch.setattr(bxr, "IDLE_TIMEOUT", 0.05)

    async def go():
        p = Probe()
        p.rx.on_write(start_frame(total=500))
        return p, await p.rx.receive(timeout=1.0)
    p, r = run(go())
    assert r == bxr.Received(0x23, "TIMEOUT", b"")
    assert p.frames[-1] == bx.frame(bx.T_ABORT, bytes([1, bx.ST_TIMEOUT, bx.ABORT_BY_RECEIVER]))


# ---- other events (§7.3) ---------------------------------------------------
def test_sender_abort_and_disconnect_and_cancel():
    async def go():
        p = Probe()
        p.rx.on_write(start_frame(xid=5, total=500))
        p.rx.on_write(bx.frame(bx.T_ABORT, bytes([5, bx.ST_ABORTED, bx.ABORT_BY_SENDER])))
        p.rx.on_write(start_frame(xid=6, total=500))
        p.rx.disconnected()
        p.rx.on_write(start_frame(xid=7, total=500))
        p.rx.cancel()
        return p
    p = run(go())
    got = [p.rx.results.get_nowait().status for _ in range(3)]
    assert got == ["REMOTE_ABORTED", "DISCONNECTED", "ABORTED"]
    assert p.frames[-1] == bx.frame(bx.T_ABORT, bytes([7, bx.ST_ABORTED, bx.ABORT_BY_RECEIVER]))
    assert bx.frame(bx.T_ABORT, bytes([6, bx.ST_ABORTED, bx.ABORT_BY_RECEIVER])) not in p.frames


@pytest.mark.parametrize("frame", [
    b"", b"\x05", b"\x03\xf0\x01",                            # length field wrong
    bx.frame(bx.T_ACK, bytes([1, 0, 16])),                    # receiver frames do not go on DATA
    bx.frame(bx.T_END, bytes([1, 0])),
    bx.frame(bx.T_ABORT, bytes([1, 5, bx.ABORT_BY_RECEIVER])),
    bx.frame(0xF6, b""),                                      # reserved
    bytes([11, bx.T_START]) + bytes(11),                      # START of the wrong length
])
def test_invalid_frames_are_dropped(frame):
    async def go():
        p = Probe()
        p.rx.on_write(frame)
        return p
    p = run(go())
    assert p.frames == [] and p.rx.results.empty() and p.rx.short_q.empty()


def test_short_messages_are_queued():
    async def go():
        p = Probe()
        p.rx.on_write(bx.frame(0x05, b"\x01\x02"))
        p.rx.on_write(bx.frame(0x00, b""))
        return p
    p = run(go())
    assert p.rx.short_q.get_nowait() == (0x05, b"\x01\x02")
    assert p.rx.short_q.get_nowait() == (0x00, b"")


def test_receive_filters_and_times_out():
    async def go():
        p = Probe()
        p.rx.results.put_nowait(bxr.Received(0x10, "OK", b"x"))
        p.rx.results.put_nowait(bxr.Received(0x23, "OK", b"y"))
        r = await p.rx.receive(0x23, timeout=1.0)
        with pytest.raises(asyncio.TimeoutError):
            await p.rx.receive(0x23, timeout=0.01)
        return r
    assert run(go()) == bxr.Received(0x23, "OK", b"y")


# ---- golden wire vectors ---------------------------------------------------
def test_receiver_frames_match_vectors(vectors):
    v = {f["name"]: bytes.fromhex(f["hex"]) for f in vectors["frames"]}
    golden_start = v["start_from_log"]
    xid, app, total, chunk, win, crc = struct.unpack("<BBIBBI", golden_start[2:])

    async def go():
        p = Probe()
        p.rx.on_write(golden_start)
        return p
    p = run(go())
    assert p.frames[0] == v["ack_from_log"], "accepting the real upload's START"

    async def go_rejected():
        p = Probe(accept=lambda t, n: False)
        p.rx.on_write(start_frame(xid=9))
        return p
    assert run(go_rejected()).frames == [v["abort_rejected_by_receiver"]]
