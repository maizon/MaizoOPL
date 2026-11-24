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
           (vif&0x00000f0fu)==0u &&
           (gif&0x00000dc0u)==0u;
}
#endif
