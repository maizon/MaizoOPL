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
static inline int mzLiveCatalogOK(const mz_live_cheats_t *p)
{
    unsigned i,j,seen=0;
    if(!p->count||p->count>MZ_LIVE_GROUPS||!p->lines||p->lines>MZ_LIVE_CODES||
       (p->mask>>p->count))return 0;
    for(i=0;i<p->lines;i++){
        if(p->owner[i]>=p->count)return 0;
        seen|=1u<<p->owner[i];
    }
    if(seen!=((1u<<p->count)-1u))return 0;
    for(i=0;i<p->count;i++){
        if(!p->name[i][0]||p->name[i][MZ_LIVE_NAME-1])return 0;
        if(p->variant[i]&&(p->mask&(1u<<i)))
            for(j=0;j<i;j++)
                if(p->variant[i]==p->variant[j]&&(p->mask&(1u<<j)))return 0;
    }
    return 1;
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
