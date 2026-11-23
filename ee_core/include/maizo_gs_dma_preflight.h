/*
 * MAIZO PS2 GIF DMA pre-submit adapter (opt-in, bounded, no MMIO hook).
 *
 * This is an adapter for a cooperating caller BEFORE it starts GIF PATH3 DMA.
 * It DOES NOT install a hook into an arbitrary game. The PS2 has three GIF
 * paths; intercepting only GIF DMAC channel 2 cannot cover VU1 XGKICK
 * (PATH1) or VIF1 DIRECT (PATH2). Never claim universal compatibility.
 *
 * Callback/caller responsibilities:
 * - Own a writable 16-byte-aligned DMA buffer (not executable / mapped ROM).
 * - Have exclusively stopped DMA or not yet submitted the transfer.
 * - FlushCache(0) or equivalent after successful edits, before DMA start.
 * - Keep this packet owned, not concurrently changed by EE, VU or IOP.
 * - Preserve the original packet if future filter disable requires rollback.
 *
 * The validated branch supports PATH3 normal GIF packets or contiguous
 * CNT/END (DMA chain mode) in an owned buffer, without external pointers.
 * REF/REFS/NEXT/CALL/RET/REFE, SPR and TTE are deliberately rejected:
 * following arbitrary game DMATag pointers is unsafe without a mapper.
 */
#ifndef MAIZO_GS_DMA_PREFLIGHT_H
#define MAIZO_GS_DMA_PREFLIGHT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "maizo_gs_gif_filter.h"

enum {
    MZ_GS_DMA_OK = 0,
    MZ_GS_DMA_SKIPPED = 1,
    MZ_GS_DMA_BAD_PACKET = -1,
    MZ_GS_DMA_UNSUPPORTED = -2
};

enum {
    MZ_GS_PATH1_XGKICK = 1,
    MZ_GS_PATH2_VIF1 = 2,
    MZ_GS_PATH3_GIF_DMA = 3
};

typedef struct {
    uint8_t path;
    uint8_t caller_owns_buffer;
    uint8_t channel_idle;    /* caller-proven, never guessed by this API */
    uint8_t chain_tag_transfer; /* CHCR.TTE */
    uint8_t source_is_scratchpad;
} mz_gs_dma_guard_t;

typedef struct {
    uint32_t submitted;
    uint32_t accepted;
    uint32_t refused;
    uint32_t skipped;
    mz_gs_gif_stats_t edits;
} mz_gs_dma_metrics_t;

static inline int mzGsDmaPolicyEnabled(const mz_gs_gif_policy_t *p)
{
    return p && (p->aa1 || p->bilinear || p->dither || p->scanmask);
}

static inline int mzGsDmaGuardOK(const void *buffer, size_t len,
                                const mz_gs_gif_policy_t *policy,
                                const mz_gs_dma_guard_t *guard)
{
    if (!buffer || !policy || !guard || len == 0 ||
        len > (1u << 20) || (len & 15u))
        return MZ_GS_DMA_BAD_PACKET;
    if (guard->path != MZ_GS_PATH3_GIF_DMA ||
        !guard->caller_owns_buffer || !guard->channel_idle ||
        guard->source_is_scratchpad || guard->chain_tag_transfer ||
        ((uintptr_t)buffer & 15u))
        return MZ_GS_DMA_SKIPPED;
    if (!mzGsDmaPolicyEnabled(policy))
        return MZ_GS_DMA_SKIPPED;
    return MZ_GS_DMA_OK;
}

static inline void mzGsDmaMergeStats(mz_gs_gif_stats_t *dst,
                                     const mz_gs_gif_stats_t *src)
{
    dst->tags += src->tags;
    dst->primitive_changes += src->primitive_changes;
    dst->texture_changes += src->texture_changes;
    dst->dither_changes += src->dither_changes;
    dst->scanmask_changes += src->scanmask_changes;
    dst->unsupported_primitives += src->unsupported_primitives;
    dst->protected_dither += src->protected_dither;
}

static inline void mzGsDmaRecord(mz_gs_dma_metrics_t *metrics,
                                 int result,
                                 const mz_gs_gif_stats_t *details)
{
    if (!metrics)
        return;
    metrics->submitted++;
    if (result == MZ_GS_DMA_SKIPPED)
        metrics->skipped++;
    else if (result == MZ_GS_DMA_OK) {
        metrics->accepted++;
        if (details)
            mzGsDmaMergeStats(&metrics->edits, details);
    } else
        metrics->refused++;
}

static inline int mzGsDmaPatchNormal(void *buffer, size_t len,
                                    const mz_gs_gif_policy_t *policy,
                                    const mz_gs_dma_guard_t *guard,
                                    mz_gs_gif_state_t *state,
                                    mz_gs_dma_metrics_t *metrics)
{
    mz_gs_gif_stats_t stats;
    int result = mzGsDmaGuardOK(buffer, len, policy, guard);
    if (result == MZ_GS_DMA_OK) {
        if (!state)
            result = MZ_GS_DMA_BAD_PACKET;
        else {
            memset(&stats, 0, sizeof(stats));
            result = mzGifApplyFilters(buffer, len, policy, state, &stats);
        }
    }
    mzGsDmaRecord(metrics, result, result == MZ_GS_DMA_OK ? &stats : NULL);
    return result;
}

