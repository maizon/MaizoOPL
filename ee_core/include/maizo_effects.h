/* MAEC: a renderer advertises only effects that it can independently
 * control. No inference from a texture, alpha value, sprite or frame
 * address; those are shared with HUD, depth, gameplay and other passes.
 * A mode is a request for a verified optional effect before construction,
 * not permission to drop arbitrary GIF/VIF commands. */
#ifndef MAIZO_EFFECTS_H
#define MAIZO_EFFECTS_H
#include <stdint.h>
enum {
    MZ_EFFECT_MOTION_BLUR, MZ_EFFECT_SCREEN_BLUR, MZ_EFFECT_BLOOM,
    MZ_EFFECT_GLOW, MZ_EFFECT_DOF, MZ_EFFECT_GRAIN, MZ_EFFECT_FOG,
    MZ_EFFECT_HEAT_HAZE, MZ_EFFECT_COLOR, MZ_EFFECT_LENS_FLARE,
    MZ_EFFECT_DISTORTION, MZ_EFFECT_SHADOWS, MZ_EFFECT_PARTICLES,
    MZ_EFFECT_REFLECTIONS, MZ_EFFECT_COUNT
};
enum { MZ_EFFECT_ORIGINAL, MZ_EFFECT_REDUCED, MZ_EFFECT_OFF };
#define MZ_EFFECT_ALL ((1u<<MZ_EFFECT_COUNT)-1u)
static inline unsigned mzEffectMode(uint32_t modes,unsigned id)
{
    if(id>=MZ_EFFECT_COUNT)return MZ_EFFECT_ORIGINAL;
    unsigned mode=(modes>>(2u*id))&3u;
    return mode<=MZ_EFFECT_OFF?mode:MZ_EFFECT_ORIGINAL;
}
static inline uint32_t mzEffectPut(uint32_t modes,unsigned id,unsigned mode)
{
    if(id>=MZ_EFFECT_COUNT||mode>MZ_EFFECT_OFF)return modes;
    return (modes&~(3u<<(2u*id)))|(mode<<(2u*id));
}
static inline unsigned mzEffectNext(unsigned mode,int reduced)
{
    return mode==MZ_EFFECT_ORIGINAL?(reduced?MZ_EFFECT_REDUCED:MZ_EFFECT_OFF):
           mode==MZ_EFFECT_REDUCED?MZ_EFFECT_OFF:MZ_EFFECT_ORIGINAL;
}
static inline unsigned mzEffectAllowed(unsigned caps,unsigned reduced,
                                       uint32_t modes,unsigned id)
{
    if(id>=MZ_EFFECT_COUNT||!(caps&(1u<<id)))return MZ_EFFECT_ORIGINAL;
    unsigned mode=mzEffectMode(modes,id);
    return mode==MZ_EFFECT_REDUCED&&!(reduced&(1u<<id))?MZ_EFFECT_ORIGINAL:mode;
}
static inline uint32_t mzEffectStrength(unsigned mode,uint32_t original,
                                        int certified_optional)
{
    if(!certified_optional)return original;
    if(mode==MZ_EFFECT_OFF)return 0;
    if(mode==MZ_EFFECT_REDUCED)return original/2u+(original&1u);
    return original;
}
#endif
