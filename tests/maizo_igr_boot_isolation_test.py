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

# The recovery must not bring the permanent boot observer back.
request=pad.split("config->EnableGSMOp || EnableMaizoGsMonitor()",1)[1].split("mz_capture_done:",1)[0]
assert "attempt<180" in request
assert "SetAlarm(256,IGR_Poll_Alarm" in request
assert request.index("if(maizoIGR_Preflight(0)){ready=1;break;}") < request.index("maizoIgrPauseThreads()")
assert "mzPadMenuMerge(keys,v)" in pad
assert "if(now&MZ_KEY_START)" in menu
assert "mzPadMenuStep(now,&prev,&held)" in menu
assert "MZ_EE_EXP_TOP-64u" in read("modules/pademu/pademu.c")
print("Request-scoped capture window and independent menu navigation PASS")

# No interrupted VIF DIRECT payload, no channel reset or IRQ acknowledge.
assert "mzIgrSafePath(" in menu
assert "mzIgrFifoFits(*MZ_GIF_STAT,qwc)" in menu
assert "mzDmaRegs" not in menu
assert "MZ_D_ENABLEW=" not in menu and "*MZ_D_STAT=" not in menu
assert "x+=32u" in menu
close = pad.split("choice=maizoIGR_MenuRun(",1)[1].split("goto mz_capture_done;",1)[0]
assert close.index("MaizoGsMonitorEnd(0)") < close.index("maizoIgrUnhold()")
print("Menu recovery: complete FIFO packets, no VIF DMA borrowing PASS")

assert "mzIgrVifFifoFits(*MZ_VIF1_STAT,*MZ_GIF_STAT,qwc)" in menu
assert "0x50000000u|qwc" in menu and "fifo=0x10005000u" in menu
drain=menu.split("static int mzOverlayDrain(void)",1)[1].split("static int mzSendLine",1)[0]
assert drain.index("mzWait(MZ_VIF1_STAT") < drain.index("mzWait(MZ_GIF_STAT")
assert "mzDmaRegs" not in menu
print("Dual FIFO route: full DIRECT submission and ordered drain PASS")

# No IRQ dispatch on a user-mode stack during temporary kernel MMIO.
power = pad.split("int powerIntr=DI();",1)[1].split("if(powerIntr)EI();",1)[0]
assert power.index("ee_kmode_enter();") < power.index("ee_kmode_exit();")
assert "pending && !mzPowerLatched" in power
sample = pad.split("static __attribute__((noinline)) int mzIgrReadButtons",1)[1].split("static int maizoIgrButtons",1)[0]
assert sample.index("int intr=DI();") < sample.index("p[k]=mb[k]")
assert sample.index("mzPadSample(") < sample.index("if(intr)EI();")
run = menu.split("int maizoIGR_MenuRun(",1)[1]
assert run.index("if(!first)") < run.index("if(mzProfilePending)")
system = read("src/system.c").split("void sysLaunchLoaderElf(",1)[1]
assert system.index("fallbackCore = !proEnabled;") < system.index("ModuleStorageSize = sendIrxKernelRAM")
assert system.index("ModuleStorage = (void *)MZ_EE_LEGACY_TOP;") < system.index("ModuleStorageSize = sendIrxKernelRAM")
print("IRQ-safe pad snapshot and kernel MMIO, deferred profiles and OFF isolation PASS")

# Transactional changes: no early writes to the live cheat mask.
assert "mzPendingLiveMask=mzLiveToggle(" in run
assert run.index("if(!mzOverlayDrain())result=MAIZO_IGR_UNAVAILABLE;") < run.index("MaizoLiveMask=mzPendingLiveMask;")
assert "result==MAIZO_IGR_RESUME && mzState.cheat_caps" in run
assert "mzCapturePath(heldDmac)" in run and "mzIgrMfifoPath(" in menu
assert "mzIgrGsFifoEmpty((u32)*MZ_GS_CSR)" in drain
assert "for (i = 255; i > 0; i--)" in pad
assert "#define IGR_STACK_SIZE (5 * 1024)" in pad
print("Transaction: staged cheats, reversible thread hold, GS drain and empty MFIFO gate PASS")

