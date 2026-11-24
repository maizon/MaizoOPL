"""Synthetic fixture checks the detector, not a commercial game."""
import pathlib
import struct
import sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"scripts"))
from maizo_mage_sky_discover import discover

base=0x100000
data=bytearray(8192)
struct.pack_into("<16sHHIIIIIHHHHHH",data,0,b"\x7fELF\x01\x01\x01"+b"\0"*9,2,8,1,base,52,0,0,52,32,1,0,0,0)
struct.pack_into("<8I",data,52,1,128,base,base,len(data)-128,len(data)-128,7,16)
setter=base+0x100
state=setter+0x500
dispatch=base+0x1000
table=dispatch+0x80
device=base+0x1200
def put(a,w):
    struct.pack_into("<%dI"%len(w),data,128+a-base,*w)
def pair(a):return [0x3c020000|((a+0x8000)>>16),0x24420000|(a&65535)]
hi,lo=pair(dispatch)
w=[0]*16
w[:3]=[0x27bdffd0,0x2483ffff,0x7fb00020]
w[8:15]=[hi,0x00031880,lo,0x00621821,0x8c640000,0x00800008,0]
put(setter,w)
hi,lo=pair(table)
w=[0]*53
w[:10]=[0x2603ffff,0x2c620006,0,hi,0x00031880,lo,0x00621821,0x8c640000,0x00800008,0]
for i in (11,15,19,23,27,31):w[i]=0x2403fe1f
for i,v in zip((17,21,25,29,32),(0x24040060,0x24040080,0x24040120,0x240400c0,0x24040160)):w[i]=v
w[33:35]=[0x00431024,0x00441025]
w[49]=0x24040014
put(state,w)
put(dispatch,[setter]*8+[state])
put(table,[state+0x28+16*i for i in range(6)])
put(device-16,[0x3f800000,setter,0x477fff00,0,setter,setter])
assert len(discover(bytes(data))["matches"])==1
put(table,[state+0x28+16*i for i in range(5)]+[0])
assert discover(bytes(data))["matches"]==[]
try:discover(b"invalid")
except ValueError:pass
else:raise AssertionError("invalid ELF accepted")
print("Sky discovery structure and mismatch gates: OK")
