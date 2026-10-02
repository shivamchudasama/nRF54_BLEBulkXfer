# SPDX-License-Identifier: MIT
"""The provisioning CA (_TOOLS/BleHostGUI/blehost/pki/authority.py) against the
certificate profile in _DOC/Provisioning/PROTOCOL.md, using the real device CSR
from wire.json (built by the device's DER encoder, real signature)."""

import datetime
import hashlib
import json
import os

import pytest
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509.oid import NameOID

from blehost.pki import authority as pki
from blehost.pki.authority import CaError, CertificateAuthority, CsrRejected

P256_SPKI_PREFIX = bytes.fromhex("3059301306072a8648ce3d020106082a8648ce3d030107034200")


# ---- minimal DER walker: the device compares raw bytes, so the tests do too ----
def tlv(buf, pos=0):
    """-> (tag, start of value, end of TLV) of the TLV at pos."""
    tag, ln = buf[pos], buf[pos + 1]
    hdr = 2
    if ln & 0x80:
        n = ln & 0x7F
        ln, hdr = int.from_bytes(buf[pos + 2:pos + 2 + n], "big"), 2 + n
    return tag, pos + hdr, pos + hdr + ln


def children(buf):
    """TLVs inside the constructed value buf[0] (as raw byte strings)."""
    _, start, end = tlv(buf)
    out, pos = [], start
    while pos < end:
        _, _, nxt = tlv(buf, pos)
        out.append(buf[pos:nxt])
        pos = nxt
    return out


def csr_subject_raw(csr):
    return children(csr.tbs_certrequest_bytes)[1]           # version, subject, spki, attrs


def cert_tbs_fields(cert):
    f = children(cert.tbs_certificate_bytes)               # [0]version, serial, sig, issuer,
    return {"issuer": f[3], "subject": f[5], "spki": f[6]}  # validity, subject, spki, [3]ext


# ---- fixtures ----------------------------------------------------------------
@pytest.fixture
def ca(tmp_path):
    return CertificateAuthority.create(str(tmp_path / "ca"), {"C": "IN", "O": "Test Org", "CN": "Test CA"})


@pytest.fixture(scope="session")
def device_csr(vectors):
    return bytes.fromhex(vectors["provisioning"]["csr"]["der"])


def make_csr(key=None, cn="dev", ku=True, key_agreement=True, ca_flag=False, alg=hashes.SHA256()):
    key = key or ec.generate_private_key(ec.SECP256R1())
    b = x509.CertificateSigningRequestBuilder().subject_name(
        x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, cn)] if cn else []))
    b = b.add_extension(x509.BasicConstraints(ca=ca_flag, path_length=None), critical=False)
    if ku:
        b = b.add_extension(x509.KeyUsage(False, False, False, False, key_agreement, False, False, False, False),
                            critical=False)
    return b.sign(key, alg).public_bytes(serialization.Encoding.DER)


# ---- the CA itself -----------------------------------------------------------
def test_create_makes_a_self_signed_p256_ca(tmp_path, ca):
    c = ca.certificate
    assert c.version == x509.Version.v3
    assert c.subject == c.issuer and c.subject.rfc4514_string() == "CN=Test CA,O=Test Org,C=IN"
    c.verify_directly_issued_by(c)
    assert isinstance(c.public_key().curve, ec.SECP256R1)
    assert c.signature_hash_algorithm.name == "sha256"
    bc = c.extensions.get_extension_for_class(x509.BasicConstraints)
    assert bc.critical and bc.value.ca
    ku = c.extensions.get_extension_for_class(x509.KeyUsage).value
    assert ku.key_cert_sign, "the device checks keyCertSign when KeyUsage is present"
    for name in (pki.KEY_FILE, pki.CERT_FILE, pki.DB_FILE):
        assert os.path.exists(tmp_path / "ca" / name)
    assert CertificateAuthority.exists(str(tmp_path / "ca"))


def test_load_returns_the_same_ca(tmp_path, ca):
    again = CertificateAuthority.load(str(tmp_path / "ca"))
    assert again.cert_der == ca.cert_der and again.fingerprint == ca.fingerprint


def test_create_refuses_bad_input(tmp_path, ca):
    with pytest.raises(CaError, match="already exists"):
        CertificateAuthority.create(str(tmp_path / "ca"))
    with pytest.raises(CaError, match="CN"):
        CertificateAuthority.create(str(tmp_path / "x"), {"O": "no cn"})
    with pytest.raises(CaError, match="unknown"):
        CertificateAuthority.create(str(tmp_path / "y"), {"CN": "a", "EMAIL": "b"})
    with pytest.raises(CaError, match="validity"):
        CertificateAuthority.create(str(tmp_path / "z"), validity_days=0)


def test_load_errors(tmp_path, ca):
    with pytest.raises(CaError, match="no CA"):
        CertificateAuthority.load(str(tmp_path / "missing"))
    other = CertificateAuthority.create(str(tmp_path / "other"))
    os.replace(tmp_path / "other" / pki.CERT_FILE, tmp_path / "ca" / pki.CERT_FILE)
    with pytest.raises(CaError, match="does not match"):
        CertificateAuthority.load(str(tmp_path / "ca"))
    assert other


