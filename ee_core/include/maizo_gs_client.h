/* Renderer-side integration. Include this before any runtime header,
 * define mzGsShared once in the game's C source, and call Bind once.
 * No speculative commercial-game patching or DMA register trapping. */
#ifndef MAIZO_GS_CLIENT_H
#define MAIZO_GS_CLIENT_H
#define MZ_GS_CLIENT 1
#include "maizo_gs_shadow.h"

static inline int mzGsClientBindRange(void *base,size_t bytes)
{
    size_t at;
    mz_gs_shared_t *found=0;
    mzGsShared=0;
    if(!base||((uintptr_t)base&3u)||bytes<sizeof(mz_gs_shared_t))return 0;
    for(at=0;at<=bytes-sizeof(mz_gs_shared_t);at+=4u){
        volatile uint32_t *p=(volatile uint32_t *)((uint8_t *)base+at);
        if(p[0]!=MZ_GS_SHARED_MAGIC0||p[1]!=MZ_GS_SHARED_MAGIC1||
           p[2]!=MZ_GS_SHARED_ABI||p[3]!=sizeof(mz_gs_shared_t)||p[4]!=1u)
            continue;
        if(found)return 0; /* ambiguous ABI marker: refuse */
        found=(mz_gs_shared_t *)(uintptr_t)p;
    }
    mzGsShared=found;
    return found!=0;
}
#ifdef __mips__
static inline int mzGsClientBind(void)
{
    return mzGsClientBindRange((void *)0x84000u,0x9b000u-0x84000u);
}
#endif
static inline int mzGsClientAttach(unsigned int caps)
{
    return mzGsShared&&mzGsShared->enabled&&mzGsRuntimeAttach(caps);
}
/* Caller owns an immutable packet and separate scratch until DMA
 * completion. Protected UI/text or unknown pass -> original. */
static inline int mzGsClientStage(unsigned type,const void *original,void *scratch,
                                  size_t capacity,size_t bytes,
                                  const mz_gs_dma_guard_t *guard,int eligible,
                                  void **selected)
{
    if(!selected||!original)return MZ_GS_DMA_BAD_PACKET;
    *selected=(void *)original;
    if(!mzGsShared||!mzGsShared->enabled||!eligible)return MZ_GS_DMA_SKIPPED;
    return mzGsRuntimeStageShadow(type,original,scratch,capacity,bytes,guard,selected);
}
/* Real pre-submit dispatch. The callbacks are supplied by a verified
 * renderer; never claim an idle channel or buffer ownership ourselves.
 * Submit exactly once, even after parser failure. */
typedef int (*mz_gs_submit_fn)(void *ctx,void *source,size_t bytes);
typedef void (*mz_gs_flush_fn)(void *ctx,void *source,size_t bytes);
static inline int mzGsClientSend(unsigned type,const void *original,void *scratch,
                                  size_t capacity,size_t bytes,
                                  const mz_gs_dma_guard_t *guard,int eligible,
                                  mz_gs_submit_fn submit,mz_gs_flush_fn flush,void *ctx)
{
    void *selected=(void *)original;
    if(!submit||!original)return MZ_GS_DMA_BAD_PACKET;
    if(flush)
        mzGsClientStage(type,original,scratch,capacity,bytes,guard,eligible,&selected);
    if(selected!=original)flush(ctx,selected,bytes);
    return submit(ctx,selected,bytes);
}
#endif
