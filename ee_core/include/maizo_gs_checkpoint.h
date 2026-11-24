/* Cooperative checkpoint contract. No title memory is reserved by OPL.
 * The renderer owns and retains a 64-byte aligned 4 MiB buffer, drains
 * every producer, and publishes the actual final GS transfer registers.
 * All workers must stay at a safe boundary until request is cleared.
 * Not an automatic hook for commercial titles. */
#ifndef MAIZO_GS_CHECKPOINT_H
#define MAIZO_GS_CHECKPOINT_H
#include <stdint.h>
#define MZ_GS_VRAM_BYTES 0x400000u
#define MZ_GS_PAGE_BYTES 8192u
#define MZ_GS_PAGE_COUNT 512u
typedef struct {
    volatile uint32_t registered,request,ready,fault;
    uint32_t address,bytes,known,reserved;
    uint64_t transfer[4];
} mz_gs_checkpoint_t;
/* Exact last completed writes to transfer setup. A renderer may track
 * all GS register writes using this shadow, but hidden pipeline state
 * and side-effect commands are never replayed. */
typedef struct {
    uint64_t value[128];
    uint32_t known[4];
} mz_gs_register_shadow_t;
static inline void mzGsShadowObserve(mz_gs_register_shadow_t *s,unsigned reg,uint64_t v)
{
    if(reg>=128u)return;
    s->value[reg]=v;s->known[reg>>5]|=1u<<(reg&31u);
}
static inline void mzGsCheckpointBarrier(void)
{
#ifdef __mips__
    asm volatile("sync.l" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}
static inline int mzGsCheckpointBufferOK(uint32_t address,uint32_t bytes)
{
    return bytes==MZ_GS_VRAM_BYTES && !(address&63u) &&
        address>=0x00100000u && address<=0x02000000u-MZ_GS_VRAM_BYTES;
}
static inline int mzGsCheckpointReady(const mz_gs_checkpoint_t *c)
{
    return c->registered && c->request && c->ready && !c->fault &&
        mzGsCheckpointBufferOK(c->address,c->bytes) &&
        c->known==15u && c->transfer[3]==3u;
}
static inline int mzGsCheckpointPublish(mz_gs_checkpoint_t *c,
                                        const mz_gs_register_shadow_t *s)
{
    if(!s||!c->registered||!c->request||c->ready||c->fault)return 0;
    if((s->known[0x50u>>5] & (15u<<(0x50u&31u)))!=(15u<<(0x50u&31u)) ||
       s->value[0x53]!=3u)return 0;
    for(unsigned i=0;i<4;i++)c->transfer[i]=s->value[0x50u+i];
    c->known=15u;
    mzGsCheckpointBarrier();c->ready=1;
    return 1;
}
static inline void mzGsCheckpointRelease(mz_gs_checkpoint_t *c)
{
    c->ready=0;mzGsCheckpointBarrier();c->request=0;
}
static inline uint32_t mzGsPageBase(unsigned page){return page*32u;}
#endif
