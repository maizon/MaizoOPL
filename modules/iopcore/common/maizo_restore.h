/* MAIZO optional restoration protocol. Inspired by RIPPS2/RiptOPL
 * igrmbox (akilluminati47, AFL-3.0). No RPC while a game is paused.
 * A single in-flight request, sequence last, all DMA areas aligned. */
#ifndef MAIZO_RESTORE_WIRE_H
#define MAIZO_RESTORE_WIRE_H
#include <tamtypes.h>
#include "maizo_restore_integrity.h"
#define MZR_MAX (320u*252u*4u)
#define MZR_LINE 1280u
#define MZR_HELLO 0x4d5a4803u
enum { MZR_BEGIN=1, MZR_READ, MZR_END, MZR_HUSH, MZR_WAKE, MZR_KEEP, MZR_WRITE, MZR_PING, MZR_UNHUSH, MZR_PROBE };
typedef struct { u32 op,off,len,ee; u32 pad[3],seq; } __attribute__((aligned(16))) mzr_req;
typedef struct { u32 buf, pad; s32 result; u32 seq; } __attribute__((aligned(16))) mzr_ack;
static inline int mzrRange(u32 off,u32 len,u32 size)
{
    return len && len<=MZR_LINE && !((off|len)&15u) && off<=size && len<=size-off;
}
#endif
