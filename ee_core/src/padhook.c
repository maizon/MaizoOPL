/*
  padhook.c Open PS2 Loader In Game Reset

  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Reset SPU function taken from PS2SDK freesd.
  Copyright (c) 2004 TyRaNiD <tiraniddo@hotmail.com>
  Copyright (c) 2004,2007 Lukasz Bruun <mail@lukasz.dk>

  PadOpen Hooking function inspired from ps2rd.
  Hook scePadPortOpen/scePad2CreateSocket instead of scePadRead/scePad2Read
  Copyright (C) 2009 jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Copyright (C) 2009 misfire <misfire@xploderfreax.de>
*/

#include <ee_regs.h>
#include <iopcontrol.h>
#include "asm.h"
#include "ee_core.h"
#include "iopmgr.h"
#include "modmgr.h"
#include "modules.h"
#include "util.h"
#include "padhook.h"
#include "padpatterns.h"
#include "syshook.h"
#include "tlb.h"
#include "gsm_api.h"
#ifdef MAIZO_IGR_MENU
#include "maizo_igrmenu.h"
#include "maizo_gs_probe.h"
#include "maizo_igr_input.h"
#endif
#ifdef IGS
#include "igs_api.h"
#endif
#include "cheat_api.h"
#include "cd_igr_rpc.h"
#include "coreconfig.h"

/* scePadPortOpen & scePad2CreateSocket prototypes */
static int (*scePadPortOpen)(int port, int slot, void *addr);
static int (*scePad2CreateSocket)(pad2socketparam_t *SocketParam, void *addr);

/* Monitored pad data */
static paddata_t Pad_Data;

/* Monitored power button data */
static powerbuttondata_t Power_Button;

/* IGR Thread ID and interrupt handler */
static int IGR_Thread_ID = -1;
#ifdef MAIZO_IGR_MENU
/*
 * Optional compact overlay. Only pauses threads actually suspended here,
 * and keeps the SIF DMA alive so IOP padman can update the controller buffer.
 * Technique credited to RiptOPL / RIPPS2 / ORBIT (AFL-3.0).
 */
static u32 MaizoIgrSuspended[8];
static u32 MaizoIgrIntcMask, MaizoIgrDmacMask;
static int MaizoIgrWaitRelease;
/* Restore the background scheduler used by Beta-39 and the first
 * working overlay builds. Priority 0 is for an explicit capture only,
 * not ordinary controller/streaming work or an alarm callback. */
#define MAIZO_IGR_POLL_PRIORITY 127
/* Dedicated cache line: IOP never targets a game-owned buffer. */
volatile u32 MaizoPadMailbox[16] __attribute__((aligned(64)));
static u8 *mzPadSlots[8];
static u32 mzPadFrames[9],mzPadEpoch;
static u8 mzPadAge[9];

/* Padman updates this area asynchronously via SIF DMA, not EE code.
 * UNCACHED alone does not stop LTO from caching ordinary C loads. */
static int mzPadSample(const u8 *addr,u16 *buttons,u32 *frame,int menu)
{
    const volatile u8 *b=(const volatile u8 *)UNCACHED_SEG(addr);
    u32 best=0;
    unsigned count=Pad_Data.libpad==IGR_LIBPAD?2:1,i,valid=0;
    if(!addr||((u32)addr&3u)||((u32)addr&0x1fffffffu)<0x100000u||
       ((u32)addr&0x1fffffffu)>0x02000000u-256u||
       (Pad_Data.libpad!=IGR_LIBPAD&&Pad_Data.libpad!=IGR_LIBPAD2))return 0;
    for(i=0;i<count;i++){
        u32 f=*(const volatile u32 *)(b+Pad_Data.pos_frame);
        u8 state=b[Pad_Data.pos_state];
        u8 lo=b[Pad_Data.pos_combo2],hi=b[Pad_Data.pos_combo1];
        unsigned keys=(u16)~(lo|hi<<8);
        if(menu){
            unsigned mode=b[Pad_Data.pos_combo2-1];
            if(mode==0x73u||mode==0x79u)
                keys=mzPadMenuAnalog(keys,b[Pad_Data.pos_combo2+4],b[Pad_Data.pos_combo2+5]);
        }
        if(f==*(const volatile u32 *)(b+Pad_Data.pos_frame) &&
           state==(Pad_Data.libpad==IGR_LIBPAD?IGR_PAD_STABLE_V1:IGR_PAD_STABLE_V2) &&
           (!valid||(s32)(f-best)>0)){
            best=f;*buttons=(u16)keys;valid=1;
        }
        b+=Pad_Data.libversion>=0x0160?128:64;
    }
    *frame=best;
    return valid;
}
static int mzPadFresh(unsigned i,u32 f)
{
    if(f!=mzPadFrames[i]){mzPadFrames[i]=f;mzPadAge[i]=0;mzPadEpoch++;}
    else if(mzPadAge[i]<255)mzPadAge[i]++;
    return mzPadAge[i]<60;
}
static __attribute__((noinline)) int mzIgrReadButtons(u16 *buttons,u32 *frame,int menu)
{
    const volatile u32 *mb=(const volatile u32 *)UNCACHED_SEG(MaizoPadMailbox);
    uint32_t p[4];
    unsigned k,valid=0,keys=0;
    /* Prefer independent live PadEmu input, then all opened game slots.
     * A stale/disconnected slot cannot monopolize navigation. */
    for(k=0;k<4;k++)p[k]=mb[k];
    if(p[0]==mb[0]&&mzPadPacketOK(p)&&mzPadFresh(8,p[0])){
        for(k=0;k<4;k++)if(p[3]&(1u<<k)){
            unsigned v=(p[1+k/2]>>((k&1u)*16u))&65535u;
            keys=menu?mzPadMenuMerge(keys,v):mzPadPick(keys,v);
            valid=1;
        }
    }
    for(k=0;k<8;k++){
        u16 v;u32 f;
        if(mzPadSample(mzPadSlots[k],&v,&f,menu)&&mzPadFresh(k,f)){
            keys=menu?mzPadMenuMerge(keys,v):mzPadPick(keys,v);
            valid=1;
        }
    }
    *buttons=(u16)keys;*frame=mzPadEpoch;
    return valid;
}
static int maizoIgrButtons(u16 *buttons,u32 *frame)
{
    return mzIgrReadButtons(buttons,frame,0);
}
static int maizoIgrMenuButtons(u16 *buttons,u32 *frame)
{
    return mzIgrReadButtons(buttons,frame,1);
}
static void maizoIgrHold(void)
{
    int i;
    /* Keep kernel timers, SIF, CDVD, IPU and other service IRQs alive.
     * GS/VBLANK/VIF1/GIF handlers may submit graphics while paused. */
    MaizoIgrIntcMask = *(vu32 *)0x1000f010 & 0x003du;
    MaizoIgrDmacMask = (*R_EE_D_STAT >> 16) & 0x0006u;
    for (i = 0; i < 15; i++) {
        if (MaizoIgrIntcMask & (1u << i)) DisableIntc(i);
        if (MaizoIgrDmacMask & (1u << i)) DisableDmac(i);
    }
}
static void maizoIgrUnhold(void)
{
    int i;
    /* Restore exact previous masks once, with IRQ dispatch disabled.
     * The menu never acknowledges a game's pending DMA/INTC event. */
    DI();
    for (i = 0; i < 15; i++) {
        if (MaizoIgrDmacMask & (1u << i)) EnableDmac(i);
        if (MaizoIgrIntcMask & (1u << i)) EnableIntc(i);
    }
    MaizoIgrDmacMask=MaizoIgrIntcMask=0;
    EI();
}
static int maizoIgrPauseThreads(void)
{
    int i,ok=1;
    for (i = 0; i < 8; i++) MaizoIgrSuspended[i] = 0;
    for (i = 1; i < 256; i++) {
        ee_thread_status_t info;
        if (i == IGR_Thread_ID || ReferThreadStatus(i,&info)<0)
            continue;
        /* Never resume a thread the game had already suspended. */
        if (info.status != THS_READY && info.status != THS_WAIT &&
            info.status != THS_RUN)
            continue;
        if (SuspendThread(i) >= 0)
            MaizoIgrSuspended[i >> 5] |= (1u << (i & 31));
        else ok=0;
    }
    return ok;
}
static void maizoIgrResumeThreads(void)
{
    int i;
    for (i = 1; i < 256; i++)
        if (MaizoIgrSuspended[i >> 5] & (1u << (i & 31))) {
            MaizoIgrSuspended[i >> 5] &= ~(1u << (i & 31));
            ResumeThread(i);
        }
}

