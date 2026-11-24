#include "maizo_gs_client_ps2.h"
#include "maizo_gs_autolink.h"
/* Renderer RAM, not resident loader RAM. Each channel owns its
 * shadow until its hardware STR goes idle; never reuse across channels. */
#define MZ_AUTO_BYTES 8192u
static unsigned char buffers[2][MZ_AUTO_BYTES] __attribute__((aligned(64)));
static int eligible;
static const mz_chain_range_t *chainRanges;
static size_t chainCount;
extern int __real_dma_channel_send_normal(int,void *,int,int,int);
extern int __real_dma_channel_send_chain(int,void *,int,int,int);
void mzGsAutoEligible(int exclusive){eligible=!!exclusive;chainRanges=0;chainCount=0;}
void mzGsAutoChainEligible(const mz_chain_range_t *ranges,size_t count)
{
    eligible=0;chainRanges=ranges;chainCount=count;
}
static int mzAutoSubmit(void *ctx,void *data,size_t bytes)
{
    mz_ps2_dma_call_t *d=ctx;
    return __real_dma_channel_send_normal(d->channel,data,(int)(bytes/16u),d->flags,d->spr);
}
int __wrap_dma_channel_send_normal(int ch,void *data,int qwc,int flags,int spr)
{
    int owned=eligible;
    unsigned idx,type;
    mz_ps2_dma_call_t call={ch,flags,spr};
    mz_gs_dma_guard_t guard={0,0,0,0,0};
    /* The mark is consumed by precisely one call, even on failure. */
    eligible=0;chainRanges=0;chainCount=0;
    if(!owned||!mzGsShared||!mzGsShared->enabled||flags||spr||qwc<=0||
       qwc>(int)(MZ_AUTO_BYTES/16u)||
       (ch!=DMA_CHANNEL_GIF&&ch!=DMA_CHANNEL_VIF1)||!mzPs2DmaIdle(ch))
        return __real_dma_channel_send_normal(ch,data,qwc,flags,spr);
    idx=ch==DMA_CHANNEL_GIF?0u:1u;
    type=idx?MZ_SUBMIT_PATH2_VIF1:MZ_SUBMIT_PATH3_NORMAL;
    guard.path=idx?MZ_GS_PATH2_VIF1:MZ_GS_PATH3_GIF_DMA;
    guard.caller_owns_buffer=1;
    guard.channel_idle=1;
    return mzGsClientSend(type,data,buffers[idx],MZ_AUTO_BYTES,(size_t)qwc*16u,
        &guard,1,mzAutoSubmit,mzPs2DmaFlush,&call);
}

/* Real interposition at the SDK chain boundary. The original submission
 * is always forwarded exactly once, even for unsupported packet types.
 * GIF normal and GIF chain share the same scratch: hardware STR guards
 * its lifetime. No allocator or interrupt handler in the draw path. */
int __wrap_dma_channel_send_chain(int ch,void *data,int qwc,int flags,int spr)
{
    const mz_chain_range_t *ranges=chainRanges;
    size_t count=chainCount,bytes=0;
    mz_gs_dma_guard_t guard={MZ_GS_PATH3_GIF_DMA,1,1,0,0};
    uintptr_t address=(uintptr_t)data;
    eligible=0;chainRanges=0;chainCount=0;
    if(ranges&&count&&mzGsShared&&mzGsShared->enabled&&ch==DMA_CHANNEL_GIF&&
       !flags&&!spr&&qwc>0&&address>=0x100000u&&address<0x02000000u&&
       mzPs2DmaIdle(ch)&&mzGsChainStage((uint32_t)address,ranges,count,
           buffers[0],sizeof(buffers[0]),&guard,&bytes)==MZ_GS_DMA_OK){
        return __real_dma_channel_send_chain(ch,buffers[0],(int)(bytes/16u),0,0);
    }
    return __real_dma_channel_send_chain(ch,data,qwc,flags,spr);
}
