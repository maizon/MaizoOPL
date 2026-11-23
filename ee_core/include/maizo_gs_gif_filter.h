/*
 * MAIZO GS GIF packet filter core (experimental, NOT a universal game hook).
 *
 * PS2 GS receives PRIM/PRMODE, TEX1, DTHE, DIMX and SCANMSK as GIF drawing
 * commands, unlike DISPLAY/PMODE scanout configuration. This library can
 * patch an OWNED, WRITABLE GIF command buffer before DMA submission.
 *
 * IMPORTANT: It DOES NOT intercept game DMAs, allocate memory, or run each
 * frame. Not yet wired to gameplay. It is intentionally not exposed as
 * a working In-Game toggle. On/off below means policies for a packet
 * that is actually supplied to mzGifApplyFilters().
 *
 * This is original MAIZO code. GIF register behavior documented by ps2tek,
 * PS2 GS User's Manual and the PCSX2 GSRegs definitions.
 */
#ifndef MAIZO_GS_GIF_FILTER_H
#define MAIZO_GS_GIF_FILTER_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MZ_GS_KEEP = 0,
    MZ_GS_FORCE_ON = 1,
    MZ_GS_FORCE_OFF = 2
};

/*
 * scanmask is NOT a true scanlines shader: it masks GS drawing to one
 * parity of framebuffer rows, which can leave old-frame ghosts.
 * Explicitly experimental and never enabled by default.
 */
enum {
    MZ_GS_SCAN_UNCHANGED = 0,
    MZ_GS_SCAN_ODD_ONLY = 1,
    MZ_GS_SCAN_EVEN_ONLY = 2
};

typedef struct {
    uint8_t aa1;
    uint8_t bilinear;
    uint8_t dither;
    uint8_t scanmask;
} mz_gs_gif_policy_t;

typedef struct {
    uint32_t tags;
    uint32_t primitive_changes;
    uint32_t texture_changes;
    uint32_t dither_changes;
    uint32_t scanmask_changes;
    uint32_t unsupported_primitives;
    uint32_t protected_dither;
} mz_gs_gif_stats_t;

typedef struct {
    uint8_t framebuffer_psm[2];
    uint8_t framebuffer_known[2];
    uint8_t dither_matrix_known;
    /* Track the selected GS rendering context for 16-bit dithering. */
    uint8_t prim_known;
    uint8_t prim_kind;
    uint8_t active_ctx;
    uint8_t active_ctx_known;
    /* PRMODECONT.AC selects PRIM (1) or PRMODE (0) for GS attributes. */
    uint8_t prmode_cont_known;
    uint8_t prmode_cont_ac;
    uint8_t prim_ctx;
    uint8_t prmode_ctx;
    uint8_t prmode_ctx_known;
} mz_gs_gif_state_t;

static inline uint64_t mzGifRead64(const uint8_t *ptr)
{
    uint64_t v;
    memcpy(&v, ptr, sizeof(v));
    return v;
}

static inline void mzGifWrite64(uint8_t *ptr, uint64_t value)
{
    memcpy(ptr, &value, sizeof(value));
}

static inline int mzGifPrimitiveSupported(uint64_t prim)
{
    unsigned int kind = (unsigned int)(prim & 7ULL);
    /* AA1 is a per-primitive GS feature for lines and triangles.
     * GS draws coverage-fringe pixels with its own blending regardless of
     * PRIM.ABE; requiring ABE here wrongly skips most opaque geometry.
     * DO NOT set ABE: changing the regular blend mode breaks games. */
    return kind >= 1 && kind <= 5;
}

static inline uint64_t mzGifPrimitive(uint64_t src,
                                      unsigned int mode,
                                      mz_gs_gif_stats_t *stats)
{
    uint64_t result = src;
    if (mode == MZ_GS_FORCE_OFF) {
        result &= ~(1ULL << 7);
    } else if (mode == MZ_GS_FORCE_ON) {
        if (mzGifPrimitiveSupported(src))
            result |= 1ULL << 7;
        else
            stats->unsupported_primitives++;
    }
    if (result != src)
        stats->primitive_changes++;
    return result;
}

