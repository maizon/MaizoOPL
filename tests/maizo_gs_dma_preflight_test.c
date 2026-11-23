/* PATH3 controlled DMA buffer preflight; no universal gameplay hook. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "maizo_gs_dma_preflight.h"

static void gif_ad(uint8_t *p,unsigned int reg,uint64_t val)
{
    /* One packed A+D, one QW payload, complete EOP-tagged list. */
    mzGifWrite64(p,1ULL | (1ULL<<15) | (1ULL<<60));
    mzGifWrite64(p+8,0xeULL);
    mzGifWrite64(p+16,val);
    mzGifWrite64(p+24,reg);
}
int main(void)
{
    uint8_t data[256] __attribute__((aligned(16)));
    uint8_t snapshot[256];
    mz_gs_gif_policy_t policy = {0};
    mz_gs_gif_state_t state = {0};
    mz_gs_dma_metrics_t metrics = {0};
    mz_gs_dma_guard_t guard = {MZ_GS_PATH3_GIF_DMA,1,1,0,0};
    const uint64_t tri=3ULL|(1ULL<<6);

    gif_ad(data,MZ_GS_PRIM,tri);
    policy.aa1=MZ_GS_FORCE_ON;
    assert(mzGsDmaPatchNormal(data,32,&policy,&guard,&state,&metrics)==0);
    assert(mzGifRead64(data+16)==(tri|128ULL));
    assert(metrics.accepted==1 && metrics.edits.primitive_changes==1);

    gif_ad(data,MZ_GS_PRIM,tri);
    guard.path=MZ_GS_PATH2_VIF1;
    assert(mzGsDmaPatchNormal(data,32,&policy,&guard,&state,&metrics)==MZ_GS_DMA_SKIPPED);
    assert(mzGifRead64(data+16)==tri);
    guard.path=MZ_GS_PATH1_XGKICK;
    assert(mzGsDmaPatchNormal(data,32,&policy,&guard,&state,&metrics)==MZ_GS_DMA_SKIPPED);
    guard.path=MZ_GS_PATH3_GIF_DMA;guard.channel_idle=0;
    assert(mzGsDmaPatchNormal(data,32,&policy,&guard,&state,&metrics)==MZ_GS_DMA_SKIPPED);
    guard.channel_idle=1;guard.caller_owns_buffer=0;
    assert(mzGsDmaPatchNormal(data,32,&policy,&guard,&state,&metrics)==MZ_GS_DMA_SKIPPED);
    guard.caller_owns_buffer=1;
    assert(mzGsDmaPatchNormal(data+1,32,&policy,&guard,&state,&metrics)==MZ_GS_DMA_SKIPPED);
    assert(mzGifRead64(data+16)==tri);
    guard.source_is_scratchpad=1;
    assert(mzGsDmaPatchNormal(data,32,&policy,&guard,&state,&metrics)==MZ_GS_DMA_SKIPPED);
    guard.source_is_scratchpad=0;guard.chain_tag_transfer=1;
    assert(mzGsDmaPatchNormal(data,32,&policy,&guard,&state,&metrics)==MZ_GS_DMA_SKIPPED);
    guard.chain_tag_transfer=0;
    policy.aa1=0;
    assert(mzGsDmaPatchNormal(data,32,&policy,&guard,&state,&metrics)==MZ_GS_DMA_SKIPPED);
    assert(metrics.skipped==8);
    policy.aa1=MZ_GS_FORCE_ON;

    /* A self-contained CNT / END DMA chain is fully patchable. */
    memset(data,0,sizeof(data));
    mzGifWrite64(data,2ULL|(1ULL<<28)); /* CNT, QWC2 */
    gif_ad(data+16,MZ_GS_PRIM,tri);
    mzGifWrite64(data+48,2ULL|(7ULL<<28)); /* END, QWC2 */
    gif_ad(data+64,MZ_GS_PRIM,tri);
    assert(mzGsDmaPatchInlineChain(data,96,&policy,&guard,&state,&metrics)==0);
    assert(mzGifRead64(data+32)==(tri|128ULL));
    assert(mzGifRead64(data+80)==(tri|128ULL));
    assert(metrics.accepted==2);
    assert(metrics.edits.primitive_changes==3);

    /* Refuse all pointer-following REF/NEXT/CALL modes: no partial edits. */
    for(int type=0;type<8;type++) {
        if(type==1||type==7)continue;
        memset(data,0,sizeof(data));
        mzGifWrite64(data,2ULL|(1ULL<<28));
        gif_ad(data+16,MZ_GS_PRIM,tri);
        mzGifWrite64(data+48,2ULL|((uint64_t)type<<28));
        gif_ad(data+64,MZ_GS_PRIM,tri);
        memcpy(snapshot,data,96);
        assert(mzGsDmaPatchInlineChain(data,96,&policy,&guard,&state,&metrics)!=0);
        assert(memcmp(data,snapshot,96)==0);
    }
    /* Malformed second GIFtag: first stays unchanged. */
    memset(data,0,sizeof(data));
    mzGifWrite64(data,2ULL|(1ULL<<28));
    gif_ad(data+16,MZ_GS_PRIM,tri);
    mzGifWrite64(data+48,2ULL|(7ULL<<28));
    gif_ad(data+64,MZ_GS_PRIM,tri);
    mzGifWrite64(data+64,0ULL|(1ULL<<15)|(3ULL<<58));
    memcpy(snapshot,data,96);
    assert(mzGsDmaPatchInlineChain(data,96,&policy,&guard,&state,&metrics)!=0);
    assert(memcmp(data,snapshot,96)==0);

    /* A nonterminated CNT chain is rejected. */
    memset(data,0,sizeof(data));
    mzGifWrite64(data,2ULL|(1ULL<<28));
    gif_ad(data+16,MZ_GS_PRIM,tri);
    memcpy(snapshot,data,48);
    assert(mzGsDmaPatchInlineChain(data,48,&policy,&guard,&state,&metrics)!=0);
    assert(memcmp(data,snapshot,48)==0);

    puts("MAIZO GS DMA preflight: safe owned PATH3 normal/CNT/END, guard and atomicity PASS");
    return 0;
}
