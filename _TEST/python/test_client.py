# SPDX-License-Identifier: MIT
"""BulkXferClient (the Client role of the PC tools) against a scripted server,
without a BLE adapter. Contract: _DOC/BulkXfer/API_REFERENCE.md §6 (wire) and
_DOC/HexUpload/PROTOCOL.md §4 (upload sequence)."""

import asyncio
import os
import struct

import pytest

import bulkxfer_client as bx
from conftest import FakeServer


def run(coro):
    return asyncio.run(coro)


@pytest.mark.parametrize("seq, base, expected", [
    (0, 0, 0), (200, 0, 200),            # a distance >= 128 needs all 8 bits
    (0, 255, 256), (121, 250, 377),      # across the 255 -> 0 wrap (as the C engine)
    (3, 0x1FFFF, 0x20003), (255, 256, 511),
])
def test_seq_to_abs(seq, base, expected):
    assert bx.seq_to_abs(seq, base) == expected


def frames_of(writes, ftype):
    return [w for w in writes if w[1] == ftype]


@pytest.mark.parametrize("size", [1, 239, 240, 241, 5000, 70000])
def test_send_delivers_intact_object(server, client, size):
    data = os.urandom(size)
    progress = []
    assert run(client.send(0x10, data, lambda a, t: progress.append((a, t)))) == "OK"
    assert server.objects == [(0x10, data)]
    assert progress[-1] == (size, size)
    assert all(b[0] >= a[0] for a, b in zip(progress, progress[1:])), "progress must not go back"


def test_start_and_chunks_follow_the_mtu():
    server = FakeServer(mtu=23)
    client = bx.BulkXferClient(server, log=lambda *_: None)
    server.client = client
    assert run(client.send(0x10, bytes(1000))) == "OK"
    start = frames_of(server.writes, bx.T_START)[0]
    assert start[8] == 23 - 3 - 4, "chunk = min(MTU - 3, 244) - 4"   # [len][type][xid][app][u32 total][chunk]
    assert max(len(w) for w in server.writes) <= 20


def test_window_is_never_exceeded(server, client):
    server.window = 4
    sent_before_ack = []

    orig = server._notify

    def notify(p):
        sent_before_ack.append(len(frames_of(server.writes, bx.T_DATA)))
        orig(p)

    server._notify = notify
    assert run(client.send(0x10, os.urandom(240 * 40))) == "OK"
    # between two receiver frames the client never has more than `window` DATA frames unacked
    assert all(b - a <= 4 for a, b in zip(sent_before_ack, sent_before_ack[1:]))


def test_nack_recovers_lost_frame_without_timeout(server, client):
    server.drop_once = 300                       # past a sequence-number wrap
    data = os.urandom(240 * 400)
    loop_time = []

    async def go():
        t0 = asyncio.get_running_loop().time()
        st = await client.send(0x10, data)
        loop_time.append(asyncio.get_running_loop().time() - t0)
        return st

    assert run(go()) == "OK"
    assert server.objects[-1][1] == data
    assert loop_time[0] < bx.ACK_TIMEOUT, "recovery must come from the NACK, not the timeout"


def test_silent_server_times_out_and_aborts(server, client, fast_timeouts):
    server.silent = True
    assert run(client.send(0x10, bytes(100))) == "TIMEOUT"
    assert len(frames_of(server.writes, bx.T_START)) == 1 + bx.MAX_RETRIES, "START is retried"
    assert server.writes[-1] == bx.frame(bx.T_ABORT, bytes([client.xfer_id, bx.ST_TIMEOUT,
                                                             bx.ABORT_BY_SENDER]))


def test_server_rejection_is_reported(server, client):
    # _DOC/BulkXfer/README.md: a rejected START is answered with ABORT(by receiver)
    server.reject = 5
    assert run(client.send(0x42, bytes(10))) == "ABORTED_BY_SERVER:REJECTED"


def test_crc_error_is_reported(server, client):
    server.corrupt = True
    assert run(client.send(0x10, bytes(500))) == "CRC_ERROR"


def test_cancel_sends_abort(server, client):
    server.silent = True

    async def go():
        task = asyncio.create_task(client.send(0x10, bytes(100)))
        await asyncio.sleep(0.05)
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task

    run(go())
    assert server.writes[-1] == bx.frame(bx.T_ABORT, bytes([client.xfer_id, bx.ST_ABORTED,
                                                             bx.ABORT_BY_SENDER]))


