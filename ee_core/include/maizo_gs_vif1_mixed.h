/*
 * MAIZO VIF1 cooperative mixed-command parser. This is a pre-DMA adapter,
 * NOT a DMA hook. It only edits complete GIF DIRECT/DIRECTHL payloads
 * while preserving all VIF control/microprogram/UNPACK payload bytes.
 *
 * Decode fixed-length VIF1 commands using ps2tek/PS2SDK VIF command layout.
 * For UNPACK, the caller must provide a self-contained STCYCL in this
 * buffer. Incomplete/unknown streams are always rejected without changes.
 */
#ifndef MAIZO_GS_VIF1_MIXED_H
#define MAIZO_GS_VIF1_MIXED_H

#include "maizo_gs_dma_preflight.h"

static inline int mzGsDmaPatchVif1Mixed(
    void *buffer, size_t len, const mz_gs_gif_policy_t *policy,
    const mz_gs_dma_guard_t *guard, mz_gs_gif_state_t *state,
    mz_gs_dma_metrics_t *metrics)
{
    static const mz_gs_gif_policy_t keep = {0, 0, 0, 0};
    uint8_t *buf = (uint8_t *)buffer;
    mz_gs_gif_state_t initial, current;
    mz_gs_gif_stats_t total;
    int pass, result = MZ_GS_DMA_OK;

    if (!buffer || !policy || !guard || !state || !len ||
        len > (1u << 20) || (len & 15u)) {
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
    if (policy->aa1 > MZ_GS_FORCE_OFF ||
        policy->bilinear > MZ_GS_FORCE_OFF ||
        policy->dither > MZ_GS_FORCE_OFF ||
        (policy->scanmask == 1u || policy->scanmask > MZ_GS_SCAN_EVEN_ONLY)) {
        mzGsDmaRecord(metrics, MZ_GS_DMA_UNSUPPORTED, NULL);
        return MZ_GS_DMA_UNSUPPORTED;
    }

    initial = *state;
    memset(&total, 0, sizeof(total));
    for (pass = 0; pass < 2; ++pass) {
        size_t at = 0;
        unsigned int found = 0, cl = 0, wl = 0, cycle_known = 0;
        current = initial;
        while (at < len) {
            uint32_t code;
            unsigned int op, num, imm, count;
            size_t payload = 0;

            if (len - at < sizeof(code)) {
                result = MZ_GS_DMA_BAD_PACKET;
                goto done;
            }
            memcpy(&code, buf + at, sizeof(code));
            op = (unsigned int)(code >> 24);
            num = (unsigned int)((code >> 16) & 255u);
            imm = (unsigned int)(code & 65535u);

            /* The interrupt bit can stall VIF1 and needs special handling.
             * Never change a packet whose stall/interrupt semantics we
             * haven't explicitly verified on console hardware. */
            if (op & 0x80u) {
                result = MZ_GS_DMA_UNSUPPORTED;
                goto done;
            }
            at += 4u;
            if (op == 0x01u) {
                cl = imm & 255u;
                wl = imm >> 8;
                if (!cl) cl = 256u;
                if (!wl) wl = 256u;
                cycle_known = 1;
            } else if (op == 0x20u) { /* STMASK: one 32-bit word */
                payload = 4u;
            } else if (op == 0x30u || op == 0x31u) {
                payload = 16u;        /* STROW/STCOL: four 32-bit words */
            } else if (op == 0x4au) {
                payload = (size_t)(num ? num : 256u) * 8u; /* MPG */
            } else if (op == 0x50u || op == 0x51u) {
                size_t qwc = (size_t)(imm ? imm : 65536u);
                payload = qwc * 16u;
                if (payload > len - at) {
                    result = MZ_GS_DMA_BAD_PACKET;
                    goto done;
                }
                {
                    mz_gs_gif_stats_t local;
                    memset(&local, 0, sizeof(local));
                    result = mzGifApplyFilters(buf + at, payload,
                                    pass == 0 ? &keep : policy,
                                    &current, &local);
                    if (result != MZ_GS_DMA_OK) {
                        result = MZ_GS_DMA_BAD_PACKET;
                        goto done;
                    }
                    if (pass == 1)
                        mzGsDmaMergeStats(&total, &local);
                }
                found++;
            } else if (op >= 0x60u && op <= 0x7fu) {
                /* UNPACK has no direct GS output. Correctly consume its
                 * payload to find subsequent DIRECT packets, otherwise
                 * VU data could be mistaken for opcodes or GIF tags. */
                unsigned int fmt = op & 15u;
                size_t vectors, bytes_per_vector;
                unsigned int components, vl;
                if (!cycle_known || ((fmt & 3u) == 3u && fmt != 15u)) {
                    result = MZ_GS_DMA_UNSUPPORTED;
                    goto done;
                }
                components = (fmt >> 2) + 1u;
                vl = fmt & 3u;
                bytes_per_vector = fmt == 15u ?
                    2u : (size_t)components * (4u >> vl);
                count = num ? num : 256u;
                vectors = cl >= wl ? (size_t)count :
                    ((size_t)(count / wl) * cl +
                     (size_t)((count % wl) < cl ? (count % wl) : cl));
                payload = ((vectors * bytes_per_vector) + 3u) & ~(size_t)3u;
            } else {
                /* Known VIF control-only commands. VU microprogram
                 * execution is NOT interpreted or intercepted (PATH1). */
                switch (op) {
                case 0x00u: /* NOP */
                case 0x02u: /* OFFSET */
                case 0x03u: /* BASE */
                case 0x04u: /* ITOP */
                case 0x05u: /* STMOD */
                case 0x06u: /* MSKPATH3 */
                case 0x07u: /* MARK */
                case 0x10u: /* FLUSHE */
                case 0x11u: /* FLUSH */
                case 0x13u: /* FLUSHA */
                case 0x14u: /* MSCAL */
                case 0x15u: /* MSCALF */
                case 0x17u: /* MSCNT */
                    break;
                default:
                    result = MZ_GS_DMA_UNSUPPORTED;
                    goto done;
                }
            }
            if (payload > len - at) {
                result = MZ_GS_DMA_BAD_PACKET;
                goto done;
            }
            at += payload;
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

#endif
