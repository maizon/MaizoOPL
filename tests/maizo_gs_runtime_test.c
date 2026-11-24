/* Host tests: IGR draw policy works ONLY for registered cooperative hooks. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "maizo_gs_runtime.h"

static void gif_primitive(uint8_t *buf, uint64_t prim)
{
    memset(buf, 0, 64);
    mzGifWrite64(buf, 1ULL | (1ULL<<15) | (1ULL<<60));
    mzGifWrite64(buf+8, 0xeULL);
    mzGifWrite64(buf+16, prim);
    mzGifWrite64(buf+24, MZ_GS_PRIM);
}

int main(void)
{
    uint8_t buf[64] __attribute__((aligned(16)));
    const uint64_t tri=3ULL|(1ULL<<6);
    mz_gs_dma_guard_t guard={MZ_GS_PATH3_GIF_DMA,1,1,0,0};

    mzGsRuntimeReset();
    assert(!mzGsRuntimeCaps());
    assert(!mzGsRuntimeSetMode(MZ_DRAW_AA1, MZ_GS_FORCE_ON));
    gif_primitive(buf,tri);
    assert(mzGsRuntimeSubmit(MZ_SUBMIT_PATH3_NORMAL,buf,32,&guard,0,0)
           ==MZ_GS_DMA_SKIPPED);
    assert(mzGifRead64(buf+16)==tri);
    assert(!mzGsRuntimeAttach(0));
    assert(!mzGsRuntimeAttach(16));
    assert(mzGsRuntimeAttach(MZ_DRAW_ALL));
    assert(!mzGsRuntimeSetMode(16,MZ_GS_FORCE_ON));
    assert(mzGsRuntimeSetMode(MZ_DRAW_SCANLINES,3));
    assert(mzGsRuntimeMode(MZ_DRAW_SCANLINES)==3);
    assert(!mzGsRuntimeSetMode(MZ_DRAW_SCANLINES,4));
    assert(mzGsRuntimeSetMode(MZ_DRAW_SCANLINES,0));
    assert(!mzGsRuntimeSetMode(MZ_DRAW_AA1,3));
    assert(mzGsRuntimeSetMode(MZ_DRAW_AA1,MZ_GS_FORCE_ON));
    assert(mzGsRuntimeMode(MZ_DRAW_AA1)==MZ_GS_FORCE_ON);
    assert(mzGsRuntimeSubmit(MZ_SUBMIT_PATH3_NORMAL,buf,32,&guard,0,0)==0);
    assert(mzGifRead64(buf+16)==(tri|128ULL));
    assert(mzGsRuntimeAccepted()==1);

    /* All other backends use the same policy only when selected. */
    guard.path=MZ_GS_PATH2_VIF1;
    memset(buf,0,sizeof(buf));
    {
        uint32_t cmd=(0x51u<<24)|2u;
        memcpy(buf+12,&cmd,4);
    }
    mzGifWrite64(buf+16,1ULL|(1ULL<<15)|(1ULL<<60));
    mzGifWrite64(buf+24,0xeULL);
    mzGifWrite64(buf+32,tri);
    mzGifWrite64(buf+40,MZ_GS_PRIM);
    assert(mzGsRuntimeSubmit(MZ_SUBMIT_PATH2_VIF1,buf,48,&guard,0,0)==0);
    assert(mzGifRead64(buf+32)==(tri|128ULL));

    /* Exercise the ACTUAL runtime PATH2 dispatcher with a mixed
     * VIF1 stream: STCYCL + UNPACK + MSCAL + DIRECT. The legacy
     * NOP+DIRECT-only backend cannot parse this legal layout. */
    memset(buf,0,sizeof(buf));
    {
        uint32_t cmd;
        cmd=(0x01u<<24)|0x0101u;
        memcpy(buf,&cmd,4);
        cmd=(0x6cu<<24)|(1u<<16)|0x03fcu;
        memcpy(buf+4,&cmd,4);
        /* 16 bytes of VU1 payload are intentionally opaque. */
        memset(buf+8,0xa5,16);
        cmd=(0x14u<<24)|0x0080u; /* MSCAL */
        memcpy(buf+24,&cmd,4);
        cmd=(0x50u<<24)|2u; /* DIRECT QWC2 */
        memcpy(buf+28,&cmd,4);
    }
    mzGifWrite64(buf+32,1ULL|(1ULL<<15)|(1ULL<<60));
    mzGifWrite64(buf+40,0xeULL);
    mzGifWrite64(buf+48,3ULL); /* opaque triangle, ABE=0 */
    mzGifWrite64(buf+56,MZ_GS_PRIM);
    assert(mzGsRuntimeSubmit(MZ_SUBMIT_PATH2_VIF1,buf,64,&guard,0,0)
           == MZ_GS_DMA_OK);
    assert(mzGifRead64(buf+48)==(3ULL|128ULL));
    for(int i=8;i<24;i++)assert(buf[i]==0xa5);
    assert(mzGsRuntimeAccepted()==3);

    guard.path=MZ_GS_PATH1_XGKICK;
    gif_primitive(buf,tri);
    assert(mzGsRuntimeSubmit(MZ_SUBMIT_PATH1_STAGING,buf,32,&guard,0,0)==0);
    assert(mzGifRead64(buf+16)==(tri|128ULL));

    /* GAME mode leaves newly submitted packets unchanged; no false
     * guarantee that earlier packets inside the GS were rolled back. */
    assert(mzGsRuntimeSetMode(MZ_DRAW_AA1,MZ_GS_KEEP));
    gif_primitive(buf,tri);
    guard.path=MZ_GS_PATH3_GIF_DMA;
    assert(mzGsRuntimeSubmit(MZ_SUBMIT_PATH3_NORMAL,buf,32,&guard,0,0)
           ==MZ_GS_DMA_SKIPPED);
    assert(mzGifRead64(buf+16)==tri);

    /* Three consecutive malformed owned buffers auto-disable the
     * registration; game rendering must continue without patching. */
    assert(mzGsRuntimeSetMode(MZ_DRAW_AA1,MZ_GS_FORCE_ON));
    memset(buf,0,sizeof(buf));
    mzGifWrite64(buf,10ULL|(1ULL<<15)|(1ULL<<60));
    mzGifWrite64(buf+8,0xeULL);
    for(int i=0;i<3;i++)
        assert(mzGsRuntimeSubmit(MZ_SUBMIT_PATH3_NORMAL,buf,16,&guard,0,0)
               ==MZ_GS_DMA_BAD_PACKET);
    assert(!mzGsRuntimeCaps());
    assert(mzGsRuntimeFailures() == 3u); /* failure telemetry survives detach */
    assert(mzGsRuntimeSubmit(MZ_SUBMIT_PATH3_NORMAL,buf,16,&guard,0,0)
           ==MZ_GS_DMA_SKIPPED);
    assert(!mzGsRuntimeAttach(MZ_DRAW_ALL)); /* no auto reattach after failure */
    mzGsRuntimeReset();
    assert(!mzGsRuntimeCaps() && mzGsRuntimeMode(MZ_DRAW_BILINEAR)==0);
    assert(mzGsRuntimeAttach(MZ_DRAW_BILINEAR | MZ_DRAW_SCANLINES));
    assert(mzGsRuntimeMode(MZ_DRAW_BILINEAR)==MZ_GS_KEEP);
    assert(mzGsRuntimeSetMode(MZ_DRAW_BILINEAR,MZ_GS_TRILINEAR));
    assert(mzGsRuntimeSetMode(MZ_DRAW_SCANLINES,2));
    assert(mzGsRuntimeAttach(MZ_DRAW_BILINEAR));
    assert(mzGsRuntimeMode(MZ_DRAW_BILINEAR)==MZ_GS_TRILINEAR);
    assert(mzGsRuntimeMode(MZ_DRAW_SCANLINES)==0);
    mzGsRuntimeReset();
    assert(mzGsRuntimeAttach(MZ_DRAW_ALL));
    assert(mzRuntimeModes==0);
    puts("MAIZO GS runtime: opt-in PATH1/PATH2/PATH3 policy and fallback PASS");
    return 0;
}
