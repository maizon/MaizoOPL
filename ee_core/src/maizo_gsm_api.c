/*
#
# Graphics Synthesizer Mode Selector (a.k.a. GSM) - Force (set and keep) a GS Mode, then load & exec a PS2 ELF
#-------------------------------------------------------------------------------------------------------------
# Copyright 2009, 2010, 2011 doctorxyz & dlanor
# Copyright 2011, 2012 doctorxyz, SP193 & reprep
# Copyright 2013 Bat Rastard
# Copyright 2014, 2015, 2016 doctorxyz
# Licenced under Academic Free License version 2.0
# Review LICENSE file for further details.
#
*/

#include <tamtypes.h>
#include <kernel.h>
#include <syscallnr.h>

#include "gsm_api.h"
#include "dve_reg.h"

#define MAKE_J(func) (u32)((0x02 << 26) | (((u32)func) / 4)) // Jump (MIPS instruction)
#define NOP          0x00000000                              // No Operation (MIPS instruction)

extern void (*Old_SetGsCrt)(short int interlace, short int mode, short int ffmd);

// Taken from gsKit
// NTSC
#define GS_MODE_NTSC 0x02
// PAL
#define GS_MODE_PAL  0x03

struct GSMDestSetGsCrt
{
    s16 interlace;
    s16 mode;
    s16 ffmd;
} __attribute__((packed));

struct GSMDestGSRegs
{
    u64 pmode;
    u64 smode1;
    u64 smode2;
    u64 srfsh;
    u64 synch1;
    u64 synch2;
    u64 syncv;
    u64 dispfb1;
    u64 display1;
    u64 dispfb2;
    u64 display2;
} __attribute__((packed));

struct GSMFlags
{
    u32 dx_offset;
    u32 dy_offset;
    u8 ADAPTATION_fix;
    u8 PMODE_fix;
    u8 SMODE1_fix;
    u8 SMODE2_fix;
    u8 SRFSH_fix;
    u8 SYNCH_fix;
    u8 SYNCV_fix;
    u8 DISPFB_fix;
    u8 DISPLAY_fix;
    u8 FIELD_fix;
    u8 gs576P_param;
} __attribute__((packed));

extern struct GSMDestSetGsCrt GSMDestSetGsCrt;
extern struct GSMDestGSRegs GSMDestGSRegs;
extern struct GSMFlags GSMFlags;

extern void Hook_SetGsCrt();
extern void GSHandler();
extern volatile u32 MaizoGsMonitorOnly;
extern u64 GSMSourceGSRegs[11];

static int gsInstalled;
static unsigned int KSEG_backup[2]; // Copies of the original words at 0x80000100 and 0x80000104.

