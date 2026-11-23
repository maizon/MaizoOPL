#!/usr/bin/env python3
"""Verify MAIZO Store's static security invariants without a PS2 console."""
import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
catalog = (root / "include/maizo_store_catalog.h").read_text()
store = (root / "src/maizo_store.c").read_text()
tls = (root / "src/maizo_store_tls.c").read_text()
rootgen = (root / "scripts/gen_maizo_store_roots.py").read_text()
rootpem = (root / "thirdparty/maizo_store_roots.pem").read_text()

size = int(re.search(r"#define STORE_ITEMS\s+(\d+)", catalog).group(1))
lines = [line for line in catalog.splitlines() if line.strip().startswith('{"')]
assert len(lines) == size and size >= 50, (size, len(lines))
ids = set()
for line in lines:
    m = re.match(r'\s*\{"([a-z0-9-]+)",\s*"([^"]+)",', line)
    assert m, line[:200]
    assert m.group(1) not in ids, m.group(1)
    ids.add(m.group(1))
    h = re.search(r'"([a-f0-9]{64})",\s*(\d+)u,', line)
    assert h and int(h.group(2)) >= 64, line[:200]

assert "br_ssl_client_init_full" in tls
assert "br_x509_minimal_set_time" in tls
assert "br_ssl_client_reset(&ctx->client, host" in tls  # SNI / DNSName
assert "MAIZO_STORE_ROOTS_COUNT" in tls
assert "noverify" not in tls.lower()
assert "br_x509_decoder" not in tls  # client must use BearSSL full validator
assert "memcmp(reply + 24, req + 40, 8)" in tls  # NTP origin echo
assert "if (!host || !*host || !maizo_tls_clock(&epoch))" in tls
assert "src/maizo_store.c" not in catalog  # No arbitrary executable loads
assert "maizo_sha256_final(&sha, hex)" in store
assert "strcmp(hex, app->hash)" in store
assert "fileXioRename(tmp, path)" in store
assert "storeOnline[i] = 1" in store
assert "for (int i = 0; i < STORE_ITEMS; i++)" in store
assert "raw.githubusercontent.com" in store
assert "cdn.jsdelivr.net" in store
assert "decode_root" in rootgen
assert rootpem.count("BEGIN CERTIFICATE") >= 5
print("MAIZO Store: %d pinned ELFs, CA-validated TLS, attribution and SHA-256 contracts OK" % size)
