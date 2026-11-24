/* Pure synchronization policy for the restoration transaction. */
#ifndef MZ_RESTORE_SYNC_H
#define MZ_RESTORE_SYNC_H
#include <stdint.h>
#define MZR_CD_IDLE 0x4d5a4300u
#define MZR_CD_BUSY 0x4d5a4301u
#define MZR_CD_UNKNOWN 0x4d5a4302u
static inline int mzrDiscQuiet(uint32_t result){return result==MZR_CD_IDLE;}
/* Thread IDs are reusable. Never resume a different thread using a
 * stale suspension bit. Store the observed entry and stack, not a hash. */
typedef struct {uintptr_t entry,stack;unsigned size;} mzr_thread_identity;
static inline int mzrSameThread(const mzr_thread_identity *a,
                                const mzr_thread_identity *b)
{
    return a->entry==b->entry && a->stack==b->stack && a->size==b->size;
}
static inline int mzrCanResume(unsigned state)
{
    return state==8u || state==12u; /* SUSPEND / WAITSUSPEND */
}
#endif
