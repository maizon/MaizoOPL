/*
 * MAIZO GS game draw filters — opt-in EE core bridge.
 *
 * IMPORTANT: API registration is a capability claim by a VERIFIED
 * per-game pre-DMA adapter. No automatic game hooks are installed here.
 * Unsupported games have zero capabilities and the IGR menu must not
 * show working AA1/texture/dither switches.
 */
#ifndef MAIZO_GS_RUNTIME_H
#define MAIZO_GS_RUNTIME_H
#include "maizo_gs_dma_preflight.h"

enum {
    MZ_DRAW_AA1 = 1,
    MZ_DRAW_BILINEAR = 2,
    MZ_DRAW_DITHER = 4,
    MZ_DRAW_ALL = 7
};

enum {
    MZ_SUBMIT_PATH3_NORMAL = 1,
    MZ_SUBMIT_PATH3_INLINE = 2,
    MZ_SUBMIT_PATH3_TRUSTED = 3,
    MZ_SUBMIT_PATH2_VIF1 = 4,
    MZ_SUBMIT_PATH1_STAGING = 5
};

void mzGsRuntimeReset(void);
int mzGsRuntimeAttach(unsigned int capabilities);
unsigned int mzGsRuntimeCaps(void);
unsigned int mzGsRuntimeMode(unsigned int feature);
int mzGsRuntimeSetMode(unsigned int feature, unsigned int mode);
/*
 * Keep the heavyweight GIF parser OUT of the tiny protected EE resident
 * core. The renderer-specific adapter compiles this inline dispatcher,
 * while the core only exports policy, state and fail-closed bookkeeping.
 */
void mzGsRuntimeGetPolicy(mz_gs_gif_policy_t *policy);
mz_gs_gif_state_t *mzGsRuntimeGetState(void);
mz_gs_dma_metrics_t *mzGsRuntimeGetMetrics(void);
void mzGsRuntimeRecord(int result);

static inline int mzGsRuntimeSubmit(unsigned int submit_type, void *packet,
                                    size_t bytes,
                                    const mz_gs_dma_guard_t *guard,
                                    const mz_gs_dma_owned_ref_t *refs,
                                    size_t ref_count)
{
    mz_gs_gif_policy_t policy;
    mz_gs_gif_state_t *state;
    mz_gs_dma_metrics_t *metrics;
    int result = MZ_GS_DMA_SKIPPED;

    if (!mzGsRuntimeCaps() || !guard)
        return MZ_GS_DMA_SKIPPED;
    mzGsRuntimeGetPolicy(&policy);
    if (!mzGsDmaPolicyEnabled(&policy))
        return MZ_GS_DMA_SKIPPED;

    state = mzGsRuntimeGetState();
    metrics = mzGsRuntimeGetMetrics();
    switch (submit_type) {
    case MZ_SUBMIT_PATH3_NORMAL:
        result = mzGsDmaPatchNormal(packet, bytes, &policy, guard,
                                    state, metrics);
        break;
    case MZ_SUBMIT_PATH3_INLINE:
        result = mzGsDmaPatchInlineChain(packet, bytes, &policy, guard,
                                         state, metrics);
        break;
    case MZ_SUBMIT_PATH3_TRUSTED:
        result = mzGsDmaPatchTrustedRefs(packet, bytes, refs, ref_count,
                                         &policy, guard, state, metrics);
        break;
    case MZ_SUBMIT_PATH2_VIF1:
        result = mzGsDmaPatchVif1Stream(packet, bytes, &policy, guard,
                                        state, metrics);
        break;
    case MZ_SUBMIT_PATH1_STAGING:
        result = mzGsDmaPatchVu1Staging(packet, bytes, &policy, guard,
                                         state, metrics);
        break;
    default:
        return MZ_GS_DMA_UNSUPPORTED;
    }
    mzGsRuntimeRecord(result);
    return result;
}
unsigned int mzGsRuntimeFailures(void);
unsigned int mzGsRuntimeAccepted(void);
#endif
