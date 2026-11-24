/* Renderer-side integration. Include this before any runtime header,
 * define mzGsShared once in the game's C source, and call Bind once.
 * No speculative commercial-game patching or DMA register trapping. */
#ifndef MAIZO_GS_CLIENT_H
#define MAIZO_GS_CLIENT_H
#define MZ_GS_CLIENT 1
#include "maizo_gs_shadow.h"
#include "maizo_gs_scanlines.h"
#include "maizo_ee_layout.h"

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
    return mzGsClientBindRange((void *)0x84000u,MZ_EE_EXP_TOP-0x84000u);
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
/* A renderer may advertise scanlines ONLY when it calls this after
 * its last drawing command, with a complete frame and known state.
 * Do not route this packet through drawing filters. */
static inline int mzGsClientEndFrame(uint64_t *packet,size_t capacity,
                                     const mz_scanline_frame_t *frame,
                                     const mz_gs_dma_guard_t *guard,
                                     mz_gs_submit_fn submit,mz_gs_flush_fn flush,void *ctx)
{
    unsigned strength,qwc;
    int result;
    if(!mzGsShared||!mzGsShared->enabled||!submit||!flush||
       !guard||guard->path!=MZ_GS_PATH3_GIF_DMA||!guard->caller_owns_buffer||
       !guard->channel_idle||guard->chain_tag_transfer||guard->source_is_scratchpad)
        return MZ_GS_DMA_SKIPPED;
    strength=mzGsRuntimeMode(MZ_DRAW_SCANLINES);
    if(!strength)return MZ_GS_DMA_SKIPPED;
    qwc=mzScanlineBuild(packet,capacity,frame,strength);
    if(!qwc)return MZ_GS_DMA_UNSUPPORTED;
    flush(ctx,packet,qwc*16u);
    result=submit(ctx,packet,qwc*16u);
    if(result>=0){mzRuntimeMetrics.edits.scanmask_changes++;mzRuntimeAccepted++;}
    return result;
}
#endif
