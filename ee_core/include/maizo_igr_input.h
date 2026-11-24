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
/* All live controllers may navigate. Aggregate ONLY the menu keys,
 * never Select or the full shoulder set. A complete resume chord
 * must have been observed on one controller. Keeping all held keys
 * also prevents one source from hiding a new direction on another. */
#define MZ_PAD_MENU_KEYS 0x68f8u
static inline unsigned mzPadMenuMerge(unsigned combined,unsigned keys)
{
    if(combined==MZ_PAD_CHORD || (keys&MZ_PAD_CHORD)==MZ_PAD_CHORD)
        return MZ_PAD_CHORD;
    return combined|(keys&MZ_PAD_MENU_KEYS);
}
/* The native D-pad wins over the stick. Deadzone and dominant axis
 * prevent diagonal jitter; do not change gameplay or opening chords. */
static inline unsigned mzPadMenuAnalog(unsigned keys,unsigned x,unsigned y)
{
    if((keys&0xf0u) || (keys&0x0f00u)==0x0f00u)return keys;
    unsigned dx=x>128?x-128:128-x,dy=y>128?y-128:128-y;
    if(dx<80u&&dy<80u)return keys;
    if(dy>=dx)return keys|(y<128?0x10u:0x40u);
    return keys|(x<128?0x80u:0x20u);
}
/* Consume rising edges once. Repeat ONLY directions, after a delay,
 * from fresh input samples. Never repeat OK, BACK, START or a chord. */
static inline unsigned mzPadMenuStep(unsigned keys,unsigned *previous,unsigned *held)
{
    unsigned edge=keys&~*previous;
    unsigned dir=keys&0xf0u;
    if(dir && dir==(*previous&0xf0u)){
        if(*held<16u)(*held)++;
        else if(*held==16u){edge|=dir;*held=13u;}
    }else *held=0;
    *previous=keys;
    return edge;
}
#endif
