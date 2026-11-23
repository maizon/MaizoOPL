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
 * PATH2 cooperative VIF1 stream. The source is an owned, pre-DMA buffer.
 *
 * Accepts ONLY VIF NOP (0) and DIRECT (0x50) / DIRECTHL (0x51) commands.
 * A stream may contain several independent, complete GIF transfers,
 * preceded/separated/followed by VIF NOP words. Everything is validated
 * in pass 0 before pass 1 changes a byte. Other VIF opcodes, interrupt
 * handling, arbitrary DMA chains, VU microprograms and in-flight transfers
 * are deliberately out of scope; this is NOT a universal game hook.
 *
 * The VIF command word immediately precedes its declared QWC*16 bytes
 * of GIF data. All reads and writes use memcpy-safe GIF parsing.
 */
static inline int mzGsDmaPatchVif1Stream(void *buffer, size_t len,
                                         const mz_gs_gif_policy_t *policy,
                                         const mz_gs_dma_guard_t *guard,
                                         mz_gs_gif_state_t *state,
                                         mz_gs_dma_metrics_t *metrics)
{
    static const mz_gs_gif_policy_t keep = {0, 0, 0, 0};
    uint8_t *bytes = (uint8_t *)buffer;
    mz_gs_gif_state_t snapshot, current;
    mz_gs_gif_stats_t total;
    int result = MZ_GS_DMA_OK;
    int pass;

    if (!buffer || !policy || !guard || !state ||
        !len || len > (1u << 20) || (len & 15u)) {
        mzGsDmaRecord(metrics, MZ_GS_DMA_BAD_PACKET, NULL);
        return MZ_GS_DMA_BAD_PACKET;
    }
    if (guard->path != MZ_GS_PATH2_VIF1 ||
        !guard->caller_owns_buffer || !guard->channel_idle ||
        guard->source_is_scratchpad || guard->chain_tag_transfer ||
        ((uintptr_t)buffer & 15u) || !mzGsDmaPolicyEnabled(policy)) {
        mzGsDmaRecord(metrics, MZ_GS_DMA_SKIPPED, NULL);
        return MZ_GS_DMA_SKIPPED;
    }
    snapshot = *state;
    memset(&total, 0, sizeof(total));

    for (pass = 0; pass < 2; ++pass) {
        size_t at = 0;
        unsigned int found = 0;
        current = snapshot;
        while (at < len) {
            uint32_t code;
            unsigned int opcode, qwc;
            size_t payload;
            mz_gs_gif_stats_t local;

            /* Commands always occupy one 32-bit word. */
            if (len - at < sizeof(code)) {
                result = MZ_GS_DMA_BAD_PACKET;
                goto done;
            }
            memcpy(&code, bytes + at, sizeof(code));
            if (!code) {
                at += sizeof(code);  /* VIF NOP padding */
                continue;
            }
            opcode = (unsigned int)(code >> 24);
            qwc = (unsigned int)(code & 0xffffu);
            /* Interrupt bit, NUM and all unknown opcodes are rejected:
             * never silently skip VIF commands or reinterpret data. */
            if ((opcode != 0x50u && opcode != 0x51u) ||
                (code & 0x00ff0000u) || !qwc) {
                result = MZ_GS_DMA_UNSUPPORTED;
                goto done;
            }
            if ((size_t)qwc > (len - at - 4u) / 16u) {
                result = MZ_GS_DMA_BAD_PACKET;
                goto done;
            }
            payload = (size_t)qwc * 16u;
            memset(&local, 0, sizeof(local));
            result = mzGifApplyFilters(bytes + at + 4u, payload,
                           pass == 0 ? &keep : policy, &current, &local);
            if (result != MZ_GS_DMA_OK) {
                result = MZ_GS_DMA_BAD_PACKET;
                goto done;
            }
            if (pass == 1)
                mzGsDmaMergeStats(&total, &local);
            at += 4u + payload;
            found++;
        }
        if (!found) {
            result = MZ_GS_DMA_UNSUPPORTED;
            goto done;
        }
    }
    *state = current;
    result = MZ_GS_DMA_OK;
done:
    mzGsDmaRecord(metrics, result,
                  result == MZ_GS_DMA_OK ? &total : NULL);
    return result;
}

/* Backward-compatible API name; supports DIRECTHL and bounded multi-DIRECT
 * streams as well as the original three-NOP + DIRECT layout. */
static inline int mzGsDmaPatchVif1Direct(void *buffer, size_t len,
                                         const mz_gs_gif_policy_t *policy,
                                         const mz_gs_dma_guard_t *guard,
                                         mz_gs_gif_state_t *state,
                                         mz_gs_dma_metrics_t *metrics)
{
    return mzGsDmaPatchVif1Stream(buffer, len, policy, guard,
                                  state, metrics);
}

#endif
