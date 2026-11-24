#!/usr/bin/env python3
"""Install an independently verified, already linked EE renderer adapter.

This is an offline, exact-revision binary patch installer, not an adapter
generator. Never writes the input. The manifest must describe an audited
executable cave and JAL call site; an all-zero cave alone proves nothing.
The adapter must preserve the original ABI, call the original function,
and manage packet ownership, cache and frame synchronization.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from maizo_gs_scan_elf import loads

def digest(data):
    return hashlib.sha256(data).hexdigest()

def number(value):
    if isinstance(value, str):
        return int(value, 0)
    if type(value) is int:
        return value
    raise ValueError("expected integer address")

def file_location(ranges, address, length):
    if address & 3 or length <= 0 or length & 3:
        raise ValueError("unaligned executable range")
    matches = [off + address-va for off, va, size in ranges
               if va <= address and address+length <= va+size]
    if len(matches) != 1:
        raise ValueError("address not uniquely file-backed/executable")
    return matches[0]

def jal_target(word, pc):
    if word >> 26 != 3:
        raise ValueError("expected JAL at verified call site")
    return ((pc+4)&0xf0000000)|((word&0x03ffffff)<<2)

def install(data, payload, profile):
    if profile.get("schema") != 1 or profile.get("reviewed") is not True:
        raise ValueError("explicit reviewed profile required")
    if digest(data) != profile["elf_sha256"] or digest(payload) != profile["adapter_sha256"]:
        raise ValueError("ELF or adapter hash mismatch")
    if not payload or len(payload) > 65536 or len(payload)&3:
        raise ValueError("invalid adapter size")
    ranges = loads(data)
    cave = number(profile["adapter_address"])
    entry = number(profile["adapter_entry"])
    if entry < cave or entry >= cave+len(payload) or entry&3:
        raise ValueError("adapter entry outside payload")
    pos = file_location(ranges,cave,len(payload))
    # Reject aliases/overlapping executable segments and header overwrite.
    phoff = struct.unpack_from("<I",data,28)[0]
    phnum = struct.unpack_from("<H",data,44)[0]
    header_end = max(52,phoff+phnum*32)
    if pos < header_end:
        raise ValueError("adapter overlaps ELF headers")
    if digest(data[pos:pos+len(payload)]) != profile["cave_sha256"]:
        raise ValueError("original code cave mismatch")
    if any(data[pos:pos+len(payload)]):
        raise ValueError("only explicitly audited zero-filled caves supported")
    sites = profile["sites"]
    if not isinstance(sites,list) or not 1 <= len(sites) <= 32:
        raise ValueError("invalid site count")
    patches = []
    for site in sites:
        pc = number(site["address"])
        original = number(site["jal"])
        delay = number(site["delay"])
        target = number(site["original_target"])
        off = file_location(ranges,pc,8)
        file_location(ranges,target,4)
        if off < header_end or cave <= target < cave+len(payload):
            raise ValueError("invalid site or original target")
        if off < pos+len(payload) and pos < off+8:
            raise ValueError("call site overlaps cave")
        if any(off < prev+8 and prev < off+8 for prev,_ in patches):
            raise ValueError("overlapping sites")
        if struct.unpack_from("<II",data,off) != (original,delay) or jal_target(original,pc) != target:
            raise ValueError("original call or delay slot mismatch")
        if ((pc+4)^entry)&0xf0000000:
            raise ValueError("adapter outside JAL region")
        # Keep the original delay slot unchanged, including argument setup.
        patches.append((off,0x0c000000|((entry>>2)&0x03ffffff)))
    result = bytearray(data)
    result[pos:pos+len(payload)] = payload
    for off, word in patches:
        struct.pack_into("<I",result,off,word)
    return bytes(result)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("elf",type=Path)
    p.add_argument("adapter",type=Path)
    p.add_argument("profile",type=Path)
    p.add_argument("-o","--output",type=Path,required=True)
    a=p.parse_args()
    if a.output.exists() or a.output.resolve() in (a.elf.resolve(),a.adapter.resolve(),a.profile.resolve()):
        p.error("output must be a new file")
    patched=install(a.elf.read_bytes(),a.adapter.read_bytes(),json.loads(a.profile.read_text()))
    with a.output.open("xb") as f:
        f.write(patched)
    print("Patched copy SHA-256:",digest(patched))

if __name__=="__main__":
    main()
