#include <assert.h>
#include <string.h>
#include "maizo_gs_client.h"
mz_gs_shared_t *mzGsShared;
static mz_gs_shared_t fake __attribute__((aligned(16)))={
 .magic0=MZ_GS_SHARED_MAGIC0,.magic1=MZ_GS_SHARED_MAGIC1,
 .abi=MZ_GS_SHARED_ABI,.bytes=sizeof(mz_gs_shared_t),.enabled=1};
static unsigned sent,flushed;static void *last;
static int send(void *ctx,void *source,size_t bytes){
 (void)ctx;assert(bytes==32||bytes==48);sent++;last=source;return 17;
}
static void flush(void *ctx,void *source,size_t bytes){
 (void)ctx;(void)source;(void)bytes;flushed++;
}
int main(void){
 uint8_t a[64] __attribute__((aligned(16)))={0},b[64] __attribute__((aligned(16)))={0};
 mz_gs_dma_guard_t g={MZ_GS_PATH3_GIF_DMA,1,1,0,0};
 assert(!mzGsClientBindRange(a,sizeof(a)));
 assert(mzGsClientBindRange(&fake,sizeof(fake)));
 assert(mzGsClientAttach(MZ_DRAW_ALL));
 assert(mzGsRuntimeSetMode(MZ_DRAW_AA1,MZ_GS_FORCE_ON));
 mzGifWrite64(a,1ULL|(1ULL<<15)|(1ULL<<60));mzGifWrite64(a+8,14);
 mzGifWrite64(a+16,3);mzGifWrite64(a+24,MZ_GS_PRIM);
 assert(mzGsClientSend(MZ_SUBMIT_PATH3_NORMAL,a,b,64,32,&g,1,send,flush,0)==17);
 assert(last==b&&sent==1&&flushed==1&&mzGifRead64(a+16)==3&&mzGifRead64(b+16)==131);
 assert(mzGsClientSend(MZ_SUBMIT_PATH3_NORMAL,a,b,64,32,&g,0,send,flush,0)==17);
 assert(last==a&&sent==2&&flushed==1);
 g.channel_idle=0;
 assert(mzGsClientSend(MZ_SUBMIT_PATH3_NORMAL,a,b,64,32,&g,1,send,flush,0)==17);
 assert(last==a&&sent==3&&flushed==1);
 g.channel_idle=1;g.path=MZ_GS_PATH1_XGKICK;
 assert(mzGsClientSend(MZ_SUBMIT_PATH1_STAGING,a,b,64,32,&g,1,send,flush,0)==17);
 assert(last==b&&sent==4&&flushed==2);
 memmove(a+16,a,32);memset(a,0,16);
 {uint32_t cmd=(0x50u<<24)|2;memcpy(a+12,&cmd,4);}
 g.path=MZ_GS_PATH2_VIF1;
 assert(mzGsClientSend(MZ_SUBMIT_PATH2_VIF1,a,b,64,48,&g,1,send,flush,0)==17);
 assert(last==b&&sent==5&&flushed==3&&mzGifRead64(b+32)==131);
 {
    mz_gs_register_shadow_t shadow={0};
    assert(!mzGsClientCheckpointRegister(0x100000,0x200000));
    assert(!mzGsClientCheckpointRegister(0x1c00040,MZ_GS_VRAM_BYTES));
    assert(mzGsClientCheckpointRegister(0x100000,MZ_GS_VRAM_BYTES));
    assert(!mzGsClientCheckpointRequested());
    fake.checkpoint.request=1;
    assert(mzGsClientCheckpointRequested());
    assert(!mzGsClientCheckpointPark(&shadow));
    for(unsigned i=0;i<4;i++)mzGsShadowObserve(&shadow,0x50+i,i);
    assert(mzGsClientCheckpointPark(&shadow));
    assert(mzGsCheckpointReady(&fake.checkpoint));
    assert(!mzGsClientCheckpointPark(&shadow));
    assert(mzGsPageBase(511)==16352);
    assert(MZ_GS_PAGE_COUNT*MZ_GS_PAGE_BYTES==MZ_GS_VRAM_BYTES);
    fake.checkpoint.fault=1;
    assert(!mzGsCheckpointReady(&fake.checkpoint)&&mzGsClientCheckpointFault());
    mzGsCheckpointRelease(&fake.checkpoint);
    assert(!mzGsClientCheckpointRequested());
    mzGsRuntimeReset();
    assert(!fake.checkpoint.registered&&!fake.checkpoint.fault);
 }
 fake.enabled=0;assert(!mzGsClientBindRange(&fake,sizeof(fake)));
 return 0;
}
