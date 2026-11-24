/* Shared packed boot choices for frontend and experimental EE core.
 * 0 game original; 1 auto enable; 2 force off; texture 3 trilinear;
 * scanlines 1..3 strength. Unknown/reserved choices are cleared. */
#ifndef MAIZO_MAGE_MODES_H
#define MAIZO_MAGE_MODES_H
static inline unsigned mzMageModes(unsigned v)
{
    v &= 1023u;
    if ((v & 3u) == 3u) v &= ~3u;
    if (((v >> 4) & 3u) == 3u) v &= ~(3u << 4);
    if (((v >> 8) & 3u) == 3u) v &= ~(3u << 8);
    return v;
}
static inline unsigned mzMageModesForCaps(unsigned v, unsigned caps)
{
    unsigned result = 0;
    v = mzMageModes(v);
    for (unsigned i = 0; i < 5; i++)
        if (caps & (1u << i)) result |= v & (3u << (2u * i));
    return result;
}
#endif
