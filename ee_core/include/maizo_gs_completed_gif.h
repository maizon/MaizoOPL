/* Observe only certified completed GIF streams, in actual GS order.
 * Unknown producers must invalidate this shadow. No automatic hooks.
 * Tracks A+D writes (including transfer registers), not hidden state. */
#ifndef MZ_GS_COMPLETED_GIF_H
#define MZ_GS_COMPLETED_GIF_H
#include <stddef.h>
#include <string.h>
#include "maizo_gs_checkpoint.h"
static inline uint64_t mzCompleted64(const uint8_t *p)
{
    uint64_t v;memcpy(&v,p,8);return v;
}
static inline void mzCompletedInvalidate(mz_gs_register_shadow_t *s)
{
    memset(s->known,0,sizeof(s->known));
}
static inline int mzGsObserveCompletedGif(mz_gs_register_shadow_t *s,const void *data,size_t bytes)
{
    const uint8_t *p=(const uint8_t *)data;
    size_t at=0;
    unsigned eop=0;
    if(!s)return 0;
    if(!data||!bytes||(bytes&15u)||bytes>(1u<<20))goto bad;
    while(at<bytes){
        uint64_t tag=mzCompleted64(p+at),regs=mzCompleted64(p+at+8);
        unsigned loops=tag&0x7fffu,fmt=(tag>>58)&3u,nreg=(tag>>60)&15u;
        if(!nreg)nreg=16;
        unsigned count=loops*nreg;
        size_t length=fmt==0?(size_t)count*16u:fmt==1?((size_t)count+1u)/2u*16u:(size_t)loops*16u;
        at+=16;
        if(length>bytes-at)goto bad;
        if(fmt==0 && (tag&(1ull<<46)))mzGsShadowObserve(s,0,(tag>>47)&0x7ffu);
        if(fmt<=1)for(unsigned i=0;i<count;i++){
            unsigned reg=(regs>>((i%nreg)*4u))&15u;
            const uint8_t *v=p+at+i*(fmt==0?16u:8u);
            if(reg==14){
                if(fmt!=0)goto bad;
                unsigned id=mzCompleted64(v+8)&255u;
                if(id<128)mzGsShadowObserve(s,id,mzCompleted64(v));
            }else if(reg!=15){
                /* Do not claim exact values for non-A+D formats. */
                s->known[0]&=~(1u<<reg);
                if(reg==4||reg==5||reg==12||reg==13)s->known[0]&=~0x3030u;
                if(reg==1||reg==2)s->known[0]&=~6u;
            }
        }
        at+=length;eop=(tag>>15)&1u;
    }
    if(eop)return 1;
bad:
    mzCompletedInvalidate(s);return 0;
}
#endif
