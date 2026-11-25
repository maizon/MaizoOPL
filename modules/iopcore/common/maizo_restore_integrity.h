/* Pure shared restoration integrity checks (EE, IOP and host tests).
 * Hash detects accidental corruption, not adversarial manipulation. */
#ifndef MZR_INTEGRITY_H
#define MZR_INTEGRITY_H
#include <stdint.h>
#define MZR_ROWS 252u
#define MZR_IOP_RESERVE (64u*1024u)
static inline int mzrMayAllocate(uint32_t total,uint32_t largest,uint32_t n)
{
    return n && largest>=n && total>=MZR_IOP_RESERVE &&
        n<=total-MZR_IOP_RESERVE;
}
/* Reject inconsistent metadata, independently of the pixel hash. */
static inline uint32_t mzrRequestTag(uint32_t op,uint32_t off,uint32_t n,
                                     uint32_t ee,uint32_t hash,uint32_t seq)
{
    uint32_t h=0x4d5a5203u;
    h=(h^op)*16777619u;h=(h^off)*16777619u;
    h=(h^n)*16777619u;h=(h^ee)*16777619u;
    h=(h^hash)*16777619u;
    return (h^seq)*16777619u;
}
static inline uint32_t mzrAckTag(uint32_t seq,uint32_t result)
{
    return (0x4d5a4103u^seq)*16777619u^result;
}
static inline uint32_t mzrHash(const void *data,uint32_t n)
{
    const unsigned char *p=(const unsigned char *)data;
    uint32_t h=2166136261u;
    while(n--)h=(h^*p++)*16777619u;
    return h;
}
static inline int mzrAppendOK(uint32_t off,uint32_t n,uint32_t written,uint32_t total)
{
    return n && off==written && off<=total && n<=total-off;
}
static inline int mzrSealable(uint32_t written,uint32_t total)
{
    return total && written==total;
}
#endif
