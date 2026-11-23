/* Opt-in PATH1 staging and PATH3 trusted REF chain regression coverage.
 * These APIs intentionally do NOT hook arbitrary game rendering. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "maizo_gs_dma_preflight.h"

static void gif_ad(uint8_t *p, unsigned int reg, uint64_t value)
{
    mzGifWrite64(p, 1ULL | (1ULL << 15) | (1ULL << 60));
    mzGifWrite64(p + 8, 0xeULL);
    mzGifWrite64(p + 16, value);
    mzGifWrite64(p + 24, reg);
}

static void chain_ref_then_end(uint8_t *chain, uint32_t addr, uint64_t prim)
{
    memset(chain, 0, 128);
    mzGifWrite64(chain, 2ULL | (3ULL << 28) | ((uint64_t)addr << 32));
    mzGifWrite64(chain + 16, 2ULL | (7ULL << 28));
    gif_ad(chain + 32, MZ_GS_PRIM, prim);
}

int main(void)
{
    uint8_t chain[128] __attribute__((aligned(16)));
    uint8_t refdata[64] __attribute__((aligned(16)));
    uint8_t snapshot[128];
    uint8_t refcopy[64];
    mz_gs_gif_policy_t policy = {0};
    mz_gs_gif_state_t state = {0};
    mz_gs_dma_metrics_t metrics = {0};
    mz_gs_dma_guard_t guard = {MZ_GS_PATH3_GIF_DMA, 1, 1, 0, 0};
    mz_gs_dma_owned_ref_t refs[2];
    const uint64_t tri = 3ULL | (1ULL << 6);

    policy.aa1 = MZ_GS_FORCE_ON;
    memset(refdata, 0, sizeof(refdata));
    gif_ad(refdata, MZ_GS_PRIM, tri);
    chain_ref_then_end(chain, 0x4000u, tri);
    refs[0].dma_address = 0x4000u;
    refs[0].owned_buffer = refdata;
    refs[0].buffer_bytes = 32u;
    assert(mzGsDmaPatchTrustedRefs(chain, 64, refs, 1,
                                   &policy, &guard, &state, &metrics) == 0);
    assert(mzGifRead64(refdata + 16) == (tri | 128ULL));
    assert(mzGifRead64(chain + 48) == (tri | 128ULL));
    assert(metrics.accepted == 1);

    /* A chain pointing anywhere outside the declared trust list is refused.
     * No first-packet modification is allowed before that rejection. */
    gif_ad(refdata, MZ_GS_PRIM, tri);
    chain_ref_then_end(chain, 0x4000u, tri);
    memcpy(snapshot, chain, 64);
    memcpy(refcopy, refdata, 32);
    refs[0].dma_address = 0x5000u;
    assert(mzGsDmaPatchTrustedRefs(chain, 64, refs, 1,
                                   &policy, &guard, &state, &metrics)
                                   == MZ_GS_DMA_UNSUPPORTED);
    assert(!memcmp(snapshot, chain, 64));
    assert(!memcmp(refcopy, refdata, 32));

    /* Overlapping region descriptors are unsafe even when both are owned. */
    refs[0].dma_address = 0x4000u;
    refs[0].owned_buffer = refdata;
    refs[0].buffer_bytes = 32u;
    refs[1].dma_address = 0x6000u;
    refs[1].owned_buffer = refdata + 16;
    refs[1].buffer_bytes = 32u;
    assert(mzGsDmaPatchTrustedRefs(chain, 64, refs, 2,
                                   &policy, &guard, &state, &metrics)
                                   == MZ_GS_DMA_UNSUPPORTED);
    assert(!memcmp(snapshot, chain, 64));
    assert(!memcmp(refcopy, refdata, 32));

    /* Two different memory regions must not claim the same DMA address. */
    refs[1].dma_address = 0x4000u;
    refs[1].owned_buffer = refdata + 32;
    assert(mzGsDmaPatchTrustedRefs(chain, 64, refs, 2,
                                   &policy, &guard, &state, &metrics)
                                   == MZ_GS_DMA_UNSUPPORTED);

    /* A reference to a tag-chain payload would make a two-pass commit unsafe. */
    refs[0].owned_buffer = chain + 32;
    refs[0].buffer_bytes = 32u;
    assert(mzGsDmaPatchTrustedRefs(chain, 64, refs, 1,
                                   &policy, &guard, &state, &metrics)
                                   == MZ_GS_DMA_UNSUPPORTED);

    /* Guard rails: never edit a packet while its DMA channel is active. */
    refs[0].owned_buffer = refdata;
    guard.channel_idle = 0;
    assert(mzGsDmaPatchTrustedRefs(chain, 64, refs, 1,
                                   &policy, &guard, &state, &metrics)
                                   == MZ_GS_DMA_SKIPPED);
    guard.channel_idle = 1;

    /* PATH1 allows only a cooperative EE staging packet BEFORE VU1 upload. */
    guard.path = MZ_GS_PATH1_XGKICK;
    gif_ad(refdata, MZ_GS_PRIM, tri);
    assert(mzGsDmaPatchVu1Staging(refdata, 32, &policy, &guard,
                                  &state, &metrics) == 0);
    assert(mzGifRead64(refdata + 16) == (tri | 128ULL));
    guard.caller_owns_buffer = 0;
    gif_ad(refdata, MZ_GS_PRIM, tri);
    assert(mzGsDmaPatchVu1Staging(refdata, 32, &policy, &guard,
                                  &state, &metrics) == MZ_GS_DMA_SKIPPED);
    assert(mzGifRead64(refdata + 16) == tri);
    guard.caller_owns_buffer = 1;
    policy.aa1 = MZ_GS_KEEP;
    assert(mzGsDmaPatchVu1Staging(refdata, 32, &policy, &guard,
                                  &state, &metrics) == MZ_GS_DMA_SKIPPED);
    assert(mzGifRead64(refdata + 16) == tri);

    puts("MAIZO GS: trusted REF/REFE + cooperative VU1 staging PASS");
    return 0;
}