static inline uint64_t mzGifTex1(uint64_t src,
                                 unsigned int mode,
                                 mz_gs_gif_stats_t *stats)
{
    unsigned int min, new_min;
    uint64_t result;
    if (mode == MZ_GS_KEEP)
        return src;

    min = (unsigned int)((src >> 6) & 7ULL);
    new_min = min;
    if (mode == MZ_GS_FORCE_ON) {
        /* Preserve the selected mipmap family; change nearest -> linear.
           MMIN values 6/7 are reserved and must never be invented. */
        if (min == 0)
            new_min = 1;
        else if (min == 2)
            new_min = 4;
        else if (min == 3)
            new_min = 5;
    } else if (mode == MZ_GS_FORCE_OFF) {
        if (min == 1)
            new_min = 0;
        else if (min == 4)
            new_min = 2;
        else if (min == 5)
            new_min = 3;
    }

    result = (src & ~(1ULL << 5 | 7ULL << 6)) |
             ((uint64_t)new_min << 6);
    if (mode == MZ_GS_FORCE_ON)
        result |= 1ULL << 5;  /* MMAG linear */
    if (result != src)
        stats->texture_changes++;
    return result;
}

/* PRMODE has no PRIM primitive kind in bits 0..2: do not treat it as
   PRIM, otherwise AA1 silently fails on every PRMODE write. */
static inline uint64_t mzGifPrmode(uint64_t src,
                                  unsigned int mode,
                                  const mz_gs_gif_state_t *state,
                                  mz_gs_gif_stats_t *stats)
{
    uint64_t result = src;
    if (mode == MZ_GS_FORCE_OFF)
        result &= ~(1ULL << 7);
    else if (mode == MZ_GS_FORCE_ON) {
        if (state->prim_known && state->prim_kind >= 1 &&
            state->prim_kind <= 5)
            result |= 1ULL << 7;
        else
            stats->unsupported_primitives++;
    }
    if (result != src)
        stats->primitive_changes++;
    return result;
}
/* Fully packed GIF A+D register IDs. */
enum {
    MZ_GS_PRIM = 0x00,
    MZ_GS_TEX1_1 = 0x14,
    MZ_GS_TEX1_2 = 0x15,
    MZ_GS_PRMODECONT = 0x1a,
    MZ_GS_PRMODE = 0x1b,
    MZ_GS_SCANMSK = 0x22,
    MZ_GS_DIMX = 0x44,
    MZ_GS_DTHE = 0x45,
    MZ_GS_FRAME_1 = 0x4c,
    MZ_GS_FRAME_2 = 0x4d
};

