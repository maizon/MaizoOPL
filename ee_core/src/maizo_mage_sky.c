/* Compact live Sky route. The frontend selects a structural evidence record.
 * Validate the loaded code and original/current table before any write. */
#include "maizo_mage_sky.h"
#include "maizo_gs_runtime.h"
#include "coreconfig.h"
#include "util.h"
#include <kernel.h>
static int skyMode=-1;
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
    unsigned limit=mzSkyLimit((unsigned)config->ModStorageStart,(unsigned)config->ModStorageEnd);
    skyMode=-1;
    if(!p->state || !mzGsResident.enabled ||
       _strncmp(path,"cdrom0:\\",8) || _strncmp(path+8,config->GameID,11) ||
       (path[19] && _strcmp(path+19,";1")))return;
    if(mzSkyRange(p->set,64,limit) && mzSkyRange(p->state,212,limit) &&
       mzSkyRange(p->dispatch,36,limit) && mzSkyRange(p->table,24,limit) &&
       mzSkyRange(p->device-16u,24,limit) && skyValid(p,0))skyMode=0;
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
