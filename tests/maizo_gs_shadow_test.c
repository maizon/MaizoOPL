/* Shadow packet ownership guarantees for reversible per-game filtering. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "maizo_gs_shadow.h"

static void packet(uint8_t *p)
{
    const uint64_t tri=3ULL|(1ULL<<6);
    memset(p, 0, 64);
    mzGifWrite64(p, 1ULL|(1ULL<<15)|(1ULL<<60));
    mzGifWrite64(p+8, 0xeULL);
    mzGifWrite64(p+16, tri);
    mzGifWrite64(p+24, MZ_GS_PRIM);
}

int main(void)
{
    uint8_t original[64] __attribute__((aligned(16)));
    uint8_t scratch[64] __attribute__((aligned(16)));
    mz_gs_dma_guard_t guard={MZ_GS_PATH3_GIF_DMA,1,1,0,0};
    void *selected = NULL;
    const uint64_t tri=3ULL|(1ULL<<6);

    mzGsRuntimeReset();
    packet(original);
    assert(mzGsRuntimeStageShadow(MZ_SUBMIT_PATH3_NORMAL, original, scratch,
                                  sizeof(scratch),32,&guard,&selected)
           == MZ_GS_DMA_SKIPPED);
    assert(selected == original);
    assert(mzGsRuntimeAttach(MZ_DRAW_AA1));
    assert(mzGsRuntimeSetMode(MZ_DRAW_AA1,MZ_GS_FORCE_ON));

    packet(original);
    assert(mzGsRuntimeStageShadow(MZ_SUBMIT_PATH3_NORMAL, original, scratch,
                                  sizeof(scratch),32,&guard,&selected)
           == MZ_GS_DMA_OK);
    assert(selected == scratch);
    assert(mzGifRead64(original+16) == tri);
    assert(mzGifRead64(scratch+16) == (tri|128ULL));

    /* In-game GAME mode uses pristine original source, even after a
     * previous ON packet. No retroactive GS register restore is implied. */
    assert(mzGsRuntimeSetMode(MZ_DRAW_AA1,MZ_GS_KEEP));
    assert(mzGsRuntimeStageShadow(MZ_SUBMIT_PATH3_NORMAL, original, scratch,
                                  sizeof(scratch),32,&guard,&selected)
           == MZ_GS_DMA_SKIPPED);
    assert(selected == original);
    assert(mzGifRead64(original+16) == tri);

    /* Shared/overlapping memory must not corrupt original command stream. */
    assert(mzGsRuntimeSetMode(MZ_DRAW_AA1,MZ_GS_FORCE_ON));
    selected = NULL;
    assert(mzGsRuntimeStageShadow(MZ_SUBMIT_PATH3_NORMAL, original,
                                  original, sizeof(original),32,&guard,
                                  &selected) == MZ_GS_DMA_UNSUPPORTED);
    assert(selected == original);
    assert(mzGifRead64(original+16) == tri);

    /* Insufficient destination storage is a hard refusal, not an overwrite. */
    assert(mzGsRuntimeStageShadow(MZ_SUBMIT_PATH3_NORMAL, original, scratch,
                                  16,32,&guard,&selected)
           == MZ_GS_DMA_BAD_PACKET);
    assert(selected == original);
    assert(mzGifRead64(original+16) == tri);

    /* DMA active: no copying/patching or speculative new DMA address. */
    guard.channel_idle = 0;
    assert(mzGsRuntimeStageShadow(MZ_SUBMIT_PATH3_NORMAL, original, scratch,
                                  sizeof(scratch),32,&guard,&selected)
           == MZ_GS_DMA_SKIPPED);
    assert(selected == original);
    guard.channel_idle = 1;

    /* REF/REFE pointers require independent shadowing, not just copy
     * of the DMAtags. */
    assert(mzGsRuntimeStageShadow(MZ_SUBMIT_PATH3_TRUSTED, original, scratch,
                                  sizeof(scratch),32,&guard,&selected)
           == MZ_GS_DMA_UNSUPPORTED);
    assert(selected == original);
    puts("MAIZO GS shadow staging: original source preserved / safe fallback PASS");
    return 0;
}
