#!/usr/bin/env python3
"""MAGE: select independent reviewed renderer patches before game launch.
No guessed patches. Exact executable SHA is mandatory even for shared engines.
The input ELF is never written. Reports distinguish static and live adapters.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from maizo_gs_scan_elf import loads
from maizo_gs_install_elf import install, file_location, number

FEATURES = ("aa1", "bilinear", "trilinear", "dither", "mipmap_lod",
            "shimmering", "deflicker", "sharp", "scanlines", "color",
            "resolution", "supersampling", "igr_boundary")
RANK = {"game": 0, "engine_version": 1, "engine_family": 2, "signature": 3}
LIVE = {"aa1", "bilinear", "dither", "scanlines"}

def digest(data):
    return hashlib.sha256(data).hexdigest()

def hints(data):
    # Strings are leads, not evidence of compatible code.
    lower = data.lower()
    return [name for name, tokens in (
        ("RenderWare", (b"renderware", b"rwrenderstate", b"rwcamera")),
        ("Unreal", (b"unreal engine",)),
        ("Gamebryo", (b"gamebryo", b"netimmerse")),
        ("id Tech", (b"id tech", b"quake iii")),
        ("LithTech", (b"lithtech",)),
    ) if any(t in lower for t in tokens)]

def word(v):
    v = number(v)
    if not 0 <= v <= 0xffffffff:
        raise ValueError("word outside uint32")
    return v

def static_edits(data, ranges, candidate):
    edits = {}
    phoff = struct.unpack_from("<I", data, 28)[0]
    phnum = struct.unpack_from("<H", data, 44)[0]
    header_end = max(52, phoff + 32 * phnum)
    sites = candidate["sites"]
    if not isinstance(sites, list) or not 1 <= len(sites) <= 32:
        raise ValueError("invalid site count")
    for site in sites:
        before = [word(v) for v in site["before"]]
        after = [word(v) for v in site["after"]]
        if not 4 <= len(before) <= 64 or len(before) != len(after):
            raise ValueError("need 4..64 exact context words and equal replacement")
        if before == after:
            raise ValueError("empty patch")
        pattern = struct.pack("<%dI" % len(before), *before)
        if "address" in site:
            off = file_location(ranges, number(site["address"]), len(pattern))
        else:
            matches = []
            for start, va, size in ranges:
                pos = data.find(pattern, start, start + size)
                while pos >= 0:
                    if (pos - start) % 4 == 0:
                        matches.append(pos)
                        if len(matches) > 1:
                            raise ValueError("ambiguous executable signature")
                    pos = data.find(pattern, pos + 1, start + size)
            if len(matches) != 1:
                raise ValueError("signature not found")
            off = matches[0]
        if off < header_end or data[off:off + len(pattern)] != pattern:
            raise ValueError("original instructions mismatch")
        for i, (old, new) in enumerate(zip(before, after)):
            if old != new:
                pos = off + i * 4
                if pos in edits:
                    raise ValueError("overlapping patch sites")
                edits[pos] = new
    return edits

def adapter_edits(data, candidate, root):
    name = candidate["payload"]
    path = (root / name).resolve()
    if not path.is_relative_to(root.resolve()):
        raise ValueError("adapter escapes database directory")
    payload = path.read_bytes()
    patched = install(data, payload, candidate["manifest"])
    return {i: struct.unpack_from("<I", patched, i)[0]
            for i in range(0, len(data) - 3, 4)
            if data[i:i+4] != patched[i:i+4]}

def resolve(data, database, requested, root=Path(".")):
    ranges = loads(data)
    sha = digest(data)
    if database.get("schema") != 1 or not isinstance(database.get("profiles"), list):
        raise ValueError("invalid database schema")
    if len(database["profiles"]) > 4096:
        raise ValueError("too many profiles")
    if set(requested) - set(FEATURES):
        raise ValueError("unknown feature")
    profiles = []
    for p in database["profiles"]:
        if (p.get("reviewed") is True and p.get("scope") in RANK
                and isinstance(p.get("elf_sha256"), list) and sha in p["elf_sha256"]):
            profiles.append(p)
    profiles.sort(key=lambda p: (RANK[p["scope"]], str(p.get("id", ""))))
    selected, changes, reserved = {}, {}, {}
    # Preserve each candidate's full verified context (not just changed words).
    for feature in FEATURES:
        if feature not in requested:
            continue
        reasons = []
        for p in profiles:
            for c in p.get("features", {}).get(feature, []):
                try:
                    method = c["method"]
                    if method == "static":
                        edits = static_edits(data, ranges, c)
                        # Static filters are boot-only, never advertised as live.
                        status = "boot_patch"
                    elif method == "adapter":
                        if feature not in LIVE or feature not in c.get("live_features", []):
                            raise ValueError("no reviewed live capability")
                        edits = adapter_edits(data, c, root)
                        status = "live_adapter_installed"
                    else:
                        raise ValueError("unknown method")
                    if not edits:
                        raise ValueError("no changes")
                    # Conservative: never combine independent patches in
                    # overlapping context windows. Identical adapter edits
                    # may service several menu options.
                    same = any(edits == prev for prev in reserved.values())
                    if not same and any(
                        abs(i-j) < 256 for i in edits for j in changes
                    ):
                        raise ValueError("conflicting or nearby patch sites")
                    if any(i in changes and changes[i] != v for i,v in edits.items()):
                        raise ValueError("conflicting replacement")
                    changes.update(edits)
                    reserved[feature] = edits
                    selected[feature] = {"status": status, "profile": p["id"],
                                         "scope": p["scope"], "method": method}
                    break
                except (ValueError, KeyError, TypeError, OSError, struct.error) as e:
                    reasons.append({"profile": p.get("id"), "reason": str(e)})
            if feature in selected:
                break
        if feature not in selected:
            selected[feature] = {"status": "original", "rejected": reasons,
                                 "reason": "no compatible reviewed patch"}
    out = bytearray(data)
    for off, new in changes.items():
        struct.pack_into("<I", out, off, new)
    report = {"elf_sha256": sha, "output_sha256": digest(out),
              "engine_hints_only": hints(data), "matched_profiles": [p["id"] for p in profiles],
              "features": selected, "changed_words": len(changes),
              "hardware_verified_by_tool": False}
    return bytes(out), report

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("elf", type=Path)
    p.add_argument("database", type=Path)
    p.add_argument("--features", default=",".join(FEATURES))
    p.add_argument("--output", type=Path, help="new ELF; omit for dry run")
    a = p.parse_args()
    data = a.elf.read_bytes()
    db = json.loads(a.database.read_text(encoding="utf-8"))
    output, report = resolve(data, db, a.features.split(","), a.database.parent)
    if a.output:
        if a.output.resolve() in (a.elf.resolve(), a.database.resolve()):
            p.error("output must be a new file")
        with a.output.open("xb") as f:
            f.write(output)
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
