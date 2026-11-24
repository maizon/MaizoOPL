"""Synthetic ELF tests: exact call redirection, unchanged original,
unchanged delay slot, and fail-closed revision/cave validation."""
import copy
import pathlib
import struct
import sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"scripts"))
from maizo_gs_install_elf import install,digest,jal_target

base=0x100000
data=bytearray(512)
struct.pack_into("<16sHHIIIIIHHHHHH",data,0,b"\x7fELF\x01\x01\x01"+b"\0"*9,2,8,1,base,52,0,0,52,32,1,0,0,0)
struct.pack_into("<8I",data,52,1,128,base,base,384,384,5,16)
pc=base+16
target=base+64
cave=base+128
old=0x0c000000|(target>>2)
delay=0x24840001
struct.pack_into("<II",data,144,old,delay)
struct.pack_into("<I",data,192,0x03e00008)
payload=struct.pack("<II",0x03e00008,0)
profile={"schema":1,"reviewed":True,"elf_sha256":digest(data),
         "adapter_sha256":digest(payload),"adapter_address":cave,
         "adapter_entry":cave,"cave_sha256":digest(bytes(len(payload))),
         "sites":[{"address":pc,"jal":old,"delay":delay,"original_target":target}]}
original=bytes(data)
patched=install(original,payload,profile)
new,ds=struct.unpack_from("<II",patched,144)
assert jal_target(new,pc)==cave and ds==delay
assert patched[256:264]==payload and original==bytes(data)
for field,value in [("reviewed",False),("elf_sha256","bad"),
                    ("adapter_sha256","bad"),("adapter_entry",cave+8),
                    ("adapter_address",pc),("cave_sha256","bad")]:
    p=copy.deepcopy(profile);p[field]=value
    try:install(original,payload,p)
    except ValueError:pass
    else:raise AssertionError(field)
for field,value in [("jal",0),("delay",0),("original_target",cave)]:
    p=copy.deepcopy(profile);p["sites"][0][field]=value
    try:install(original,payload,p)
    except ValueError:pass
    else:raise AssertionError(field)
print("Exact-revision EE JAL binary adapter installer tests passed")
