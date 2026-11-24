/*
 * MAIZO In-Game independent framebuffer discovery.
 *
 * Active output state must come from a recorder observing real GS writes,
 * not from guessing memory offsets or reading write-only GS registers.
 * Conservative: one read circuit, known color PSM, in-range GS VRAM.
 * No allocations, hardware access or GSM video forcing in this module.
 */
#ifndef MAIZO_IGR_MONITOR_H
#define MAIZO_IGR_MONITOR_H
#include <stdint.h>
typedef struct {
    uint32_t width, height, sx, sy, x, y, fbw, fbbase;
    uint32_t psm, pixel_bytes;
} mz_igr_frame_t;

static inline int mzIgrMonitorFrame(mz_igr_frame_t *out,
                                    uint64_t pmode,
                                    uint64_t fb1, uint64_t fb2,
                                    uint64_t display1, uint64_t display2,
                                    uint64_t smode2)
{
    uint64_t fb,display,last_byte;
    uint32_t en,bpp,vram_bpp,fbw,bp,psm,dbx,dby,w,h,magh,magv,sx,sy,x,y;
    if(!out)return 0;
    en=(uint32_t)(pmode&3ULL);
    if(en!=1u && en!=2u)return 0; /* mixed circuits: unknown image */
    fb=en==1u?fb1:fb2;
    display=en==1u?display1:display2;
    if(!fb||!display)return 0;
    psm=(uint32_t)((fb>>15)&31ULL);
    bpp=psm==0u?4u:psm==1u?3u:(psm==2u||psm==10u)?2u:0u;
    if(!bpp)return 0;
    vram_bpp=bpp==3u?4u:bpp; /* PSMCT24 occupies 32-bit VRAM words */
    fbw=(uint32_t)((fb>>9)&63ULL);
    bp=(uint32_t)(fb&511ULL);
    dbx=(uint32_t)((fb>>32)&2047ULL);
    dby=(uint32_t)((fb>>43)&2047ULL);
    magh=(uint32_t)((display>>23)&15ULL)+1u;
    magv=(uint32_t)((display>>27)&3ULL)+1u;
    w=((uint32_t)((display>>32)&4095ULL)+1u)/magh;
    h=((uint32_t)((display>>44)&2047ULL)+1u)/magv;
    if((smode2&3ULL)==3ULL)h/=2u;
    if(!fbw || dbx>=fbw*64u || dby>2047u || !h)return 0;
    if(w>fbw*64u-dbx)w=fbw*64u-dbx;
    if(w<168u || h<130u || h>1024u)return 0;
    sx=w>=336u?2u:1u;
    sy=h>=268u?2u:1u;
    x=dbx+(w-160u*sx)/2u;
    y=dby+(h-126u*sy)/2u;
    if(x+160u*sx>fbw*64u || y+126u*sy>2048u)
        return 0;
    last_byte=(uint64_t)bp*8192u+
              (uint64_t)(y+126u*sy)*fbw*64u*vram_bpp;
    if(last_byte>4ULL*1024u*1024u)return 0;
    out->width=w;out->height=h;out->sx=sx;out->sy=sy;
    out->x=x;out->y=y;out->fbw=fbw;out->fbbase=bp*32u;
    out->psm=psm;out->pixel_bytes=bpp;
    return 1;
}
#endif
