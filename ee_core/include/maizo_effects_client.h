/* Renderer-owned optional pass interface, ABI 4. Include before any
 * resident runtime headers. Apply BEFORE emitting draw commands or
 * allocating temporary resources. Always perform required cleanup and
 * preserve simulation, HUD, shared textures, depth and framebuffer swaps.
 * Never call with certified_optional=1 based solely on a GS heuristic. */
#ifndef MAIZO_EFFECTS_CLIENT_H
#define MAIZO_EFFECTS_CLIENT_H
#include "maizo_gs_client.h"
static inline int mzGsClientEffectsAttach(unsigned caps,unsigned reduced)
{
    return mzGsShared && mzGsEffectsAttach(caps,reduced);
}
static inline uint32_t mzGsClientEffectStrength(unsigned id,uint32_t original,
                                                int certified_optional)
{
    if(!mzGsShared||!certified_optional)return original;
    return mzEffectStrength(mzGsEffectGet(id),original,1);
}
static inline int mzGsClientEffectShouldRun(unsigned id,int certified_optional)
{
    return !mzGsShared||!certified_optional||mzGsEffectGet(id)!=MZ_EFFECT_OFF;
}
#endif
