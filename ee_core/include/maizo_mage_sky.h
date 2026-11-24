/* Sky state 9, statically matched against nine commercial game IDs.
 * Experimental live data-table remap; not a universal renderer hook.
 * No direct GS writes. No extra per-frame work. Non-mip cases untouched.
 * The caller may write only after the IGR capture paused game threads.
 */
#ifndef MAIZO_MAGE_SKY_H
#define MAIZO_MAGE_SKY_H
#include <stdint.h>
static inline uint32_t mzSkyHash(uint32_t hash, const volatile uint32_t *p, unsigned n)
{
    while (n--) hash = (hash ^ *p++) * 16777619u;
    return hash;
}
static inline unsigned mzSkyCase(unsigned index, unsigned mode)
{
    /* Zero-based original enum cases 0,1 = non-mip nearest/linear.
     * For mipmapped cases, preserve mip family or select trilinear.
     * Never convert a texture with no requested mip filtering. */
    if (index < 2 || index >= 6 || !mode || mode > 3) return index;
    if (mode == 3) return 5;
    if (mode == 1) return index | 1u;
    return index & ~1u;
}
static inline uint32_t mzSkyTarget(uint32_t state, unsigned i, unsigned mode)
{
    return state + 0x28u + 16u * mzSkyCase(i, mode);
}
static inline int mzSkyTableOK(const volatile uint32_t *table, uint32_t state, unsigned mode)
{
    for (unsigned i=0; i<6; i++)
        if (table[i] != mzSkyTarget(state,i,mode)) return 0;
    return 1;
}
/* Four snapshots and matching executable bytes verify this state-14
 * routine. Original branch tests s0; forced branch follows its normal
 * fog-disabled code path. Never change delay slot or other words. */
#define MZ_SKY_FOG_ORIGINAL 0x12000006u
#define MZ_SKY_FOG_OFF      0x10000006u
static inline int mzSkyFogCodeOK(const volatile uint32_t *code,uint32_t hash)
{
    uint32_t first=code[0];
    /* Two verified state-14 layouts: GP-relative (14 words) and
     * absolute-address flag load (16 words). Normalize only branch
     * opcode, retaining the exact original delay slot and target. */
    uint32_t original=first|0x02000000u;
    if(original!=0x12000006u && original!=0x12000007u)return 0;
    return mzSkyHash((2166136261u^original)*16777619u,code+1,
                     (original&1u)?15u:13u)==hash;
}
/* BLACK SLUS_213.76 (5C891FF1): exact 9-word window confirmed
 * in ELF, gameplay RAM and menu RAM. The one-word change is the
 * documented reload blur override, NOT all blur or all post effects. */
#define MZ_SKY_RELOAD_ORIGINAL 0x24020001u
#define MZ_SKY_RELOAD_OFF      0x24020000u
static inline int mzSkyReloadCodeOK(const volatile uint32_t *code,uint32_t hash)
{
    uint32_t w=code[4],h;
    if(w!=MZ_SKY_RELOAD_ORIGINAL&&w!=MZ_SKY_RELOAD_OFF)return 0;
    h=mzSkyHash(2166136261u,code,4);
    h=(h^MZ_SKY_RELOAD_ORIGINAL)*16777619u;
    return mzSkyHash(h,code+5,4)==hash;
}
void maizoSkySelect(const char *path);
void maizoSkyPrepare(void);
int maizoSkyAvailable(void);
unsigned maizoSkyMode(void);
int maizoSkySetMode(unsigned mode);
int maizoSkyFogAvailable(void);
unsigned maizoSkyFogMode(void);
int maizoSkyFogSet(unsigned mode);
int maizoSkyReloadAvailable(void);
unsigned maizoSkyReloadMode(void);
int maizoSkyReloadSet(unsigned mode);
#endif
