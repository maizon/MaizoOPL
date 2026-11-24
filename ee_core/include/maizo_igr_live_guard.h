/* Idle sample is not proof of ownership of internal GS transfer state. */
#ifndef MZ_IGR_LIVE_GUARD_H
#define MZ_IGR_LIVE_GUARD_H
#include "maizo_igr_pipeline.h"
/* Raw PMODE/SMODE2 plus each enabled circuit's DISPFB and DISPLAY.
 * Filter transactions also need both circuits for exact restoration. */
static inline int mzLiveSeenOK(uint32_t pmode,uint32_t seen,int both)
{
    unsigned en=pmode&3u,need=5u;
    if(!en)return 0;
    if(both||(en&1u))need|=(1u<<7)|(1u<<8);
    if(both||(en&2u))need|=(1u<<9)|(1u<<10);
    return (seen&need)==need;
}
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
