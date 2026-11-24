/* True post-render scanlines for a COOPERATING renderer.
 * Draw one alpha-blended black sprite on alternate rows of an already
 * completed full frame. The original game draw packets are not masked.
 * No additional framebuffer; exactly one small GIF packet. Caller owns
 * the draw target, knows all touched GS state, and submits once before
 * display swap, after all earlier PATH1/2/3 drawing has completed.
 * No interlaced FIELD-only or 16-bit targets until validated.
 */
#ifndef MAIZO_GS_SCANLINES_H
#define MAIZO_GS_SCANLINES_H
#include "maizo_gs_gif_filter.h"

#define MZ_SCANLINE_QWC 31u
typedef struct {
    /* Exact state of the selected GS context and global registers. */
    uint64_t frame,zbuf,test,alpha,xyoffset,scissor,fba;
    uint64_t prmodecont,prim,rgbaq,scanmsk,pabe,colclamp,dthe;
    unsigned width,height,context;
    unsigned complete_frame,exclusive_frame_end;
} mz_scanline_frame_t;
static inline void mzScanlineAD(uint64_t *packet,unsigned *at,uint64_t val,unsigned reg)
{
    packet[*at*2]=val;packet[*at*2+1]=reg;(*at)++;
}
/* Strength 1..3 is 25%,50%,75% darkening of odd rows. The packet
 * restores all modified register state, including destination alpha
 * write mask and disabled Z writes. No interrupted primitive allowed. */
static inline unsigned mzScanlineBuild(uint64_t *packet,size_t capacity,
                                       const mz_scanline_frame_t *f,unsigned strength)
{
    unsigned at=1,c,fbp,fbw,psm,pages;
    uint64_t frame,scissor,xy1;
    if(!packet||((uintptr_t)packet&15u)||capacity<MZ_SCANLINE_QWC*16u||
       !f||!f->complete_frame||!f->exclusive_frame_end||!strength||strength>3||
       f->context>1u||f->width<2u||f->width>720u||f->height<2u||f->height>576u)return 0;
    fbp=f->frame&511u;fbw=(f->frame>>16)&63u;psm=(f->frame>>24)&63u;
    if((psm!=0u&&psm!=1u)||!fbw||f->width>fbw*64u)return 0;
    pages=fbw*((f->height+31u)/32u);
    if(fbp+pages>512u)return 0;
    c=f->context;
    /* Preserve destination alpha; overlay intentionally modifies RGB. */
    frame=(f->frame&0xffffffffULL)|0xff00000000000000ULL;
    scissor=((uint64_t)(f->width-1u)<<16)|((uint64_t)(f->height-1u)<<48);
    xy1=(uint64_t)(f->width*16u)|((uint64_t)(f->height*16u)<<16);
    mzScanlineAD(packet,&at,frame,0x4c+c);
    mzScanlineAD(packet,&at,f->zbuf|(1ULL<<32),0x4e + c);
    mzScanlineAD(packet,&at,0x30000,0x47+c); /* always pass depth; no alpha test */
    mzScanlineAD(packet,&at,0x64ULL|((uint64_t)(strength*32u)<<32),0x42+c);
    mzScanlineAD(packet,&at,0,0x18+c);
    mzScanlineAD(packet,&at,scissor,0x40+c);
    mzScanlineAD(packet,&at,0,0x4a+c);
    mzScanlineAD(packet,&at,0,0x49);
    mzScanlineAD(packet,&at,1,0x46);
    mzScanlineAD(packet,&at,0,0x45);
    mzScanlineAD(packet,&at,MZ_GS_SCAN_ODD_ONLY,0x22);
    mzScanlineAD(packet,&at,1,0x1a);
    mzScanlineAD(packet,&at,6u|64u|(c<<9),0x00);
    mzScanlineAD(packet,&at,0x3f80000080000000ULL,0x01);
    mzScanlineAD(packet,&at,0,0x05);
    mzScanlineAD(packet,&at,xy1,0x05);
    /* 14 touched GS state registers: restore in same command stream.
     * No primitive is kicked by restoration. */
    mzScanlineAD(packet,&at,f->frame,0x4c+c);
    mzScanlineAD(packet,&at,f->zbuf,0x4e + c);
    mzScanlineAD(packet,&at,f->test,0x47+c);
    mzScanlineAD(packet,&at,f->alpha,0x42+c);
    mzScanlineAD(packet,&at,f->xyoffset,0x18+c);
    mzScanlineAD(packet,&at,f->scissor,0x40+c);
    mzScanlineAD(packet,&at,f->fba,0x4a+c);
    mzScanlineAD(packet,&at,f->pabe,0x49);
    mzScanlineAD(packet,&at,f->colclamp,0x46);
    mzScanlineAD(packet,&at,f->dthe,0x45);
    mzScanlineAD(packet,&at,f->scanmsk,0x22);
    mzScanlineAD(packet,&at,f->rgbaq,0x01);
    mzScanlineAD(packet,&at,f->prim,0x00);
    mzScanlineAD(packet,&at,f->prmodecont,0x1a);
    packet[0]=(at-1u)|(1ULL<<15)|(1ULL<<60);
    packet[1]=0xe;
    return at;
}
#endif
