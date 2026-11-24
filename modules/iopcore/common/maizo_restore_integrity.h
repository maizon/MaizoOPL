/* Pure shared restoration integrity checks (EE, IOP and host tests).
 * Hash detects accidental corruption, not adversarial manipulation. */
#ifndef MZR_INTEGRITY_H
#define MZR_INTEGRITY_H
#include <stdint.h>
#define MZR_ROWS 252u
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