#endif /* MAIZO_IGR_MENU */
static int IGR_Intc_ID = -1;

/* IGR thread stack & stack size */
#define IGR_STACK_SIZE (4 * 1024)
static u8 IGR_Stack[IGR_STACK_SIZE] __attribute__((aligned(16)));

/* Extern symbol */
extern void *_gp;
extern void *_end;

// Load home ELF
static void t_loadElf(void)
{
    USE_LOCAL_EECORE_CONFIG;
    int ret;
    char *argv[3];
    int argc=1;
    t_ExecData elf;

    if (EnableDebug)
        DBGCOL(0x80FF00, LOADELF, "t_loadElf() begins");

    // Init RPC & CMD
    SifInitRpc(0);

    if (EnableDebug)
        DBGCOL(0x000080, LOADELF, "Patch prefix check");

    // Apply Sbv patches
    sbv_patch_disable_prefix_check();

    if (EnableDebug)
        DBGCOL(0xFF8000, LOADELF, "loading SIO2 modules and USBD if found");

    // Load basic modules
    LoadModule("rom0:SIO2MAN", 0, NULL);
    LoadModule("rom0:MCMAN", 0, NULL);

    if (config->ExitPath[1] == 'a') { // ie mass:
        ret = LoadModule("mc0:SYS-CONF/USBD.IRX", 0, NULL);
        if (ret >= 0)
            LoadModule("mc0:SYS-CONF/USBHDFSD.IRX", 0, NULL);
        else {
            LoadModule("mc1:SYS-CONF/USBD.IRX", 0, NULL);
            LoadModule("mc1:SYS-CONF/USBHDFSD.IRX", 0, NULL);
        }
        delay(5); // Wait for device to be detected.
    }

    // Load exit ELF
    argv[0] = config->ExitPath;
    argv[1] = NULL;
#ifdef MAIZO_IGR_MENU
    if(maizoIGR_ProfileArg()){
        argv[1]=maizoIGR_ProfileArg();
        argv[2]=NULL;
        argc=2;
    }
#endif

    // Wipe everything, even the module storage.
    WipeUserMemory((void *)&_end, (void *)GetMemorySize());

    FlushCache(0);

    ret = LoadElf(argv[0], &elf);

    if (!ret) {

        // Exit services
        LoadFileExit();
        SifExitIopHeap();
        SifExitRpc();

        FlushCache(0);
        FlushCache(2);

        if (EnableDebug)
            DBGCOL(0x0080FF, LOADELF, "ExecPS2() begins");

        // Execute BOOT.ELF
        ExecPS2((void *)elf.epc, (void *)elf.gp, argc, argv);
    }

    if (EnableDebug) {
        DBGCOL(0x0000FF, LOADELF, "LoadElf() error");
        delay(5);
    }

    // Return to PS2 Browser
    Exit(0);
}

// Wake the polling IGR thread at approximately one-frame intervals.
// SetAlarm() uses horizontal-sync ticks; 256 is close to one frame
// for both NTSC and PAL.
static void IGR_Poll_Alarm(s32 alarm_id, u16 time, void *common)
{
    (void)alarm_id;
    (void)time;

    int thread=*(int *)common;
    if(thread>0)iWakeupThread(thread);
}

#ifdef MAIZO_IGR_MENU
static void maizoIgrYield(void)
{
    /* Bounded polling during suspension: kernel alarm callbacks can
     * depend on services that a title has disabled. Do not SleepThread
     * with the game suspended. IRQs remain enabled for SIF pad DMA. */
    unsigned int n;
    for(n=0;n<0x80000u;n++)asm volatile("nop\nnop\nnop\nnop" ::: "memory");
}
/* A stable pad is required before rendering; freshness is monitored
 * throughout menu navigation. */
