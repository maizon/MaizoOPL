/*
 * MAIZO native progressive renderer -> NTSC 480i FRAME scanout bridge.
 *
 * This deliberately does NOT change the game's internal render size,
 * allocate another framebuffer or output an analog 480p signal.
 * Supported case: the game is already rendering in native 480p, GSM
 * forces NTSC 480i, and the game exposes a full-height display buffer.
 * Enable sets SMODE2.INT=1 / FFMD=1, keeping DISPLAY and DISPFB as-is.
 *
 * Source video changes are handled separately in Hook_SetGsCrt: the
 * runtime must restore the original target register/mask on that path.
 * Anything unknown is UNAVAILABLE rather than silently risking a
 * blank CRT or reading past the display buffer.
 */
#ifndef MAIZO_GS_480I_BRIDGE_H
#define MAIZO_GS_480I_BRIDGE_H

#include <stdint.h>

enum { MZ_480I_NATIVE_NTSC = 0x02, MZ_480I_SOURCE_480P = 0x50 };

static inline uint64_t mzGs480iFrameSMODE2(uint64_t original)
{
    /* Modify ONLY INT / FFMD. Keep the DPMS and other register bits. */
    return (original & ~3ULL) | 3ULL;
}

static inline int mzGs480iBridgeCaps(
    unsigned int game_interlace, unsigned int game_mode,
    unsigned int target_interlace, unsigned int target_mode,
    uint64_t source_pmode, uint64_t source_fb1, uint64_t source_fb2,
    uint64_t source_display1, uint64_t source_display2)
{
    unsigned int en;
    uint64_t fb, display;
    unsigned int fbp, fbw, dbx, dby, magh, magv, height, width, psm;
    unsigned int bytes_per_pixel;
    uint64_t scan_end;

    if (game_interlace != 0u || game_mode != MZ_480I_SOURCE_480P ||
        target_interlace != 1u || target_mode != MZ_480I_NATIVE_NTSC)
        return 0;

    en = (unsigned int)(source_pmode & 3ULL);
    /* Only a single read circuit. Dual-circuit blends may read
     * unrelated surfaces and need a renderer-specific adaptation. */
    if (en != 1u && en != 2u)
        return 0;
    fb = en == 1u ? source_fb1 : source_fb2;
    display = en == 1u ? source_display1 : source_display2;
    if (!fb || !display)
        return 0;
    fbp = (unsigned int)(fb & 511ULL);
    fbw = (unsigned int)((fb >> 9) & 63ULL);
    psm = (unsigned int)((fb >> 15) & 31ULL);
    dbx = (unsigned int)((fb >> 32) & 2047ULL);
    dby = (unsigned int)((fb >> 43) & 2047ULL);
    magh = (unsigned int)((display >> 23) & 15ULL);
    magv = (unsigned int)((display >> 27) & 3ULL);
    width = (unsigned int)(((display >> 32) & 4095ULL) + 1u);
    height = (unsigned int)(((display >> 44) & 2047ULL) + 1u);
    /* Display magnification affects how many source pixels it reads. */
    width /= magh + 1u;
    /* Only validate a full-height native source, no frame doubling or
     * imaginary extra lines. The DISPLAY width may be magnified. */
    if (magv || height < 440u || height > 480u || !fbw ||
        width < 512u || width > 704u || dbx >= fbw * 64u ||
        width > fbw * 64u - dbx ||
        dby > 2047u - height || fbp >= 512u ||
        (psm != 0u && psm != 1u && psm != 2u && psm != 10u))
        return 0;
    /* GS VRAM is exactly 4 MiB. A 24-bit display occupies 32-bit
     * VRAM words. Reject readout that would cross the VRAM boundary. */
    bytes_per_pixel = (psm == 2u || psm == 10u) ? 2u : 4u;
    scan_end = (uint64_t)fbp * 8192ULL +
               (uint64_t)(dby + height) * fbw * 64u * bytes_per_pixel;
    if (scan_end > 4ULL * 1024u * 1024u)
        return 0;
    return 1;
}
#endif