/* A fully inline DMA chain contains 16-byte tags with 16-byte-aligned GIF
 * payloads. Caller must provide a writable region containing ALL tag data.
 * Two full passes prevent half-application if the final tag is malformed. */
static inline int mzGsDmaPatchInlineChain(void *chain, size_t chain_bytes,
                                         const mz_gs_gif_policy_t *policy,
                                         const mz_gs_dma_guard_t *guard,
                                         mz_gs_gif_state_t *state,
                                         mz_gs_dma_metrics_t *metrics)
{
    static const mz_gs_gif_policy_t keep = {0, 0, 0, 0};
    mz_gs_gif_state_t snapshot, probe;
    mz_gs_gif_stats_t total;
    size_t at;
    int status, pass;
    uint8_t *bytes = (uint8_t *)chain;

    status = mzGsDmaGuardOK(chain, chain_bytes, policy, guard);
    if (status != MZ_GS_DMA_OK) {
        mzGsDmaRecord(metrics, status, NULL);
        return status;
    }
    if (!state) {
        mzGsDmaRecord(metrics, MZ_GS_DMA_BAD_PACKET, NULL);
        return MZ_GS_DMA_BAD_PACKET;
    }
    snapshot = *state;
    memset(&total, 0, sizeof(total));

    for (pass = 0; pass < 2; pass++) {
        int ended = 0;
        probe = snapshot;
        at = 0;
        while (at + 16u <= chain_bytes) {
            uint64_t tag = mzGifRead64(bytes + at);
            unsigned int qwc = (unsigned int)(tag & 0xffffULL);
            unsigned int id = (unsigned int)((tag >> 28) & 7ULL);
            size_t chunk = (size_t)qwc * 16u;
            mz_gs_gif_stats_t local;
            /* Only in-place CNT and END. All addresses/spr/references are
             * refused, even if numerically inside the provided buffer. */
            if ((id != 1 && id != 7) || (tag & (1ULL << 63)) ||
                chunk > chain_bytes - at - 16u) {
                status = MZ_GS_DMA_UNSUPPORTED;
                goto done;
            }
            at += 16u;
            if (chunk) {
                memset(&local, 0, sizeof(local));
                if (mzGifApplyFilters(bytes + at, chunk,
                          pass == 0 ? &keep : policy, &probe, &local) != 0) {
                    status = MZ_GS_DMA_BAD_PACKET;
                    goto done;
                }
                if (pass == 1)
                    mzGsDmaMergeStats(&total, &local);
            }
            at += chunk;
            if (id == 7) {
                ended = 1;
                break;
            }
        }
        if (!ended) {
            status = MZ_GS_DMA_BAD_PACKET;
            goto done;
        }
    }
    *state = probe;
    status = MZ_GS_DMA_OK;
done:
    mzGsDmaRecord(metrics, status,
                  status == MZ_GS_DMA_OK ? &total : NULL);
    return status;
}
/*
 * PATH2 bounded VIF1 DIRECT pre-submit (single standard 16-byte VIF code
 * header): three VIF NOP commands + DIRECT.IMM=GIF QWC. This is NOT a
 * parser for arbitrary VIF1 chains, UNPACK/MSCAL, DIRECTHL or VU programs.
 * Caller must be a cooperating renderer and own/synchronize its buffer.
 */
static inline int mzGsDmaPatchVif1Direct(void *buffer, size_t len,
                                        const mz_gs_gif_policy_t *policy,
                                        const mz_gs_dma_guard_t *guard,
                                        mz_gs_gif_state_t *state,
                                        mz_gs_dma_metrics_t *metrics)
{
    uint8_t *bytes = (uint8_t *)buffer;
    uint32_t codes[4];
    mz_gs_gif_stats_t local;
    size_t words;
    int result;
    if (!buffer || !policy || !guard || !state ||
        len < 32u || len > (1u << 20) || (len & 15u)) {
        mzGsDmaRecord(metrics, MZ_GS_DMA_BAD_PACKET, NULL);
        return MZ_GS_DMA_BAD_PACKET;
    }
    if (guard->path != MZ_GS_PATH2_VIF1 || !guard->caller_owns_buffer ||
        !guard->channel_idle || guard->source_is_scratchpad ||
        guard->chain_tag_transfer || ((uintptr_t)buffer & 15u) ||
        !mzGsDmaPolicyEnabled(policy)) {
        mzGsDmaRecord(metrics, MZ_GS_DMA_SKIPPED, NULL);
        return MZ_GS_DMA_SKIPPED;
    }
    memcpy(codes, bytes, sizeof(codes));
    words = (len - 16u) / 16u;
    if (codes[0] || codes[1] || codes[2] ||
        (codes[3] >> 24) != 0x50u ||
        (size_t)(codes[3] & 0xffffu) != words || words == 0) {
        mzGsDmaRecord(metrics, MZ_GS_DMA_UNSUPPORTED, NULL);
        return MZ_GS_DMA_UNSUPPORTED;
    }
    memset(&local, 0, sizeof(local));
    result = mzGifApplyFilters(bytes + 16u, len - 16u,
                               policy, state, &local);
    mzGsDmaRecord(metrics, result,
                  result == MZ_GS_DMA_OK ? &local : NULL);
    return result;
}

#endif
