/*
 * MAIZO graphics policy bridge for cooperating, verified pre-DMA hooks.
 * No untrusted MMIO breakpoint, VU1 intercept or arbitrary game pointer.
 * Fail closed after repeated malformed or unsafe game packets.
 */
#include "maizo_gs_runtime.h"
#include <string.h>

static unsigned int mzCapabilities;
static volatile unsigned int mzModes;
static unsigned int mzFailures;
static unsigned int mzAccepted;
static unsigned int mzConsecutiveFailures;
static mz_gs_gif_state_t mzState;
static mz_gs_dma_metrics_t mzMetrics;

static unsigned int mzIndex(unsigned int feature)
{
    if (feature == MZ_DRAW_AA1) return 0;
    if (feature == MZ_DRAW_BILINEAR) return 2;
    if (feature == MZ_DRAW_DITHER) return 4;
    return 8;
}

void mzGsRuntimeReset(void)
{
    mzCapabilities = mzModes = mzFailures = mzAccepted = 0;
    mzConsecutiveFailures = 0;
    memset(&mzState, 0, sizeof(mzState));
    memset(&mzMetrics, 0, sizeof(mzMetrics));
}

int mzGsRuntimeAttach(unsigned int capabilities)
{
    /* Only a verified game-specific hook may register itself. The
     * absence of any registration leaves all draw features unavailable. */
    if (!capabilities || (capabilities & ~MZ_DRAW_ALL)) return 0;
    mzGsRuntimeReset();
    mzCapabilities = capabilities;
    return 1;
}

unsigned int mzGsRuntimeCaps(void)
{
    return mzCapabilities;
}

unsigned int mzGsRuntimeMode(unsigned int feature)
{
    unsigned int shift = mzIndex(feature);
    if (shift > 4 || !(mzCapabilities & feature)) return MZ_GS_KEEP;
    return (mzModes >> shift) & 3u;
}

int mzGsRuntimeSetMode(unsigned int feature, unsigned int mode)
{
    unsigned int shift = mzIndex(feature);
    unsigned int mask;
    if (shift > 4 || mode > MZ_GS_FORCE_OFF ||
        !(mzCapabilities & feature)) return 0;
    mask = 3u << shift;
    mzModes = (mzModes & ~mask) | (mode << shift);
    return 1;
}

unsigned int mzGsRuntimeFailures(void) { return mzFailures; }
unsigned int mzGsRuntimeAccepted(void) { return mzAccepted; }

int mzGsRuntimeSubmit(unsigned int submit_type, void *packet, size_t bytes,
                      const mz_gs_dma_guard_t *guard,
                      const mz_gs_dma_owned_ref_t *refs, size_t ref_count)
{
    mz_gs_gif_policy_t policy;
    int result = MZ_GS_DMA_SKIPPED;
    if (!mzCapabilities || !guard) return MZ_GS_DMA_SKIPPED;

    policy.aa1 = (mzCapabilities & MZ_DRAW_AA1) ?
                 (uint8_t)mzGsRuntimeMode(MZ_DRAW_AA1) : MZ_GS_KEEP;
    policy.bilinear = (mzCapabilities & MZ_DRAW_BILINEAR) ?
                      (uint8_t)mzGsRuntimeMode(MZ_DRAW_BILINEAR) : MZ_GS_KEEP;
    policy.dither = (mzCapabilities & MZ_DRAW_DITHER) ?
                    (uint8_t)mzGsRuntimeMode(MZ_DRAW_DITHER) : MZ_GS_KEEP;
    policy.scanmask = 0; /* Never enable destructive pseudo-scanlines. */
    if (!mzGsDmaPolicyEnabled(&policy)) return MZ_GS_DMA_SKIPPED;

    switch (submit_type) {
    case MZ_SUBMIT_PATH3_NORMAL:
        result = mzGsDmaPatchNormal(packet, bytes, &policy, guard,
                                    &mzState, &mzMetrics);
        break;
    case MZ_SUBMIT_PATH3_INLINE:
        result = mzGsDmaPatchInlineChain(packet, bytes, &policy, guard,
                                         &mzState, &mzMetrics);
        break;
    case MZ_SUBMIT_PATH3_TRUSTED:
        result = mzGsDmaPatchTrustedRefs(packet, bytes, refs, ref_count,
                                         &policy, guard, &mzState, &mzMetrics);
        break;
    case MZ_SUBMIT_PATH2_VIF1:
        result = mzGsDmaPatchVif1Stream(packet, bytes, &policy, guard,
                                        &mzState, &mzMetrics);
        break;
    case MZ_SUBMIT_PATH1_STAGING:
        result = mzGsDmaPatchVu1Staging(packet, bytes, &policy, guard,
                                         &mzState, &mzMetrics);
        break;
    default:
        return MZ_GS_DMA_UNSUPPORTED;
    }
    if (result == MZ_GS_DMA_OK) {
        mzAccepted++;
        mzConsecutiveFailures = 0;
    } else if (result < 0) {
        mzFailures++;
        /* A repeatedly incompatible command stream is safer left alone.
         * No side effects on unrelated games: only this opt-in state is reset. */
        if (++mzConsecutiveFailures >= 3u)
            mzGsRuntimeReset();
    }
    return result;
}
