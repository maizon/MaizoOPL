#!/usr/bin/env python3
"""Read-only graphics-engine leads; never turn string hints into live hooks."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
from maizo_mage_sky_discover import discover

PATTERNS = {
    "RenderWare": rb"RenderWare|RwEngineInit|RwEngineOpen|RwCameraBeginUpdate",
    "Unreal": rb"Unreal Engine|UnrealEngine|Unreal Tournament",
    "Gamebryo/NetImmerse": rb"Gamebryo|NetImmerse",
    "LithTech": rb"LithTech",
    "id Tech": rb"id Software|Quake III",
    "Criterion": rb"Criterion Software|Criterion Games",
    "Havok (physics only)": rb"Havok|hkBaseSystem",
    "CRI (middleware)": rb"CRI Middleware|CRI ADX",
    "Bink (video)": rb"Bink Video|RAD Game Tools",
}

def leads(data, match):
    """Resolve standard-function and TEX1 shadow candidates from ELF metadata."""
    h = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    ph = [struct.unpack_from("<8I", data, h[5]+i*h[9]) for i in range(h[10])]
    def words(a, n, executable=False):
        hits = [p for p in ph if p[0] == 1 and p[2] <= a
                and a + n*4 <= p[2] + p[4] and p[1]+p[4] <= len(data)
                and (not executable or p[6]&1)]
        if a&3 or len(hits) != 1:
            raise ValueError("invalid or ambiguous load range")
        p = hits[0]
        return struct.unpack_from("<%dI" % n, data, p[1]+a-p[2])
    out = {"verified_runtime": False}
    device = int(match["device"], 16)
    try:
        values = words(device-240, 56)
        ids = values[::2]
        if set(ids) == set(range(1, 29)) and len(set(ids)) == 28:
            table = dict(zip(ids, values[1::2]))
            for k, name in [(1, "camera_begin"), (10, "camera_end"),
                            (20, "show_raster"), (28, "raster_mip_levels")]:
                words(table[k], 4, True)
                out[name] = hex(table[k])
    except ValueError:
        pass
    gp = []
    if h[11] == 40 and h[12] <= 4096 and h[6]+h[11]*h[12] <= len(data):
        for i in range(h[12]):
            s = struct.unpack_from("<10I", data, h[6]+40*i)
            if s[1] == 0x70000006 and s[5] >= 24 and s[4]+24 <= len(data):
                gp.append(struct.unpack_from("<I", data, s[4]+20)[0])
    if len(set(gp)) == 1:
        try:
            w = words(int(match["state"], 16)+35*4, 1, True)[0]
            if w>>16 == 0xff82:
                delta = w&65535
                if delta&32768:
                    delta -= 65536
                a = gp[0]+delta
                if not a&7 and any(p[0]==1 and p[6]&2 and p[2]<=a and a+8<=p[2]+p[5] for p in ph):
                    out["gp"] = hex(gp[0])
                    out["tex1_shadow"] = hex(a)
        except ValueError:
            pass
    return out

def inspect(data):
    report = {"sha256": hashlib.sha256(data).hexdigest(),
              "hints_only": [k for k, p in PATTERNS.items() if re.search(p, data, re.I)],
              "sky": [], "hardware_verified": False}
    try:
        report["sky"] = [dict(m, static_leads=leads(data,m)) for m in discover(data)["matches"]]
    except (ValueError, struct.error) as exc:
        report["rejected"] = str(exc)
    return report

if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("elf", type=Path, nargs="+")
    args = p.parse_args()
    print(json.dumps([dict(inspect(x.read_bytes()), file=x.name) for x in args.elf], indent=2))
