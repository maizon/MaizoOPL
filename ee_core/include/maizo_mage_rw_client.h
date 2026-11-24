/* Optional RenderWare-specific client bridge. An audited hook supplies
 * the original filter mode and the actual classification of the pass.
 * Reuses the existing In-Game BILINEAR option, no new resident code.
 */
#ifndef MAIZO_MAGE_RW_CLIENT_H
#define MAIZO_MAGE_RW_CLIENT_H
#include "maizo_gs_client.h"
#include "maizo_mage_renderware.h"

static inline unsigned mzMageRwLiveFilter(unsigned original, int world_texture,
                                           int mipmaps_known)
{
    if (!mzGsShared || !mzGsShared->enabled)
        return original;
    return mzMageRwFilter(original, mzGsRuntimeMode(MZ_DRAW_BILINEAR),
                          world_texture, mipmaps_known, 0);
}
#endif
