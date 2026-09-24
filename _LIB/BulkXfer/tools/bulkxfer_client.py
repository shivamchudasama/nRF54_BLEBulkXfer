#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
BulkXfer reference GATT client (PC side) for testing examples/bulk_server.

Implements the Client role of BulkXfer protocol v2 on top of `bleak`:
  - writes START / DATA frames into the server's DATA characteristic with
    Write Without Response (windowed ACK, Go-Back-N, CRC-32)
  - receives ACK / NACK / END / ABORT and short messages as CTRL notifications
  - sends single-frame ("short") messages

The PC cannot receive bulk data: in v2 the receiver must host the GATT server,
and bleak has no GATT server. Use examples/bulk_client on a second board for
device -> device transfers.

Usage:
    pip install bleak
    python bulkxfer_client.py ping
    python bulkxfer_client.py send 100000
    python bulkxfer_client.py send 20000 --address AA:BB:CC:DD:EE:FF

Hex upload (the project firmware in _ASW, see _DOC/HexUpload/PROTOCOL.md):
    python bulkxfer_client.py hex app.hex --base 16a1-4812-af35-f3f29a92f6ca --name "BLE Bulk Transfer"

--base selects the 96-bit base UUID the server was built with (BaseUUIDs.h).
"""

import argparse
import asyncio
import os
import struct
import sys
import time
import zlib

from bleak import BleakClient, BleakScanner

EXAMPLE_BASE = "dbb1-4d99-ab6e-f441ec7c092b"         # examples/BaseUUIDs.h


def char_uuid(char_id: int, base: str) -> str:
    return f"b1c0{char_id:04x}-{base.lower()}"


DATA_UUID = char_uuid(1, EXAMPLE_BASE)   # client -> server (write w/o response)
CTRL_UUID = char_uuid(2, EXAMPLE_BASE)   # server -> client (notify)
CAPS_UUID = char_uuid(3, EXAMPLE_BASE)

T_START, T_DATA, T_ACK, T_NACK, T_END, T_ABORT = 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5
ABORT_BY_SENDER, ABORT_BY_RECEIVER = 0, 1
STATUS = {0: "OK", 1: "CRC_ERROR", 2: "TIMEOUT", 3: "ABORTED", 4: "REMOTE_ABORTED",
          5: "REJECTED", 6: "DISCONNECTED", 7: "SOURCE_ERROR", 8: "SINK_ERROR",
          9: "PROTOCOL_ERROR", 10: "NO_RESOURCES", 11: "OUT_OF_ORDER"}
MAX_FRAME = 244
WINDOW = 16
ACK_TIMEOUT = 1.0
MAX_RETRIES = 5
APP_TYPE_RESULT, APP_TYPE_PING = 0x01, 0x02
APP_TYPE_SEGMENT, APP_TYPE_STORED = 0x10, 0x11       # hex upload (_DOC/HexUpload/PROTOCOL.md)


def parse_ihex(path: str, seg_max: int) -> list:
    """Intel HEX -> [(address, bytes)], contiguous runs split at seg_max bytes."""
    mem = {}
    upper = 0
    with open(path) as f:
        for n, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            if line[0] != ":":
                sys.exit(f"{path}:{n}: record does not start with ':'")
            rec = bytes.fromhex(line[1:])
            if len(rec) < 5 or len(rec) != rec[0] + 5 or sum(rec) & 0xFF:
                sys.exit(f"{path}:{n}: bad length or checksum")
            addr, rtype, data = (rec[1] << 8) | rec[2], rec[3], rec[4:-1]
            if rtype == 0x00:
                for i, b in enumerate(data):
                    mem[upper + addr + i] = b
            elif rtype == 0x01:
                break
            elif rtype == 0x02:
                upper = int.from_bytes(data, "big") << 4
            elif rtype == 0x04:
                upper = int.from_bytes(data, "big") << 16
            elif rtype in (0x03, 0x05):
                pass                                      # start address: not flash data
            else:
                sys.exit(f"{path}:{n}: unknown record type 0x{rtype:02x}")

    segments = []
    for a in sorted(mem):
        if segments and segments[-1][0] + len(segments[-1][1]) == a and len(segments[-1][1]) < seg_max:
            segments[-1][1].append(mem[a])
        else:
            segments.append((a, bytearray([mem[a]])))
    return [(a, bytes(d)) for a, d in segments]


def frame(ftype: int, payload: bytes) -> bytes:
    return bytes([len(payload), ftype]) + payload


def seq_to_abs(seq: int, base: int) -> int:
    return base + ((seq - base) & 0xFF)


class BulkXferClient:
    def __init__(self, client: BleakClient):
        self.client = client
        self.frame_cap = min(client.mtu_size - 3, MAX_FRAME)
        self.ctrl_q: asyncio.Queue = asyncio.Queue()    # ACK/NACK/END/ABORT
        self.short_q: asyncio.Queue = asyncio.Queue()
        self.xfer_id = 0

    # ---- plumbing ----------------------------------------------------------
    def on_notify(self, _handle, data: bytearray):
        data = bytes(data)
        if len(data) < 2 or data[0] + 2 != len(data):
            print(f"! malformed frame {data.hex()}")
            return
        ftype = data[1]
        if ftype <= 0xEF:
            self.short_q.put_nowait((ftype, data[2:]))
        elif ftype in (T_ACK, T_NACK, T_END) or (ftype == T_ABORT and data[4] == ABORT_BY_RECEIVER):
            self.ctrl_q.put_nowait(data)
        else:
            print(f"! frame 0x{ftype:02x} is not valid on CTRL")

    async def write(self, data: bytes):
        await self.client.write_gatt_char(DATA_UUID, data, response=False)

    # ---- short messages ----------------------------------------------------
    async def send_short(self, app_type: int, payload: bytes):
        assert len(payload) <= self.frame_cap - 2
        await self.write(frame(app_type, payload))

    # ---- client -> server transfer ----------------------------------------
    async def send(self, app_type: int, data: bytes) -> str:
        self.xfer_id = (self.xfer_id + 1) & 0xFF
        xid = self.xfer_id
        chunk = self.frame_cap - 4
        frames = (len(data) + chunk - 1) // chunk
        start = frame(T_START, struct.pack("<BBIBBI", xid, app_type, len(data), chunk,
                                           WINDOW, zlib.crc32(data)))
        window = WINDOW
        acked = nxt = 0
        started = False
        retries = 0
        await self.write(start)

        while True:
            if started:
                while nxt < frames and nxt - acked < window:
                    off = nxt * chunk
                    await self.write(frame(T_DATA, bytes([xid, nxt & 0xFF]) + data[off:off + chunk]))
                    nxt += 1
            try:
                f = await asyncio.wait_for(self.ctrl_q.get(), ACK_TIMEOUT)
            except asyncio.TimeoutError:
                retries += 1
                if retries > MAX_RETRIES:
                    await self.write(frame(T_ABORT, bytes([xid, 2, ABORT_BY_SENDER])))
                    return "TIMEOUT"
                if not started:
                    await self.write(start)
                nxt = acked                              # Go-Back-N
                continue
            if f[2] != xid:
                continue
            if f[1] == T_ACK:
                if not started:
                    if f[3] == 0:
                        started, window = True, max(1, min(window, f[4]))
                    continue
                a = seq_to_abs(f[3], acked)
                if acked < a <= nxt:
                    acked, retries = a, 0
            elif f[1] == T_NACK:
                a = seq_to_abs(f[3], acked)
                if a <= nxt:
                    acked = nxt = a
            elif f[1] == T_END:
                return STATUS.get(f[3], hex(f[3]))
            elif f[1] == T_ABORT:
                return "ABORTED_BY_SERVER:" + STATUS.get(f[3], hex(f[3]))


async def find_device(address, name):
    if address:
        return address
    print(f"scanning for '{name}' ...")
    dev = await BleakScanner.find_device_by_name(name, timeout=10.0)
    if dev is None:
        sys.exit(f"device '{name}' not found")
    return dev


async def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["ping", "send", "caps", "hex"])
    ap.add_argument("arg", nargs="?", help="send: size in bytes (default 20000); hex: .hex file")
    ap.add_argument("--address")
    ap.add_argument("--name", default="BulkXfer")
    ap.add_argument("--base", default=EXAMPLE_BASE,
                    help="96-bit base UUID of the server, e.g. 16a1-4812-af35-f3f29a92f6ca")
    ap.add_argument("--seg-max", type=int, default=65536,
                    help="hex: largest segment per transfer (server DS_BUF_SIZE)")
    args = ap.parse_args()

    global DATA_UUID, CTRL_UUID, CAPS_UUID
    DATA_UUID, CTRL_UUID, CAPS_UUID = (char_uuid(i, args.base) for i in (1, 2, 3))

    segments = []
    if args.command == "hex":
        if not args.arg:
            sys.exit("hex: missing .hex file")
        segments = parse_ihex(args.arg, args.seg_max)
        print(f"{args.arg}: {len(segments)} segment(s), {sum(len(d) for _, d in segments)} bytes")

    target = await find_device(args.address, args.name)
    async with BleakClient(target) as client:
        blk = BulkXferClient(client)
        await client.start_notify(CTRL_UUID, blk.on_notify)
        print(f"connected, ATT MTU {client.mtu_size} -> {blk.frame_cap} B frames, "
              f"{blk.frame_cap - 4} B per DATA frame")

        if args.command == "caps":
            ver, max_frame, window, _ = await client.read_gatt_char(CAPS_UUID)
            print(f"protocol v{ver}, max frame {max_frame} B, window {window}")

        elif args.command == "ping":
            t0 = time.perf_counter()
            await blk.send_short(APP_TYPE_PING, b"ping")
            app_type, payload = await asyncio.wait_for(blk.short_q.get(), 5.0)
            print(f"echo type 0x{app_type:02x} {payload!r} in {(time.perf_counter() - t0) * 1e3:.1f} ms")

        elif args.command == "hex":
            for addr, data in segments:
                t0 = time.perf_counter()
                status = await blk.send(APP_TYPE_SEGMENT, struct.pack("<I", addr) + data)
                dt = time.perf_counter() - t0
                print(f"0x{addr:08x} {len(data)} B: {status}, {len(data) * 8 / dt / 1000:.0f} kbit/s")
                if status != "OK":
                    sys.exit(1)
                # RESULT arrives right away; STORED once the server has dumped the
                # segment to its serial log (slow; the timeout allows for a 115200 baud log)
                deadline = time.perf_counter() + 30 + len(data) / 500
                while True:
                    try:
                        app_type, payload = await asyncio.wait_for(
                            blk.short_q.get(), max(0.1, deadline - time.perf_counter()))
                    except asyncio.TimeoutError:
                        sys.exit(f"0x{addr:08x}: no STORED from the server")
                    if app_type == APP_TYPE_STORED and len(payload) >= 9:
                        st, a, n = struct.unpack("<BII", payload[:9])
                        print(f"  stored: 0x{a:08x} {n} B, {STATUS.get(st, hex(st))}")
                        break
            print("hex upload done")

        else:
            data = os.urandom(int(args.arg or 20000))
            t0 = time.perf_counter()
            status = await blk.send(0x10, data)
            dt = time.perf_counter() - t0
            print(f"client -> server: {len(data)} B, {status}, {len(data) * 8 / dt / 1000:.0f} kbit/s")
            try:
                app_type, payload = await asyncio.wait_for(blk.short_q.get(), 2.0)
                if app_type == APP_TYPE_RESULT and len(payload) >= 9:
                    st, n, kbps = struct.unpack("<BII", payload[:9])
                    print(f"server reports: {STATUS.get(st, hex(st))}, {n} B, {kbps} kbit/s")
            except asyncio.TimeoutError:
                pass
            sys.exit(0 if status == "OK" else 1)


if __name__ == "__main__":
    asyncio.run(main())