# ---- signing the device's CSR -----------------------------------------------
def test_signs_the_real_device_csr(ca, device_csr, vectors):
    v = vectors["provisioning"]["csr"]
    issued = ca.sign_csr(device_csr)
    cert = issued.certificate
    csr = x509.load_der_x509_csr(device_csr)
    f = cert_tbs_fields(cert)

    cert.verify_directly_issued_by(ca.certificate)
    # The device memcmp()s the subject with its CSR's, and the key with its own
    assert f["subject"] == csr_subject_raw(csr), "subject must be byte-identical to the CSR's"
    assert f["spki"] == P256_SPKI_PREFIX + bytes.fromhex(v["public_key"])
    assert f["issuer"] == children(ca.certificate.tbs_certificate_bytes)[5], "issuer = CA subject"
    assert len(issued.der) <= 1024
    bc = cert.extensions.get_extension_for_class(x509.BasicConstraints)
    assert bc.critical and not bc.value.ca
    assert cert.extensions.get_extension_for_class(x509.KeyUsage).value.key_agreement
    ski = cert.extensions.get_extension_for_class(x509.SubjectKeyIdentifier).value.digest
    assert ski.hex() == v["ski_sha1"], "the device's SKI is copied, not recomputed"
    aki = cert.extensions.get_extension_for_class(x509.AuthorityKeyIdentifier).value.key_identifier
    assert aki == ca.certificate.extensions.get_extension_for_class(x509.SubjectKeyIdentifier).value.digest
    assert cert.signature_hash_algorithm.name == "sha256"


def test_issued_certificates_are_recorded(tmp_path, ca, device_csr):
    issued = ca.sign_csr(device_csr)
    db = json.loads((tmp_path / "ca" / pki.DB_FILE).read_text())
    entry = db[issued.serial_hex]
    assert entry["subject"] == issued.certificate.subject.rfc4514_string()
    assert entry["public_key_sha256"] == hashlib.sha256(
        issued.certificate.public_key().public_bytes(serialization.Encoding.X962,
                                                     serialization.PublicFormat.UncompressedPoint)).hexdigest()
    pem = (tmp_path / "ca" / pki.ISSUED_DIR / (issued.serial_hex + ".pem")).read_bytes()
    assert x509.load_pem_x509_certificate(pem) == issued.certificate


def test_status_hash_identifies_the_csr_key(vectors, device_csr):
    status = next(s for s in vectors["provisioning"]["shorts"] if s["name"] == "status_key_ready")
    key = x509.load_der_x509_csr(device_csr).public_key()
    assert pki.public_key_sha256(key).hex() == status["pubkey_sha256"]


def test_serials_are_unique(ca, device_csr, monkeypatch):
    a = ca.sign_csr(device_csr)
    seq = iter([int(a.serial_hex, 16), int(a.serial_hex, 16), 12345])
    monkeypatch.setattr(pki.x509, "random_serial_number", lambda: next(seq))
    b = ca.sign_csr(device_csr)
    assert b.serial_hex == f"{12345:x}"
    monkeypatch.setattr(pki.x509, "random_serial_number", lambda: 12345)
    with pytest.raises(CaError, match="serial"):
        ca.sign_csr(device_csr)


def test_validity_is_capped_by_the_ca(tmp_path, device_csr):
    ca = CertificateAuthority.create(str(tmp_path / "short"), validity_days=10)
    cert = ca.sign_csr(device_csr, validity_days=365).certificate
    assert cert.not_valid_after_utc == ca.not_after
    cert = ca.sign_csr(device_csr, validity_days=2).certificate
    assert cert.not_valid_after_utc - cert.not_valid_before_utc == datetime.timedelta(days=2)


def test_expired_ca_does_not_sign(ca, device_csr, monkeypatch):
    monkeypatch.setattr(pki, "_now", lambda: ca.not_after + datetime.timedelta(days=1))
    with pytest.raises(CaError, match="not valid"):
        ca.sign_csr(device_csr)


def test_python_csr_with_the_device_layout_is_accepted(ca):
    assert ca.sign_csr(make_csr()).certificate.subject.rfc4514_string() == "CN=dev"


# ---- rejections ------------------------------------------------------------------
def tamper(der, at):
    b = bytearray(der)
    b[at] ^= 0x01
    return bytes(b)


@pytest.mark.parametrize("csr, why", [
    (lambda d: b"\x30\x03\x02\x01\x00", "not a DER CSR"),
    (lambda d: tamper(d, len(d) - 5), "signature"),                    # inside the signature
    (lambda d: make_csr(key=ec.generate_private_key(ec.SECP384R1())), "P-256"),
    (lambda d: make_csr(alg=hashes.SHA384()), "ecdsa-with-SHA256"),
    (lambda d: make_csr(ca_flag=True), "CA certificate"),
    (lambda d: make_csr(ku=False), "keyAgreement"),
    (lambda d: make_csr(key_agreement=False), "keyAgreement"),
    (lambda d: make_csr(cn=None), "common name"),
])
def test_csr_rejections(ca, device_csr, csr, why):
    with pytest.raises(CsrRejected, match=why):
        ca.sign_csr(csr(device_csr))
    assert ca.issued() == {}


def test_validity_must_be_positive(ca, device_csr):
    with pytest.raises(CsrRejected, match="validity"):
        ca.sign_csr(device_csr, validity_days=0)