# Experimental restoration and separated overlay must never compromise
# the default resident layout or DMA into a leased, freed IOP allocation.
transport=read("ee_core/src/maizo_restore.c")
service=read("modules/iopcore/maizo_restore/maizo_restore.c")
assert "dma(line,upload,n)" in transport and "dma(line,buf+off,n)" not in transport
assert "case MZR_WRITE:" in service and "buf[r->off+j]=upload[j]" in service
assert "mzOvlHeaderOK(&overlay)" in read("src/system.c") and "overlay.run>=fileEnd" in read("src/system.c")
assert "ModuleStorageEnd=(void *)MZ_OVL_END" in system
assert "maizo_overlay_stub.o" in read("ee_core/Makefile")
assert "ASSERT(_stack_size >= 8192" in read("ee_core/linkfile.maizo")
print("Restoration isolation and protected separate overlay source checks PASS")

# Never open a modal dialog on the already dismantled GUI. Snapshot
# configuration before configEnd, validate the overlay before packing,
# and downgrade to conventional PRO before selecting the legacy core.
assert "guiMsgBox(" not in "\n".join(line for line in system.split("/* Based on OPL Daily Builds",1)[0].splitlines() if not line.lstrip().startswith("*"))
assert "int proEnabled = mzLaunchProEnabled;" in system
assert system.index("overlaySegment=mzOverlaySegment();") < system.index("ModuleStorageSize = sendIrxKernelRAM")
assert "boot_elf=(u8 *)maizo_pro_elf;" in system
assert "MAIZO_IGR_OVERLAY_EXPERIMENTAL=0 MAIZO_IGR_RESTORE_EXPERIMENTAL=0" in read("Makefile")
assert "LoadMemModule(0,module,size,sizeof(arg),arg)" in transport
print("Boot handoff: pre-deinit configuration, no dead GUI, optional restoration downgrade PASS")

# Restore-only transaction: never paint before complete backup and seal.
assert run.index("if(backup!=1 || !mzRestoreHush())") < run.index("first=mzDraw")
assert run.index("dirty=1;") < run.index("first=mzDraw")
assert "if(dirty &&" in run and "if(!full && !mzRestoreKeep())" in run
assert "mzrHash(dst,n)==lineHash[off/n]" in transport
assert "mzrSealable(written,bufSize)" in service
assert "wake();if(!sealed)release();" in service
assert "if(!mzRestoreReady())return 0;" in read("ee_core/src/maizo_overlay_stub.c")
assert "ready=quiet>=2;" in pad
print("Restore: verified sequential backup, seal, pin, heartbeat and quiet boundary PASS")

# No pitch/voice or AutoDMA mutation; preserve failed sealed rollback.
assert "SD_VP_PITCH" not in service
assert "MZR_UNHUSH" in service and "MZR_PING" in service
assert "mzRestoreEnd(retain)" in run
assert "if(retain)request=0;" in transport
assert "mzrRingIdle(dctrl" in menu
assert "if(started && !ok)mzReverseFault=1;" in menu
print("Restoration: inactive MFIFO, audio non-interference, handshake and failed rollback retention PASS")

assert "mzFullVram(0)" in run and "full?mzFullVram(1):mzRestorePixels(p)" in run
assert "mzRestoreTransferRegs()" in run and "checkpoint.fault=1" in run
assert "if(mzRestoreTransition)break;" in pad
assert "mzRestoreTransition|=1u;" in read("ee_core/src/iopmgr.c")
assert "mzRestoreTransition|=2u;" in read("ee_core/src/syshook.c")
print("Cooperative full VRAM checkpoint and observed ELF/IOP transition gates PASS")

assert "mzRestoreDiscIdle()" in pad and "MZR_PROBE" in service
assert "ReferSemaStatus(cdrom_io_sema,&info)" in read("modules/iopcore/cdvdman/cdvdman.c")
assert "pendingAck && seq==seen" in service and "++ackRetries>2u" in service
assert "mzrSameThread(&x,&mzHeldIdentity[i])" in pad
assert run.index("if(!mzOverlayDrain()){retain=1;") < run.index("mzRestoreEnd(retain)")
print("Restore: actual CDVD/NCMD idle probe, bounded ACK retry, thread identity and final drain retention PASS")
