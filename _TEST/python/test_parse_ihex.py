# SPDX-License-Identifier: MIT
"""parse_ihex() against _DOC/HexUpload/PROTOCOL.md §4 steps 1-2 and the
Intel HEX specification (record types 00-05)."""

import struct
import zlib

import pytest

import bulkxfer_client as bx


def rec(addr: int, rtype: int, data: bytes = b"") -> str:
    body = bytes([len(data), addr >> 8, addr & 0xFF, rtype]) + data
    return ":" + (body + bytes([(-sum(body)) & 0xFF])).hex().upper()


def write_hex(tmp_path, *records, name="t.hex"):
    p = tmp_path / name
    p.write_text("\n".join(records + (rec(0, 1),)) + "\n")
    return str(p)


def test_real_test_file_gives_the_uploaded_segment(repo, vectors):
    hu = vectors["hex_upload"]
    segs = bx.parse_ihex(f"{repo}/{hu['file']}")
    assert [(a, len(d)) for a, d in segs] == [(s["address"], s["length"]) for s in hu["segments"]]
    # Same object CRC as the START frame of the real upload and the device's SEG line
    golden = next(f for f in vectors["frames"] if f["name"] == hu["start_frame"])
    addr, data = segs[0]
    assert zlib.crc32(struct.pack("<I", addr) + data) == golden["crc32"]
    assert f"crc=0x{golden['crc32']:08x}" in hu["seg_line"]


def test_extended_linear_address(tmp_path):
    p = write_hex(tmp_path, rec(0, 4, b"\x08\x00"), rec(0x1000, 0, b"\x01\x02"))
    assert bx.parse_ihex(p) == [(0x08001000, b"\x01\x02")]


def test_gap_splits_segments_and_order_is_by_address(tmp_path):
    p = write_hex(tmp_path, rec(0x20, 0, b"\xbb"), rec(0x00, 0, b"\xaa\xab"), rec(0x02, 0, b"\xac"))
    assert bx.parse_ihex(p) == [(0x00, b"\xaa\xab\xac"), (0x20, b"\xbb")]


def test_long_run_split_at_seg_max(tmp_path):
    records = [rec(a, 0, bytes([a & 0xFF] * 16)) for a in range(0, 64, 16)]
    p = write_hex(tmp_path, *records)
    segs = bx.parse_ihex(p, seg_max=24)
    assert [(a, len(d)) for a, d in segs] == [(0, 24), (24, 24), (48, 16)]
    assert b"".join(d for _, d in segs) == b"".join(bytes([a] * 16) for a in range(0, 64, 16))


def test_default_split_is_the_server_buffer(tmp_path):
    # 70000 contiguous bytes -> 65536 + 4464 (PROTOCOL.md §4.2)
    records = [rec(0, 4, b"\x00\x01")]                                  # 0x10000..0x1FFFF
    records += [rec(a, 0, b"\x5a" * 16) for a in range(0, 0x10000, 16)]
    records += [rec(0, 4, b"\x00\x02")]                                 # continues at 0x20000
    records += [rec(a, 0, b"\x5a" * 16) for a in range(0, 70000 - 0x10000, 16)]
    p = write_hex(tmp_path, *records)
    assert [(a, len(d)) for a, d in bx.parse_ihex(p)] == [(0x10000, 65536), (0x20000, 70000 - 65536)]


def test_start_address_records_ignored_and_eof_stops(tmp_path):
    p = tmp_path / "t.hex"
    p.write_text("\n".join([rec(0, 0, b"\x01"), rec(0, 3, b"\x00\x00\x10\x00"),
                            rec(0, 5, b"\x00\x00\x10\x00"), rec(0, 1), rec(0x10, 0, b"\xff")]) + "\n")
    assert bx.parse_ihex(str(p)) == [(0, b"\x01")]


@pytest.mark.parametrize("line, why", [
    (":0100000001FF", "bad length or checksum"),              # checksum off by one
    (":01000000", "bad length or checksum"),                  # truncated
    ("0100000001FE", "record does not start with ':'"),
    (":zz", "not a hex record"),
    (":0000000AF6", "unknown record type 0x0a"),
])
def test_malformed_lines_report_file_and_line(tmp_path, line, why):
    p = tmp_path / "bad.hex"
    p.write_text(rec(0, 0, b"\x01") + "\n" + line + "\n")
    with pytest.raises(ValueError) as e:
        bx.parse_ihex(str(p))
    assert str(e.value) == f"{p}:2: {why}"


@pytest.mark.xfail(strict=True, reason="KNOWN BUG: type 02 (extended segment) addresses must wrap "
                   "at 64 KiB: (SBA*16 + ((offset + i) mod 65536)). parse_ihex adds without wrapping.")
def test_extended_segment_address_wraps_within_64k(tmp_path):
    # SBA 0x1000 -> base 0x10000; record at offset 0xFFFE with 4 bytes
    p = write_hex(tmp_path, rec(0, 2, b"\x10\x00"), rec(0xFFFE, 0, b"\x01\x02\x03\x04"))
    assert sorted(bx.parse_ihex(p)) == [(0x10000, b"\x03\x04"), (0x1FFFE, b"\x01\x02")]


def test_extended_segment_address_without_wrap(tmp_path):
    p = write_hex(tmp_path, rec(0, 2, b"\x10\x00"), rec(0x0010, 0, b"\x01"))
    assert bx.parse_ihex(p) == [(0x10010, b"\x01")]
