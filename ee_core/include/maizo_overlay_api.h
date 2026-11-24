#ifndef MZ_OVERLAY_API_H
#define MZ_OVERLAY_API_H
#include "ee_core.h"
#include "gsm_api.h"
#include "coreconfig.h"
#include "cheat_api.h"
#include "maizo_gs_runtime.h"
#include "maizo_gs_probe.h"
#include "maizo_mage_sky.h"
#include "maizo_restore.h"
#include "maizo_overlay_layout.h"
extern u64 GSMSourceGSRegs[11];
extern u64 GSMDestGSRegs[11];
extern u8 GSMFlags[19];
extern u16 GSMSourceSetGsCrt[3];
extern u16 GSMDestSetGsCrt[3];
extern volatile u32 MaizoNativeMask;
extern volatile u64 MaizoNativeSource[2],MaizoNativeTarget[2];
extern volatile u32 MaizoGsMonitorOnly,MaizoNativeFieldInvert;
extern volatile u32 MaizoGs480iMode,MaizoGs480iArmed;
extern u32 MaizoGs480iOriginalFix;
extern u64 MaizoGs480iOriginalSMODE2;
extern volatile u32 MaizoGsFilterMode;
extern volatile u32 MaizoGsFilterPrimary;
extern volatile u32 MaizoGsFilterSourceEN,MaizoNativeSmoothMaxFBP;
extern volatile u64 MaizoGsFilterPMODE;
extern volatile u64 MaizoGsFilterBaseFB;
extern volatile u64 MaizoGsFilterBaseDisplay;
typedef struct {
    mz_gs_shared_t *shared;
    __typeof__(g_ee_core_config) *p_g_ee_core_config;
    __typeof__(GSMSourceGSRegs) *p_GSMSourceGSRegs;
    __typeof__(GSMDestGSRegs) *p_GSMDestGSRegs;
    __typeof__(GSMFlags) *p_GSMFlags;
    __typeof__(GSMSourceSetGsCrt) *p_GSMSourceSetGsCrt;
    __typeof__(GSMDestSetGsCrt) *p_GSMDestSetGsCrt;
    __typeof__(MaizoNativeMask) *p_MaizoNativeMask;
    __typeof__(MaizoNativeSource) *p_MaizoNativeSource;
    __typeof__(MaizoNativeTarget) *p_MaizoNativeTarget;
    __typeof__(MaizoGsMonitorOnly) *p_MaizoGsMonitorOnly;
    __typeof__(MaizoNativeFieldInvert) *p_MaizoNativeFieldInvert;
    __typeof__(MaizoGs480iMode) *p_MaizoGs480iMode;
    __typeof__(MaizoGs480iArmed) *p_MaizoGs480iArmed;
    __typeof__(MaizoGs480iOriginalFix) *p_MaizoGs480iOriginalFix;
    __typeof__(MaizoGs480iOriginalSMODE2) *p_MaizoGs480iOriginalSMODE2;
    __typeof__(MaizoGsFilterMode) *p_MaizoGsFilterMode;
    __typeof__(MaizoGsFilterPrimary) *p_MaizoGsFilterPrimary;
    __typeof__(MaizoGsFilterSourceEN) *p_MaizoGsFilterSourceEN;
    __typeof__(MaizoNativeSmoothMaxFBP) *p_MaizoNativeSmoothMaxFBP;
    __typeof__(MaizoGsFilterPMODE) *p_MaizoGsFilterPMODE;
    __typeof__(MaizoGsFilterBaseFB) *p_MaizoGsFilterBaseFB;
    __typeof__(MaizoGsFilterBaseDisplay) *p_MaizoGsFilterBaseDisplay;
    __typeof__(MaizoGsSeen) *p_MaizoGsSeen;
    __typeof__(MaizoCheatPaused) *p_MaizoCheatPaused;
    __typeof__(MaizoCheatHookActive) *p_MaizoCheatHookActive;
    __typeof__(MaizoLiveMask) *p_MaizoLiveMask;
    __typeof__(MaizoLiveCount) *p_MaizoLiveCount;
    __typeof__(numcodes) *p_numcodes;
    __typeof__(mzGsProbe) *p_mzGsProbe;
    __typeof__(DIntr) *p_DIntr;
    __typeof__(EIntr) *p_EIntr;
    __typeof__(Enable_GSBreakpoint) *p_Enable_GSBreakpoint;
    __typeof__(GsGetIMR) *p_GsGetIMR;
    __typeof__(GsPutIMR) *p_GsPutIMR;
#ifdef MAIZO_MAGE_SKY_EXPERIMENTAL
    __typeof__(maizoSkyPrepare) *p_maizoSkyPrepare;
    __typeof__(maizoSkyAvailable) *p_maizoSkyAvailable;
    __typeof__(maizoSkySetMode) *p_maizoSkySetMode;
    __typeof__(maizoSkyFogAvailable) *p_maizoSkyFogAvailable;
    __typeof__(maizoSkyFogSet) *p_maizoSkyFogSet;
    __typeof__(maizoSkyReloadAvailable) *p_maizoSkyReloadAvailable;
    __typeof__(maizoSkyReloadSet) *p_maizoSkyReloadSet;
#endif
#ifdef MAIZO_IGR_RESTORE
    __typeof__(mzRestoreBegin) *p_mzRestoreBegin;
    __typeof__(mzRestoreWrite) *p_mzRestoreWrite;
    __typeof__(mzRestoreRead) *p_mzRestoreRead;
    __typeof__(mzRestoreHush) *p_mzRestoreHush;
    __typeof__(mzRestoreKeep) *p_mzRestoreKeep;
    __typeof__(mzRestoreEnd) *p_mzRestoreEnd;
#endif
} mz_ovl_api;
#endif
