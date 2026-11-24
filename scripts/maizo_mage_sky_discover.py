#!/usr/bin/env python3
"""Read-only Sky state-9 discovery for little-endian EE ELF files.
Structural evidence only; never install a profile automatically.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def discover(data):
    if len(data)<52 or data[:7]!=b'\x7fELF\x01\x01\x01':
        raise ValueError('invalid ELF32')
    h=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
    if h[1]!=2 or h[2]!=8 or h[9]!=32 or h[10]>128 or h[5]+h[10]*32>len(data):
        raise ValueError('invalid MIPS executable headers')
    segments=[]
    for i in range(h[10]):
        p=struct.unpack_from('<8I',data,h[5]+i*32)
        if p[0]!=1: continue
        if p[1]+p[4]>len(data) or p[4]>p[5] or p[2]+p[5]>0x2000000:
            raise ValueError('invalid load segment')
        if p[4]:segments.append(p)
    def words(address,count,executable=False):
        hits=[p for p in segments if p[2]<=address and address+4*count<=p[2]+p[4] and (not executable or p[6]&1)]
        if address%4 or len(hits)!=1:raise ValueError('invalid address range')
        p=hits[0]
        return struct.unpack_from('<%dI'%count,data,p[1]+address-p[2])
    def find(seq,executable=False):
        pattern=struct.pack('<%dI'%len(seq),*seq)
        for p in segments:
            if executable and not p[6]&1:continue
            pos=data.find(pattern,p[1],p[1]+p[4])
            while pos>=0:
                if (pos-p[1])%4==0:yield p[2]+pos-p[1]
                pos=data.find(pattern,pos+1,p[1]+p[4])
    def address(hi,lo):
        if hi>>16!=0x3c02 or lo>>16!=0x2442:raise ValueError('not LUI/ADDIU pair')
        return ((hi&65535)<<16)+((lo&65535)-65536 if lo&32768 else lo&65535)
    out=[]
    for state in find((0x2603ffff,0x2c620006),True):
        try:
            w=words(state,53,True)
            if w[4:5]!=(0x00031880,) or w[6:10]!=(0x00621821,0x8c640000,0x00800008,0):continue
            if [w[i] for i in (11,15,19,23,27,31)]!=[0x2403fe1f]*6:continue
            if [w[i] for i in (17,21,25,29,32)]!=[0x24040060,0x24040080,0x24040120,0x240400c0,0x24040160]:continue
            if w[33:35]!=(0x00431024,0x00441025) or 0x24040014 not in (w[49],w[51]):continue
            table=address(w[3],w[5]); targets=tuple(state+0x28+16*i for i in range(6))
            if words(table,6)!=targets:continue
        except ValueError:continue
        for setter in range(state-0x700,state-0x400,4):
            try:
                sw=words(setter,16,True)
                if sw[:2]!=(0x27bdffd0,0x2483ffff) or sw[2] not in (0x7fb00020,0x7fb00000):continue
                if sw[9]!=0x00031880 or sw[11:15]!=(0x00621821,0x8c640000,0x00800008,0):continue
                dispatch=address(sw[8],sw[10]); entries=words(dispatch,9)
                if entries[8]!=state or not all(setter<=v<=state+0x700 and v%4==0 for v in entries):continue
            except ValueError:continue
            for device in find((setter,)):
                try:dv=words(device-16,6)
                except ValueError:continue
                if dv[0]!=0x3f800000 or dv[2:4]!=(0x477fff00,0):continue
                try:words(dv[1],1,True);words(dv[5],1,True)
                except ValueError:continue
                fingerprint=2166136261
                for v in sw+w+dv:fingerprint=((fingerprint^v)*16777619)&0xffffffff
                out.append(dict(set=hex(setter),state=hex(state),dispatch=hex(dispatch),table=hex(table),device=hex(device),fingerprint=hex(fingerprint)))
    return {'sha256':hashlib.sha256(data).hexdigest(),'matches':out,'hardware_verified':False}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf',type=Path)
    a=parser.parse_args()
    print(json.dumps(discover(a.elf.read_bytes()),indent=2))
