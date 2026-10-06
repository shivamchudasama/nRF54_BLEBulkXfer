#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Write the "pairing" section of _TEST/vectors/wire.json.

The section holds the device pairing contract (_DOC/Pairing/PROTOCOL.md):
UUIDs, appTypes, CONTROL opcodes, roles, states, errors, golden CONTROL and
STATUS frames, and one signed OOB frame. The OOB frame is signed with a fresh
P-256 key (ECDSA-SHA256, raw r || s) over

    r || c || sender address || receiver address

so the C test (pair_oob, on the real TF-PSA-Crypto) and the Python tests can
both check a signature made by the other side's library.

Run after a change to the pairing wire format, then rebuild the tests:

    python _TEST/tools/gen_pair_vector.py

Only the "pairing" section is replaced (or appended); the rest of wire.json
keeps its hand formatting. Needs cryptography (in _TEST/requirements-test.txt).
"""

import json
import os
import re

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature

HERE = os.path.dirname(os.path.abspath(__file__))
WIRE = os.path.join(HERE, "..", "vectors", "wire.json")
BASE = "-16a1-4812-af35-f3f29a92f6ca"

STATES = {"IDLE": 0, "ARMED": 1, "CONNECTED": 2, "CERT_EXCHANGE": 3, "CERT_VERIFIED": 4,
          "OOB_EXCHANGE": 5, "PAIRING": 6, "PAIRED": 7, "FAILED": 8}
ERRORS = {"NONE": 0, "NOT_PROVISIONED": 1, "BAD_ARG": 2, "BUSY": 3, "TIMEOUT": 4, "CONNECT": 5,
          "NO_PEER_SVC": 6, "PEER_CERT": 7, "OOB_SIG": 8, "SMP": 9, "TRANSFER": 10, "SECURED": 11,
          "LINK_LOST": 12, "CANCELLED": 13, "INTERNAL": 14}
ROLES = {"NONE": 0, "CENTRAL": 1, "PERIPHERAL": 2}
OPS = {"START": 1, "CANCEL": 2, "UNPAIR": 3}

# Two example devices (random static addresses, as the nRF54L15 has)
DEV_A = ("C4:5E:2A:11:9F:03", 1)
DEV_B = ("E1:02:03:04:05:06", 1)


def addr_wire(addr: str, addr_type: int) -> bytes:
    """[type][6 B address, least significant byte first], as bt_addr_le_t."""
    return bytes([addr_type]) + bytes.fromhex(addr.replace(":", ""))[::-1]


def control(name, op, role=None, peer=None):
    b = bytes([op])
    entry = {"name": name, "op": op}
    if role is not None:
        b += bytes([role]) + addr_wire(*peer)
        entry.update(role=role, peer=peer[0], peer_type=peer[1])
    entry["hex"] = b.hex()
    return entry


def status(name, state, error, detail, prov, role, own, peer):
    b = (bytes([STATES[state], ERRORS[error], detail, prov, ROLES[role]])
         + (addr_wire(*own) if own else bytes(7)) + (addr_wire(*peer) if peer else bytes(7)))
    return {"name": name, "state": STATES[state], "error": ERRORS[error], "detail": detail,
            "prov_state": prov, "role": ROLES[role],
            "own": own[0] if own else "", "own_type": own[1] if own else 0,
            "peer": peer[0] if peer else "", "peer_type": peer[1] if peer else 0,
            "hex": b.hex()}


def oob_vector():
    key = ec.generate_private_key(ec.SECP256R1())
    priv = key.private_numbers().private_value.to_bytes(32, "big")
    pub = key.public_key().public_bytes(serialization.Encoding.X962,
                                        serialization.PublicFormat.UncompressedPoint)
    r = os.urandom(16)
    c = os.urandom(16)
    sender = addr_wire(*DEV_A)
    receiver = addr_wire(*DEV_B)
    message = r + c + sender + receiver
    sig_r, sig_s = decode_dss_signature(key.sign(message, ec.ECDSA(hashes.SHA256())))
    sig = sig_r.to_bytes(32, "big") + sig_s.to_bytes(32, "big")
    return {"private_key": priv.hex(), "public_key": pub.hex(), "r": r.hex(), "c": c.hex(),
            "sender": sender.hex(), "receiver": receiver.hex(), "message": message.hex(),
            "signature_raw": sig.hex(), "frame": (r + c + sig).hex()}


def section() -> dict:
    return {
        "_comment": [
            "Device pairing (_DOC/Pairing/PROTOCOL.md). 'controls' are CONTROL writes,",
            "'statuses' STATUS values [state][error][detail][provState][role][own 7 B]",
            "[peer 7 B]; an address on the wire is [type][6 B, LSB first]. 'oob' is an OOB",
            "frame [r][c][signature] whose raw ECDSA-SHA256 signature (by 'private_key')",
            "covers 'message' = r || c || sender || receiver. Regenerate the whole section",
            "with _TEST/tools/gen_pair_vector.py (see _TEST/README.md)."
        ],
        "uuids": {"service": "b1c10000" + BASE, "control": "b1c10001" + BASE,
                  "status": "b1c10002" + BASE, "secured": "b1c10003" + BASE},
        "app_types": {"PEER_CERT": 48, "OOB": 49},
        "app_type_range": [48, 63],
        "ops": OPS,
        "roles": ROLES,
        "states": STATES,
        "errors": ERRORS,
        "addr_types": {"PUBLIC": 0, "RANDOM": 1},
        "start_len": 9,
        "status_len": 19,
        "oob_frame_len": 96,
        "oob_signed_len": 46,
        "secured_value": 1,
        "controls": [
            control("start_central", OPS["START"], ROLES["CENTRAL"], DEV_B),
            control("start_peripheral", OPS["START"], ROLES["PERIPHERAL"], DEV_A),
            control("cancel", OPS["CANCEL"]),
            control("unpair", OPS["UNPAIR"]),
        ],
        "statuses": [
            status("idle_provisioned", "IDLE", "NONE", 0, 3, "NONE", DEV_A, None),
            status("armed_central", "ARMED", "NONE", 0, 3, "CENTRAL", DEV_A, DEV_B),
            status("paired_peripheral", "PAIRED", "NONE", 0, 3, "PERIPHERAL", DEV_B, DEV_A),
            status("failed_peer_cert_bad_sig", "FAILED", "PEER_CERT", 5, 3, "CENTRAL", DEV_A, DEV_B),
            status("failed_not_provisioned", "FAILED", "NOT_PROVISIONED", 0, 1, "CENTRAL", DEV_A, DEV_B),
        ],
        "oob": oob_vector(),
    }


def main() -> None:
    with open(WIRE, encoding="utf-8") as f:
        text = f.read()
    body = json.dumps(section(), indent=2)
    body = "\n".join("  " + line for line in body.splitlines()).lstrip()
    m = re.search(r',\n  "pairing": ', text)
    if m:
        text = text[:m.start()] + "\n}\n"
    assert text.rstrip().endswith("}"), "wire.json must end with its closing brace"
    head = text.rstrip()[:-1].rstrip()
    text = head + ',\n  "pairing": ' + body + "\n}\n"
    json.loads(text)                          # still valid JSON
    with open(WIRE, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("pairing vectors written")


if __name__ == "__main__":
    main()
