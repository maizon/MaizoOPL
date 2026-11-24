#include <assert.h>
#include "maizo_mage_rw_client.h"
mz_gs_shared_t *mzGsShared;
static void *last;
static unsigned sent, flushed;
static int send_packet(void *ctx,void *p,size_t n)
{
    (void)ctx; assert(n==48); last=p; sent++; return 9;
}
static void flush_packet(void *ctx,void *p,size_t n)
{
    (void)ctx; (void)p; (void)n; flushed++;
}
int main(void)
{
    mz_gs_shared_t shared={0};
    assert(mzMageRwFilter(1,MZ_GS_FORCE_ON,1,0,0)==2);
    assert(mzMageRwFilter(3,MZ_GS_FORCE_ON,1,1,0)==4);
    assert(mzMageRwFilter(5,MZ_GS_FORCE_ON,1,1,0)==6);
    assert(mzMageRwFilter(3,MZ_GS_FORCE_ON,1,0,1)==3);
    assert(mzMageRwFilter(3,MZ_GS_FORCE_ON,1,1,1)==6);
    assert(mzMageRwFilter(3,MZ_GS_TRILINEAR,1,1,0)==6);
    assert(mzMageRwFilter(1,MZ_GS_TRILINEAR,1,0,0)==2);
    assert(mzMageRwFilter(1,MZ_GS_FORCE_ON,0,1,1)==1);
    assert(mzMageRwFilter(6,MZ_GS_FORCE_OFF,1,1,0)==5);
    assert(mzMageDrawRoute(2,2,2,1)==MZ_MAGE_ENGINE);
    assert(mzMageDrawRoute(2,1,2,1)==MZ_MAGE_GIF);
    assert(mzMageDrawRoute(2,2,2,0)==MZ_MAGE_ORIGINAL);
    assert(mzMageRwLiveFilter(1,1,0)==1);
    mzGsShared=&shared;shared.enabled=1;
    assert(mzGsClientAttach(MZ_DRAW_BILINEAR));
    assert(mzGsRuntimeSetMode(MZ_DRAW_BILINEAR,MZ_GS_FORCE_ON));
    assert(mzMageRwLiveFilter(1,1,0)==2);
    assert(mzMageRwLiveFilter(1,0,0)==1);
    assert(mzGsRuntimeSetMode(MZ_DRAW_BILINEAR,MZ_GS_KEEP));
    assert(mzMageRwLiveFilter(1,1,0)==1);
    {
        uint8_t src[48] __attribute__((aligned(16)))={0};
        uint8_t dst[48] __attribute__((aligned(16)))={0};
        mz_gs_dma_guard_t g={MZ_GS_PATH3_GIF_DMA,1,1,0,0};
        assert(mzGsClientAttach(MZ_DRAW_AA1|MZ_DRAW_BILINEAR));
        assert(mzGsRuntimeSetMode(MZ_DRAW_AA1,MZ_GS_FORCE_ON));
        assert(mzGsRuntimeSetMode(MZ_DRAW_BILINEAR,MZ_GS_TRILINEAR));
        mzGifWrite64(src,2ULL|(1ULL<<15)|(1ULL<<60));
        mzGifWrite64(src+8,14);
        mzGifWrite64(src+16,3);mzGifWrite64(src+24,MZ_GS_PRIM);
        mzGifWrite64(src+32,(2ULL<<6)|(3ULL<<2));
        mzGifWrite64(src+40,MZ_GS_TEX1_1);
        /* Engine already handled textures: apply only AA1 via GIF. */
        assert(mzMageRwSendFallback(MZ_SUBMIT_PATH3_NORMAL,src,dst,48,48,
            &g,1,MZ_DRAW_BILINEAR,MZ_DRAW_AA1|MZ_DRAW_BILINEAR,
            send_packet,flush_packet,0)==9);
        assert(last==dst && sent==1 && flushed==1);
        assert(mzGifRead64(dst+16)==131);
        assert(mzGifRead64(dst+32)==mzGifRead64(src+32));
        /* Engine route absent: fallback to TEX1 on next submission. */
        assert(mzMageRwSendFallback(MZ_SUBMIT_PATH3_NORMAL,src,dst,48,48,
            &g,1,0,MZ_DRAW_AA1|MZ_DRAW_BILINEAR,
            send_packet,flush_packet,0)==9);
        assert(last==dst && sent==2 && flushed==2);
        assert(((mzGifRead64(dst+32)>>6)&7u)==5u);
        assert(mzGifRead64(src+16)==3 && ((mzGifRead64(src+32)>>6)&7u)==2u);
        /* Protected UI and original settings never alter the buffer. */
        assert(mzMageRwSendFallback(MZ_SUBMIT_PATH3_NORMAL,src,dst,48,48,
            &g,0,0,MZ_DRAW_ALL,send_packet,flush_packet,0)==9);
        assert(last==src && sent==3 && flushed==2);
        assert(mzGsRuntimeSetMode(MZ_DRAW_AA1,MZ_GS_KEEP));
        assert(mzGsRuntimeSetMode(MZ_DRAW_BILINEAR,MZ_GS_KEEP));
        assert(mzMageRwSendFallback(MZ_SUBMIT_PATH3_NORMAL,src,dst,48,48,
            &g,1,0,MZ_DRAW_ALL,send_packet,flush_packet,0)==9);
        assert(last==src && sent==4 && flushed==2);
    }
    return 0;
}
