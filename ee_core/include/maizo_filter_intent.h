/* User requests are independent of permission to touch game state.
 * OFF releases MAIZO's override. FORCED never bypasses packet ownership,
 * renderer certification, optional-pass or scoped AA1 checks. */
#ifndef MAIZO_FILTER_INTENT_H
#define MAIZO_FILTER_INTENT_H
enum { MZ_INTENT_OFF, MZ_INTENT_AUTO, MZ_INTENT_FORCE, MZ_INTENT_ALT };
static inline unsigned mzIntentGet(unsigned packed,unsigned index)
{
    return (packed>>(2u*index))&3u;
}
static inline unsigned mzIntentPut(unsigned packed,unsigned index,unsigned intent)
{
    return (packed&~(3u<<(2u*index)))|((intent&3u)<<(2u*index));
}
static inline unsigned mzIntentNext(unsigned intent)
{
    return intent>=MZ_INTENT_FORCE?MZ_INTENT_OFF:intent+1u;
}
/* Five existing drawing-policy slots; ALT preserves previous manual
 * force-off/nearest, intermediate scanlines, and performance LOD. */
static inline unsigned mzIntentDrawMode(unsigned index,unsigned intent)
{
    if(!intent)return 0;
    if(intent==MZ_INTENT_ALT)return 2;
    if(index==0)return 3; /* both requests keep scoped adaptive AA1 */
    if(index==1)return intent==MZ_INTENT_AUTO?3u:1u;
    if(index==3)return intent==MZ_INTENT_AUTO?1u:3u;
    return 1;
}
/* MAEC OFF = original effect; AUTO = certified reduced if possible,
 * otherwise certified suppression; FORCE = certified suppression.
 * An absent capability still resolves to original at the caller. */
static inline unsigned mzIntentEffectMode(unsigned intent,int reduced)
{
    return !intent?0u:(intent==MZ_INTENT_AUTO&&reduced?1u:2u);
}
#endif
