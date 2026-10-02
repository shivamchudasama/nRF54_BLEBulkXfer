#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
BulkXfer receiver (Server role) for the PC side, transport independent.

The receiver of a BulkXfer transfer is the side that hosts the DATA and CTRL
characteristics (_DOC/BulkXfer/PROTOCOL.md §7). bleak cannot host a GATT
server, so the PC uses this class behind a separate GATT server
(blehost/core/gatt_server.py, built on bless). The device then sends to the PC
with its own BulkXfer Client role, e.g. the CSR during provisioning.

The transport is two callables:
  - on_write(frame): call it, on the asyncio loop, for every value the peer
    writes into DATA;
  - notify(frame): the receiver calls it to send a CTRL notification.

Completed transfers come out of `results` (an asyncio.Queue of Received), short
messages out of `short_q` ((app_type, payload) tuples), and `receive()` waits
for the next transfer of a given appType.

Only END(OK) objects carry data; any other result discards what arrived, as the
protocol requires.
"""

import asyncio
import struct
import zlib
from typing import Callable, NamedTuple, Optional

import bulkxfer_client as bx

ACK_DELAY = 0.020           # PROTOCOL.md §8: longest delay before acknowledging
IDLE_TIMEOUT = 5.0          # PROTOCOL.md §8: no DATA for this long -> ABORT(TIMEOUT)
ST_OK, ST_CRC_ERROR, ST_REMOTE_ABORTED, ST_REJECTED = 0, 1, 4, 5
ST_DISCONNECTED, ST_PROTOCOL_ERROR, ST_OUT_OF_ORDER = 6, 9, 11
START_LEN, ABORT_LEN = 12, 3
APP_TYPE_MAX = 0xEF         # 0x00..0xEF: short messages; 0xF0..: framework frames


class Received(NamedTuple):
    """One finished incoming transfer. `data` is empty unless status is "OK"."""
    app_type: int
    status: str
    data: bytes


class _Rx:
    """State of the active incoming transfer (PROTOCOL.md §7)."""

    def __init__(self, xid, app_type, total, chunk, window, crc):
        self.xid, self.app_type, self.total = xid, app_type, total
        self.chunk, self.window, self.crc_exp = chunk, window, crc
        self.frames = (total + chunk - 1) // chunk
        self.expected = 0
        self.since_ack = 0
        self.nack_sent = False
        self.crc = 0
        self.buf = bytearray()


class BulkXferReceiver:
    """Receiver half of BulkXfer v2.

    notify(frame: bytes) sends a CTRL notification (must not block).
    accept(app_type, total_len) -> bool decides on each START (None: accept all).
    max_frame / window are this receiver's limits, as published in CAPS.
    """

    def __init__(self, notify: Callable[[bytes], None],
                 accept: Optional[Callable[[int, int], bool]] = None,
                 max_frame: int = bx.MAX_FRAME, window: int = bx.WINDOW, log=print):
        self.notify = notify
        self.accept = accept
        self.max_chunk = max_frame - 4
        self.window = window
        self.log = log
        self.subscribed = True          # the peer enabled CTRL notifications (§7.1 step 1)
        self.results: asyncio.Queue = asyncio.Queue()
        self.short_q: asyncio.Queue = asyncio.Queue()
        self._rx: Optional[_Rx] = None
        self._ack_handle = None         # pending delayed ACK
        self._idle_handle = None        # idle timeout

    # ---- transport in --------------------------------------------------------
    def on_write(self, data) -> None:
        """A value written into DATA by the peer. Call on the asyncio loop."""
        data = bytes(data)
        # §3.5: length, fixed lengths, characteristic, xferId
        if len(data) < 2 or data[0] + 2 != len(data):
            self.log(f"! malformed frame on DATA: {data.hex()}")
            return
        ftype, p = data[1], data[2:]
        if ftype <= APP_TYPE_MAX:
            self.short_q.put_nowait((ftype, p))
        elif ftype == bx.T_START and data[0] == START_LEN:
            self._on_start(p)
        elif ftype == bx.T_DATA and data[0] >= 3:
            self._on_data(p)
        elif ftype == bx.T_ABORT and data[0] == ABORT_LEN and p[2] == bx.ABORT_BY_SENDER:
            if self._rx is not None and p[0] == self._rx.xid:
                self._finish(ST_REMOTE_ABORTED)
        else:
            self.log(f"! frame 0x{ftype:02x} dropped: not valid on DATA")

    def disconnected(self) -> None:
        """The link is gone: a running transfer ends as DISCONNECTED, nothing is sent."""
        if self._rx is not None:
            self._finish(ST_DISCONNECTED)

    def cancel(self) -> None:
        """The application gives up on the running transfer."""
        if self._rx is not None:
            self._send_abort(self._rx.xid, bx.ST_ABORTED)
            self._finish(bx.ST_ABORTED)

    @property
    def active(self) -> bool:
        return self._rx is not None

    async def receive(self, app_type: Optional[int] = None, timeout: Optional[float] = None) -> Received:
        """Wait for the next finished transfer (of app_type, if given). Results of
        other appTypes are dropped with a log line."""
        async def wait():
            while True:
                r = await self.results.get()
                if app_type is None or r.app_type == app_type:
                    return r
                self.log(f"! transfer of type 0x{r.app_type:02x} ({r.status}) not expected, dropped")
        return await asyncio.wait_for(wait(), timeout)

    def drain(self) -> None:
        """Forget finished transfers and short messages nobody waited for."""
        for q in (self.results, self.short_q):
            while not q.empty():
                q.get_nowait()

    # ---- §7.1 START ----------------------------------------------------------
    def _on_start(self, p: bytes) -> None:
        if not self.subscribed:
            return
        xid, app, total, chunk, win, crc = struct.unpack("<BBIBBI", p)
        rx = self._rx
        if rx is not None and rx.xid == xid and rx.expected == 0:
            self._send_ack()                                  # our ACK was lost
            return
        if rx is not None:
            self._finish(ST_REMOTE_ABORTED)                   # replaced by the new START
        if chunk == 0 or chunk > self.max_chunk or not 1 <= win <= 128:
            self._send_abort(xid, ST_PROTOCOL_ERROR)
            return
        if self.accept is not None and not self.accept(app, total):
            self._send_abort(xid, ST_REJECTED)
            return
        self._rx = _Rx(xid, app, total, chunk, min(win, self.window), crc)
        if total == 0:
            self._end()
            return
        self._send_ack()
        self._restart_idle()

    # ---- §7.2 DATA -----------------------------------------------------------
    def _on_data(self, p: bytes) -> None:
        rx = self._rx
        if rx is None or p[0] != rx.xid:
            return                                            # stale
        self._restart_idle()
        d = (p[1] - rx.expected) & 0xFF
        if d >= 128:                                          # behind: duplicate
            self._schedule_ack()
            return
        if d != 0:                                            # ahead: gap
            if not rx.nack_sent:
                rx.nack_sent = True
                self.notify(bx.frame(bx.T_NACK, bytes([rx.xid, rx.expected & 0xFF, ST_OUT_OF_ORDER])))
            return
        data = p[2:]
        if len(data) != min(rx.chunk, rx.total - rx.expected * rx.chunk):
            self._send_abort(rx.xid, ST_PROTOCOL_ERROR)
            self._finish(ST_PROTOCOL_ERROR)
            return
        rx.buf += data
        rx.crc = zlib.crc32(data, rx.crc)
        rx.expected += 1
        rx.since_ack += 1
        rx.nack_sent = False
        if rx.expected == rx.frames:
            self._end()
        elif rx.since_ack >= max(rx.window // 2, 1):
            self._send_ack()
        else:
            self._schedule_ack()

    # ---- replies and timers --------------------------------------------------
    def _send_ack(self) -> None:
        rx = self._rx
        self._cancel_ack()
        if rx is None:
            return
        rx.since_ack = 0
        self.notify(bx.frame(bx.T_ACK, bytes([rx.xid, rx.expected & 0xFF, rx.window])))

    def _schedule_ack(self) -> None:
        if self._ack_handle is None:                          # never pushed back
            self._ack_handle = asyncio.get_running_loop().call_later(ACK_DELAY, self._delayed_ack)

    def _delayed_ack(self) -> None:
        self._ack_handle = None
        self._send_ack()

    def _cancel_ack(self) -> None:
        if self._ack_handle is not None:
            self._ack_handle.cancel()
            self._ack_handle = None

    def _restart_idle(self) -> None:
        if self._idle_handle is not None:
            self._idle_handle.cancel()
        self._idle_handle = asyncio.get_running_loop().call_later(IDLE_TIMEOUT, self._idle_expired)

    def _idle_expired(self) -> None:
        self._idle_handle = None
        if self._rx is not None:
            self._send_abort(self._rx.xid, bx.ST_TIMEOUT)
            self._finish(bx.ST_TIMEOUT)

    def _send_abort(self, xid: int, reason: int) -> None:
        self.notify(bx.frame(bx.T_ABORT, bytes([xid, reason, bx.ABORT_BY_RECEIVER])))

    def _end(self) -> None:
        rx = self._rx
        status = ST_OK if rx.crc == rx.crc_exp else ST_CRC_ERROR
        self.notify(bx.frame(bx.T_END, bytes([rx.xid, status])))
        self._finish(status)

    def _finish(self, status: int) -> None:
        rx, self._rx = self._rx, None
        self._cancel_ack()
        if self._idle_handle is not None:
            self._idle_handle.cancel()
            self._idle_handle = None
        name = bx.STATUS.get(status, f"0x{status:02x}")
        self.results.put_nowait(Received(rx.app_type, name, bytes(rx.buf) if status == ST_OK else b""))
