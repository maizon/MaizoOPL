/* Idle sample is not proof of ownership of internal GS transfer state. */
#ifndef MZ_IGR_LIVE_GUARD_H
#define MZ_IGR_LIVE_GUARD_H
#include "maizo_igr_pipeline.h"
static inline int mzLivePathOK(uint32_t dctrl,uint32_t enable,
                               uint32_t ch1,uint32_t ch2,uint32_t vif,
                               uint32_t gif,uint32_t stat,uint32_t csr,
                               unsigned qwc)
{
    return !(dctrl&12u) && !(enable&0x10000u) &&
           !(gif&0x1f001fffu) && !(vif&0x1f803f0fu) &&
           !((ch1|ch2)&0x100u) && !(stat&0xe006u) &&
           mzIgrGsFifoEmpty(csr) && mzIgrFifoFits(gif,qwc);
}
#endif
