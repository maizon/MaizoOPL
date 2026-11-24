/* Wait for an actual VIF1 command boundary. Ch1/ch2 STR clear alone
 * does not guarantee safe DIRECT; VIF1 may be stuck in an incomplete
 * UNPACK/MSCAL or waiting for an active GIF PATH. */
#ifndef MAIZO_IGR_PIPELINE_H
#define MAIZO_IGR_PIPELINE_H
#include <stdint.h>
static inline int mzIgrPipelineIdle(uint32_t ch1,uint32_t ch2,
                                    uint32_t vif,uint32_t gif)
{
    return ((ch1|ch2)&0x100u)==0u &&
           (vif&0x1f803f0fu)==0u &&
           (gif&0x1f001fe0u)==0u;
}
/* GIF_STAT M3R/M3P/PSE: do not use PATH3 if masked or paused.
 * D_STAT bits 0..15 are W1C events; upper bits are IRQ masks.
 * Borrow only a channel with no enabled pending completion or error. */
static inline int mzIgrDmaBorrowable(uint32_t stat,unsigned int channel)
{
    return channel<10u && !(stat&0xe000u) &&
           !((stat&(1u<<channel))&&(stat&(1u<<(channel+16u))));
}
static inline int mzIgrSelectPath(uint32_t stat,uint32_t gif)
{
    if(gif&0x1008u)return 0; /* reverse transfer or global GIF pause */
    /* PATH3 uses CPU FIFO stores, not channel 2 DMA. Its pending
     * completion must be preserved, not used as a rejection reason. */
    if(!(gif&3u)&&!(stat&0xe000u))return 3;
    if(mzIgrDmaBorrowable(stat,1))return 2;
    return 0;
}
static inline int mzIgrReadyPath(uint32_t ch1,uint32_t ch2,uint32_t vif,
                                 uint32_t gif,uint32_t stat)
{
    int path=mzIgrSelectPath(stat,gif);
    if(path==3 && !((ch1|ch2)&0x100u) && !(gif&0x1f001fe0u) &&
       !(vif&0x1f80300fu))return 3;
    return path==2&&mzIgrPipelineIdle(ch1,ch2,vif,gif)?2:0;
}
/* Both routes use CPU FIFO stores only; no borrowed DMA channel.
 * Prefer PATH3 (does not touch VIF CODE); if masked, allow PATH2
 * at a complete, unstalled VIF command boundary with no queued GIF
 * producer. Existing game IRQ completion bits are left untouched. */
static inline int mzIgrSafePath(uint32_t ch1,uint32_t ch2,uint32_t vif,
                                uint32_t gif,uint32_t stat)
{
    if((stat&0xe000u)||(gif&0x1008u))return 0;
    if(!(gif&3u))
        return mzIgrReadyPath(ch1,ch2,vif,gif,stat)==3?3:0;
    return mzIgrPipelineIdle(ch1,ch2,vif,gif)?2:0;
}
/* A whole independently terminated GIF packet must fit in the hardware
 * FIFO before writing its first tag. FQC occupies bits 24..28, 16 QW.
 * No timeout in the middle of an IMAGE payload. */
static inline int mzIgrFifoFits(uint32_t gif,unsigned qwc)
{
    unsigned used=(gif>>24)&31u;
    return qwc && qwc<=16u && used<=16u && qwc<=16u-used &&
           !(gif&0x1fefu);
}
/* VIF1 DIRECT: one command QW plus a complete GIF EOP packet.
 * Start only at an empty, idle VIF parser; no reset/cancel commands. */
static inline int mzIgrVifFifoFits(uint32_t vif,uint32_t gif,unsigned gifqwc)
{
    return gifqwc>0 && gifqwc<=15u &&
           !(vif&0x1f803f0fu) && !(gif&0x1f001fe8u);
}
/* The original enabled IRQ masks must survive our temporary hold.
 * A pending game completion is never treated as an overlay event. */
static inline uint32_t mzIgrOriginalDstat(uint32_t stat,uint32_t held)
{
    return stat|((held&6u)<<16);
}
#endif