static inline uint64_t mzGifAdValue(uint64_t src,
                                    unsigned int register_id,
                                    const mz_gs_gif_policy_t *policy,
                                    mz_gs_gif_state_t *state,
                                    mz_gs_gif_stats_t *stats)
{
    unsigned int ctx;
    switch (register_id) {
    case MZ_GS_PRIM:
        state->prim_known = 1;
        state->prim_kind = (uint8_t)(src & 7ULL);
        state->prim_ctx = (uint8_t)((src >> 9) & 1ULL);
        if (!state->prmode_cont_known || state->prmode_cont_ac) {
            state->active_ctx = state->prim_ctx;
            state->active_ctx_known = 1;
        }
        return mzGifPrimitive(src, policy->aa1, stats);
    case MZ_GS_PRMODE:
        state->prmode_ctx = (uint8_t)((src >> 9) & 1ULL);
        state->prmode_ctx_known = 1;
        if (!state->prmode_cont_known || !state->prmode_cont_ac) {
            state->active_ctx = state->prmode_ctx;
            state->active_ctx_known = 1;
        }
        return mzGifPrmode(src, policy->aa1, state, stats);
    case MZ_GS_PRMODECONT:
        state->prmode_cont_known = 1;
        state->prmode_cont_ac = (uint8_t)(src & 1ULL);
        if (state->prmode_cont_ac) {
            state->active_ctx = state->prim_ctx;
            state->active_ctx_known = state->prim_known;
        } else {
            state->active_ctx = state->prmode_ctx;
            state->active_ctx_known = state->prmode_ctx_known;
        }
        return src;
    case MZ_GS_TEX1_1:
    case MZ_GS_TEX1_2:
        return mzGifTex1(src, policy->bilinear, stats);
    case MZ_GS_DIMX:
        state->dither_matrix_known = 1;
        return src;
    case MZ_GS_FRAME_1:
    case MZ_GS_FRAME_2:
        ctx = register_id == MZ_GS_FRAME_1 ? 0u : 1u;
        state->framebuffer_psm[ctx] = (uint8_t)((src >> 24) & 63ULL);
        state->framebuffer_known[ctx] = 1;
        return src;
    case MZ_GS_DTHE:
        if (policy->dither == MZ_GS_FORCE_OFF) {
            uint64_t value = src & ~1ULL;
            if (value != src)
                stats->dither_changes++;
            return value;
        }
        if (policy->dither == MZ_GS_FORCE_ON) {
            unsigned int active = state->active_ctx;
            int depth_16 = state->active_ctx_known &&
                state->framebuffer_known[active] &&
                (state->framebuffer_psm[active] == 2 ||
                 state->framebuffer_psm[active] == 10);
            /* Dithering needs a known 16-bit framebuffer AND an actual
               DIMX matrix supplied by the game. */
            if (!depth_16 || !state->dither_matrix_known) {
                stats->protected_dither++;
                return src;
            }
            if (!(src & 1ULL))
                stats->dither_changes++;
            return src | 1ULL;
        }
        return src;
    case MZ_GS_SCANMSK:
        if (policy->scanmask == MZ_GS_SCAN_ODD_ONLY ||
            policy->scanmask == MZ_GS_SCAN_EVEN_ONLY) {
            uint64_t value = (src & ~3ULL) | policy->scanmask;
            if (src != value)
                stats->scanmask_changes++;
            return value;
        }
        return src;
    default:
        return src;
    }
}

/*
 * Rewrite a COMPLETE GIF packet stream, bounded by packet_size bytes.
 *  0    OK
 * -1    Invalid input / malformed tag / truncated packet; buffer unchanged
 * -2    Unsupported length/mode/policy; buffer unchanged
 *
 * The parser operates in two passes: validate the entire stream without any
 * write first; only then rewrite. No partial modifications on malformed data.
 * EOP stops the scan; DMA padding after EOP is not parsed or modified.
 *
 * The caller MUST own and synchronize the DMA buffer, flush the D-cache,
 * and call this only BEFORE submitting the GIF packet to hardware.
 * No live game-hook exists in this module.
 */