def test_transfer_ids_increment_and_wrap(server, client):
    client.xfer_id = 0xFE
    run(client.send(0x10, b"a"))
    run(client.send(0x10, b"b"))
    assert [w[2] for w in frames_of(server.writes, bx.T_START)] == [0xFF, 0x00]


def test_stale_frames_of_another_transfer_are_ignored(server, client):
    async def go():
        # An END for an old transfer id is already queued when the next one starts
        client.on_notify(0, bytearray(bx.frame(bx.T_END, bytes([0x33, 1]))))
        return await client.send(0x10, bytes(300))

    assert run(go()) == "OK"


# ---- hex upload (PROTOCOL.md §4) ----------------------------------------------
def report(app_type, status, addr, n):
    return app_type, struct.pack("<BII", status, addr, n)


def test_upload_segment_waits_for_stored(server, client):
    server.after_end = [report(bx.APP_TYPE_RESULT, 0, 0x8000, 64),
                        report(bx.APP_TYPE_STORED, 0, 0x8000, 64)]
    st, stored = run(client.upload_segment(0x8000, bytes(64)))
    assert (st, stored) == ("OK", (0, 0x8000, 64))
    app, obj = server.objects[0]
    assert app == bx.APP_TYPE_SEGMENT and obj[:4] == struct.pack("<I", 0x8000)


def test_upload_segment_drops_leftover_shorts(server, client):
    client.short_q.put_nowait(report(bx.APP_TYPE_STORED, 0, 0xDEAD, 1))    # from an earlier segment
    server.after_end = [report(bx.APP_TYPE_STORED, 0, 0x100, 8)]
    assert run(client.upload_segment(0x100, bytes(8))) == ("OK", (0, 0x100, 8))


def test_upload_segment_failed_transfer_has_no_stored(server, client):
    server.corrupt = True
    assert run(client.upload_segment(0x100, bytes(8))) == ("CRC_ERROR", None)


def test_upload_segment_times_out_without_stored(server, client, monkeypatch):
    server.after_end = [report(bx.APP_TYPE_RESULT, 0, 0x100, 8)]
    clock = iter(range(0, 10_000, 100))                       # every read jumps 100 s
    monkeypatch.setattr(bx.time, "perf_counter", lambda: next(clock))
    with pytest.raises(TimeoutError, match="0x00000100: no STORED"):
        run(client.upload_segment(0x100, bytes(8)))


# ---- CTRL notification handling -----------------------------------------------
def test_on_notify_routes_frames(client):
    client.on_notify(0, bytearray(bx.frame(bx.T_ACK, bytes([1, 0, 16]))))
    client.on_notify(0, bytearray(bx.frame(0x05, b"hi")))
    assert client.ctrl_q.get_nowait()[1] == bx.T_ACK
    assert client.short_q.get_nowait() == (0x05, b"hi")


def test_on_notify_drops_bad_length_and_sender_frames():
    logged = []
    c = bx.BulkXferClient(FakeServer(), log=logged.append)
    c.on_notify(0, bytearray(b"\x05\xf2\x01"))                   # len field says 5, has 1
    c.on_notify(0, bytearray(bx.frame(bx.T_DATA, b"\x01\x00x")))  # sender frame on CTRL
    c.on_notify(0, bytearray(bx.frame(bx.T_ABORT, bytes([1, 3, bx.ABORT_BY_SENDER]))))
    assert c.ctrl_q.empty() and c.short_q.empty()
    assert len(logged) == 3


@pytest.mark.xfail(strict=True, reason="KNOWN BUG: on_notify checks only the length byte, not the "
                   "minimum payload per frame type; a truncated ABORT raises IndexError in the "
                   "notification callback and a truncated ACK/NACK/END is queued and later crashes "
                   "send() with IndexError")
@pytest.mark.parametrize("ftype", [bx.T_ACK, bx.T_NACK, bx.T_END, bx.T_ABORT])
def test_on_notify_rejects_truncated_control_frames(ftype):
    c = bx.BulkXferClient(FakeServer(), log=lambda *_: None)
    c.on_notify(0, bytearray(bx.frame(ftype, b"\x01")))
    assert c.ctrl_q.empty()
