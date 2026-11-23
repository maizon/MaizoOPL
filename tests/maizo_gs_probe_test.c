/* GS live probe: hardware state sampled, not transfer interception. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "maizo_gs_probe.h"

int main(void)
{
    mz_gs_probe_t p;
    memset(&p, 0xff, sizeof(p));
    mzGsProbeReset(&p);
    assert(p.samples == 0u && p.path1_active == 0u);
    /* OPH=1 and APATH=1 is current PATH1 (VU1 XGKICK).
       VIF1 DMAC may independently be active/blocked. */
    mzGsProbeSample(&p, (1u << 9) | (1u << 10),
                    (1u << 8), 0u);
    assert(p.samples == 1u && p.any_active == 1u);
    assert(p.path1_active == 1u && p.vif1_dma_active == 1u);
    assert(p.path2_active == 0u && p.path3_active == 0u);

    mzGsProbeSample(&p, (1u << 9) | (2u << 10),
                    (1u << 8), (1u << 8));
    assert(p.path2_active == 1u);
    assert(p.vif1_dma_active == 2u);
    assert(p.gif_dma_active == 1u);

    /* PATH3 output plus all queued bits; queued != finished DMA. */
    mzGsProbeSample(&p, (1u << 9) | (3u << 10) | (7u << 6),
                    0u, (1u << 8));
    assert(p.path3_active == 1u && p.gif_queued == 1u);
    assert(p.any_active == 3u && p.gif_dma_active == 2u);

    /* Never classify old APATH bits while GIF output PATH is idle. */
    mzGsProbeSample(&p, (3u << 10), 0u, 0u);
    assert(p.samples == 4u);
    assert(p.path3_active == 1u && p.any_active == 3u);

    /* Counter arithmetic saturates: no wraparound to fake zero samples. */
    p.samples = UINT32_MAX;
    p.path1_active = UINT32_MAX;
    mzGsProbeSample(&p, (1u << 9) | (1u << 10), 0u, 0u);
    assert(p.samples == UINT32_MAX);
    assert(p.path1_active == UINT32_MAX);

    puts("MAIZO GS probe: read-only path sample classification PASS");
    return 0;
}
