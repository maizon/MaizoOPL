import pathlib
import struct
import sys
import copy
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))
from maizo_mage import resolve, digest

base = 0x100000
data = bytearray(1024)
struct.pack_into("<16sHHIIIIIHHHHHH",data,0,b"\x7fELF\x01\x01\x01"+b"\0"*9,2,8,1,base,52,0,0,52,32,1,0,0,0)
struct.pack_into("<8I",data,52,1,128,base,base,896,896,5,16)
a=[0x24040001,0x24050002,0x24060003,0x24070004]
b=[0x24080005,0x24090006,0x240a0007,0x240b0008]
struct.pack_into("<4I",data,144,*a)
struct.pack_into("<4I",data,512,*b)
data=bytes(data)
def candidate(before,after,address=None):
    site={"before":before,"after":after}
    if address is not None:site["address"]=address
    return {"method":"static","sites":[site]}
def profile(id,scope,features):
    return {"id":id,"scope":scope,"reviewed":True,"elf_sha256":[digest(data)],"features":features}
wrong=candidate([1,2,3,4],[1,2,3,5],base+16)
good=candidate(a,[a[0]+1,*a[1:]],base+16)
second=candidate(b,[b[0]+1,*b[1:]])
db={"schema":1,"profiles":[profile("specific","game",{"bilinear":[wrong]}),
    profile("family","engine_family",{"bilinear":[good],"dither":[second],"aa1":[wrong]})]}
out,r=resolve(data,db,["bilinear","dither","aa1","scanlines"])
assert r["features"]["bilinear"]["profile"]=="family"
assert r["features"]["dither"]["status"]=="boot_patch"
assert r["features"]["aa1"]["status"]=="original"
assert r["features"]["scanlines"]["status"]=="original"
assert struct.unpack_from("<I",out,144)[0]==a[0]+1
assert struct.unpack_from("<I",out,512)[0]==b[0]+1
assert r["changed_words"]==2
bad=copy.deepcopy(db);bad["profiles"][1]["reviewed"]=False
assert resolve(data,bad,["bilinear"])[0]==data
bad=copy.deepcopy(db);bad["profiles"][1]["elf_sha256"]=["0"*64]
assert resolve(data,bad,["bilinear"])[0]==data
conflict=copy.deepcopy(db)
conflict["profiles"][1]["features"]["dither"]=[candidate(a,[a[0]+2,*a[1:]])]
out,r=resolve(data,conflict,["bilinear","dither"])
assert r["features"]["dither"]["status"]=="original"
assert r["changed_words"]==1
amb=bytearray(data);struct.pack_into("<4I",amb,800,*b)
amb=bytes(amb);db["profiles"][1]["elf_sha256"]=[digest(amb)]
assert resolve(amb,db,["dither"])[0]==amb
print("MAGE exact revision, per-feature fallback, ambiguity and conflicts: OK")
