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
int mzGsRuntimeSubmit(unsigned int submit_type, void *packet, size_t bytes,
                      const mz_gs_dma_guard_t *guard,
                      const mz_gs_dma_owned_ref_t *refs, size_t ref_count);
unsigned int mzGsRuntimeFailures(void);
unsigned int mzGsRuntimeAccepted(void);
#endif
