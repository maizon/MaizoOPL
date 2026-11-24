"""Validate the actual ELF image and its versioned header before upload."""
import re
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
layout = (root / "ee_core/include/maizo_overlay_layout.h").read_text()
def value(name):
    return int(re.search(r"^#define\s+" + name + r"\s+(0x[0-9a-fA-F]+|[0-9]+)u", layout, re.M)[1], 0)

base, limit, magic, abi = (value(n) for n in ("MZ_OVL_BASE", "MZ_OVL_END", "MZ_OVL_MAGIC", "MZ_OVL_ABI"))
assert limit == 0xD0000 and base == 0xC8000 and limit-base == 32768
raw = (root / "ee_core/overlay/maizo_overlay.elf").read_bytes()
assert raw[:6] == b"\x7fELF\x01\x01", "expected ELF32 little endian"
eh = struct.unpack_from("<16sHHIIIIIHHHHHH", raw)
phoff, phentsize, phnum = eh[5], eh[9], eh[10]
assert phentsize == 32 and phoff + phentsize*phnum <= len(raw)
loads = [struct.unpack_from("<IIIIIIII", raw, phoff+i*phentsize) for i in range(phnum)]
loads = [p for p in loads if p[0] == 1]
assert len(loads) == 1
_, off, addr, _, filesz, memsz, flags, _ = loads[0]
assert addr == base and 28 <= filesz <= memsz <= limit-base
assert off+filesz <= len(raw) and flags & 1
m, a, end, pre, run, profile, api_size = struct.unpack_from("<7I", raw, off)
assert (m, a) == (magic, abi)
assert base+28 < end <= base+memsz <= limit
assert all(base+28 <= p < base+filesz and p%4 == 0 for p in (pre, run, profile))
assert 0 < api_size <= 1024 and api_size%4 == 0
print(f"MAIZO overlay: {memsz} bytes / {limit-base}, {limit-base-memsz} bytes free; ABI {abi}; module arena {base-0x9e000} bytes")
