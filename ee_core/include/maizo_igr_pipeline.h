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
       !(vif&0x1f803f0fu))return 3;
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
/* RiptOPL's empty-ring test, restricted to non-borrowing CPU FIFO.
 * MFIFO mode alone is not a busy condition. Never stop a drain or
 * change D_CTRL. An active VIF1 drain may be bypassed ONLY by PATH3,
 * with the producer stopped, QWC zero and a latched, masked empty
 * event proving no tag is ready. An active GIF drain is not borrowed.
 * PS2tek: MFD 2=VIF1, 3=GIF; D_STAT 14=empty, 30=its IRQ mask. */
static inline int mzIgrMfifoPath(uint32_t dctrl,uint32_t ch1,uint32_t ch2,
                                uint32_t ch8,uint32_t qwc,uint32_t tadr,
                                uint32_t madr,uint32_t rbsr,uint32_t vif,
                                uint32_t gif,uint32_t stat)
{
    unsigned mode=(dctrl>>2)&3u;
    if(!mode)return mzIgrSafePath(ch1,ch2,vif,gif,stat);
    if(mode==1u || (ch8&0x100u) || (stat&0x40000000u) ||
       ((stat&0x100u)&&(stat&0x01000000u)))return 0;
    if(!((ch1|ch2)&0x100u))
        return mzIgrSafePath(ch1,ch2,vif,gif,stat&~0x4000u);
    if(mode!=2u || (ch2&0x100u) || qwc || !rbsr ||
       !(stat&0x4000u) || ((tadr^madr)&rbsr) ||
       ((tadr^madr)&~rbsr))return 0;
    return mzIgrSafePath(ch1&~0x100u,ch2,vif,gif,stat&~0x4000u)==3?3:0;
}
/* Read-only CSR FIFO status. This is not an internal GS state snapshot
 * or a FINISH event; never clear the game's CSR flags. */
static inline int mzIgrGsFifoEmpty(uint32_t csr)
{
    return (csr&0xc000u)==0x4000u;
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
