/* Both producer and resident linker use the same fixed experimental
 * boundary. Never grow into ramD0, which some games use. */
#ifndef MAIZO_EE_LAYOUT_H
#define MAIZO_EE_LAYOUT_H
#define MZ_EE_LEGACY_TOP 0x00097000u
#define MZ_EE_EXP_TOP    0x0009b800u
#define MZ_EE_LOW_LIMIT  0x000d0000u
/* Pure, overflow-safe check; size includes table and alignment. */
static inline int mzEeArenaFits(unsigned int start,unsigned int limit,unsigned int size)
{
    return start<limit && size<=limit-start;
}
#endif
