#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Write the "hex_file" and "file_system" sections of _TEST/vectors/wire.json.

hex_file     Storing a hex upload as a file (_DOC/HexUpload/PROTOCOL.md):
             BEGIN / COMMIT short messages, the FILE reply, and the record of
             the first segment of AA00000100.hex with the file's size and
             CRC-32 after it.
file_system  The BLE file commands (_DOC/FileSysManager/PROTOCOL.md): appTypes,
             ops, status codes, limits and golden CMD / REPLY / ENTRY frames.

Short frames are [payload length][appType][payload]. Run after a change to
either wire format, then rebuild the tests:

    python _TEST/tools/gen_fs_vector.py

Only these two sections are replaced (see wire_section.py).
"""

import os
import struct
import zlib

from gen_vectors import read_ihex
from wire_section import replace_section

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")

# FsmgrStatus_E (_LIB/FileSysManager/FileSysManager_Types.h)
STATUS = {"OK": 0, "NOT_FOUND": 1, "EXISTS": 2, "NOT_EMPTY": 3, "NO_SPACE": 4, "BAD_ARG": 5,
          "BAD_STATE": 6, "BUSY": 7, "NOT_MOUNTED": 8, "IO": 9, "NOT_SUPPORTED": 10,
          "WRONG_TYPE": 11, "DENIED": 12}

HEX_TYPES = {"BEGIN": 0x12, "COMMIT": 0x13, "FILE": 0x14}
FS_TYPES = {"CMD": 0x40, "REPLY": 0x41, "ENTRY": 0x42}
OPS = {"MKDIR": 1, "CD": 2, "OPENR": 3, "OPENW": 4, "WRITE": 5, "READ": 6, "LS": 7,
       "DELFILE": 8, "DELDIR": 9, "CLOSE": 10, "ABORT": 11}
ENTRY_TYPES = {"FILE": 0, "DIR": 1}
SHORT_MAX = 242                                       # SETU short payload
LIMITS = {"arg_max": SHORT_MAX - 2, "read_max": SHORT_MAX - 3, "entry_path_max": SHORT_MAX - 6}


def short(name: str, app_type: int, payload: bytes, **fields) -> dict:
    assert len(payload) <= SHORT_MAX, name
    entry = {"name": name, "app_type": app_type}
    entry.update(fields)
    entry["hex"] = (bytes([len(payload), app_type]) + payload).hex()
    return entry


def file_reply(name, op, status, size=0, crc=0):
    payload = struct.pack("<BBII", op, STATUS[status], size, crc)
    return short(name, HEX_TYPES["FILE"], payload, op=op, status=STATUS[status], size=size, crc32=crc)


def hex_file_section() -> dict:
    mem = read_ihex(os.path.join(ROOT, "AA00000100.hex"))
    addr, length = 0, 6560                            # first segment (see "hex_upload")
    data = bytes(mem[a] for a in range(addr, addr + length))
    record = struct.pack("<II", addr, length) + data
    crc = zlib.crc32(record) & 0xFFFFFFFF
    return {
        "_comment": [
            "Storing a hex upload as a file (_DOC/HexUpload/PROTOCOL.md). BEGIN carries the",
            "file name, COMMIT nothing; FILE answers both: [u8 op][u8 status][u32 LE file",
            "size][u32 LE CRC-32 of the file], status as in file_system.status. The file",
            "holds one record [u32 LE address][u32 LE length][data] per segment; 'record' is",
            "the first segment of AA00000100.hex. Written by _TEST/tools/gen_fs_vector.py."
        ],
        "app_types": HEX_TYPES,
        "dir": "/FLASH_DISK:/FW",
        "temp_name": "UPLOAD.TMP",
        "name_max": 32,
        "file_reply_len": 10,
        "record": {"address": addr, "length": length, "header_hex": record[:8].hex(),
                   "file_size": len(record), "file_crc32": crc},
        "shorts": [
            short("begin_fw1", HEX_TYPES["BEGIN"], b"FW1", file="FW1"),
            short("begin_long_name", HEX_TYPES["BEGIN"], b"ECU_App-v1.2.3.bin", file="ECU_App-v1.2.3.bin"),
            short("commit", HEX_TYPES["COMMIT"], b""),
            file_reply("file_begin_ok", HEX_TYPES["BEGIN"], "OK"),
            file_reply("file_begin_busy", HEX_TYPES["BEGIN"], "BUSY"),
            file_reply("file_begin_bad_name", HEX_TYPES["BEGIN"], "BAD_ARG"),
            file_reply("file_commit_ok", HEX_TYPES["COMMIT"], "OK", len(record), crc),
            file_reply("file_commit_no_begin", HEX_TYPES["COMMIT"], "BAD_STATE"),
            file_reply("file_commit_io", HEX_TYPES["COMMIT"], "IO"),
        ],
    }


def cmd(name, seq, op, arg=b""):
    return short(name, FS_TYPES["CMD"], bytes([seq, OPS[op]]) + arg, seq=seq, op=OPS[op],
                 arg_hex=arg.hex())


def reply(name, seq, op, status, data=b""):
    return short(name, FS_TYPES["REPLY"], bytes([seq, OPS[op], STATUS[status]]) + data, seq=seq,
                 op=OPS[op], status=STATUS[status], data_hex=data.hex())


def entry(name, seq, etype, size, path):
    return short(name, FS_TYPES["ENTRY"], struct.pack("<BBI", seq, ENTRY_TYPES[etype], size)
                 + path.encode(), seq=seq, type=ENTRY_TYPES[etype], size=size, path=path)


def file_system_section() -> dict:
    return {
        "_comment": [
            "BLE file commands (_DOC/FileSysManager/PROTOCOL.md). CMD [seq][op][arg];",
            "REPLY [seq][op][status][data]; ENTRY [seq][type][u32 LE size][path], one per",
            "listed entry before the REPLY of LS. Status codes are FsmgrStatus_E.",
            "Written by _TEST/tools/gen_fs_vector.py."
        ],
        "app_types": FS_TYPES,
        "app_type_range": [0x40, 0x4F],
        "ops": OPS,
        "status": STATUS,
        "entry_types": ENTRY_TYPES,
        "limits": LIMITS,
        "shorts": [
            cmd("cmd_mkdir", 1, "MKDIR", b"FW"),
            reply("reply_mkdir_ok", 1, "MKDIR", "OK", b"/FLASH_DISK:/FW"),
            cmd("cmd_cd_abs", 2, "CD", b"/FLASH_DISK:/FW"),
            reply("reply_cd_not_found", 2, "CD", "NOT_FOUND"),
            cmd("cmd_openw", 3, "OPENW", b"a.txt"),
            reply("reply_openw_ok", 3, "OPENW", "OK", b"/FLASH_DISK:/FW/a.txt"),
            cmd("cmd_write", 4, "WRITE", b"hello"),
            reply("reply_write_ok", 4, "WRITE", "OK", struct.pack("<I", 5)),
            cmd("cmd_close", 5, "CLOSE"),
            reply("reply_close_ok", 5, "CLOSE", "OK", struct.pack("<I", 5)),
            cmd("cmd_openr", 6, "OPENR", b"a.txt"),
            cmd("cmd_read_default", 7, "READ"),
            cmd("cmd_read_16", 8, "READ", struct.pack("<H", 16)),
            reply("reply_read_data", 8, "READ", "OK", b"hello"),
            reply("reply_read_eof", 9, "READ", "OK"),
            cmd("cmd_ls", 10, "LS"),
            entry("entry_dir_fw", 10, "DIR", 0, "/FLASH_DISK:/FW"),
            entry("entry_file_fw1", 10, "FILE", 6568, "/FLASH_DISK:/FW/FW1"),
            reply("reply_ls_ok", 10, "LS", "OK", struct.pack("<H", 2)),
            cmd("cmd_delfile", 11, "DELFILE", b"/FW/a.txt"),
            cmd("cmd_deldir", 12, "DELDIR", b"FW"),
            reply("reply_deldir_not_supported", 12, "DELDIR", "NOT_SUPPORTED"),
            cmd("cmd_abort", 13, "ABORT"),
            reply("reply_write_bad_state", 14, "WRITE", "BAD_STATE"),
            reply("reply_busy", 15, "OPENW", "BUSY"),
            reply("reply_not_mounted", 16, "LS", "NOT_MOUNTED"),
            reply("reply_bad_op", 17, "MKDIR", "BAD_ARG"),
        ],
    }


def main() -> None:
    replace_section("hex_file", hex_file_section())
    replace_section("file_system", file_system_section())
    print("hex_file and file_system vectors written")


if __name__ == "__main__":
    main()
