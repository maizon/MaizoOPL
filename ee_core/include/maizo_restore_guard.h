/* No writes to MFIFO, DMA channels, VU or IPU. Configured MFIFO is
 * admitted only when BOTH drains and the producer are stopped, QWC
 * zero, ring addresses equal and no error/event is pending. */
#ifndef MAIZO_RESTORE_GUARD_H
#define MAIZO_RESTORE_GUARD_H
#include <stdint.h>
static inline int mzrRingIdle(uint32_t ctrl,uint32_t ch1,uint32_t ch2,
                              uint32_t ch8,uint32_t qwc,uint32_t tadr,
                              uint32_t madr,uint32_t rbsr)
{
    unsigned mode=(ctrl>>2)&3u;
    if((ch1|ch2|ch8)&0x100u)return 0;
    if(!mode)return 1;
    return mode!=1 && !qwc && rbsr && tadr==madr;
}
/* A TRXDIR write starts a transfer unless XDIR=3 (deactivated).
 * Never replay a saved active direction, even after a FIFO idle sample. */
static inline int mzrTransferReplayable(uint32_t known,uint64_t dir)
{
    return known==15u && dir==3u;
}
#endif
