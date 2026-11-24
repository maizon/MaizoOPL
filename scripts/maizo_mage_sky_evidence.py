#!/usr/bin/env python3
"""Read-only verifier for supplied Sky ELF revisions; never patches a file."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

def check(data, profile):
    if hashlib.sha256(data).hexdigest() not in profile["sha256"]:
        return False
    h=struct.unpack_from("<16sHHIIIIIHHHHHH",data)
    if h[0][:7]!=b"\x7fELF\x01\x01\x01" or h[2]!=8:
        return False
    segments=[struct.unpack_from("<8I",data,h[5]+i*h[9]) for i in range(h[10])]
    def words(address,count):
        matches=[p for p in segments if p[0]==1 and p[2]<=address and address+4*count<=p[2]+p[4]]
        if len(matches)!=1:raise ValueError("nonunique load range")
        p=matches[0]
        return struct.unpack_from("<%dI"%count,data,p[1]+address-p[2])
    a,s,d,t,v,f=[int(profile[k],16) for k in ("set","state","dispatch","table","device","fingerprint")]
    hashword=2166136261
    for w in words(a,16)+words(s,53)+words(v-16,6):
        hashword=((hashword^w)*16777619)&0xffffffff
    return hashword==f and words(d+32,1)==(s,) and words(t,6)==tuple(s+0x28+16*i for i in range(6))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("elf",type=Path)
    p.add_argument("--database",type=Path,default=Path(__file__).resolve().parents[1]/"profiles/mage/sky_evidence.json")
    a=p.parse_args()
    data=a.elf.read_bytes()
    matches=[r["id"] for r in json.loads(a.database.read_text())["profiles"] if check(data,r)]
    print(json.dumps({"sha256":hashlib.sha256(data).hexdigest(),"static_sky_matches":matches,"hardware_verified":False}))
if __name__=="__main__":
    main()
