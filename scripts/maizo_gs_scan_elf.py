#!/usr/bin/env python3
"""Read-only R5900 ELF discovery for GS submission candidates.
Not a disassembler of arbitrary VU code, nor an automatic patch installer.
Only executable PT_LOAD file ranges are considered, and the SHA-256 pins
the exact revision. A candidate requires human/trace verification before
it can be registered as a renderer adapter.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

MMIO = {
    0x1000a000: "PATH3_CHCR", 0x1000a010: "PATH3_MADR",
    0x1000a020: "PATH3_QWC", 0x1000a030: "PATH3_TADR",
    0x10009000: "PATH2_CHCR", 0x10009010: "PATH2_MADR",
    0x10009020: "PATH2_QWC", 0x10009030: "PATH2_TADR",
    0x10006000: "GIF_FIFO", 0x10005000: "VIF1_FIFO",
    0x12000070: "DISPFB1", 0x12000090: "DISPFB2",
}
STORE = {0x28, 0x29, 0x2b, 0x3f, 0x1f} # SB, SH, SW, SD, SQ
BRANCH = {1, 2, 3, 4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17}

def loads(data):
    if len(data) < 52 or data[:7] != b"\x7fELF\x01\x01\x01":
        raise ValueError("expected ELF32 little-endian")
    h = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    if h[2] != 8 or h[1] != 2 or h[8] != 52 or h[9] != 32:
        raise ValueError("expected executable MIPS ELF with standard headers")
    off, count = h[5], h[10]
    if count > 128 or off + count * 32 > len(data):
        raise ValueError("invalid program header table")
    ranges = []
    for i in range(count):
        typ, foff, va, _, filesz, memsz, flags, _ = struct.unpack_from("<8I", data, off+i*32)
        if typ != 1 or not (flags & 1):
            continue
        if (foff+filesz > len(data) or filesz > memsz or va < 0x100000
                or va+memsz > 0x2000000 or va & 3 or filesz & 3):
            raise ValueError("invalid executable load range")
        if filesz:
            ranges.append((foff, va, filesz))
    if not ranges:
        raise ValueError("no executable EE load range")
    return ranges

def signed16(x):
    return x-65536 if x & 32768 else x

def discover(data):
    found = []
    for off, va, size in loads(data):
        # Track literal addresses over only a short straight-line window.
        # Clear on unknown writes and control transfers (including delay slots).
        regs = {0: (0, 0)}
        for n in range(0, size, 4):
            word, = struct.unpack_from("<I", data, off+n)
            op, rs, rt, rd = word>>26, (word>>21)&31, (word>>16)&31, (word>>11)&31
            imm = word&65535
            regs = {r: (v,t) for r,(v,t) in regs.items() if r == 0 or n-t <= 32}
            if op in STORE and rs in regs:
                addr = (regs[rs][0]+signed16(imm)) & 0xffffffff
                addr &= 0x1fffffff
                if addr in MMIO:
                    start, end = max(0,n-16), min(size,n+20)
                    found.append({
                        "kind": MMIO[addr], "vaddr": f"0x{va+n:08x}",
                        "file_offset": off+n, "word": f"{word:08x}",
                        "window_start": f"0x{va+start:08x}",
                        "window_hex": data[off+start:off+end].hex(),
                        "verified": False,
                    })
            if op == 0xf:
                if rt: regs[rt] = (imm<<16,n)
            elif op in (9, 0xd, 0x19) and rs in regs:
                value = regs[rs][0]
                if rt: regs[rt] = (((value|imm) if op==0xd else value+signed16(imm))&0xffffffff,n)
            elif op in BRANCH or op in (0x10,0x11,0x12,0x13):
                regs = {0:(0,0)}
            elif op == 0:
                if (word&63) in (8,9):
                    regs = {0:(0,0)}
                elif rd: regs.pop(rd,None)
            elif op not in STORE:
                if rt: regs.pop(rt,None)
            # Do not infer an address from a delay-slot instruction.
            if op in BRANCH or (op==0 and (word&63) in (8,9)):
                regs = {0:(0,0)}
        # No writes, no generated patches, no guessing from ELF strings.
    return {"sha256": hashlib.sha256(data).hexdigest(),
            "status": "candidates_only", "candidates": found}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("elf", type=Path)
    p.add_argument("-o", "--output", type=Path)
    a = p.parse_args()
    result = json.dumps(discover(a.elf.read_bytes()), indent=2)+"\n"
    if a.output: a.output.write_text(result, encoding="utf-8")
    else: print(result, end="")

if __name__ == "__main__":
    main()