static inline int mzGifApplyFilters(void *packet,
                                    size_t packet_size,
                                    const mz_gs_gif_policy_t *policy,
                                    mz_gs_gif_state_t *state,
                                    mz_gs_gif_stats_t *stats)
{
    uint8_t *buf = (uint8_t *)packet;
    int pass;
    mz_gs_gif_state_t snapshot;

    if (!packet || !policy || !state || !stats || !packet_size ||
        (packet_size & 15u) || packet_size > (1u << 20))
        return -1;
    if (policy->aa1 > MZ_GS_FORCE_OFF ||
        policy->bilinear > MZ_GS_FORCE_OFF ||
        policy->dither > MZ_GS_FORCE_OFF ||
        policy->scanmask > MZ_GS_SCAN_EVEN_ONLY)
        return -2;

    snapshot = *state;

    for (pass = 0; pass < 2; pass++) {
        mz_gs_gif_state_t current = snapshot;
        mz_gs_gif_stats_t tally;
        size_t at = 0;
        int saw_end = 0;
        memset(&tally, 0, sizeof(tally));

        while (at + 16 <= packet_size) {
            uint64_t tag = mzGifRead64(buf + at);
            uint64_t registers = mzGifRead64(buf + at + 8);
            size_t loops = (size_t)(tag & 0x7fffULL);
            size_t nreg = (size_t)((tag >> 60) & 15ULL);
            size_t kind = (size_t)((tag >> 58) & 3ULL);
            size_t entries, payload;
            size_t i;
            int end = (int)((tag >> 15) & 1ULL);
            /* FLG=3 is IMAGE2, not a reserved mode. Like IMAGE it
             * transports opaque GS pixel data without register writes. */
            if (nreg == 0)
                nreg = 16;
            entries = loops * nreg;
            if (kind == 0)
                payload = entries * 16u;
            else if (kind == 1)
                payload = ((entries + 1u) / 2u) * 16u;
            else
                payload = loops * 16u;

            if (payload > packet_size - at - 16u)
                return -1;

            if (kind == 0 && (tag & (1ULL << 46))) {
                /* Embedded PRIM is meaningful for PACKED mode only. */
                uint64_t orig_prim = (tag >> 47) & 0x7ffULL;
                current.prim_kind = (uint8_t)(orig_prim & 7ULL);
                current.prim_known = 1;
                current.prim_ctx = (uint8_t)((orig_prim >> 9) & 1ULL);
                if (!current.prmode_cont_known || current.prmode_cont_ac) {
                    current.active_ctx = current.prim_ctx;
                    current.active_ctx_known = 1;
                }
                if (pass == 1) {
                    uint64_t new_prim = mzGifPrimitive(orig_prim,
                                                        policy->aa1, &tally);
                    if (new_prim != orig_prim) {
                        tag &= ~(0x7ffULL << 47);
                        tag |= new_prim << 47;
                        mzGifWrite64(buf + at, tag);
                    }
                }
            }
            at += 16u;
            if (kind <= 1) {
                for (i = 0; i < entries; i++) {
                    size_t index = i % nreg;
                    unsigned int reg = (unsigned int)((registers >> (4u * index)) & 15ULL);
                    size_t offset = at + (kind == 0 ? 16u * i : 8u * i);
                    uint64_t value;
                    if (kind == 0 && reg == 0xe) {
                        uint64_t addr = mzGifRead64(buf + offset + 8);
                        unsigned int reg_id = (unsigned int)(addr & 255ULL);
                        value = mzGifRead64(buf + offset);
                        if (pass == 1) {
                            uint64_t changed = mzGifAdValue(value, reg_id,
                                         policy, &current, &tally);
                            if (value != changed)
                                mzGifWrite64(buf + offset, changed);
                        }
                    } else if (reg == 0x00) {
                        value = mzGifRead64(buf + offset);
                        if (pass == 1) {
                            uint64_t changed = mzGifAdValue(value,
                                               MZ_GS_PRIM, policy,
                                               &current, &tally);
                            if (value != changed)
                                mzGifWrite64(buf + offset, changed);
                        }
                    }
                }
            }
            tally.tags++;
            at += payload;
            saw_end = end;
            if (end && at < packet_size) {
                /* One DMA payload may contain several complete GIF
                 * command lists. Stop only on zero-filled DMA padding. */
                size_t j;
                int padding = 1;
                for (j = at; j < packet_size; j++) {
                    if (buf[j] != 0) {
                        padding = 0;
                        break;
                    }
                }
                if (padding)
                    break;
            }
        }
        /* A GIF command list is complete only when its final tag has EOP.
         * Never attempt to rewrite streaming state with an open tag. */
        if (!saw_end)
            return -1;
        if (pass == 1) {
            *stats = tally;
            *state = current;
        }
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
