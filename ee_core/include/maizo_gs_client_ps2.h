/* PS2SDK renderer adapter: actual DMA submission, no register trap.
 * The renderer must guarantee exclusive ownership of its channel and
 * original/scratch buffers until completion. UI/text can pass eligible=0.
 * No speculative in-place edits, no new thread or interrupt hook. */
#ifndef MAIZO_GS_CLIENT_PS2_H
#define MAIZO_GS_CLIENT_PS2_H
#include <tamtypes.h>
#include <kernel.h>
#include <dma.h>
#include "maizo_gs_client.h"
typedef struct {int channel,flags,spr;} mz_ps2_dma_call_t;
static inline int mzPs2DmaSubmit(void *ctx,void *p,size_t bytes)
{
    mz_ps2_dma_call_t *d=(mz_ps2_dma_call_t *)ctx;
    return dma_channel_send_normal(d->channel,p,(int)(bytes/16u),d->flags,d->spr);
}
static inline void mzPs2DmaFlush(void *ctx,void *p,size_t bytes)
{
    (void)ctx;
    SyncDCache(p,(unsigned char *)p+bytes);
}
static inline int mzPs2DmaIdle(int channel)
{
    unsigned int reg;
    if(channel==DMA_CHANNEL_GIF)reg=0x1000a000u;
    else if(channel==DMA_CHANNEL_VIF1)reg=0x10009000u;
    else return 0;
    return !(*(volatile unsigned int *)reg&0x100u) &&
           !(*(volatile unsigned int *)0x1000e000u&0xcu); /* no MFIFO */
}
static inline int mzGsPs2Normal(int channel,void *data,int qwc,int flags,int spr,
                                void *scratch,size_t capacity,int exclusive,int eligible)
{
    mz_ps2_dma_call_t call={channel,flags,spr};
    mz_gs_dma_guard_t guard={0,0,0,0,0};
    unsigned type;
    if(qwc<=0||qwc>65535||flags||spr||!exclusive||
       (channel!=DMA_CHANNEL_GIF&&channel!=DMA_CHANNEL_VIF1))
        return dma_channel_send_normal(channel,data,qwc,flags,spr);
    type=channel==DMA_CHANNEL_GIF?MZ_SUBMIT_PATH3_NORMAL:MZ_SUBMIT_PATH2_VIF1;
    guard.path=channel==DMA_CHANNEL_GIF?MZ_GS_PATH3_GIF_DMA:MZ_GS_PATH2_VIF1;
    guard.caller_owns_buffer=1;guard.channel_idle=mzPs2DmaIdle(channel);
    return mzGsClientSend(type,data,scratch,capacity,(size_t)qwc*16u,&guard,
                          eligible,mzPs2DmaSubmit,mzPs2DmaFlush,&call);
}
/* PATH1: stage only a complete GIF stream that the renderer's own
 * VU microprogram is known to XGKICK unchanged, before VU upload.
 * Never read or rewrite executing VU1 microcode. */
static inline int mzGsPs2VuStage(const void *original,void *scratch,size_t capacity,
                                 size_t bytes,int exclusive,int eligible,void **selected)
{
    mz_gs_dma_guard_t guard={MZ_GS_PATH1_XGKICK,0,0,0,0};
    guard.caller_owns_buffer=!!exclusive;
    guard.channel_idle=mzPs2DmaIdle(DMA_CHANNEL_VIF1);
    return mzGsClientStage(MZ_SUBMIT_PATH1_STAGING,original,scratch,capacity,bytes,
                           &guard,eligible,selected);
}
static inline int mzGsPs2Scanlines(uint64_t *packet,size_t capacity,
                                  const mz_scanline_frame_t *frame,int exclusive)
{
    mz_ps2_dma_call_t call={DMA_CHANNEL_GIF,0,0};
    mz_gs_dma_guard_t guard={MZ_GS_PATH3_GIF_DMA,0,0,0,0};
    guard.caller_owns_buffer=!!exclusive;
    guard.channel_idle=mzPs2DmaIdle(DMA_CHANNEL_GIF)&&mzPs2DmaIdle(DMA_CHANNEL_VIF1)&&
        !(*(volatile unsigned int *)0x10003020u&0x1f000c00u);
    return mzGsClientEndFrame(packet,capacity,frame,&guard,mzPs2DmaSubmit,mzPs2DmaFlush,&call);
}
#endif
