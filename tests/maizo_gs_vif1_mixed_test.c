/* Mixed VIF1 PATH2 bounds, opaque-VU data and atomic GIF edits. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "maizo_gs_vif1_mixed.h"

static void code(uint8_t *b, size_t at, uint32_t value)
{
    memcpy(b + at, &value, sizeof(value));
}
static void gif_prim(uint8_t *b, size_t at, uint64_t prim)
{
    mzGifWrite64(b + at, 1ULL | (1ULL << 15) | (1ULL << 60));
    mzGifWrite64(b + at + 8, 0xeULL);
    mzGifWrite64(b + at + 16, prim);
    mzGifWrite64(b + at + 24, MZ_GS_PRIM);
}
int main(void)
{
    uint8_t b[256] __attribute__((aligned(16)));
    uint8_t snapshot[256];
    mz_gs_gif_policy_t policy = {0};
    mz_gs_gif_state_t state = {0};
    mz_gs_dma_metrics_t metrics = {0};
    mz_gs_dma_guard_t guard = {MZ_GS_PATH2_VIF1, 1, 1, 0, 0};
    const uint64_t tri = 3ULL | (1ULL << 6);

    policy.aa1 = MZ_GS_FORCE_ON;
    memset(b, 0, sizeof(b));
    code(b, 0, (0x01u << 24) | 0x0101u); /* STCYCL CL1 WL1 */
    code(b, 4, (0x6cu << 24) | (1u << 16) | 0x3fcu); /* UNPACK V4_32x1 */
    memset(b + 8, 0xa5, 16); /* Keep opaque VU data byte-for-byte */
    code(b, 24, (0x14u << 24) | 0x12u); /* MSCAL, not a PATH1 hook */
    code(b, 28, (0x50u << 24) | 2u);
    gif_prim(b, 32, tri);
    code(b, 64, (0x30u << 24)); /* STROW: 4x32 bits opaque */
    memset(b + 68, 0x3c, 16);
    code(b, 84, (0x51u << 24) | 2u);
    gif_prim(b, 88, tri);
    memcpy(snapshot, b, 128);
    assert(mzGsDmaPatchVif1Mixed(b, 128, &policy, &guard, &state, &metrics)
           == MZ_GS_DMA_OK);
    assert(mzGifRead64(b + 48) == (tri | 128ULL));
    assert(mzGifRead64(b + 104) == (tri | 128ULL));
    assert(!memcmp(b, snapshot, 48));
    assert(!memcmp(b + 56, snapshot + 56, 48));
    assert(!memcmp(b + 112, snapshot + 112, 16));
    assert(metrics.accepted == 1 && metrics.edits.primitive_changes == 2);

    /* STCYCL with WL > CL means fewer source vectors than the write
     * count: for NUM6, CL2, WL4, consume 2 + 2 = 4 input vectors. */
    memset(b, 0, sizeof(b));
    code(b, 0, (0x01u << 24) | (4u << 8) | 2u);
    code(b, 4, (0x6cu << 24) | (6u << 16));
    memset(b + 8, 0xd7, 4 * 16);
    code(b, 72, (0x50u << 24) | 2u);
    gif_prim(b, 76, tri);
    memcpy(snapshot, b, 112);
    assert(mzGsDmaPatchVif1Mixed(b, 112, &policy, &guard, &state, &metrics)
           == MZ_GS_DMA_OK);
    assert(mzGifRead64(b + 92) == (tri | 128ULL));
    assert(!memcmp(b, snapshot, 92));

    /* An unknown cycle at start makes UNPACK size ambiguous.
     * Reject even when a later DIRECT packet would be syntactically valid. */
    memset(b, 0, sizeof(b));
    code(b, 0, (0x6cu << 24) | (1u << 16));
    memset(b + 4, 0xf7, 16);
    code(b, 20, (0x50u << 24) | 2u);
    gif_prim(b, 24, tri);
    memcpy(snapshot, b, 64);
    assert(mzGsDmaPatchVif1Mixed(b, 64, &policy, &guard, &state, &metrics)
           == MZ_GS_DMA_UNSUPPORTED);
    assert(!memcmp(b, snapshot, 64));

    /* A late invalid DIRECT payload must not partially edit first GIF. */
    memset(b, 0, sizeof(b));
    code(b, 12, (0x50u << 24) | 2u);
    gif_prim(b, 16, tri);
    code(b, 48, (0x51u << 24) | 2u);
    gif_prim(b, 52, tri);
    mzGifWrite64(b + 52, 3ULL | (1ULL << 15) | (2ULL << 58));
    memcpy(snapshot, b, 96);
    assert(mzGsDmaPatchVif1Mixed(b, 96, &policy, &guard, &state, &metrics)
           == MZ_GS_DMA_BAD_PACKET);
    assert(!memcmp(b, snapshot, 96));

    /* Validate fixed-size STMASK, STROW, MPG before the next DIRECT. */
    memset(b, 0, sizeof(b));
    code(b, 0, (0x20u << 24));
    code(b, 4, 0x3c3c3c3cu);
    code(b, 8, (0x31u << 24));
    memset(b + 12, 0x65, 16);
    code(b, 28, (0x4au << 24) | (2u << 16));
    memset(b + 32, 0x5a, 16);
    code(b, 48, (0x50u << 24) | 2u);
    gif_prim(b, 52, tri);
    assert(mzGsDmaPatchVif1Mixed(b, 96, &policy, &guard, &state, &metrics)
           == MZ_GS_DMA_OK);
    assert(mzGifRead64(b + 68) == (tri | 128ULL));

    /* Interrupt/stall and truncated MPG cases refuse without writes. */
    code(b, 48, (0xd0u << 24) | 2u);
    memcpy(snapshot, b, 96);
    assert(mzGsDmaPatchVif1Mixed(b, 96, &policy, &guard, &state, &metrics)
           == MZ_GS_DMA_UNSUPPORTED);
    assert(!memcmp(b, snapshot, 96));

    memset(b, 0, sizeof(b));
    code(b, 12, (0x50u << 24) | 2u);
    gif_prim(b, 16, tri);
    code(b, 48, (0x4au << 24) | (250u << 16));
    memcpy(snapshot, b, 64);
    assert(mzGsDmaPatchVif1Mixed(b, 64, &policy, &guard, &state, &metrics)
           == MZ_GS_DMA_BAD_PACKET);
    assert(!memcmp(b, snapshot, 64));

    /* No owned or idle channel: fail closed without changing the source. */
    memset(b, 0, sizeof(b));
    code(b, 12, (0x50u << 24) | 2u);
    gif_prim(b, 16, tri);
    guard.channel_idle = 0;
    assert(mzGsDmaPatchVif1Mixed(b, 48, &policy, &guard, &state, &metrics)
           == MZ_GS_DMA_SKIPPED);
    assert(mzGifRead64(b + 32) == tri);

    /* Real GS register policy pass: one DIRECT carries FRAME, DIMX,
     * PRIM, TEX1 and DTHE in an A+D packed GIF list. */
    guard.channel_idle = 1;
    memset(&state, 0, sizeof(state));
    memset(b, 0, sizeof(b));
    policy.aa1 = policy.bilinear = policy.dither = MZ_GS_FORCE_ON;
    code(b, 12, (0x50u << 24) | 6u);
    mzGifWrite64(b + 16, 5ULL | (1ULL << 15) | (1ULL << 60));
    mzGifWrite64(b + 24, 0xeULL);
    mzGifWrite64(b + 32, 2ULL << 24); /* framebuffer format PSMCT16 */
    mzGifWrite64(b + 40, MZ_GS_FRAME_1);
    mzGifWrite64(b + 48, 0x1234ULL);
    mzGifWrite64(b + 56, MZ_GS_DIMX);
    mzGifWrite64(b + 64, tri);
    mzGifWrite64(b + 72, MZ_GS_PRIM);
    mzGifWrite64(b + 80, 2ULL << 6); /* TEX1 MMIN=2 (nearest mip) */
    mzGifWrite64(b + 88, MZ_GS_TEX1_1);
    mzGifWrite64(b + 96, 0ULL);
    mzGifWrite64(b + 104, MZ_GS_DTHE);
    assert(mzGsDmaPatchVif1Mixed(b, 112, &policy, &guard, &state, &metrics)
           == MZ_GS_DMA_OK);
    assert(mzGifRead64(b + 64) == (tri | 128ULL));
    assert(((mzGifRead64(b + 80) >> 6) & 7ULL) == 4ULL);
    assert((mzGifRead64(b + 80) & (1ULL << 5)) != 0);
    assert(mzGifRead64(b + 96) == 1ULL);

    puts("MAIZO GS mixed VIF1: STCYCL/UNPACK/MSCAL/MPG/DIRECT/DIRECTHL/GS filters PASS");
    return 0;
}
