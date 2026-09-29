# SPDX-License-Identifier: MIT
"""The PC client against the golden wire vectors the C tests also check
(_TEST/vectors/wire.json), so firmware and client agree byte for byte."""

import asyncio
import struct

import pytest

import bulkxfer_client as bx
from blehost.protocols import bulkxfer as proto


def test_protocol_constants_match(vectors):
    assert bx.MAX_FRAME == vectors["max_frame_len"]
    assert bx.WINDOW == vectors["window"]
    assert proto.APP_TYPE_MAX == vectors["app_type_max"]
    ft = vectors["frame_types"]
    assert (bx.T_START, bx.T_DATA, bx.T_ACK, bx.T_NACK, bx.T_END, bx.T_ABORT) == \
        (ft["START"], ft["DATA"], ft["ACK"], ft["NACK"], ft["END"], ft["ABORT"])
    assert (bx.ABORT_BY_SENDER, bx.ABORT_BY_RECEIVER) == \
        (vectors["abort_dir"]["BY_SENDER"], vectors["abort_dir"]["BY_RECEIVER"])
    hx = vectors["hex_app_types"]
    assert (bx.APP_TYPE_RESULT, bx.APP_TYPE_SEGMENT, bx.APP_TYPE_STORED) == \
        (hx["RESULT"], hx["SEGMENT"], hx["STORED"])
    assert bx.SEG_MAX == 65536


def test_status_names_match_firmware(vectors):
    assert bx.STATUS == {v: k for k, v in vectors["status"].items()}
    assert (bx.ST_TIMEOUT, bx.ST_ABORTED) == (vectors["status"]["TIMEOUT"], vectors["status"]["ABORTED"])


def _payload(fr):
    k = fr["kind"]
    if k == "START":
        return struct.pack("<BBIBBI", fr["xid"], fr["app_type"], fr["total_len"], fr["chunk"],
                           fr["window"], fr["crc32"])
    if k == "DATA":
        return bytes([fr["xid"], fr["seq"]]) + bytes.fromhex(fr["data"])
    if k in ("ACK",):
        return bytes([fr["xid"], fr["seq"], fr["window"]])
    if k == "NACK":
        return bytes([fr["xid"], fr["seq"], fr["reason"]])
    if k == "END":
        return bytes([fr["xid"], fr["status"]])
    if k == "ABORT":
        return bytes([fr["xid"], fr["reason"], fr["dir"]])
    return bytes.fromhex(fr["payload"])


def _ftype(fr, vectors):
    return fr["app_type"] if fr["kind"] == "SHORT" else vectors["frame_types"][fr["kind"]]


def test_frame_encoding(vectors):
    for fr in vectors["frames"]:
        assert bx.frame(_ftype(fr, vectors), _payload(fr)).hex() == fr["hex"], fr["name"]


def test_monitor_decodes_every_vector(vectors):
    for fr in vectors["frames"]:
        kind, summary = proto.decode_frame(bytes.fromhex(fr["hex"]))
        assert kind == fr["kind"], fr["name"]
        assert "len field" not in summary, f"{fr['name']}: {summary}"
        if fr["kind"] == "START":
            assert f"len={fr['total_len']}" in summary and f"crc=0x{fr['crc32']:08x}" in summary


def test_reports_unpack(vectors):
    for r in vectors["reports"]:
        wire = bytes.fromhex(r["hex"])
        assert wire[0] == len(wire) - 2 and wire[1] == r["app_type"], r["name"]
        assert struct.unpack("<BII", wire[2:11]) == (r["status"], r["address"], r["length"]), r["name"]


def test_client_start_frame_is_byte_identical_to_the_real_upload(vectors, repo, server, client):
    """The first START the client sends for AA00000100.hex is the one in _LOG."""
    hu = vectors["hex_upload"]
    golden = next(f for f in vectors["frames"] if f["name"] == hu["start_frame"])
    (addr, data), = bx.parse_ihex(f"{repo}/{hu['file']}")
    asyncio.run(client.send(bx.APP_TYPE_SEGMENT, struct.pack("<I", addr) + data))
    assert server.writes[0].hex() == golden["hex"]


def test_client_reads_caps(vectors, server, client):
    server.caps = bytes.fromhex(vectors["caps"]["hex"])
    assert asyncio.run(client.read_caps()) == (vectors["protocol_version"], vectors["max_frame_len"],
                                               vectors["window"])


@pytest.mark.parametrize("uuid_id", [1, 2, 3])
def test_characteristic_uuids_use_project_base(uuid_id):
    # _ASW/_BLE_GENERIX/BaseUUIDs.h and _DOC/HexUpload/PROTOCOL.md §2
    assert bx.char_uuid(uuid_id, bx.PROJECT_BASE) == f"b1c0000{uuid_id}-16a1-4812-af35-f3f29a92f6ca"
