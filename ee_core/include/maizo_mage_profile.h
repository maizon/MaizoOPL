/* Only the chosen profile crosses into the protected resident core. */
#ifndef MAIZO_MAGE_PROFILE_H
#define MAIZO_MAGE_PROFILE_H
#include <stdint.h>
typedef struct {
    uint32_t set,state,dispatch,table,device,hash;
    uint32_t fog,fog_hash;
    uint32_t reload_blur,reload_blur_hash;
} mz_sky_record_t;
static inline unsigned mzSkyLimit(unsigned start,unsigned end)
{
    if(end<=0x100000u && start<=end)return 0x02000000u;
    if(start>=0x100000u && start<=end && end<=0x02000000u)return start;
    return 0;
}
static inline int mzSkyRange(unsigned a,unsigned n,unsigned limit)
{
    return a>=0x100000u && a<=limit && n<=limit-a && !(a&3u);
}
#endif
