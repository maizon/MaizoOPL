"""Source-linked regression checks for the background/boot policy.
Does not run a commercial game or claim hardware compatibility.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
def read(path):
    return (root / path).read_text()

pad = read("ee_core/src/padhook.c")
boot = read("ee_core/src/main.c")
sky = read("ee_core/src/maizo_mage_sky.c")
menu = read("ee_core/src/maizo_igrmenu.c")
api = read("ee_core/src/maizo_gsm_api.c")
assert "#define MAIZO_IGR_POLL_PRIORITY 127" in pad
assert "thread_param.initial_priority = 127;" in pad
alarm = pad.split("static void IGR_Poll_Alarm(", 1)[1].split("#ifdef MAIZO_IGR_MENU", 1)[0]
assert "iChangeThreadPriority(" not in alarm
assert "iWakeupThread(" in alarm
assert "mzPollBoost" not in pad
assert "ChangeThreadPriority(TH_SELF, 0);" in pad
assert "maizoIGR_BootProfile" not in boot
select = sky.split("void maizoSkySelect(", 1)[1].split("void maizoSkyPrepare(", 1)[0]
assert "skyPending=1;" in select
for unsafe in ("skyValid(", "mzSkyRange(", "mzSkyFogCodeOK(", "mzSkyReloadCodeOK("):
    assert unsafe not in select
prepare = sky.split("void maizoSkyPrepare(", 1)[1].split("int maizoSkyAvailable(", 1)[0]
assert "skyValid(p,0)" in prepare
assert "mzSkyFogCodeOK(" in prepare
assert "mzSkyReloadCodeOK(" in prepare
assert "maizoSkyPrepare();" in menu.split("int maizoIGR_MenuRun(", 1)[1]
assert "if(mzProfilePending)" in menu
install = api.split("static void Install_GSHandler(", 1)[1].split("static void Remove_GSHandler(", 1)[0]
assert install.index("FlushCache(2);") < install.index("Enable_GSBreakpoint();")
print("MAIZO launch isolation: legacy poll priority, deferred live options, vector publication PASS")

# Native games must execute without a permanently installed GS trap.
assert "EnableMaizoGsMonitor();" not in boot
assert "config->EnableGSMOp || EnableMaizoGsMonitor()" in pad
capture = pad.split("mz_capture_done:",1)[1].split("goto maizo_igr_poll_again",1)[0]
assert capture.index("MaizoGsMonitorEnd(0)") < capture.index("maizoIgrResumeThreads()")
end = api.split("void MaizoGsMonitorEnd(",1)[1].split("#endif",1)[0]
assert "if(!gsInstalled||!MaizoGsMonitorOnly)return;" in end
assert "DisableGSM();" in end and "mtbpc %2" in end
assert "(MaizoGsSeen&need)!=need" in menu
assert "MaizoGsMonitorEnd(1)" in read("ee_core/src/syshook.c")
asm = read("ee_core/src/maizo_gsm_engine.S")
other = asm.split("\nmaizo_monitor_other_store:",1)[1].split("\nmaizo_native_bridge_write:",1)[0]
import re
for op in ("sq","sb","sh","sw","swl","swr","sdl","sdr"):
    assert re.search(r"^\s+"+op+r"\s+\$\w+, 0\(\$a3\)",other,re.M), op
assert "MaizoGsSeen" in other
print("Native GS: on-demand capture, exact store widths, no default boot trap PASS")
