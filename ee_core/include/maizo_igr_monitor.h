/*
 * MAIZO In-Game independent framebuffer discovery.
 *
 * Active output state must come from a recorder observing real GS writes,
 * not from guessing memory offsets or reading write-only GS registers.
 * Conservative: one circuit or two proven same-image scanout circuits,
 * known color PSM, in-range GS VRAM. Dual independent images fail closed.
 * No allocations, hardware access or GSM video forcing in this module.
 */
#ifndef MAIZO_IGR_MONITOR_H
#define MAIZO_IGR_MONITOR_H
#include <stdint.h>
#if defined(__mips__)
/* The EE core has an 8 KiB stack guard: do not inline the same 64-bit
 * framebuffer validator six times in its resident text section. */
#define MZ_IGR_FN static __attribute__((noinline))
#else
#define MZ_IGR_FN static inline
#endif
typedef struct {
    uint32_t width, height, sx, sy, x, y, fbw, fbbase;
    uint32_t psm, pixel_bytes;
} mz_igr_frame_t;

MZ_IGR_FN int mzIgrMonitorFrame(mz_igr_frame_t *out,
                                    uint64_t pmode,
                                    uint64_t fb1, uint64_t fb2,
                                    uint64_t display1, uint64_t display2,
                                    uint64_t smode2, int legacy_gsm_height)
{
    uint64_t fb,display, ymask, hmask;
    uint32_t last_byte;
    uint32_t en,bpp,vram_bpp,fbw,bp,psm,dbx,dby,w,h,magh,magv,sx,sy,x,y;
    if(!out)return 0;
    en=(uint32_t)(pmode&3ULL);
    /* A common GS deflicker configuration uses BOTH read circuits
     * with identical FB/format and a 0/1-line vertical offset.
     * Show the panel in the shared image. Reject unrelated mixed
     * framebuffers to avoid drawing into another game's surface. */
    if(en==3u){
        uint32_t y1,y2,h1,h2;
        ymask=(uint64_t)2047<<43;
        hmask=(uint64_t)2047<<44;
        y1=(uint32_t)((fb1>>43)&2047ULL);
        y2=(uint32_t)((fb2>>43)&2047ULL);
        h1=(uint32_t)((display1>>44)&2047ULL);
        h2=(uint32_t)((display2>>44)&2047ULL);
        if(!fb1||!fb2||!display1||!display2||
           (fb1&~ymask)!=(fb2&~ymask)||
           (display1&~hmask)!=(display2&~hmask)||
           (y1>y2?y1-y2:y2-y1)>1u||
           (h1>h2?h1-h2:h2-h1)>1u)
            return 0;
        en=y1<=y2?1u:2u;
    }
    if(en!=1u && en!=2u)return 0;
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
    /* DISPLAY.DH is the *output* field raster. Interlaced FRAME
     * scanout commonly reports 448/480 output lines while its VRAM
     * buffer is 224/240: halve only those full-height readouts.
     * Some games already program DH=224; never halve it to 112. */
    (void)legacy_gsm_height;
    if((smode2&3ULL)==3ULL && h>=260u)h/=2u;
    if(!fbw || dbx>=fbw*64u || dby>2047u || !h)return 0;
    if(w>fbw*64u-dbx)w=fbw*64u-dbx;
    if(w<168u || h<130u || h>1024u)return 0;
    sx=w>=336u?2u:1u;
    sy=h>=268u?2u:1u;
    x=dbx+(w-160u*sx)/2u;
    y=dby+(h-126u*sy)/2u;
    if(x+160u*sx>fbw*64u || y+126u*sy>2048u)
        return 0;
    last_byte=bp*8192u+(y+126u*sy)*fbw*64u*vram_bpp;
    if(last_byte>4u*1024u*1024u)return 0;
    out->width=w;out->height=h;out->sx=sx;out->sy=sy;
    out->x=x;out->y=y;out->fbw=fbw;out->fbbase=bp*32u;
    out->psm=psm;out->pixel_bytes=bpp;
    return 1;
}
/* Mixed PMODE may blend two independently backed GS read circuits.
 * Accept only a currently enabled and individually VRAM-validated
 * display framebuffer. No guessed display layout or video forcing. */
MZ_IGR_FN int mzIgrResolveFrame(mz_igr_frame_t *out,
                                    uint64_t pm,
                                    uint64_t fb1,uint64_t fb2,
                                    uint64_t d1,uint64_t d2,
                                    uint64_t sm,int legacy_gsm_height)
{
    if(mzIgrMonitorFrame(out,pm,fb1,fb2,d1,d2,sm,legacy_gsm_height))
        return 1;
    if((pm&3ULL)!=3ULL)return 0;
    /* When mixing distinct surfaces with a FIXED ALP value, an
     * ALP below 0x80 means circuit 2 dominates the displayed pixel.
     * Drawing only on circuit 1 could look like a no-op. Never
     * guess: both candidate readouts undergo full bounds checks. */
    if(!(pm&0x80ULL) && (pm&0x20ULL) &&
       ((pm>>8)&0xffULL)<128ULL){
        return mzIgrMonitorFrame(out,(pm&~3ULL)|2ULL,fb1,fb2,d1,d2,sm,legacy_gsm_height) ||
               mzIgrMonitorFrame(out,(pm&~3ULL)|1ULL,fb1,fb2,d1,d2,sm,legacy_gsm_height);
    }
    return mzIgrMonitorFrame(out,(pm&~3ULL)|1ULL,fb1,fb2,d1,d2,sm,legacy_gsm_height) ||
           mzIgrMonitorFrame(out,(pm&~3ULL)|2ULL,fb1,fb2,d1,d2,sm,legacy_gsm_height);
}
/* Negative steps crop the output; positive steps use the GS integer
 * magnifier without increasing the number of source pixels read.
 * CRT timing and framebuffer addressing remain unchanged. */
MZ_IGR_FN int mzNativeDisplayEdit(uint64_t base,int cropX,int cropY,
                                      int offsetX,int offsetY,uint64_t *out)
{
    int dx=base&4095u,dy=(base>>12)&2047u;
    int dw=((base>>32)&4095u)+1,dh=((base>>44)&2047u)+1;
    int mh=((base>>23)&15u)+1,mv=((base>>27)&3u)+1;
    int nw=dw,nh=dh,newH=mh,newV=mv;
    const uint64_t mask=4095ULL|(2047ULL<<12)|(63ULL<<23)|
                        (4095ULL<<32)|(2047ULL<<44);
    if(!out||!base||cropX>2||cropX< -32||cropY>2||cropY< -32||
       offsetX< -32||offsetX>31||offsetY< -32||offsetY>31)return 0;
    if(cropX>0){newH+=cropX;nw=(dw/mh)*newH;}
    else nw+=cropX*mh;
    if(cropY>0){newV+=cropY;nh=(dh/mv)*newV;}
    else nh+=cropY*mv;
    if(newH>16||newV>4||nw<168*newH||nh<130*newV||
       nw>4096||nh>2048)return 0;
    dx+=offsetX*mh+(dw-nw)/2;dy+=offsetY*mv+(dh-nh)/2;
    if(dx<0||dx>4095||dy<0||dy>2047||
       dx+nw>4096||dy+nh>2048)return 0;
    *out=(base&~mask)|(uint64_t)dx|((uint64_t)dy<<12)|
         ((uint64_t)(newH-1)<<23)|((uint64_t)(newV-1)<<27)|
         ((uint64_t)(nw-1)<<32)|((uint64_t)(nh-1)<<44);
    return 1;
}
#endif
