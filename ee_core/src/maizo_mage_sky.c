/* All addresses/fingerprints are from supplied ELF revisions, not guesses.
 * A runtime fingerprint is a local structural check, NOT full ELF SHA-256.
 * Preserve the stock renderer if code/data or the current table differs. */
#include "maizo_mage_sky.h"
#include "maizo_gs_runtime.h"
#include "util.h"
#include <kernel.h>
typedef struct {
    char id[12];
    uint32_t set,state,dispatch,table,device,hash;
} mz_sky_profile_t;
static const mz_sky_profile_t skyProfiles[] = {
    {"SLES_525.84", 0x001f4658u, 0x001f4b58u, 0x004b8980u, 0x004b8a00u, 0x0049b1c8u, 0x17cbed3fu},
    {"SLES_539.67", 0x002fd4a0u, 0x002fd9f8u, 0x005ec4a0u, 0x005ec520u, 0x005c5568u, 0x1fc4fa39u},
    {"SLUS_205.52", 0x003bd5c8u, 0x003bdac8u, 0x004ae360u, 0x004ae3b0u, 0x0044dea0u, 0xc651bb36u},
    {"SLUS_207.77", 0x003b6ca8u, 0x003b71a8u, 0x004cd8a0u, 0x004cd8f0u, 0x004ba930u, 0x45413539u},
    {"SLUS_209.46", 0x0034fd68u, 0x00350268u, 0x0064e2d0u, 0x0064e350u, 0x00600d88u, 0x7fd09a3bu},
    {"SLUS_212.15", 0x0048e250u, 0x0048e750u, 0x0058f060u, 0x0058f0e0u, 0x0052f628u, 0x2107049fu},
    {"SLUS_212.69", 0x002356c8u, 0x00235bc8u, 0x005c36d0u, 0x005c3750u, 0x00578b70u, 0x55e938b1u},
    {"SLUS_213.76", 0x002b5088u, 0x002b5590u, 0x00402f20u, 0x00402fa0u, 0x003c3d90u, 0xb5784ba3u},
    {"SLUS_218.99", 0x002fef48u, 0x002ff4a0u, 0x0058a3c0u, 0x0058a440u, 0x0047c390u, 0x6c8a0849u},
};
static const mz_sky_profile_t *skyProfile;
static unsigned skyMode;
static int skyRange(unsigned address,unsigned bytes,unsigned limit)
{
    return address >= 0x100000u && address <= limit &&
           bytes <= limit-address && !(address&3u);
}
static int skyValid(const mz_sky_profile_t *p,unsigned mode)
{
    const volatile uint32_t *set=(const volatile uint32_t *)(uintptr_t)p->set;
    const volatile uint32_t *state=(const volatile uint32_t *)(uintptr_t)p->state;
    const volatile uint32_t *dev=(const volatile uint32_t *)(uintptr_t)(p->device-16u);
    uint32_t h=mzSkyHash(2166136261u,set,16);
    h=mzSkyHash(h,state,53);
    h=mzSkyHash(h,dev,6);
    return h==p->hash &&
           *(const volatile uint32_t *)(uintptr_t)(p->dispatch+32u)==p->state &&
           mzSkyTableOK((const volatile uint32_t *)(uintptr_t)p->table,p->state,mode);
}
void maizoSkySelect(const char *path,const char *id,unsigned limit)
{
    skyProfile=0;
    skyMode=0;
    if (!path || !id || !mzGsResident.enabled || limit>0x02000000u ||
        _strncmp(path,"cdrom0:\\",8) || _strncmp(path+8,id,11) ||
        (path[19] && _strcmp(path+19,";1"))) return;
    for (unsigned i=0;i<sizeof(skyProfiles)/sizeof(skyProfiles[0]);i++) {
        const mz_sky_profile_t *p=&skyProfiles[i];
        if (_strcmp(id,p->id)) continue;
        if (skyRange(p->set,64,limit) && skyRange(p->state,212,limit) &&
            skyRange(p->dispatch,36,limit) && skyRange(p->table,24,limit) &&
            skyRange(p->device-16u,24,limit) && skyValid(p,0)) skyProfile=p;
        break;
    }
}
int maizoSkyAvailable(void)
{
    return skyProfile && skyValid(skyProfile,skyMode);
}
unsigned maizoSkyMode(void)
{
    return skyMode;
}
int maizoSkySetMode(unsigned mode)
{
    if (mode>3 || !maizoSkyAvailable()) return 0;
    if (mode!=skyMode) {
        volatile uint32_t *table=(volatile uint32_t *)(uintptr_t)skyProfile->table;
        /* Every intermediate entry points into original validated code.
         * Only cases 2..5 are changed. No instruction or VU memory edit. */
        for (unsigned i=2;i<6;i++) table[i]=mzSkyTarget(skyProfile->state,i,mode);
        FlushCache(0);
        skyMode=mode;
    }
    return 1;
}
