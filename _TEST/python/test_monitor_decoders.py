# SPDX-License-Identifier: MIT
"""Traffic-monitor decoders, replayed against a real capture: every fully
logged frame in _LOG/BulkXfer_GUI_Client.txt must decode to the summary that
was shown for it."""

import re

import pytest

from blehost.core.decoders import DecoderRegistry
from blehost.protocols import bulkxfer as proto

LINE = re.compile(r"^\S+\s+(TX|RX)\s+(DATA|CTRL|CAPS)\s+(\w+)\s+\d+\s+([0-9a-f ]+?)\s+\|\s+(.*)$")


def captured_frames(repo):
    with open(f"{repo}/_LOG/BulkXfer_GUI_Client.txt", encoding="utf-8") as f:
        for line in f:
            m = LINE.match(line.rstrip("\n"))
            if m and m.group(3) != "CCCD" and "…" not in line:
                yield m.group(2), bytes.fromhex(m.group(4)), m.group(5)


@pytest.fixture(scope="module")
def hex_upload_types():
    # The Hex Upload tab registers SEGMENT / RESULT / STORED when it is imported
    pytest.importorskip("tkinter")
    import blehost.features.hex_upload  # noqa: F401


def test_capture_replays_to_the_same_summaries(repo, hex_upload_types):
    frames = list(captured_frames(repo))
    assert len(frames) >= 8, "capture format changed?"
    for char, data, shown in frames:
        decode = proto.decode_caps if char == "CAPS" else proto.decode_frame
        assert decode(data)[1] == shown, data.hex(" ")


def test_malformed_frames_are_flagged_not_raised():
    assert proto.decode_frame(b"\x01") == ("?", "short frame")
    kind, s = proto.decode_frame(bytes([5, 0xF2, 1]))
    assert kind == "ACK" and "(len field 5, got 1)" in s
    assert proto.decode_frame(bytes([0, 0xF1]))[0] == "DATA"             # no payload: hex fallback


def test_registry_names_and_decode():
    reg = DecoderRegistry()
    base = "16a1-4812-af35-f3f29a92f6ca"
    proto.register_decoders(reg, base)
    ctrl = f"b1c00002-{base}"
    assert reg.name(ctrl) == "CTRL"
    assert reg.name("0000180a-0000-1000-8000-00805f9b34fb") == "180a"   # unknown: short form
    assert reg.decode(ctrl, bytes.fromhex("02f40100")) == ("END", "END xid=1 OK")
    assert reg.decode("unknown", b"\x00") == ("", "")


def test_registry_contains_decoder_exceptions():
    reg = DecoderRegistry()
    reg.register("u", "U", lambda d: 1 / 0)
    kind, s = reg.decode("u", b"")
    assert kind == "" and s.startswith("<decode error")
