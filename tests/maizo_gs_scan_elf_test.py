import importlib.util
import struct
from pathlib import Path
spec = importlib.util.spec_from_file_location("gs", Path(__file__).resolve().parents[1]/"scripts/maizo_gs_scan_elf.py")
gs=importlib.util.module_from_spec(spec);spec.loader.exec_module(gs)
def image(words, flags=5):
    code=struct.pack("<"+"I"*len(words),*words)
    ident=b"\x7fELF\x01\x01\x01"+bytes(9)
    h=struct.pack("<16sHHIIIIIHHHHHH",ident,2,8,1,0x100000,52,0,0,52,32,1,0,0,0)
    ph=struct.pack("<8I",1,84,0x100000,0x100000,len(code),len(code),flags,4)
    return h+ph+code
# lui t0,0x1001; sw t1,-0x6000(t0) = GIF CHCR
a=image([0x3c081001,0xad09a000,0x3c081001,0xad099010])
r=gs.discover(a)
assert [x["kind"] for x in r["candidates"]]==["PATH3_CHCR","PATH2_MADR"]
assert all(not x["verified"] for x in r["candidates"])
assert r["candidates"][0]["vaddr"]=="0x00100004"
assert not gs.discover(image([0x3c081001,0x08040000,0xad09a000]))["candidates"]
assert not gs.discover(image([0x3c081001,0x8d080000,0xad09a000]))["candidates"]
for bad in (a[:50],image([0],flags=4)):
    try:gs.discover(bad)
    except ValueError:pass
    else:raise AssertionError("invalid ELF accepted")
print("GS discovery: exact literal MMIO candidates, no speculative patches")
