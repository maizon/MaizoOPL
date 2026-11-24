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
#include "maizo_gs_vif1_mixed.h"

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

/*
 * A versioned shared mailbox lives in the protected EE-core region.
 * The parser is instantiated in a cooperating game/renderer, not the
 * resident EE core. Games without a verified integration never attach.
 */
#define MZ_GS_SHARED_MAGIC0 0x4d5a4753u
#define MZ_GS_SHARED_MAGIC1 0x92a5b8acu
#define MZ_GS_SHARED_ABI 1u
typedef struct {
    uint32_t magic0, magic1, abi, bytes;
    volatile unsigned int enabled, capabilities, modes;
    unsigned int failures, accepted, consecutive_failures;
    mz_gs_gif_state_t state;
    mz_gs_dma_metrics_t metrics;
} mz_gs_shared_t;
#ifdef MZ_GS_CLIENT
extern mz_gs_shared_t *mzGsShared;
#else
extern mz_gs_shared_t mzGsResident;
#define mzGsShared (&mzGsResident)
#endif
#define mzRuntimeCapabilities (mzGsShared->capabilities)
#define mzRuntimeModes (mzGsShared->modes)
#define mzRuntimeFailures (mzGsShared->failures)
#define mzRuntimeAccepted (mzGsShared->accepted)
#define mzRuntimeConsecutiveFailures (mzGsShared->consecutive_failures)
#define mzRuntimeState (mzGsShared->state)
#define mzRuntimeMetrics (mzGsShared->metrics)
#ifndef MZ_GS_CLIENT
static inline void mzGsRuntimePublish(int enabled)
{
    mzGsResident.enabled=!!enabled;
}
#endif

static inline unsigned int mzGsRuntimeCaps(void)
{
    return mzRuntimeCapabilities;
}

static inline unsigned int mzGsRuntimeShift(unsigned int feature)
{
    return feature == MZ_DRAW_AA1 ? 0u :
           feature == MZ_DRAW_BILINEAR ? 2u :
           feature == MZ_DRAW_DITHER ? 4u : 8u;
}

static inline void mzGsRuntimeReset(void)
{
    mzRuntimeCapabilities = 0;
    mzRuntimeModes = 0;
    mzRuntimeFailures = 0;
    mzRuntimeAccepted = 0;
    mzRuntimeConsecutiveFailures = 0;
    memset(&mzRuntimeState, 0, sizeof(mzRuntimeState));
    memset(&mzRuntimeMetrics, 0, sizeof(mzRuntimeMetrics));
}

static inline int mzGsRuntimeAttach(unsigned int capabilities)
{
    if (!capabilities || (capabilities & ~MZ_DRAW_ALL)) return 0;
    mzGsRuntimeReset();
    mzRuntimeCapabilities = capabilities;
    return 1;
}

static inline unsigned int mzGsRuntimeMode(unsigned int feature)
{
    unsigned int shift = mzGsRuntimeShift(feature);
    if (shift > 4 || !(mzRuntimeCapabilities & feature))
        return MZ_GS_KEEP;
    return (mzRuntimeModes >> shift) & 3u;
}

static inline int mzGsRuntimeSetMode(unsigned int feature, unsigned int mode)
{
    unsigned int shift = mzGsRuntimeShift(feature);
    unsigned int mask;
    if (shift > 4 || mode > MZ_GS_FORCE_OFF ||
        !(mzRuntimeCapabilities & feature)) return 0;
    mask = 3u << shift;
    mzRuntimeModes = (mzRuntimeModes & ~mask) | (mode << shift);
    return 1;
}

static inline void mzGsRuntimeGetPolicy(mz_gs_gif_policy_t *policy)
{
    if (!policy) return;
    policy->aa1 = (uint8_t)mzGsRuntimeMode(MZ_DRAW_AA1);
    policy->bilinear = (uint8_t)mzGsRuntimeMode(MZ_DRAW_BILINEAR);
    policy->dither = (uint8_t)mzGsRuntimeMode(MZ_DRAW_DITHER);
    policy->scanmask = MZ_GS_SCAN_UNCHANGED;
}

static inline mz_gs_gif_state_t *mzGsRuntimeGetState(void)
{
    return &mzRuntimeState;
}
static inline mz_gs_dma_metrics_t *mzGsRuntimeGetMetrics(void)
{
    return &mzRuntimeMetrics;
}

static inline void mzGsRuntimeRecord(int result)
{
    if (result == MZ_GS_DMA_OK) {
        mzRuntimeAccepted++;
        mzRuntimeConsecutiveFailures = 0;
    } else if (result < 0) {
        mzRuntimeFailures++;
        if (++mzRuntimeConsecutiveFailures >= 3u) {
            /* Fail closed but preserve counters for field diagnostics.
             * A new game launch or explicit reset clears the counters. */
            mzRuntimeCapabilities = 0;
            mzRuntimeModes = 0;
            mzRuntimeConsecutiveFailures = 0;
        }
    }
}

static inline unsigned int mzGsRuntimeFailures(void)
{
    return mzRuntimeFailures;
}

static inline unsigned int mzGsRuntimeAccepted(void)
{
    return mzRuntimeAccepted;
}

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
        /* Mixed VIF1 is the operational PATH2 backend; the older
         * NOP+DIRECT-only adapter remains available for strict callers. */
        result = mzGsDmaPatchVif1Mixed(packet, bytes, &policy, guard,
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
#endif
