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
           (vif&0x1f800f0fu)==0u &&
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
    if(!(gif&3u)&&mzIgrDmaBorrowable(stat,2))return 3;
    if(mzIgrDmaBorrowable(stat,1))return 2;
    return 0;
}
static inline int mzIgrReadyPath(uint32_t ch1,uint32_t ch2,uint32_t vif,
                                 uint32_t gif,uint32_t stat)
{
    int path=mzIgrSelectPath(stat,gif);
    if(path==3 && !((ch1|ch2)&0x100u) && !(gif&0x1f001fe0u) &&
       !(vif&0x1f80000fu))return 3;
    return path==2&&mzIgrPipelineIdle(ch1,ch2,vif,gif)?2:0;
}
/* The original enabled IRQ masks must survive our temporary hold.
 * A pending game completion is never treated as an overlay event. */
static inline uint32_t mzIgrOriginalDstat(uint32_t stat,uint32_t held)
{
    return stat|((held&6u)<<16);
}
#endif
