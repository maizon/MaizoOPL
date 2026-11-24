/* MAGE: RenderWare adapter policy, not an automatic hook installer.
 * Only a reviewed driver call site can declare a world texture eligible.
 * No dependency on proprietary SDK headers or game-specific addresses.
 */
#ifndef MAIZO_MAGE_RENDERWARE_H
#define MAIZO_MAGE_RENDERWARE_H
#include <stdint.h>
#include "maizo_gs_gif_filter.h"

/* RenderWare 3.x filter enumerators: 1 nearest, 2 linear,
 * 3 mip nearest, 4 mip linear, 5 linear mip nearest, 6 linear mip linear.
 * 'linear mip' denotes interpolation between mip levels.
 * Keep mip family and refuse modes that require unavailable levels.
 */
static inline unsigned mzMageRwFilter(unsigned original, unsigned mode,
                                      int world_texture, int mipmaps_known,
                                      int trilinear)
{
    if (!world_texture || original < 1 || original > 6 ||
        mode == MZ_GS_KEEP || mode > MZ_GS_TRILINEAR)
        return original;
    if (original >= 3 && !mipmaps_known)
        return original;
    if (mode == MZ_GS_FORCE_OFF)
        return original == 2 ? 1 : original == 4 ? 3 : original == 6 ? 5 : original;
    if ((trilinear || mode == MZ_GS_TRILINEAR) && mipmaps_known && original >= 3)
        return 6;
    return original == 1 ? 2 : original == 3 ? 4 : original == 5 ? 6 : original;
}

/* Per-option fallback chosen by the *verified* renderer, never inferred
 * from a DMA address or an engine-name string. Engine path wins; if the
 * pass cannot be classified as safe, neither method may alter it. */
enum { MZ_MAGE_ORIGINAL, MZ_MAGE_ENGINE, MZ_MAGE_GIF };
static inline unsigned mzMageDrawRoute(unsigned feature, unsigned engine_caps,
                                       unsigned gif_caps, int eligible)
{
    if (!eligible || !feature || (feature & (feature - 1u)))
        return MZ_MAGE_ORIGINAL;
    if (engine_caps & feature)
        return MZ_MAGE_ENGINE;
    if (gif_caps & feature)
        return MZ_MAGE_GIF;
    return MZ_MAGE_ORIGINAL;
}
#endif
