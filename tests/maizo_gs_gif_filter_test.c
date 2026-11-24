/* Portable tests of PS2 GIF packet field editing. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "maizo_gs_gif_filter.h"
static uint64_t tag(unsigned int loops,int end,int pre,unsigned int prim,
                    unsigned int kind,unsigned int regs)
{
    return (uint64_t)loops|((uint64_t)end<<15)|
           ((uint64_t)pre<<46)|((uint64_t)prim<<47)|
           ((uint64_t)kind<<58)|((uint64_t)regs<<60);
}
static void put_ad(uint8_t *p,unsigned int reg,uint64_t value)
{
    mzGifWrite64(p,value);
    mzGifWrite64(p+8,reg);
}
static void init(mz_gs_gif_policy_t *p,mz_gs_gif_state_t *s,mz_gs_gif_stats_t *st)
{
    memset(p,0,sizeof(*p));memset(s,0,sizeof(*s));memset(st,0,sizeof(*st));
}
static void one(uint8_t *buf,unsigned int reg,uint64_t v,
                mz_gs_gif_policy_t *p,mz_gs_gif_state_t *s,mz_gs_gif_stats_t *stats)
{
    memset(buf,0,32);
    mzGifWrite64(buf,tag(1,1,0,0,0,1));
    mzGifWrite64(buf+8,0xe);
    put_ad(buf+16,reg,v);
    assert(mzGifApplyFilters(buf,32,p,s,stats)==0);
}
int main(void)
{
    uint8_t buf[64],before[64];
    mz_gs_gif_policy_t p;
    mz_gs_gif_state_t s;
    mz_gs_gif_stats_t st;
    uint64_t tri=3ULL|(1ULL<<6);
    init(&p,&s,&st);p.aa1=MZ_GS_FORCE_ON;
    memset(buf,0,sizeof(buf));
    mzGifWrite64(buf,tag(0,1,1,(unsigned int)tri,0,1));
    assert(mzGifApplyFilters(buf,16,&p,&s,&st)==0);
    assert(((mzGifRead64(buf)>>47)&0x7ffULL)==(tri|128ULL));
    assert(st.primitive_changes==1);
    /* Real opaque game geometry normally clears ABE. AA1 must still
     * apply, without changing ABE/blending or primitive type. */
    mzGifWrite64(buf,tag(0,1,1,3u,0,1));
    assert(mzGifApplyFilters(buf,16,&p,&s,&st)==0);
    assert(((mzGifRead64(buf)>>47)&0x7ffULL)==(3ULL|128ULL));
    assert(!(((mzGifRead64(buf)>>47)&0x7ffULL)&(1ULL<<6)));
    one(buf,MZ_GS_PRIM,3ULL,&p,&s,&st);
    assert(mzGifRead64(buf+16)==(3ULL|128ULL));
    one(buf,MZ_GS_PRMODE,0ULL,&p,&s,&st);
    assert(mzGifRead64(buf+16)==128ULL);
    /* PRMODE is commonly uploaded before any primitive. The AA1 mode
     * must already be present when the game later chooses PRMODECONT=0. */
    init(&p,&s,&st);
    p.aa1=MZ_GS_FORCE_ON;
    one(buf,MZ_GS_PRMODE,0ULL,&p,&s,&st);
    assert(mzGifRead64(buf+16)==(1ULL<<7));
    assert(st.primitive_changes==1 && !st.unsupported_primitives);
    one(buf,MZ_GS_PRMODECONT,0,&p,&s,&st);
    one(buf,MZ_GS_PRIM,3ULL,&p,&s,&st);
    assert(mzGifRead64(buf+16)==(3ULL|128ULL));
    one(buf,MZ_GS_PRMODE,1ULL<<9,&p,&s,&st);
    assert(mzGifRead64(buf+16)==((1ULL<<9)|(1ULL<<7)));
    assert(!(mzGifRead64(buf+16)&(1ULL<<6))); /* ABE preserved */

    mzGifWrite64(buf,tag(0,1,1,(unsigned int)(6ULL|(1ULL<<6)),0,1));
    assert(mzGifApplyFilters(buf,16,&p,&s,&st)==0);
    assert(st.unsupported_primitives==1); /* sprite may not use AA1 */
    one(buf,MZ_GS_PRIM,tri,&p,&s,&st);
    assert(mzGifRead64(buf+16)==(tri|128ULL));
    one(buf,MZ_GS_PRMODE,tri,&p,&s,&st);
    assert(mzGifRead64(buf+16)==(tri|128ULL));
    p.aa1=MZ_GS_FORCE_OFF;
    one(buf,MZ_GS_PRIM,tri|128ULL,&p,&s,&st);
    assert(mzGifRead64(buf+16)==tri);

    init(&p,&s,&st);p.bilinear=MZ_GS_FORCE_ON;
    one(buf,MZ_GS_TEX1_1,2ULL<<6,&p,&s,&st);
    assert(((mzGifRead64(buf+16)>>6)&7ULL)==4ULL);
    assert(mzGifRead64(buf+16)&(1ULL<<5));
    p.bilinear=MZ_GS_FORCE_OFF;
    one(buf,MZ_GS_TEX1_2,mzGifRead64(buf+16),&p,&s,&st);
    assert(((mzGifRead64(buf+16)>>6)&7ULL)==2ULL);
    assert(!(mzGifRead64(buf+16)&(1ULL<<5)));

    init(&p,&s,&st);p.bilinear=MZ_GS_TRILINEAR;
    one(buf,MZ_GS_TEX1_1,(2ULL<<6)|(3ULL<<2),&p,&s,&st);
    assert(((mzGifRead64(buf+16)>>6)&7ULL)==5ULL);
    assert(((mzGifRead64(buf+16)>>2)&7ULL)==3ULL);
    one(buf,MZ_GS_TEX1_2,0,&p,&s,&st);
    assert(((mzGifRead64(buf+16)>>6)&7ULL)==1ULL);
    one(buf,MZ_GS_TEX1_2,6ULL<<6,&p,&s,&st);
    assert(mzGifRead64(buf+16)==(6ULL<<6));
    /* LOD independently preserves original sampling and non-K bits. */
    init(&p,&s,&st);p.bilinear=4;
    {
        uint64_t t=(3ULL<<2)|(1ULL<<5)|(2ULL<<6)|(0xff0ULL<<32);
        one(buf,MZ_GS_TEX1_1,t,&p,&s,&st);
        assert(mzGifRead64(buf+16)==((t&~(4095ULL<<32))|(0xfe8ULL<<32)));
        p.bilinear=8;
        one(buf,MZ_GS_TEX1_2,t,&p,&s,&st);
        assert(mzGifRead64(buf+16)==((t&~(4095ULL<<32))|(0xff8ULL<<32)));
        one(buf,MZ_GS_TEX1_2,t|1ULL,&p,&s,&st);
        assert(mzGifRead64(buf+16)==(t|1ULL));
        one(buf,MZ_GS_TEX1_1,t&~(7ULL<<2),&p,&s,&st);
        assert(mzGifRead64(buf+16)==(t&~(7ULL<<2)));
    }
    init(&p,&s,&st);p.dither=MZ_GS_FORCE_ON;
    one(buf,MZ_GS_DTHE,0,&p,&s,&st);
    assert(mzGifRead64(buf+16)==0&&st.protected_dither==1);
    one(buf,MZ_GS_FRAME_1,2ULL<<24,&p,&s,&st);
    one(buf,MZ_GS_DTHE,0,&p,&s,&st);
    assert(mzGifRead64(buf+16)==0);
    one(buf,MZ_GS_DIMX,0x12,&p,&s,&st);
    one(buf,MZ_GS_PRIM,tri,&p,&s,&st); /* Context 0 is now known. */
    one(buf,MZ_GS_DTHE,0,&p,&s,&st);
    assert(mzGifRead64(buf+16)==1);
    p.dither=MZ_GS_FORCE_OFF;
    one(buf,MZ_GS_DTHE,1,&p,&s,&st);
    assert(mzGifRead64(buf+16)==0);

    init(&p,&s,&st);p.scanmask=MZ_GS_SCAN_EVEN_ONLY;
    one(buf,MZ_GS_SCANMSK,0,&p,&s,&st);
    assert(mzGifRead64(buf+16)==3);
    init(&p,&s,&st);p.aa1=MZ_GS_FORCE_ON;
    memset(buf,0,sizeof(buf));
    mzGifWrite64(buf,tag(1,1,0,0,1,1));
    mzGifWrite64(buf+16,tri);
    assert(mzGifApplyFilters(buf,32,&p,&s,&st)==0);
    assert(mzGifRead64(buf+16)==(tri|128ULL));
    memset(buf,0,sizeof(buf));mzGifWrite64(buf,tag(1,1,0,0,2,1));
    mzGifWrite64(buf+16,tri);memcpy(before,buf,32);
    assert(mzGifApplyFilters(buf,32,&p,&s,&st)==0);
    assert(!memcmp(before,buf,32));
    memset(buf,0,sizeof(buf));
    mzGifWrite64(buf,tag(0,0,1,(unsigned int)tri,0,1));
    mzGifWrite64(buf+16,tag(2,1,0,0,0,1));
    memcpy(before,buf,32);
    assert(mzGifApplyFilters(buf,32,&p,&s,&st)==-1);
    assert(!memcmp(before,buf,32)); /* no partially patched packets */
    p.aa1=3;
    assert(mzGifApplyFilters(buf,32,&p,&s,&st)==-2);
    assert(!memcmp(before,buf,32));
    init(&p,&s,&st);
    /* IMAGE2 (FLG=3) is an opaque 16-byte/loop image transfer,
     * identical in length to IMAGE (FLG=2). Never edit pixel bytes. */
    memset(buf,0,sizeof(buf));
    mzGifWrite64(buf,tag(2,1,0,0,3,1));
    memset(buf+16,0x5a,32);
    memcpy(before,buf,48);
    assert(mzGifApplyFilters(buf,48,&p,&s,&st)==0);
    assert(!memcmp(buf,before,48));
    /* Invalid IMAGE2 length must be rejected without mutation. */
    mzGifWrite64(buf,tag(3,1,0,0,3,1));
    memcpy(before,buf,48);
    assert(mzGifApplyFilters(buf,48,&p,&s,&st)==-1);
    assert(!memcmp(buf,before,48));
    /* A normal GIF DMA packet can contain multiple EOP-tagged lists. */
    p.aa1=MZ_GS_FORCE_ON;
    memset(buf,0,sizeof(buf));
    for(int k=0;k<2;k++) {
        mzGifWrite64(buf+k*32,tag(1,1,0,0,0,1));
        mzGifWrite64(buf+k*32+8,0xe);
        put_ad(buf+k*32+16,MZ_GS_PRIM,tri);
    }
    assert(mzGifApplyFilters(buf,64,&p,&s,&st)==0);
    assert(mzGifRead64(buf+16)==(tri|128ULL));
    assert(mzGifRead64(buf+48)==(tri|128ULL));
    /* Context-specific dithering must not affect a 32-bit framebuffer
       just because the other context happens to use 16-bit. */
    init(&p,&s,&st);
    p.dither=MZ_GS_FORCE_ON;
    one(buf,MZ_GS_FRAME_1,2ULL<<24,&p,&s,&st);
    one(buf,MZ_GS_FRAME_2,0ULL<<24,&p,&s,&st);
    one(buf,MZ_GS_DIMX,0x12,&p,&s,&st);
    one(buf,MZ_GS_PRIM,tri|(1ULL<<9),&p,&s,&st);
    one(buf,MZ_GS_DTHE,0,&p,&s,&st);
    assert(mzGifRead64(buf+16)==0 && st.protected_dither==1);
    one(buf,MZ_GS_PRIM,tri,&p,&s,&st);
    one(buf,MZ_GS_DTHE,0,&p,&s,&st);
    assert(mzGifRead64(buf+16)==1);
    /* PRMODECONT determines which drawing context is active. A PRMODE
     * write must not override PRIM when AC=1, or vice versa. */
    init(&p,&s,&st);
    p.dither=MZ_GS_FORCE_ON;
    one(buf,MZ_GS_FRAME_1,2ULL<<24,&p,&s,&st);
    one(buf,MZ_GS_FRAME_2,0,&p,&s,&st);
    one(buf,MZ_GS_DIMX,0x12,&p,&s,&st);
    one(buf,MZ_GS_PRIM,tri,&p,&s,&st);  /* 16-bit context 0 */
    one(buf,MZ_GS_PRMODE,tri|(1ULL<<9),&p,&s,&st); /* 32-bit context 1 */
    one(buf,MZ_GS_PRMODECONT,1,&p,&s,&st);
    one(buf,MZ_GS_DTHE,0,&p,&s,&st);
    assert(mzGifRead64(buf+16)==1);
    one(buf,MZ_GS_PRMODECONT,0,&p,&s,&st);
    one(buf,MZ_GS_DTHE,0,&p,&s,&st);
    assert(mzGifRead64(buf+16)==0);
    one(buf,MZ_GS_PRMODECONT,1,&p,&s,&st);
    one(buf,MZ_GS_DTHE,0,&p,&s,&st);
    assert(mzGifRead64(buf+16)==1);

    init(&p,&s,&st);
    assert(mzGifApplyFilters(buf,17,&p,&s,&st)==-1);
    assert(mzGifApplyFilters(NULL,32,&p,&s,&st)==-1);
    assert(mzGifApplyFilters(buf,1u<<21,&p,&s,&st)==-1);
    puts("MAIZO GIF filter packet tests: PASS");
    return 0;
}
