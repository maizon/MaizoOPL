/*
 * MAIZO shadow DMA staging for cooperating renderers.
 * The original command stream is NEVER edited. With filter=JOGO (KEEP),
 * the original pointer is passed through. With filters active, an owned
 * scratch buffer receives a copy and the patched version is submitted.
 *
 * WARNING: the caller must retain scratch memory until its DMA completes,
 * FlushCache(0) after staging, and supply only verified pre-DMA channels.
 * Reference chains (REF/REFE) are deliberately excluded because their
 * pointed-to memory would also need independently owned shadow regions.
 */
#ifndef MAIZO_GS_SHADOW_H
#define MAIZO_GS_SHADOW_H

#include "maizo_gs_runtime.h"
#include <stdint.h>
#include <string.h>

static inline int mzGsRuntimeStageShadow(
    unsigned int submit_type,
    const void *original, void *scratch, size_t scratch_capacity, size_t bytes,
    const mz_gs_dma_guard_t *guard,
    void **dma_source)
{
    uintptr_t ss, se, os, oe;
    mz_gs_gif_policy_t policy;
    int status;

    if (!dma_source || !original) return MZ_GS_DMA_BAD_PACKET;
    *dma_source = (void *)original;
    if (!mzGsRuntimeCaps()) return MZ_GS_DMA_SKIPPED;
    mzGsRuntimeGetPolicy(&policy);
    if (!mzGsDmaPolicyEnabled(&policy)) return MZ_GS_DMA_SKIPPED;

    if (!guard || !guard->caller_owns_buffer || !guard->channel_idle ||
        guard->chain_tag_transfer || guard->source_is_scratchpad)
        return MZ_GS_DMA_SKIPPED;

    if (submit_type != MZ_SUBMIT_PATH3_NORMAL &&
        submit_type != MZ_SUBMIT_PATH3_INLINE &&
        submit_type != MZ_SUBMIT_PATH2_VIF1 &&
        submit_type != MZ_SUBMIT_PATH1_STAGING)
        return MZ_GS_DMA_UNSUPPORTED;

    if (!scratch || !bytes || bytes > (1u << 20) ||
        bytes > scratch_capacity || (bytes & 15u) ||
        ((uintptr_t)scratch & 15u) ||
        ((uintptr_t)original & 15u))
        return MZ_GS_DMA_BAD_PACKET;

    ss = (uintptr_t)scratch;
    se = ss + bytes;
    os = (uintptr_t)original;
    oe = os + bytes;
    if (se < ss || oe < os || (ss < oe && se > os))
        return MZ_GS_DMA_UNSUPPORTED;

    memcpy(scratch, original, bytes);
    status = mzGsRuntimeSubmit(submit_type, scratch, bytes,
                               guard, NULL, 0);
    if (status == MZ_GS_DMA_OK)
        *dma_source = scratch;
    return status;
}

#endif
