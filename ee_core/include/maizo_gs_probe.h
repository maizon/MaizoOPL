/*
 * MAIZO GS in-game activity probe (read-only, low-overhead).
 *
 * The PS2 GIF has independent PATH1 (XGKICK), PATH2 (VIF1) and
 * PATH3 (GIF DMAC). This optional monitor SAMPLES register state at
 * the rate the ordinary IGR thread already wakes up. It does not hook
 * games or intercept transfers. Counts are sample occurrences, NOT
 * total DMAs, frame counts, or proof a game never used a path.
 *
 * No allocations, GS register writes, interrupts, or DMA manipulation.
 */
#ifndef MAIZO_GS_PROBE_H
#define MAIZO_GS_PROBE_H

#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t samples;
    uint32_t path1_active;
    uint32_t path2_active;
    uint32_t path3_active;
    uint32_t vif1_dma_active;
    uint32_t gif_dma_active;
    uint32_t gif_queued;
    uint32_t any_active;
} mz_gs_probe_t;

static inline void mzGsProbeReset(mz_gs_probe_t *probe)
{
    if (probe)
        memset(probe, 0, sizeof(*probe));
}

static inline uint32_t mzGsProbeBump(uint32_t n)
{
    return n == UINT32_MAX ? n : n + 1u;
}

static inline void mzGsProbeSample(mz_gs_probe_t *probe, uint32_t gif_stat,
                                   uint32_t vif1_chcr, uint32_t gif_chcr)
{
    uint32_t active;
    if (!probe)
        return;
    probe->samples = mzGsProbeBump(probe->samples);

    /* GIF_STAT bits 10:11 name the active PATH. Bit 9 is
     * OPH (output PATH busy), so discard stale APATH when not busy. */
    active = (gif_stat & (1u << 9)) ? (gif_stat >> 10) & 3u : 0u;
    if (active)
        probe->any_active = mzGsProbeBump(probe->any_active);
    if (active == 1u)
        probe->path1_active = mzGsProbeBump(probe->path1_active);
    else if (active == 2u)
        probe->path2_active = mzGsProbeBump(probe->path2_active);
    else if (active == 3u)
        probe->path3_active = mzGsProbeBump(probe->path3_active);

    /* Queue bits 6..8: any PATH pending at this exact sample time. */
    if (gif_stat & (7u << 6))
        probe->gif_queued = mzGsProbeBump(probe->gif_queued);

    /* D1/D2 CHCR.STR alone does NOT establish which GIF PATH is
     * rendering: it can indicate waiting, blocked or chained DMA. */
    if (vif1_chcr & (1u << 8))
        probe->vif1_dma_active = mzGsProbeBump(probe->vif1_dma_active);
    if (gif_chcr & (1u << 8))
        probe->gif_dma_active = mzGsProbeBump(probe->gif_dma_active);
}

extern mz_gs_probe_t mzGsProbe;

#endif
