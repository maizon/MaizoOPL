/* Optional RenderWare-specific client bridge. An audited hook supplies
 * the original filter mode and the actual classification of the pass.
 * Reuses the existing In-Game BILINEAR option, no new resident code.
 */
#ifndef MAIZO_MAGE_RW_CLIENT_H
#define MAIZO_MAGE_RW_CLIENT_H
#include "maizo_gs_client.h"
#include "maizo_mage_renderware.h"

static inline unsigned mzMageRwLiveFilter(unsigned original, int world_texture,
                                           int mipmaps_known)
{
    if (!mzGsShared || !mzGsShared->enabled)
        return original;
    return mzMageRwFilter(original, mzGsRuntimeMode(MZ_DRAW_BILINEAR),
                          world_texture, mipmaps_known, 0);
}

/* Per-pass fallback in actual live submission. The renderer must declare
 * the engine-handled features only AFTER the engine route succeeds.
 * Unknown/UI passes remain untouched; a feature handled by the engine
 * must never be patched again by GIF. Scratch lifetime is the caller's.
 * Forward exactly once even when no compatible fallback is available. */
static inline int mzMageRwSendFallback(
    unsigned type, const void *original, void *scratch,
    size_t capacity, size_t bytes, const mz_gs_dma_guard_t *guard,
    int eligible, unsigned int engine_handled, unsigned int gif_supported,
    mz_gs_submit_fn submit, mz_gs_flush_fn flush, void *ctx)
{
    void *selected = (void *)original;
    if (!submit || !original) return MZ_GS_DMA_BAD_PACKET;
    if (mzGsShared && mzGsShared->enabled && eligible && flush) {
        unsigned int mask = gif_supported & ~engine_handled & MZ_DRAW_ALL;
        if (mask)
            mzGsRuntimeStageShadowMasked(type, original, scratch, capacity,
                                          bytes, guard, &selected, mask);
    }
    if (selected != original) flush(ctx, selected, bytes);
    return submit(ctx, selected, bytes);
}
#endif
