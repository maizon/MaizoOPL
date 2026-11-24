"""Protect the Beta-39 core stack and reject the measured Beta-82 case."""
import importlib.util
from pathlib import Path

p = Path(__file__).resolve().parents[1] / "scripts/check_ee_core_stack.py"
spec = importlib.util.spec_from_file_location("check_stack", p)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

old = "                0x00093900                        _end_bss = ."
new = "                0x00096d40                        _end_bss = ."
assert module.check_map(old, 8192) == (0x93900, 14080)
assert module.check_map(new, 8192) == (0x96D40, 704)
assert module.check_map("0x00095670 _end_bss = .",8192,0x9B000)==(0x95670,0x5990)
try:
    module.check_map("0x0009B004 _end_bss = .",8192,0x9B000)
except ValueError:
    pass
else:
    raise AssertionError("expanded stack overflow accepted")
for text in ["", "0x00097004 _end_bss = .", "0x00080000 _end_bss = ."]:
    try:
        module.check_map(text)
    except ValueError:
        continue
    raise AssertionError(f"Unsafe map accepted: {text!r}")
print("EE core stack regression detection: PASS")
