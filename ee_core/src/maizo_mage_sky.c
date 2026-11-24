/* Compact live Sky route. The frontend selects a structural evidence record.
 * Validate the loaded code and original/current table before any write. */
#include "maizo_mage_sky.h"
#include "maizo_gs_runtime.h"
#include "coreconfig.h"
#include "util.h"
#include <kernel.h>
static int skyMode=-1;
static int skyFogReady,skyReloadReady,skyPending;
static int skyValid(const mz_sky_record_t *p,unsigned mode)
{
    uint32_t h=mzSkyHash(2166136261u,(const volatile uint32_t *)(uintptr_t)p->set,16);
    h=mzSkyHash(h,(const volatile uint32_t *)(uintptr_t)p->state,53);
    h=mzSkyHash(h,(const volatile uint32_t *)(uintptr_t)(p->device-16u),6);
    return h==p->hash &&
           *(const volatile uint32_t *)(uintptr_t)(p->dispatch+32u)==p->state &&
           mzSkyTableOK((const volatile uint32_t *)(uintptr_t)p->table,p->state,mode);
}
void maizoSkySelect(const char *path)
{
    USE_LOCAL_EECORE_CONFIG;
    const mz_sky_record_t *p=&config->MaizoSky;
    skyMode=-1;
    skyFogReady=skyReloadReady=skyPending=0;
    if(!p->state || !mzGsResident.enabled ||
       _strncmp(path,"cdrom0:\\",8) || _strncmp(path+8,config->GameID,11) ||
       (path[19] && _strcmp(path+19,";1")))return;
    /* Only identify the candidate at ELF load. Do not inspect game
     * addresses until a user-requested, suspended menu boundary. */
    skyPending=1;
}
void maizoSkyPrepare(void)
{
    USE_LOCAL_EECORE_CONFIG;
    const mz_sky_record_t *p=&config->MaizoSky;
    unsigned limit=mzSkyLimit((unsigned)config->ModStorageStart,(unsigned)config->ModStorageEnd);
    if(!skyPending || skyMode>=0 || !mzGsResident.enabled)return;
    if(mzSkyRange(p->set,64,limit) && mzSkyRange(p->state,212,limit) &&
       mzSkyRange(p->dispatch,36,limit) && mzSkyRange(p->table,24,limit) &&
       mzSkyRange(p->device-16u,24,limit) && skyValid(p,0)){skyMode=0;skyPending=0;}
    if(skyMode>=0 && p->fog_hash && mzSkyRange(p->dispatch,56,limit) &&
       mzSkyRange(p->fog,64,limit) &&
       *(const volatile uint32_t *)(uintptr_t)(p->dispatch+52u)==p->fog &&
       (*(const volatile uint32_t *)(uintptr_t)p->fog & ~1u)==MZ_SKY_FOG_ORIGINAL &&
       mzSkyFogCodeOK((const volatile uint32_t *)(uintptr_t)p->fog,p->fog_hash))skyFogReady=1;
    if(skyMode>=0 && p->reload_blur_hash && p->reload_blur>=16u &&
       mzSkyRange(p->reload_blur-16u,36,limit) &&
       *(const volatile uint32_t *)(uintptr_t)p->reload_blur==MZ_SKY_RELOAD_ORIGINAL &&
       mzSkyReloadCodeOK((const volatile uint32_t *)(uintptr_t)(p->reload_blur-16u),p->reload_blur_hash))skyReloadReady=1;
}
int maizoSkyAvailable(void)
{
    return skyMode>=0 && skyValid(&g_ee_core_config.MaizoSky,(unsigned)skyMode);
}
unsigned maizoSkyMode(void)
{
    return skyMode<0?0u:(unsigned)skyMode;
}
int maizoSkySetMode(unsigned mode)
{
    if(mode>3 || !maizoSkyAvailable())return 0;
    if(mode!=(unsigned)skyMode){
        const mz_sky_record_t *p=&g_ee_core_config.MaizoSky;
        volatile uint32_t *table=(volatile uint32_t *)(uintptr_t)p->table;
        for(unsigned i=2;i<6;i++)table[i]=mzSkyTarget(p->state,i,mode);
        FlushCache(0);
        skyMode=mode;
    }
    return 1;
}

/* Only called at the paused IGR boundary. Do not skip a whole render
 * pass, modify a global blend flag, or guess a GS register value.
 * The next state-14 request uses the title's own normal OFF path.
 * Original means release the override, not force fog back on. */
int maizoSkyFogAvailable(void)
{
    const mz_sky_record_t *p=&g_ee_core_config.MaizoSky;
    return skyFogReady && skyMode>=0 &&
           *(const volatile uint32_t *)(uintptr_t)(p->dispatch+52u)==p->fog &&
           mzSkyFogCodeOK((const volatile uint32_t *)(uintptr_t)p->fog,p->fog_hash);
}
unsigned maizoSkyFogMode(void)
{
    return maizoSkyFogAvailable() &&
           (*(const volatile uint32_t *)(uintptr_t)g_ee_core_config.MaizoSky.fog & ~1u)==MZ_SKY_FOG_OFF?2u:0u;
}
int maizoSkyFogSet(unsigned mode)
{
    volatile uint32_t *code=(volatile uint32_t *)(uintptr_t)g_ee_core_config.MaizoSky.fog;
    uint32_t desired;
    if((mode!=0u && mode!=2u)||!maizoSkyFogAvailable())return 0;
    desired=(mode==2u?MZ_SKY_FOG_OFF:MZ_SKY_FOG_ORIGINAL)|(*code&1u);
    if(*code!=desired){
        *code=desired;
        FlushCache(0);
        FlushCache(2);
    }
    return 1;
}

int maizoSkyReloadAvailable(void)
{
    const mz_sky_record_t *p=&g_ee_core_config.MaizoSky;
    return skyReloadReady && skyMode>=0 &&
           mzSkyReloadCodeOK((const volatile uint32_t *)(uintptr_t)(p->reload_blur-16u),p->reload_blur_hash);
}
unsigned maizoSkyReloadMode(void)
{
    return maizoSkyReloadAvailable() &&
           *(const volatile uint32_t *)(uintptr_t)g_ee_core_config.MaizoSky.reload_blur==MZ_SKY_RELOAD_OFF?2u:0u;
}
int maizoSkyReloadSet(unsigned mode)
{
    volatile uint32_t *code=(volatile uint32_t *)(uintptr_t)g_ee_core_config.MaizoSky.reload_blur;
    uint32_t desired=mode==2u?MZ_SKY_RELOAD_OFF:MZ_SKY_RELOAD_ORIGINAL;
    if((mode!=0u && mode!=2u)||!maizoSkyReloadAvailable())return 0;
    if(*code!=desired){
        *code=desired;
        FlushCache(0);
        FlushCache(2);
    }
    return 1;
}
