#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Regenerate the "provisioning.csr" vector of _TEST/vectors/wire.json.

The vector is the CSR that the device's DER encoder (_ASW/_CSR/DER.c) builds,
with a REAL ECDSA-SHA256 signature, so the Python tests can check that the
CA's library (cryptography) accepts what the device sends:

  1. make a new P-256 key; write its public point and SHA-1(X||Y) (the SKI the
     device computes) into the vector, with an empty TBS/CSR, and SHA-256 of
     the point into the STATUS short vectors (the device reports that hash);
  2. rebuild the csr_der host test and run it with --dump: it encodes the CSR
     for the vector's subject and key and prints the TBS;
  3. sign the TBS with the key, write the raw signature (r||s), rebuild, dump
     again: the printed CSR is the golden one; write TBS and CSR.

Run after any change to DER.c (the C test fails until then), from a configured
test build (e.g. after _TEST/run_tests.ps1):

    python _TEST/tools/gen_csr_vector.py build_test

Needs cryptography (in _TEST/requirements-test.txt) and cmake on PATH (or
CMAKE=<path>). Only the "csr" object's hex values are rewritten in place;
the rest of wire.json keeps its hand formatting.
"""

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature

HERE = os.path.dirname(os.path.abspath(__file__))
WIRE = os.path.join(HERE, "..", "vectors", "wire.json")


def set_hex(text: str, key: str, value: str) -> str:
    """Replace "key": "<hex>" inside the provisioning.csr object only."""
    start = text.index('"csr"', text.index('"provisioning"'))
    pat = re.compile(r'("%s":\s*")[0-9a-fA-F]*(")' % key)
    head, tail = text[:start], text[start:]
    tail, n = pat.subn(lambda m: m.group(1) + value + m.group(2), tail, count=1)
    if n != 1:
        sys.exit(f"{key} not found in provisioning.csr")
    return head + tail


def set_status_hash(text: str, pubkey_sha256: bytes) -> str:
    """STATUS vectors carry SHA-256(public key): point them at the new key."""
    prov = json.loads(text)["provisioning"]
    for s in prov["shorts"]:
        if "pubkey_sha256" not in s:
            continue
        payload = (bytes([s["state"], s["flags"]]) + s["csr_len"].to_bytes(2, "little")
                   + pubkey_sha256)
        wire = bytes([len(payload), s["app_type"]]) + payload
        start = text.index(f'"name": "{s["name"]}"')
        end = text.index("}", start)
        obj = text[start:end]
        obj = re.sub(r'("pubkey_sha256":\s*")[0-9a-f]*(")',
                     lambda m: m.group(1) + pubkey_sha256.hex() + m.group(2), obj)
        obj = re.sub(r'("hex":\s*")[0-9a-f]*(")',
                     lambda m: m.group(1) + wire.hex() + m.group(2), obj)
        text = text[:start] + obj + text[end:]
    return text


def write(text: str) -> None:
    json.loads(text)
    with open(WIRE, "w", newline="") as f:
        f.write(text)


def build_and_dump(build: str) -> dict:
    cmake = os.environ.get("CMAKE") or shutil.which("cmake")
    if not cmake:
        sys.exit("cmake not found (set CMAKE=<path>)")
    subprocess.run([cmake, "--build", build, "--target", "csr_der"], check=True)
    exe = os.path.join(build, "csr_der.exe" if os.name == "nt" else "csr_der")
    out = subprocess.run([exe, "--dump"], check=True, capture_output=True, text=True).stdout
    return dict(line.split(" ", 1) for line in out.strip().splitlines())


def main(build: str) -> None:
    with open(WIRE, newline="") as f:
        text = f.read()

    key = ec.generate_private_key(ec.SECP256R1())
    pub = key.public_key().public_bytes(serialization.Encoding.X962,
                                        serialization.PublicFormat.UncompressedPoint)
    text = set_hex(text, "public_key", pub.hex())
    text = set_hex(text, "ski_sha1", hashlib.sha1(pub[1:]).hexdigest())
    text = set_status_hash(text, hashlib.sha256(pub).digest())
    text = set_hex(text, "tbs", "")
    text = set_hex(text, "signature_raw", "00" * 64)
    text = set_hex(text, "der", "")
    write(text)

    tbs = bytes.fromhex(build_and_dump(build)["TBS"])
    r, s = decode_dss_signature(key.sign(tbs, ec.ECDSA(hashes.SHA256())))
    text = set_hex(text, "signature_raw", (r.to_bytes(32, "big") + s.to_bytes(32, "big")).hex())
    write(text)

    dump = build_and_dump(build)
    assert bytes.fromhex(dump["TBS"]) == tbs
    text = set_hex(text, "tbs", dump["TBS"])
    text = set_hex(text, "der", dump["DER"])
    write(text)
    build_and_dump(build)
    print(f"CSR vector regenerated: {len(dump['DER']) // 2} bytes")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