static int maizoIgrPadReady(void)
{
    u16 keys;
    u32 frame;
    /* A valid stable pad sample suffices to draw. The menu itself
     * enforces live progress, release-before-input and a finite timeout.
     * Do not require an extra frame transition at this exact instant. */
    return maizoIgrButtons(&keys,&frame);
}
#endif

// In Game Reset Thread
static void IGR_Thread(void *arg)
{
    USE_LOCAL_EECORE_CONFIG;

    u32 Cop0_Perf;
    u32 dmaEnableR;
    int i;
    int alarm_id;

    u8 pad_pos_state;
    u8 pad_pos_frame;
    u8 pad_pos_combo1;
    u8 pad_pos_combo2;
#ifdef MAIZO_IGR_MENU
    const int pollPriority=MAIZO_IGR_POLL_PRIORITY;
#endif

    (void)arg;

    /*
     * Battle Archives 2 conflicts with OPL registering a handler on
     * VBLANK_END, even when that handler has an empty body.
     *
     * Poll the same controller buffer from an ordinary EE thread instead.
     */
#ifdef MAIZO_IGR_MENU
maizo_igr_poll_again:
#endif
    for (;;) {
#ifdef MAIZO_IGR_MENU
        /*
         * Read-only once-per-alarm GS activity sample. No extra IRQ,
         * DMAC interception, VU reads or contention with GSM's hardware
         * breakpoint. Designed not to change rendering or pad timing.
         * The sample may miss a short-lived transfer; it is telemetry,
         * never a universal-game compatibility claim.
         */
        if (config->EnableMaizoIgrMenu && config->EnableGSMOp) {
            mzGsProbeSample(&mzGsProbe,
                            *(vu32 *)0x10003020, /* GIF_STAT */
                            *(vu32 *)0x10009000, /* D1_CHCR */
                            *(vu32 *)0x1000a000); /* D2_CHCR */
        }
#endif
#ifdef MAIZO_IGR_MENU
        if (1) {
#else
        if (Pad_Data.pad_buf != NULL) {
#endif
            // Read through the uncached segment to obtain live pad data.
#ifdef MAIZO_IGR_MENU
            /* Use the same coherent bank for opening, release and
             * navigation. The legacy polling path is kept for builds
             * without the experimental menu. */
            u16 keys;
            u32 frame;
            if(maizoIgrButtons(&keys,&frame)){
                pad_pos_state=Pad_Data.libpad==IGR_LIBPAD2?IGR_PAD_STABLE_V2:IGR_PAD_STABLE_V1;
                pad_pos_frame=(u8)frame;
                pad_pos_combo1=(u8)~(keys>>8);
                pad_pos_combo2=(u8)~keys;
            }else{
                pad_pos_state=0;
                pad_pos_frame=pad_pos_combo1=pad_pos_combo2=0;
            }
#else
            pad_pos_state =
                ((u8 *)UNCACHED_SEG(Pad_Data.pad_buf))
                    [Pad_Data.pos_state];

            pad_pos_frame =
                ((u8 *)UNCACHED_SEG(Pad_Data.pad_buf))
                    [Pad_Data.pos_frame];

            pad_pos_combo1 =
                ((u8 *)UNCACHED_SEG(Pad_Data.pad_buf))
                    [Pad_Data.pos_combo1];

            pad_pos_combo2 =
                ((u8 *)UNCACHED_SEG(Pad_Data.pad_buf))
                    [Pad_Data.pos_combo2];
#endif

            // Check for the stable state used by the detected libpad.
#ifdef MAIZO_IGR_MENU
            if (pad_pos_state) {
#else
            if (((Pad_Data.libpad == IGR_LIBPAD) &&
                 (pad_pos_state == IGR_PAD_STABLE_V1)) ||
                ((Pad_Data.libpad == IGR_LIBPAD2) &&
                 (pad_pos_state == IGR_PAD_STABLE_V2))) {
#endif

                /*
                 * Check periodically whether the captured pad buffer is
                 * still active. This preserves the original VBLANK
                 * handler's pad-hook recovery behaviour.
                 */
                if (Pad_Data.vb_count++ >= 10) {
#ifdef MAIZO_IGR_MENU
                    /* An input sample is not evidence of a replaced
                     * function. A paused/stale DMA frame or a mailbox
                     * sequence wrap must not trigger live RAM patching
                     * via Hook_CreateThread during a loading screen.
                     * Real ELF transitions still reinstall the hook. */
                    if (!config->EnableMaizoIgrMenu)
#endif
                    {
                        padOpen_hooked = pad_pos_frame != Pad_Data.prev_frame;
                    }
                    Pad_Data.prev_frame = pad_pos_frame;
                    Pad_Data.vb_count = 0;
                }

#ifdef MAIZO_IGR_MENU
                /* A closed menu must not reopen until the combo is released. */
                if (pad_pos_combo1 != IGR_COMBO_R1_L1_R2_L2 ||
                    pad_pos_combo2 != IGR_COMBO_START_SELECT)
                    MaizoIgrWaitRelease = 0;
#endif
                // R1 + L1 + R2 + L2
                if (pad_pos_combo1 == IGR_COMBO_R1_L1_R2_L2) {
                    // Start + Select: reset to configured exit path.
#ifdef MAIZO_IGR_MENU
                    if (pad_pos_combo2 == IGR_COMBO_START_SELECT && !MaizoIgrWaitRelease) {
#else
                    if (pad_pos_combo2 == IGR_COMBO_START_SELECT) {
#endif
                        Pad_Data.combo_type = pad_pos_combo2;
                    }
                    // R3 + L3: power off.
                    else if (pad_pos_combo2 == IGR_COMBO_R3_L3) {
                        Pad_Data.combo_type = pad_pos_combo2;
                    }
#ifdef IGS
                    // Up: in-game screenshot when GSM/IGS is active.
                    else if ((pad_pos_combo2 == IGR_COMBO_UP) &&
                             config->EnableGSMOp) {
                        Pad_Data.combo_type = pad_pos_combo2;
                    }
#endif
                }
            }
        }

        /*
         * Preserve the original power-button monitoring.
         * These registers require kernel mode.
         */
        ee_kmode_enter();

        if ((*CDVD_R_NDIN & 0x20) &&
            (*CDVD_R_POFF & 0x04)) {
            Power_Button.press++;

            // Cancel immediate poweroff to allow double-press detection.
            *CDVD_R_SDIN = 0x00;
            *CDVD_R_SCMD = 0x1B;
        }

        if (Power_Button.press) {
            if (Power_Button.vb_count++ >= 50) {
                if (Power_Button.press == 1) {
                    Pad_Data.combo_type = IGR_COMBO_R3_L3;
                } else {
                    Pad_Data.combo_type = IGR_COMBO_START_SELECT;
                }
            }
        }

        ee_kmode_exit();

        if (Pad_Data.combo_type != 0x00) {
            break;
        }

        /*
         * Sleep until an EE alarm wakes this thread.
         * This avoids a permanent busy loop and does not use VBLANK.
         */
        alarm_id = SetAlarm(
            256,
            IGR_Poll_Alarm,
            &IGR_Thread_ID);

        if (alarm_id >= 0) {
            SleepThread();
        } else {
            /*
             * An alarm slot should normally be available. Fall back to
             * a low-priority delay rather than sleeping forever.
             */
            /* An exhausted alarm pool must never leave a high-priority
             * monitor spinning and starving the game. */
#ifdef MAIZO_IGR_MENU
            ChangeThreadPriority(TH_SELF,127);
#endif
            nopdelay();
        }
    }

    DPRINTF("IGR combination detected by polling thread.\n");

#ifdef MAIZO_IGR_MENU
    if (Pad_Data.combo_type == IGR_COMBO_START_SELECT &&
        !Power_Button.press) {
        int choice = MAIZO_IGR_UNAVAILABLE;
        /*
         * The six-button command is ALSO stock OPL's Browser-reset.
         * In this experimental ELF it belongs to the MAIZO menu.
         * If a saved/missing setting disables the overlay, or if GS
         * cannot render it, NEVER fall through to the stock IGR.
         * Only an explicit EXIT selected inside a visible menu can
         * trigger the destructive legacy reset path.
         */
        ChangeThreadPriority(TH_SELF, 0);
        if (config->EnableMaizoIgrMenu &&
            (config->EnableGSMOp || EnableMaizoGsMonitor())) {
            /* Stop EE producers first, but leave completion IRQs live
             * while outstanding VIF/GIF work drains. Only acquire the
             * graphics IRQ hold after a valid idle sample, then recheck.
             * Never wait for a stalled chain with its completion IRQ
             * masked, and always release failed captures. */
            int retries,attempt;
            /* A short request-scoped window covers many more frame flips
             * without enabling the GS recorder at boot. */
            for(attempt=0;attempt<180;attempt++){
                /* Check the real DMA boundary while producers are still
                 * runnable. Avoid suspending a game in the middle of
                 * constructing a VIF/GIF command. A short bounded sample
                 * increases chances of catching the end-of-frame gap. */
                int ready=0;
                if(maizoIGR_Preflight(MZ_IGR_CAPTURE_PROBE))
                    for(retries=0;retries<16;retries++)
                        if(maizoIGR_Preflight(0)){ready=1;break;}
                if(ready){
                    int suspended=maizoIgrPauseThreads();
                    for(retries=0;suspended&&retries<8;retries++){
                        if(maizoIGR_Preflight(0)){
                            maizoIgrHold();
                            if(maizoIgrPadReady() &&
                               maizoIGR_Preflight(MaizoIgrDmacMask)){
                                choice=maizoIGR_MenuRun(maizoIgrMenuButtons,maizoIgrYield,MaizoIgrDmacMask);
                                /* Finish recorder teardown before a restored
                                 * graphics IRQ can enter the old vector. */
                                if(!config->EnableGSMOp)MaizoGsMonitorEnd(0);
                                maizoIgrUnhold();
                                goto mz_capture_done;
                            }
                            maizoIgrUnhold();
                        }
                        maizoIgrYield();
                    }
                    maizoIgrResumeThreads();
                }
                /* Let title workers and their IRQs make progress before
                 * another short capture. Never leave the title paused
                 * while sleeping on an alarm. */
                ChangeThreadPriority(TH_SELF,pollPriority);
                alarm_id=SetAlarm(256,IGR_Poll_Alarm,&IGR_Thread_ID);
                if(alarm_id>=0)SleepThread();else{
                    ChangeThreadPriority(TH_SELF,127);
                    nopdelay();
                }
                ChangeThreadPriority(TH_SELF,0);
            }
        }
mz_capture_done:
        /* No always-on GS trap just because PRO is enabled. Release
         * temporary capture before ordinary game threads resume. */
        if(!config->EnableGSMOp)MaizoGsMonitorEnd(0);
        choice = maizoIgrSafeChoice(config->EnableMaizoIgrMenu, choice);
        if (choice == MAIZO_IGR_RESUME) {
            if (config->EnableMaizoIgrMenu)
                maizoIgrResumeThreads();
            Pad_Data.combo_type = 0;
            MaizoIgrWaitRelease = 1;
            ChangeThreadPriority(TH_SELF, pollPriority);
            goto maizo_igr_poll_again;
        }
        if (choice == MAIZO_IGR_POWEROFF)
            Pad_Data.combo_type = IGR_COMBO_R3_L3;
    }
#endif /* MAIZO_IGR_MENU */

    /*
     * Raise this thread to the priority previously assigned by the
     * interrupt handler before waking the old IGR worker.
     */
    ChangeThreadPriority(TH_SELF, 0);

    /*
     * Perform the shutdown preparation that the original VBLANK
     * handler performed before waking IGR_Thread().
     */

    // Wait for preceding loads and stores.
    asm volatile("sync.l\n");

    // Stop all DMA except SIF0, SIF1 and SIF2.
    dmaEnableR = *R_EE_D_ENABLER;

    *R_EE_D_ENABLEW = dmaEnableR | 0x10000;

    *R_EE_D_CTRL;
    *R_EE_D_STAT;

    *R_EE_D0_CHCR = 0;
    *R_EE_D1_CHCR = 0;
    *R_EE_D2_CHCR = 0;
    *R_EE_D3_CHCR = 0;
    *R_EE_D4_CHCR = 0;
    *R_EE_D8_CHCR = 0;
    *R_EE_D9_CHCR = 0;

    *R_EE_D_ENABLEW = dmaEnableR;

    asm volatile("sync.l\n");

    // Reset the GS.
    *R_EE_GS_CSR = 0x100;

    asm volatile("sync.l\n");

    while (*R_EE_GS_CSR & 0x100) {
    }

    /*
     * We are now in normal thread context, not interrupt context,
     * so use ResetEE rather than OPL's interrupt-only iResetEE wrapper.
     */
    ResetEE(0x7F);

    // Suspend every game thread except this IGR polling thread.
    for (i = 1; i < 256; i++) {
        if (i != IGR_Thread_ID) {
            SuspendThread(i);
        }
    }

    DPRINTF("IGR polling thread beginning shutdown.\n");

    if (EnableDebug) {
        DBGCOL(0xFFFFFF, IGR, "Thread Poll Trigger");
    }

    // Reinitialize RPC and CMD.
    SifInitRpc(0);

    /*
     * Start + Select returns to the configured exit ELF.
     * IGS Up follows the same reset path.
     */
    if ((Pad_Data.combo_type == IGR_COMBO_START_SELECT)
#ifdef IGS
        || ((Pad_Data.combo_type == IGR_COMBO_UP) &&
            config->EnableGSMOp)
#endif
    ) {
        if (EnableDebug) {
            DBGCOL(0xFF8000, IGR, "oplIGRShutdown()");
        }

        oplIGRShutdown(0);

        if (EnableDebug) {
            DBGCOL(0x0000FF, IGR, "Reset IOP");
        }

        // Reset the IOP.
        while (!Reset_Iop("", 0)) {
        }

        // Remove OPL's kernel hooks.
        Remove_Kernel_Hooks();

        /*
         * Some games alter the memory map, so restore the TLB before
         * loading the configured exit ELF.
         */
        InitializeTLB();

        Cop0_Perf = GetCop0(25);

        // Stop an active performance counter.
        if (Cop0_Perf & 0x80000000) {
            __asm__ __volatile__(
                " mfc0  $3, $25;"
                " lui   $2, 0x8000;"
                " or    $3, $3, $2;"
                " xor   $3, $3, $2;"
                " mtc0  $3, $25;"
                " sync.p;");
        }

        if (config->EnableGSMOp
#ifdef MAIZO_IGR_MENU
            || config->EnableMaizoIgrMenu
#endif
        ) {
            if (EnableDebug) {
                DBGCOL(0x00FF00, IGR, "Stopping GSM");
            }

            DPRINTF("Stopping GSM...\n");
            DisableGSM();
        }

        if (config->gCheatList) {
            if (EnableDebug) {
                DBGCOL(0xFF0000, IGR, "Stopping CheatEngine");
            }

            DPRINTF("Stopping PS2RD Cheat Engine...\n");
            DisableCheats();
        }

        if (EnableDebug) {
            DBGCOL(0x00FFFF, IGR, "Waiting for IOP Reboot");
        }

        while (!SifIopSync()) {
        }

        if (EnableDebug) {
            DBGCOL(
                0xFF80FF,
                IGR,
                "Initializing RPC and services");
        }

        SifInitRpc(0);
        SifInitIopHeap();
        LoadFileInit();
        sbv_patch_enable_lmb();

        if (EnableDebug) {
            DBGCOL(0x800000, IGR, "Execute RESETSPU.IRX");
        }

        /*
         * Reset SPU after the IOP reboot so no IOP module competes
         * with the EE for the sound hardware.
         */
        LoadOPLModule(
            OPL_MODULE_ID_RESETSPU,
            0,
            0,
            NULL);

#ifdef IGS
        if ((Pad_Data.combo_type == IGR_COMBO_UP) &&
            config->EnableGSMOp) {
            InGameScreenshot();
        }
#endif

        if (EnableDebug) {
            DBGCOL(0x008000, IGR, "Exiting services");
        }

        SifExitIopHeap();
        LoadFileExit();
        SifExitRpc();

        IGR_Exit(0);
    } else {
        if (EnableDebug) {
            DBGCOL(0x0000FF, IGR, "oplIGRShutdown(1)");
        }

        // R3 + L3 powers off the PS2.
        oplIGRShutdown(1);
    }
}

void IGR_Exit(s32 exit_code)
{
    USE_LOCAL_EECORE_CONFIG;
    // Execute home loader
    if (config->ExitPath[0] != '\0')
        ExecPS2(t_loadElf, &_gp, 0, NULL);

    // Return to PS2 Browser
    Exit(exit_code);
}

// IGR VBLANK_END interrupt handler install to monitor combo trick in pad data aera
static int IGR_Intc_Handler(int cause)
{
    USE_LOCAL_EECORE_CONFIG;
    int i;
    u8 pad_pos_state, pad_pos_frame, pad_pos_combo1, pad_pos_combo2;

    if (Pad_Data.pad_buf != NULL) {
        // Copy values via the uncached segment, to bypass the cache.
        pad_pos_state = ((u8 *)UNCACHED_SEG(Pad_Data.pad_buf))[Pad_Data.pos_state];
        pad_pos_frame = ((u8 *)UNCACHED_SEG(Pad_Data.pad_buf))[Pad_Data.pos_frame];
        pad_pos_combo1 = ((u8 *)UNCACHED_SEG(Pad_Data.pad_buf))[Pad_Data.pos_combo1];
        pad_pos_combo2 = ((u8 *)UNCACHED_SEG(Pad_Data.pad_buf))[Pad_Data.pos_combo2];

        // First check pad state
        if (((Pad_Data.libpad == IGR_LIBPAD) && (pad_pos_state == IGR_PAD_STABLE_V1)) ||
            ((Pad_Data.libpad == IGR_LIBPAD2) && (pad_pos_state == IGR_PAD_STABLE_V2))) {
            // Check if pad buffer is still alive with pad data frame counter
            // If pad frame change save it, otherwise tell to syshook to re-install padOpen hook
            if (Pad_Data.vb_count++ >= 10) {
                if (pad_pos_frame != Pad_Data.prev_frame) {
                    padOpen_hooked = 1;
                    Pad_Data.prev_frame = pad_pos_frame;
                } else {
                    padOpen_hooked = 0;
                }
                Pad_Data.vb_count = 0;
            }

            // Combo R1 + L1 + R2 + L2
            if (pad_pos_combo1 == IGR_COMBO_R1_L1_R2_L2) {
                // Combo Start + Select, R3 + L3 or UP
                if ((pad_pos_combo2 == IGR_COMBO_START_SELECT) || // Start + Select combo, so reset
                    (pad_pos_combo2 == IGR_COMBO_R3_L3)           // R3 + L3 combo, so poweroff
#ifdef IGS
                    || ((pad_pos_combo2 == IGR_COMBO_UP) && (config->EnableGSMOp)) // UP combo, so take IGS
#endif
                )

                    Pad_Data.combo_type = pad_pos_combo2;
            }
        }
    }

    ee_kmode_enter();

    // Check power button press
    if ((*CDVD_R_NDIN & 0x20) && (*CDVD_R_POFF & 0x04)) {
        // Increment button press counter
        Power_Button.press++;

        // Cancel poweroff to catch the second button press
        *CDVD_R_SDIN = 0x00;
        *CDVD_R_SCMD = 0x1B;
    }

    // Start VBlank counter when power button is pressed
    if (Power_Button.press) {
        // Check number of power button press after 1 ~ sec
        if (Power_Button.vb_count++ >= 50) {
            if (Power_Button.press == 1)
                Pad_Data.combo_type = IGR_COMBO_R3_L3; // power button press 1 time, so poweroff
            else
                Pad_Data.combo_type = IGR_COMBO_START_SELECT; // power button press 2 time, so reset
        }
    }

    ee_kmode_exit();

    // If power button or combo is press
    // Disable all interrupts & reset some peripherals.
    // Suspend and Change priority of all threads other then our IGR thread
    // Wakeup and Change priority of our IGR thread
    if (Pad_Data.combo_type != 0x00) {
        // While ExecPS2() would also do some of these (also calls ResetEE),
        // initialization seems to sometimes get stuck at "Initializing GS", perhaps when waiting for the V-Sync start interrupt.
        // That happens before ResetEE is called, so ResetEE has to be called earlier.

        // Wait for preceding loads & stores to complete.
        asm volatile("sync.l\n");

        // Stop all ongoing transfers (except for SIF0, SIF1 & SIF2 - DMA CH 5, 6 & 7).
        u32 dmaEnableR = *R_EE_D_ENABLER;
        *R_EE_D_ENABLEW = dmaEnableR | 0x10000;
        *R_EE_D_CTRL;
        *R_EE_D_STAT;
        *R_EE_D0_CHCR = 0;
        *R_EE_D1_CHCR = 0;
        *R_EE_D2_CHCR = 0;
        *R_EE_D3_CHCR = 0;
        *R_EE_D4_CHCR = 0;
        *R_EE_D8_CHCR = 0;
        *R_EE_D9_CHCR = 0;
        *R_EE_D_ENABLEW = dmaEnableR;

        // Wait for preceding loads & stores to complete.
        asm volatile("sync.l\n");

        *R_EE_GS_CSR = 0x100; // Reset GS
        asm volatile("sync.l\n");
        while (*R_EE_GS_CSR & 0x100) {
        };

        // Disable interrupts & reset some peripherals, back to a standard state.
        // Call ResetEE(0x7F) from an interrupt handler.
        iResetEE(0x7F);

        // Loop for each threads, skipping the idle & IGR threads.
        for (i = 1; i < 256; i++) {
            if (i != IGR_Thread_ID) {
                // Suspend all threads
                iSuspendThread(i);
            }
        }

        DPRINTF("IGR: trying to wake IGR thread...\n");
        iChangeThreadPriority(IGR_Thread_ID, 0);
        // WakeUp IGR thread
        iWakeupThread(IGR_Thread_ID);
    }

    ExitHandler();

    return 0;
}

// Install_IGR() must be run first.
static void Set_libpad_Params(void *addr)
{
    DI();

    Pad_Data.pad_buf = addr;

    // Set positions of pad data and pad state in buffer
    if (Pad_Data.libpad == IGR_LIBPAD) {
        if (Pad_Data.libversion >= 0x0160) {
            Pad_Data.pos_combo1 = 3;
            Pad_Data.pos_combo2 = 2;
            Pad_Data.pos_state = 112;
            Pad_Data.pos_frame = 88;
        } else {
            Pad_Data.pos_combo1 = 11;
            Pad_Data.pos_combo2 = 10;
            Pad_Data.pos_state = 4;
            Pad_Data.pos_frame = 0;
        }
    } else if (Pad_Data.libpad == IGR_LIBPAD2) {
        Pad_Data.pos_combo1 = 29;
        Pad_Data.pos_combo2 = 28;
        Pad_Data.pos_state = 4;
        Pad_Data.pos_frame = 124;
    }

    EI();
}

// Install the polling IGR thread.
// Do not register anything on VBLANK_END.
void Install_IGR(void)
{
    ee_thread_t thread_param = {0};
#ifdef MAIZO_IGR_MENU
    if(IGR_Thread_ID>=0)return;
#endif

    // Reset power-button state.
    Power_Button.press = 0;
    Power_Button.vb_count = 0;

    // The pad-open hook will supply the live pad-buffer address.
    Pad_Data.pad_buf = NULL;
#ifdef MAIZO_IGR_MENU
    if(IGR_Thread_ID<0){
        unsigned j;
        for(j=0;j<8;j++)mzPadSlots[j]=0;
        for(j=0;j<9;j++){mzPadFrames[j]=0;mzPadAge[j]=255;}
        mzPadEpoch=0;
    }
#endif

    // Reset runtime IGR state.
    Pad_Data.vb_count = 0;
    Pad_Data.combo_type = 0x00;
    Pad_Data.prev_frame = 0x00;
#ifdef MAIZO_IGR_MENU
    MaizoIgrWaitRelease=0;
#endif

    // Do not create the polling thread more than once.
    if (IGR_Thread_ID < 0) {
        thread_param.gp_reg = &_gp;
        thread_param.func = IGR_Thread;
        thread_param.stack = (void *)IGR_Stack;
        thread_param.stack_size = IGR_STACK_SIZE;

        /*
         * Legacy OPL retains low priority. The experimental menu uses
         * a short, sleeping high-priority monitor to avoid starvation
         * during CPU-bound 3D rendering. Capture alone uses priority 0.
         */
        thread_param.initial_priority = 127;

        IGR_Thread_ID = CreateThread(&thread_param);

        if (IGR_Thread_ID >= 0) {
            StartThread(IGR_Thread_ID, NULL);
        }
    }

    /*
     * Deliberately do not call:
     *
     *   AddIntcHandler(kINTC_VBLANK_END, ...)
     *   EnableIntc(kINTC_VBLANK_END)
     *
     * Merely registering that handler causes Battle Archives 2 to
     * black-screen, even when its body is empty.
     *
     * Keep a compile-time reference to the existing function so that
     * it may remain in the source without an unused-function warning.
     */
    (void)IGR_Intc_Handler;

    IGR_Intc_ID = -1;
}

void Reset_Padhook(void)
{
    IGR_Intc_ID = -1;
    IGR_Thread_ID = -1;
}

// Hook function for libpad scePadPortOpen
static int Hook_scePadPortOpen(int port, int slot, void *addr)
{
    int ret;

    // Make sure scePadPortOpen function is still available
    if (port == 0 && slot == 0) {
        DPRINTF("IGR: Hook_scePadPortOpen - padOpen hooking check...\n");
        Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_CHECK);
    }

    // Call original scePadPortOpen function
    ret = scePadPortOpen(port, slot, addr);

    // Install IGR with libpad1 parameters
#ifdef MAIZO_IGR_MENU
    if (ret>0 && (unsigned)port<2 && (unsigned)slot<4 && addr) {
        if(IGR_Thread_ID<0)Install_IGR();
        Set_libpad_Params(addr);
        mzPadSlots[port*4+slot]=addr;
        mzPadAge[port*4+slot]=255;
    }
#else
    if (port == 0 && slot == 0) {
        DPRINTF("IGR: Hook_scePadPortOpen - installing IGR...\n");
        Install_IGR();
        Set_libpad_Params(addr);
    }
#endif

    return ret;
}

// Hook function for libpad2 scePad2CreateSocket
static int Hook_scePad2CreateSocket(pad2socketparam_t *SocketParam, void *addr)
{
    int ret;

    // Make sure scePad2CreateSocket function is still available
    if ((SocketParam == NULL) || (SocketParam->port == 0 && SocketParam->slot == 0))
        Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_CHECK);

    // Call original scePad2CreateSocket function
    ret = scePad2CreateSocket(SocketParam, addr);

    // Install IGR with libpad2 parameters
#ifdef MAIZO_IGR_MENU
    if(addr && (!SocketParam || ((unsigned)SocketParam->port<2 && (unsigned)SocketParam->slot<4))){
        unsigned k=SocketParam?SocketParam->port*4+SocketParam->slot:0;
        if(IGR_Thread_ID<0)Install_IGR();
        Set_libpad_Params(addr);
        mzPadSlots[k]=addr;mzPadAge[k]=255;
    }
#else
    if ((SocketParam == NULL) || (SocketParam->port == 0 && SocketParam->slot == 0)) {
        Install_IGR();
        Set_libpad_Params(addr);
    }
#endif

    return ret;
}

// This function patch the padOpen calls. (scePadPortOpen or scePad2CreateSocket)
int Install_PadOpen_Hook(u32 mem_start, u32 mem_end, int mode)
{
    u32 *ptr, *ptr2;
    u32 inst, fncall;
    u32 mem_size, mem_size2;
    u32 pattern[1], mask[1];
    int i, found, patched;

    pattern_t padopen_patterns[NB_PADOPEN_PATTERN] = {
        {padPortOpenpattern0, padPortOpenpattern0_mask, sizeof(padPortOpenpattern0), 1, 0x0211},
        {pad2CreateSocketpattern0, pad2CreateSocketpattern0_mask, sizeof(pad2CreateSocketpattern0), 2, 0x0200},
        {pad2CreateSocketpattern1, pad2CreateSocketpattern1_mask, sizeof(pad2CreateSocketpattern1), 2, 0x0200},
        {pad2CreateSocketpattern2, pad2CreateSocketpattern2_mask, sizeof(pad2CreateSocketpattern2), 2, 0x0200},
        {padPortOpenpattern1, padPortOpenpattern1_mask, sizeof(padPortOpenpattern1), 1, 0x0210},
        {padPortOpenpattern2, padPortOpenpattern2_mask, sizeof(padPortOpenpattern2), 1, 0x0160},
        {padPortOpenpattern3, padPortOpenpattern3_mask, sizeof(padPortOpenpattern3), 1, 0x0150}};

    found = 0;
    patched = 0;
#ifdef MAIZO_IGR_MENU
    /* Pad reopen is common at loading boundaries. If the installed
     * original still matches its exact signature, do not search 31 MiB
     * of game RAM or replace its pointer with another library copy. */
    if(mode==PADOPEN_CHECK){
        u32 original=Pad_Data.libpad==IGR_LIBPAD?(u32)scePadPortOpen:(u32)scePad2CreateSocket;
        for(i=0;i<NB_PADOPEN_PATTERN;i++){
            unsigned size=padopen_patterns[i].size;
            if(Pad_Data.libpad==padopen_patterns[i].type &&
               Pad_Data.libversion==padopen_patterns[i].version &&
               original>=mem_start && original<mem_end && size<=mem_end-original &&
               find_pattern_with_mask((u32 *)original,size,padopen_patterns[i].pattern,
                                      padopen_patterns[i].mask,size))return 0;
        }
    }
#endif

    // Loop for each libpad version
    for (i = 0; i < NB_PADOPEN_PATTERN; i++) {
#ifdef MAIZO_IGR_MENU
        /* CHECK must not switch the installed wrapper to another ABI. */
        if(mode==PADOPEN_CHECK && (Pad_Data.libpad!=padopen_patterns[i].type ||
           Pad_Data.libversion!=padopen_patterns[i].version))continue;
#endif
        ptr = (u32 *)mem_start;
        while (ptr) {
            // Purple while PadOpen pattern search
            if (EnableDebug)
                DBGCOL(0x800080, PADHOOK, "Searching PadOpen() pattern");

            mem_size = mem_end - (u32)ptr;

            // First try to locate the orginal libpad's PadOpen function
            ptr = find_pattern_with_mask(ptr, mem_size, padopen_patterns[i].pattern, padopen_patterns[i].mask, padopen_patterns[i].size);
            if (ptr) {
                DPRINTF("IGR: found padopen pattern%d at 0x%08x mode=%d\n", i, (int)ptr, mode);
                found = 1;

                // Green while PadOpen patches
                if (EnableDebug)
                    DBGCOL(0x008000, PADHOOK, "Patching PadOpen()");

                // Save original PadOpen function
                if (padopen_patterns[i].type == IGR_LIBPAD)
                    scePadPortOpen = (void *)ptr;
                else
                    scePad2CreateSocket = (void *)ptr;

                if (mode == PADOPEN_HOOK) {
                    // Generate generic instruction pattern & mask for a J/JAL to PadOpen()
                    // Use 000010 as the operation, to match both J & JAL.
                    inst = 0x08000000 | (0x03ffffff & ((u32)ptr >> 2));

                    // Ignore bit 26 for the mask because the jump type can be either J (000010) or JAL (000011)
                    pattern[0] = inst;
                    mask[0] = 0xfbffffff;

                    DPRINTF("IGR: searching opcode %08x witk mask %08x\n", (int)pattern[0], (int)mask[0]);

                    // Search & patch for calls to PadOpen
                    ptr2 = (u32 *)mem_start;
                    while (ptr2) {
                        mem_size2 = (u32)((u8 *)mem_end - (u8 *)ptr2);

                        ptr2 = find_pattern_with_mask(ptr2, mem_size2, pattern, mask, sizeof(pattern));
                        if (ptr2) {
                            DPRINTF("IGR: found padOpen call at 0x%08x\n", (int)ptr2);

                            patched = 1;

                            fncall = (u32)ptr2;

                            // Get PadOpen call Jump Instruction type (JAL or J).
                            inst = (ptr2[0] & 0xfc000000);

                            // Get Hook_PadOpen call Instruction code
                            if (padopen_patterns[i].type == IGR_LIBPAD) {
                                DPRINTF("IGR: Hook_scePadPortOpen addr 0x%08x\n", (int)Hook_scePadPortOpen);
                                inst |= 0x03ffffff & ((u32)Hook_scePadPortOpen >> 2);
                            } else {
                                DPRINTF("IGR: Hook_scePad2CreateSocket addr 0x%08x\n", (int)Hook_scePad2CreateSocket);
                                inst |= 0x03ffffff & ((u32)Hook_scePad2CreateSocket >> 2);
                            }

                            DPRINTF("IGR: patching padopen call at addr 0x%08x with opcode %08x\n", (int)fncall, (int)inst);
                            // Overwrite the original PadOpen function call with our function call
                            _sw(inst, fncall);

                            Pad_Data.libpad = padopen_patterns[i].type;
                            Pad_Data.libversion = padopen_patterns[i].version;
                        }
                    }

                    // Locate pointers to scePadOpen(), likely used for JALR.
                    if (!patched) {
                        DPRINTF("IGR: 2nd padOpen patch attempt...\n");

                        // Make pattern with function address saved above
                        pattern[0] = (u32)ptr;
                        mask[0] = 0xffffffff;

                        DPRINTF("IGR: searching opcode %08x witk mask %08x\n", (int)pattern[0], (int)mask[0]);

                        // Search & patch for PadOpen function address
                        ptr2 = (u32 *)mem_start;
                        while (ptr2) {
                            mem_size2 = (u32)((u8 *)mem_end - (u8 *)ptr2);

                            ptr2 = find_pattern_with_mask(ptr2, mem_size2, pattern, mask, sizeof(pattern));
                            if (ptr2) {
                                DPRINTF("IGR: found padOpen call at 0x%08x\n", (int)ptr2);

                                patched = 1;

                                fncall = (u32)ptr2;

                                // Get Hook_PadOpen function address
                                if (padopen_patterns[i].type == IGR_LIBPAD) {
                                    DPRINTF("IGR: Hook_scePadPortOpen addr 0x%08x\n", (int)Hook_scePadPortOpen);
                                    inst = (u32)Hook_scePadPortOpen;
                                } else {
                                    DPRINTF("IGR: Hook_scePad2CreateSocket addr 0x%08x\n", (int)Hook_scePad2CreateSocket);
                                    inst = (u32)Hook_scePad2CreateSocket;
                                }

                                DPRINTF("IGR: patching padopen call at addr 0x%08x with opcode %08x\n", (int)fncall, (int)inst);
                                // Overwrite the original PadOpen function address with our function address
                                _sw(inst, fncall);

                                Pad_Data.libpad = padopen_patterns[i].type;
                                Pad_Data.libversion = padopen_patterns[i].version;
                            }
                        }
                    }
                } else {
                    DPRINTF("IGR: no hooking requested, breaking loop...\n");
                    // Hooking is not required and padOpen function was found, so stop searching
                    break;
                }

                // Increment search pointer
                // ptr += padopen_patterns[i].size;
                ptr += (padopen_patterns[i].size >> 2);
            }
        }

        // If a padOpen function call was patched or ( hooking is not required and a padOpen function was found ), so stop the libpad version search loop
        if (patched == 1 || (mode == PADOPEN_CHECK && found == 1)) {
            DPRINTF("IGR: job done exiting...\n");
            break;
        }
    }

    // Done
    if (EnableDebug)
        BGCOLND(0x000000); // Black

    return patched;
}
