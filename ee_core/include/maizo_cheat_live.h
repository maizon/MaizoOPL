/* Compact resident catalog. Only independent constant writes are
 * eligible. Complex PS2RD groups retain the legacy launch path. */
#ifndef MAIZO_CHEAT_LIVE_H
#define MAIZO_CHEAT_LIVE_H
#include <stdint.h>
#define MZ_LIVE_GROUPS 8
#define MZ_LIVE_CODES 48
#define MZ_LIVE_NAME 24
typedef struct {
    unsigned int count, lines, mask;
    unsigned char owner[MZ_LIVE_CODES];
    unsigned char variant[MZ_LIVE_GROUPS];
    char name[MZ_LIVE_GROUPS][MZ_LIVE_NAME];
} mz_live_cheats_t;
static inline int mzLiveWriteOK(uint32_t addr)
{
    unsigned type=addr>>28, a=addr&0x01ffffffu;
    return type<=2 && !(addr&0x0e000000u) && a>=0x00100000u &&
           !(a&((1u<<type)-1u));
}
static inline unsigned mzLiveToggle(const mz_live_cheats_t *p,unsigned mask,unsigned i)
{
    unsigned j,bit;
    if(i>=p->count||i>=MZ_LIVE_GROUPS)return mask;
    bit=1u<<i;
    if(mask&bit)return mask&~bit;
    if(p->variant[i])
        for(j=0;j<p->count&&j<MZ_LIVE_GROUPS;j++)
            if(p->variant[j]==p->variant[i])mask&=~(1u<<j);
    return mask|bit;
}
#endif
