import pathlib
import sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"scripts"))
from maizo_mage_inspect import inspect
r=inspect(b"Havok RenderWare Gamebryo")
assert r["sky"]==[] and not r["hardware_verified"]
assert "Havok (physics only)" in r["hints_only"]
assert "RenderWare" in r["hints_only"]
assert "rejected" in r
print("Engine string hints never become live capabilities")
