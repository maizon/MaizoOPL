/* Shared EE/IOP input transport: normalized active-high PS2 buttons.
 * A single 16-byte SIF DMA quadword, with a consistency check. */
#ifndef MAIZO_IGR_INPUT_H
#define MAIZO_IGR_INPUT_H
#include <stdint.h>
#define MZ_PAD_MAGIC 0x4d5au
#define MZ_PAD_CHORD 0x0f09u
static inline uint32_t mzPadCheck(uint32_t seq,uint32_t a,uint32_t b,unsigned valid)
{
    return ((uint32_t)MZ_PAD_MAGIC<<16)|(((seq^a^(a>>16)^b^(b>>16)^valid)&0xfffu)<<4)|(valid&15u);
}
static inline int mzPadPacketOK(const uint32_t *p)
{
    return p[0] && p[3]==mzPadCheck(p[0],p[1],p[2],p[3]&15u);
}
/* Never create IGR/power-off by combining two different controllers. */
static inline unsigned mzPadPick(unsigned best,unsigned keys)
{
    if((keys&MZ_PAD_CHORD)==MZ_PAD_CHORD)return MZ_PAD_CHORD;
    if(keys==0x0f06u && best!=MZ_PAD_CHORD)return keys;
    return best?best:keys;
}
#endif
