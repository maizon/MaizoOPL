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

/*
 * Kept in EE-core: only compact policy + stats, NOT the per-packet
 * transformer. This avoids overflowing the ram84 protected reservation.
 */
void mzGsRuntimeGetPolicy(mz_gs_gif_policy_t *policy)
{
    if (!policy) return;
    policy->aa1 = (mzCapabilities & MZ_DRAW_AA1) ?
                  (uint8_t)mzGsRuntimeMode(MZ_DRAW_AA1) : MZ_GS_KEEP;
    policy->bilinear = (mzCapabilities & MZ_DRAW_BILINEAR) ?
                       (uint8_t)mzGsRuntimeMode(MZ_DRAW_BILINEAR) : MZ_GS_KEEP;
    policy->dither = (mzCapabilities & MZ_DRAW_DITHER) ?
                     (uint8_t)mzGsRuntimeMode(MZ_DRAW_DITHER) : MZ_GS_KEEP;
    policy->scanmask = MZ_GS_SCAN_UNCHANGED;
}

mz_gs_gif_state_t *mzGsRuntimeGetState(void) { return &mzState; }
mz_gs_dma_metrics_t *mzGsRuntimeGetMetrics(void) { return &mzMetrics; }

void mzGsRuntimeRecord(int result)
{
    if (result == MZ_GS_DMA_OK) {
        mzAccepted++;
        mzConsecutiveFailures = 0;
    } else if (result < 0) {
        mzFailures++;
        if (++mzConsecutiveFailures >= 3u)
            mzGsRuntimeReset();
    }
}
