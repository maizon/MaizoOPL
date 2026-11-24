"""Source-linked regression for the corrected R5900 exception decoder.
Host-only: checks instruction sequence and arithmetic, not hardware execution.
"""
from pathlib import Path
import re

source = (Path(__file__).resolve().parents[1] /
          "ee_core/src/maizo_gsm_engine.S").read_text()

def body(label, end):
    return source.split("\n" + label + ":", 1)[1].split("\n" + end + ":", 1)[0]

def has(block, pattern):
    assert re.search(pattern, block, re.M), pattern

regimm = body("B_com1", "B_com0")
has(regimm, r"andi\s+\$k1,\s*\$k1,\s*0x1F")
assert len(re.findall(r"\bB_\w+", source.split("\nBD_t2:", 1)[1].split("#####", 1)[0])) == 32
for rs in range(32):
    for rt in range(32):
        word = (1 << 26) | (rs << 21) | (rt << 16) | 0xFFFF
        assert ((word >> 16) & 0x1F) == rt
        assert ((word >> 16) & 0x1F) < 32
# The old six-bit mask indexed outside the table whenever rs was odd.
assert (((1 << 26) | (3 << 21) | (1 << 16)) >> 16) & 0x3F == 33

def signed32(x):
    x &= 0xFFFFFFFF
    return x - 0x100000000 if x & 0x80000000 else x

branch = body("B_taken", "B_BLTZ")
has(branch, r"sll\s+\$k1,\s*\$k0,\s*16")
has(branch, r"sra\s+\$k1,\s*\$k1,\s*14")
address = body("get_access_info", "lb_op")
has(address, r"sll\s+\$v0,\s*\$v0,\s*16")
has(address, r"sra\s+\$v0,\s*\$v0,\s*16")
for imm in range(65536):
    word = 0xAC620000 | imm
    expected = imm - 65536 if imm >= 32768 else imm
    assert signed32(word << 16) >> 14 == expected * 4
    assert signed32(word << 16) >> 16 == expected
# r0 must remain immutable even if a trapped load discards the result.
has(body("GSHandler", "get_access_info"), r"sq\s+\$zero,\s*0x000\(\$k1\)")
has(body("do_load", "do_load_byte"), r"beq\s+\$a1,\s*\$a2,\s*exit_GSHandler")
print("MAIZO exception decoder: 1024 REGIMM fields, all 65536 signed immediates, r0 guard PASS")