/* Update GSM params */
/*-------------------*/
/*-------------------*/
// Update parameters to be enforced by Hook_SetGsCrt syscall hook and GSHandler service routine functions
void UpdateGSMParams(s16 interlace, s16 mode, s16 ffmd, u64 display, u64 syncv, u64 smode2, u32 dx_offset, u32 dy_offset, int k576p_fix, int kGsDxDyOffsetSupported, int FIELD_fix)
{
    unsigned int hvParam = GetGsVParam();
    int gs_DH, gs_DW, gs_DY, gs_DX;

    GSMDestSetGsCrt.interlace = interlace;
    GSMDestSetGsCrt.mode = mode;
    GSMDestSetGsCrt.ffmd = ffmd;

    GSMDestGSRegs.smode2 = (u8)smode2;
    GSMDestGSRegs.display1 = (u64)display;
    GSMDestGSRegs.display2 = (u64)display;
    GSMDestGSRegs.syncv = (u64)syncv;

    GSMFlags.dx_offset = (u32)dx_offset; // X-axis offset -> Use it only when automatic adaptations formulas don't suffice
    GSMFlags.dy_offset = (u32)dy_offset; // Y-axis offset -> Use it only when automatic adaptations formulas don't suffice
    // 0 = Off, 1 = On
    GSMFlags.ADAPTATION_fix = 1; // Default = 1 = On
    GSMFlags.PMODE_fix = 0;      // Default = 0 = Off
    GSMFlags.SMODE1_fix = 0;     // Default = 0 = Off
    GSMFlags.SMODE2_fix = 1;     // Default = 1 = On
    GSMFlags.SRFSH_fix = 0;      // Default = 0 = Off
    GSMFlags.SYNCH_fix = 0;      // Default = 0 = Off
    GSMFlags.SYNCV_fix = 0;      // Default = 0 = Off
    GSMFlags.DISPFB_fix = 1;     // Default = 1 = On
    GSMFlags.DISPLAY_fix = 1;    // Default = 1 = On
    GSMFlags.FIELD_fix = (FIELD_fix ? (1 << 2) : 0);
    GSMFlags.gs576P_param = (k576p_fix ? (1 << 1) : 0) | (hvParam & 1);

    if (kGsDxDyOffsetSupported && (!(mode >= GS_MODE_NTSC && mode <= GS_MODE_PAL))) {
        _GetGsDxDyOffset(mode, &gs_DX, &gs_DY, &gs_DW, &gs_DH);
        GSMFlags.dx_offset += gs_DX;
        GSMFlags.dy_offset += gs_DY;
    }
}

/*------------------------------------------------------------------*/
/* Replace SetGsCrt in kernel. (Graphics Synthesizer Mode Selector) */
/*------------------------------------------------------------------*/
static inline void Install_Hook_SetGsCrt(void)
{
    Old_SetGsCrt = GetSyscallHandler(__NR_SetGsCrt);
    SetSyscall(__NR_SetGsCrt, Hook_SetGsCrt);
}

/*-----------------------------------------------------------------*/
/* Restore original SetGsCrt. (Graphics Synthesizer Mode Selector) */
/*-----------------------------------------------------------------*/
static inline void Remove_Hook_SetGsCrt(void)
{
    SetSyscall(__NR_SetGsCrt, Old_SetGsCrt);
}

/*----------------------------------------------------------------------------------------------------*/
/* Install Display Handler in place of the Core Debug Exception Handler (V_DEBUG handler)             */
/* Exception Vector Address for Debug Level 2 Exception when Stadus.DEV bit is 0 (normal): 0x80000100 */
/* 'Level 2' is a generalization of Error Level (from previous MIPS processors)                       */
/* When this exception is recognized, control is transferred to the applicable service routine;       */
/* in our case the service routine is 'GSHandler'!                                                    */
/*----------------------------------------------------------------------------------------------------*/
static void Install_GSHandler(void)
{
    DI();
    ee_kmode_enter();

    KSEG_backup[0] = *(volatile u32 *)0x80000100;
    KSEG_backup[1] = *(volatile u32 *)0x80000104;

    *(volatile u32 *)0x80000100 = MAKE_J((int)GSHandler);
    *(volatile u32 *)0x80000104 = NOP;
    ee_kmode_exit();

    /* Publish the replacement exception vector before any GS access
     * can trap. The old instruction cache must never be executed
     * with our new breakpoint configuration. */
    FlushCache(0);
    FlushCache(2);
    DI();
    Enable_GSBreakpoint();
    gsInstalled=1;
    EI();
}

static void Remove_GSHandler(void)
{
    DI();

    Disable_GSBreakpoint();

    // Restore only the vector we installed, never replace a newer owner.
    ee_kmode_enter();
    if(*(volatile u32 *)0x80000100 == MAKE_J((int)GSHandler) &&
       *(volatile u32 *)0x80000104 == NOP){
        *(volatile u32 *)0x80000100 = KSEG_backup[0];
        *(volatile u32 *)0x80000104 = KSEG_backup[1];
    }
    ee_kmode_exit();
    gsInstalled=0;
    FlushCache(0);
    FlushCache(2);
    EI();
}

