/* Resident entry gate. No call into arbitrary RAM: validate range,
 * ABI and reserved module end each time. The menu's code and data
 * are linked independently at MZ_OVL_BASE. */
#include "maizo_overlay_api.h"
#include "maizo_igrmenu.h"
#include "maizo_ee_layout.h"
static const mz_ovl_api api={
    .shared=&mzGsResident,
    .p_g_ee_core_config=&g_ee_core_config,
    .p_GSMSourceGSRegs=&GSMSourceGSRegs,
    .p_GSMDestGSRegs=&GSMDestGSRegs,
    .p_GSMFlags=&GSMFlags,
    .p_GSMSourceSetGsCrt=&GSMSourceSetGsCrt,
    .p_GSMDestSetGsCrt=&GSMDestSetGsCrt,
    .p_MaizoNativeMask=&MaizoNativeMask,
    .p_MaizoNativeSource=&MaizoNativeSource,
    .p_MaizoNativeTarget=&MaizoNativeTarget,
    .p_MaizoGsMonitorOnly=&MaizoGsMonitorOnly,
    .p_MaizoNativeFieldInvert=&MaizoNativeFieldInvert,
    .p_MaizoGs480iMode=&MaizoGs480iMode,
    .p_MaizoGs480iArmed=&MaizoGs480iArmed,
    .p_MaizoGs480iOriginalFix=&MaizoGs480iOriginalFix,
    .p_MaizoGs480iOriginalSMODE2=&MaizoGs480iOriginalSMODE2,
    .p_MaizoGsFilterMode=&MaizoGsFilterMode,
    .p_MaizoGsFilterPrimary=&MaizoGsFilterPrimary,
    .p_MaizoGsFilterSourceEN=&MaizoGsFilterSourceEN,
    .p_MaizoNativeSmoothMaxFBP=&MaizoNativeSmoothMaxFBP,
    .p_MaizoGsFilterPMODE=&MaizoGsFilterPMODE,
    .p_MaizoGsFilterBaseFB=&MaizoGsFilterBaseFB,
    .p_MaizoGsFilterBaseDisplay=&MaizoGsFilterBaseDisplay,
    .p_MaizoGsSeen=&MaizoGsSeen,
    .p_MaizoCheatPaused=&MaizoCheatPaused,
    .p_MaizoCheatHookActive=&MaizoCheatHookActive,
    .p_MaizoLiveMask=&MaizoLiveMask,
    .p_MaizoLiveCount=&MaizoLiveCount,
    .p_numcodes=&numcodes,
    .p_mzGsProbe=&mzGsProbe,
    .p_DIntr=&DIntr,
    .p_EIntr=&EIntr,
    .p_Enable_GSBreakpoint=&Enable_GSBreakpoint,
    .p_GsGetIMR=&GsGetIMR,
    .p_GsPutIMR=&GsPutIMR,
#ifdef MAIZO_MAGE_SKY_EXPERIMENTAL
    .p_maizoSkyPrepare=&maizoSkyPrepare,
    .p_maizoSkyAvailable=&maizoSkyAvailable,
    .p_maizoSkySetMode=&maizoSkySetMode,
    .p_maizoSkyFogAvailable=&maizoSkyFogAvailable,
    .p_maizoSkyFogSet=&maizoSkyFogSet,
    .p_maizoSkyReloadAvailable=&maizoSkyReloadAvailable,
    .p_maizoSkyReloadSet=&maizoSkyReloadSet,
#endif
#ifdef MAIZO_IGR_RESTORE
    .p_mzRestoreBegin=&mzRestoreBegin,
    .p_mzRestoreWrite=&mzRestoreWrite,
    .p_mzRestoreRead=&mzRestoreRead,
    .p_mzRestoreHush=&mzRestoreHush,
    .p_mzRestoreKeep=&mzRestoreKeep,
    .p_mzRestoreEnd=&mzRestoreEnd,
#endif
};
static const mz_ovl_header *find(void)
{
    USE_LOCAL_EECORE_CONFIG;
    const mz_ovl_header *h=(const mz_ovl_header *)MZ_OVL_BASE;
    if(!config->EnableMaizoIgrMenu ||
       (u32)config->ModStorageStart!=MZ_EE_EXP_TOP ||
       (u32)config->ModStorageEnd!=MZ_OVL_END ||
       !mzOvlHeaderOK(h) || h->api_size!=sizeof(api))return 0;
    return h;
}
int maizoIGR_Preflight(u32 mask)
{
    const mz_ovl_header *h=find();
    #ifdef MAIZO_IGR_RESTORE
    if(!mzRestoreReady())return 0;
    #endif
    return h?((int (*)(const mz_ovl_api *,u32))h->preflight)(&api,mask):0;
}
int maizoIGR_MenuRun(int (*buttons)(u16 *,u32 *),void (*yield)(void),u32 mask)
{
    const mz_ovl_header *h=find();
    return h?((int (*)(const mz_ovl_api *,int (*)(u16 *,u32 *),void (*)(void),u32))h->run)(&api,buttons,yield,mask):MAIZO_IGR_UNAVAILABLE;
}
char *maizoIGR_ProfileArg(void)
{
    const mz_ovl_header *h=find();
    return h?((char *(*)(const mz_ovl_api *))h->profile)(&api):0;
}
