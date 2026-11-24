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
#include "maizo_mage_modes.h"
#include "maizo_filter_intent.h"

enum {
    MZ_DRAW_AA1 = 1,
    MZ_DRAW_BILINEAR = 2,
    MZ_DRAW_DITHER = 4,
    MZ_DRAW_SCANLINES = 8,
    MZ_DRAW_LOD = 16,
    MZ_DRAW_FOG = 32, /* explicit adapter capability, MAEC-owned mode */
    MZ_DRAW_ALL = 63
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
#include "maizo_effects.h"
#define MZ_GS_SHARED_ABI 6u
typedef struct {
    uint32_t magic0, magic1, abi, bytes;
    volatile unsigned int enabled, capabilities, modes;
    unsigned int failures, accepted, consecutive_failures;
    mz_gs_gif_state_t state;
    mz_gs_dma_metrics_t metrics;
    unsigned int requested_modes;
    volatile uint32_t effect_caps, effect_reduced, effect_modes;
    uint32_t draw_intents, effect_intents, native_owned; /* draw bits 0..4; effects 8..21 */
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
           feature == MZ_DRAW_DITHER ? 4u :
           feature == MZ_DRAW_SCANLINES ? 6u :
           feature == MZ_DRAW_LOD ? 8u : 10u;
}

static inline void mzGsRuntimeReset(void)
{
    mzGsShared->requested_modes = 0;
    mzGsShared->draw_intents=mzGsShared->effect_intents=mzGsShared->native_owned=0;
    mzGsShared->effect_caps = mzGsShared->effect_reduced = mzGsShared->effect_modes = 0;
    mzRuntimeCapabilities = 0;
    mzRuntimeModes = 0;
    mzRuntimeFailures = 0;
    mzRuntimeAccepted = 0;
    mzRuntimeConsecutiveFailures = 0;
    memset(&mzRuntimeState, 0, sizeof(mzRuntimeState));
    memset(&mzRuntimeMetrics, 0, sizeof(mzRuntimeMetrics));
}

/* Apply queued requests only to certified routes. Native Sky ownership
 * prevents a later cooperative registration from applying a second override. */
static inline unsigned mzGsEffectCaps(void)
{
    return mzGsShared->effect_caps |
        ((mzRuntimeCapabilities&MZ_DRAW_FOG)?(1u<<MZ_EFFECT_FOG):0u);
}
static inline void mzGsResolveEffects(void)
{
    unsigned caps=mzGsEffectCaps()&~(mzGsShared->native_owned>>8);
    for(unsigned i=0;i<MZ_EFFECT_COUNT;i++){
        unsigned intent=mzIntentGet(mzGsShared->effect_intents,i);
        if(intent || !(caps&(1u<<i)))
            mzGsShared->effect_modes=mzEffectPut(mzGsShared->effect_modes,i,
                (caps&(1u<<i))?mzIntentEffectMode(intent,!!(mzGsShared->effect_reduced&(1u<<i))):0);
    }
}
static inline void mzGsDrawRequest(unsigned index,unsigned intent)
{
    if(index>=5 || intent>3)return;
    unsigned bit=1u<<index,shift=2u*index,mask=3u<<shift;
    unsigned mode=(mzGsShared->native_owned&bit)?0:mzIntentDrawMode(index,intent);
    mzGsShared->draw_intents=mzIntentPut(mzGsShared->draw_intents,index,intent);
    mzGsShared->requested_modes=(mzGsShared->requested_modes&~mask)|(mode<<shift);
    mzRuntimeModes=(mzRuntimeModes&~mask)|
        ((mzGsShared->enabled&&(mzRuntimeCapabilities&bit))?mode<<shift:0u);
}
static inline void mzGsEffectRequest(unsigned id,unsigned intent)
{
    if(id>=MZ_EFFECT_COUNT || intent>MZ_INTENT_FORCE)return;
    unsigned bit=1u<<id;
    unsigned caps=mzGsEffectCaps()&~(mzGsShared->native_owned>>8);
    mzGsShared->effect_intents=mzIntentPut(mzGsShared->effect_intents,id,intent);
    mzGsShared->effect_modes=mzEffectPut(mzGsShared->effect_modes,id,
        (mzGsShared->enabled&&(caps&bit))?mzIntentEffectMode(intent,!!(mzGsShared->effect_reduced&bit)):0);
}
/* Preserve live menu choices across a verified reattachment in the
 * same ELF. A new ELF always resets choices to the game's original. */
static inline int mzGsRuntimeAttach(unsigned int capabilities)
{
    unsigned int requested = mzGsShared->requested_modes;
    unsigned di=mzGsShared->draw_intents,ei=mzGsShared->effect_intents,owned=mzGsShared->native_owned;
    unsigned ec=mzGsShared->effect_caps, er=mzGsShared->effect_reduced, em=mzGsShared->effect_modes;
    if (!capabilities || (capabilities & ~MZ_DRAW_ALL)) return 0;
    if (!mzRuntimeCapabilities && mzRuntimeFailures >= 3u) return 0;
    mzGsRuntimeReset();
    mzGsShared->requested_modes = requested;
    mzGsShared->draw_intents=di;mzGsShared->effect_intents=ei;mzGsShared->native_owned=owned;
    mzGsShared->effect_reduced=er; mzGsShared->effect_modes=em; mzGsShared->effect_caps=ec;
    mzRuntimeModes = mzMageModesForCaps(requested, capabilities);
    mzRuntimeCapabilities = capabilities;
    mzGsResolveEffects();
    return 1;
}

/* Register effects separately from drawing filters. A fresh registration
 * starts from original; new ELF and fail-closed reset revoke the request.
 * Reduced is a subset of independently certified optional effects. */
static inline int mzGsEffectsAttach(unsigned caps,unsigned reduced)
{
    if((caps&~MZ_EFFECT_ALL)||(reduced&~caps)||!mzGsShared->enabled||
       mzRuntimeFailures>=3u)return 0;
    mzGsShared->effect_caps=0;
    mzGsShared->effect_modes=0;
    mzGsShared->effect_reduced=reduced;
    mzGsShared->effect_caps=caps;
    mzGsResolveEffects();
    return 1;
}
static inline int mzGsEffectSet(unsigned id,unsigned mode)
{
    if(id>=MZ_EFFECT_COUNT||mode>MZ_EFFECT_OFF||
       !(mzGsEffectCaps()&(1u<<id))||(mzGsShared->native_owned&(1u<<(8u+id)))||
       (mode==MZ_EFFECT_REDUCED&&!(mzGsShared->effect_reduced&(1u<<id))))return 0;
    mzGsShared->effect_modes=mzEffectPut(mzGsShared->effect_modes,id,mode);
    return 1;
}
static inline unsigned mzGsEffectGet(unsigned id)
{
    if(!mzGsShared->enabled || id>=MZ_EFFECT_COUNT ||
       (mzGsShared->native_owned&(1u<<(8u+id))))return MZ_EFFECT_ORIGINAL;
    return mzEffectAllowed(mzGsShared->effect_caps,mzGsShared->effect_reduced,
                           mzGsShared->effect_modes,id);
}
static inline unsigned int mzGsRuntimeMode(unsigned int feature)
{
    unsigned int shift = mzGsRuntimeShift(feature);
    if (shift > 8 || !(mzRuntimeCapabilities & feature) || (mzGsShared->native_owned&feature))
        return MZ_GS_KEEP;
    return (mzRuntimeModes >> shift) & 3u;
}

static inline int mzGsRuntimeSetMode(unsigned int feature, unsigned int mode)
{
    unsigned int shift = mzGsRuntimeShift(feature);
    unsigned int mask;
    if (shift > 8 || mode > ((feature==MZ_DRAW_SCANLINES || feature==MZ_DRAW_BILINEAR || feature==MZ_DRAW_AA1)?3u:MZ_GS_FORCE_OFF) ||
        !(mzRuntimeCapabilities & feature) || (mzGsShared->native_owned&feature)) return 0;
    mask = 3u << shift;
    mzRuntimeModes = (mzRuntimeModes & ~mask) | (mode << shift);
    mzGsShared->requested_modes = (mzGsShared->requested_modes & ~mask) | (mode << shift);
    return 1;
}

static inline void mzGsRuntimeGetPolicy(mz_gs_gif_policy_t *policy)
{
    if (!policy) return;
    policy->aa1 = (uint8_t)mzGsRuntimeMode(MZ_DRAW_AA1);
    policy->bilinear = (uint8_t)(mzGsRuntimeMode(MZ_DRAW_BILINEAR) | (mzGsRuntimeMode(MZ_DRAW_LOD)<<2));
    policy->dither = (uint8_t)mzGsRuntimeMode(MZ_DRAW_DITHER);
    policy->scanmask = ((mzRuntimeCapabilities&MZ_DRAW_FOG) &&
        !(mzGsShared->native_owned&(1u<<(8u+MZ_EFFECT_FOG))) &&
        !(mzGsShared->effect_caps&(1u<<MZ_EFFECT_FOG)) &&
        mzEffectMode(mzGsShared->effect_modes,MZ_EFFECT_FOG)==MZ_EFFECT_OFF)?
        MZ_GS_FOG_OFF_FLAG:MZ_GS_SCAN_UNCHANGED;
}

/* A verified adapter supplies the capabilities of THIS draw pass.
 * Never temporarily change shared menu settings to avoid duplicate edits. */
static inline void mzGsRuntimeGetPolicyMasked(mz_gs_gif_policy_t *policy,
                                              unsigned int mask)
{
    mzGsRuntimeGetPolicy(policy);
    if (!policy) return;
    if (!(mask & MZ_DRAW_AA1)) policy->aa1 = MZ_GS_KEEP;
    if (!(mask & MZ_DRAW_BILINEAR)) policy->bilinear &= ~3u;
    if (!(mask & MZ_DRAW_LOD)) policy->bilinear &= 3u;
    if (!(mask & MZ_DRAW_DITHER)) policy->dither = MZ_GS_KEEP;
    if (!(mask & MZ_DRAW_FOG)) policy->scanmask &= ~MZ_GS_FOG_OFF_FLAG;
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
            mzGsShared->effect_caps = mzGsShared->effect_reduced = mzGsShared->effect_modes = 0;
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

static inline int mzGsRuntimeSubmitMasked(unsigned int submit_type, void *packet,
                                          size_t bytes,
                                          const mz_gs_dma_guard_t *guard,
                                          const mz_gs_dma_owned_ref_t *refs,
                                          size_t ref_count, unsigned int mask)
{
    mz_gs_gif_policy_t policy;
    mz_gs_gif_state_t *state;
    mz_gs_dma_metrics_t *metrics;
    int result = MZ_GS_DMA_SKIPPED;

    if (!mzGsRuntimeCaps() || !guard)
        return MZ_GS_DMA_SKIPPED;
    mzGsRuntimeGetPolicyMasked(&policy, mask);
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

static inline int mzGsRuntimeSubmit(unsigned int submit_type, void *packet,
                                    size_t bytes,
                                    const mz_gs_dma_guard_t *guard,
                                    const mz_gs_dma_owned_ref_t *refs,
                                    size_t ref_count)
{
    return mzGsRuntimeSubmitMasked(submit_type, packet, bytes, guard,
                                   refs, ref_count, MZ_DRAW_ALL);
}
#endif