/*-------------------------------------------*/
/* Enable Graphics Synthesizer Mode Selector */
/*-------------------------------------------*/
void EnableGSM(void)
{
    MaizoGsMonitorOnly=0;
    // Install Hook SetGsCrt
    Install_Hook_SetGsCrt();
    // Install Display Handler
    Install_GSHandler();
}

/*
 * Menu-only recorder. Does not select or force a CRT video mode; the
 * GS write breakpoint forwards game writes unchanged in this mode.
 */
#ifdef MAIZO_IGR_MENU
volatile u32 MaizoGsSeen;
extern volatile u32 MaizoNativeMask,MaizoNativeFieldInvert;
extern volatile u32 MaizoGsFilterMode,MaizoGs480iMode,MaizoGs480iArmed;
static u32 mzSavedBpc,mzSavedDab,mzSavedDabm;

/* No GS debugging exception or syscall patch exists during ordinary
 * gameplay unless a user explicitly requests a native video override.
 * A new capture must not reuse stale write-only scanout values. */
int EnableMaizoGsMonitor(void)
{
    int i;
    u32 bpc;
    volatile u8 *flags=(volatile u8 *)&GSMFlags;
    if(gsInstalled)return MaizoGsMonitorOnly!=0;
    int intr=DI();
    ee_kmode_enter();
    asm volatile("mfbpc %0\n\tsync.p":"=r"(bpc));
    if(bpc&0xe0000000u){ee_kmode_exit();if(intr)EI();return 0;}
    mzSavedBpc=bpc;
    asm volatile("mfdab %0\n\tmfdabm %1\n\tsync.p":"=r"(mzSavedDab),"=r"(mzSavedDabm));
    ee_kmode_exit();
    if(intr)EI();
    MaizoGsSeen=0;
    for(i=0;i<11;i++)GSMSourceGSRegs[i]=0;
    for(i=0;i<19;i++)flags[i]=0;
    MaizoGsMonitorOnly=1;
    Install_GSHandler(); /* vector is visible before syscall can rearm */
    Install_Hook_SetGsCrt();
    return 1;
}

void MaizoGsMonitorEnd(int force)
{
    if(!gsInstalled||!MaizoGsMonitorOnly)return;
    if(!force && (MaizoNativeMask||MaizoNativeFieldInvert||
        MaizoGsFilterMode||MaizoGs480iMode||MaizoGs480iArmed))return;
    DisableGSM();
    /* Restore the idle debug comparator state borrowed for capture. */
    int intr=DI();
    ee_kmode_enter();
    asm volatile("mtdab %0\n\tmtdabm %1\n\tsync.p\n\tmtbpc %2\n\tsync.p"::
        "r"(mzSavedDab),"r"(mzSavedDabm),"r"(mzSavedBpc):"memory");
    ee_kmode_exit();
    if(intr)EI();
    MaizoGsSeen=0;
    if(force){
        MaizoNativeMask=MaizoNativeFieldInvert=MaizoGsFilterMode=0;
        MaizoGs480iMode=MaizoGs480iArmed=0;
    }
}
#endif

/*--------------------------------------------*/
/* Disable Graphics Synthesizer Mode Selector */
/*--------------------------------------------*/
void DisableGSM(void)
{
    if(!gsInstalled)return;
    // Disable trapping before changing the mode or restoring the vector.
    Remove_GSHandler();
    if(GetSyscallHandler(__NR_SetGsCrt)==Hook_SetGsCrt)
        Remove_Hook_SetGsCrt();
    MaizoGsMonitorOnly=0;
}

/*--------------------------------------------*/
/* Set up the DVE for 576P mode               */
/*--------------------------------------------*/
void setdve_576P(void)
{ // The parameters are exactly the same as the 480P mode's. Regardless of the model, GS revision or region.
    dve_prepare_bus();

    dve_set_reg(0x77, 0x11);
    dve_set_reg(0x93, 0x01);
    dve_set_reg(0x91, 0x02);
}
