/* Minimal MAIZO GS in-game menu. AFL-3.0 / RIPPS2 / RiptOPL credits. */
#ifndef MAIZO_IGRMENU_H
#define MAIZO_IGRMENU_H
#include <tamtypes.h>
#include "maizo_igr_route.h"
/* Return 1 for a coherent, stable pad sample, 0 for retry. */
int maizoIGR_MenuRun(int (*buttons)(u16 *keys, u32 *frame),void (*yield)(void),u32 heldDmac);
#define MZ_IGR_CAPTURE_PROBE 0x80000000u
int maizoIGR_Preflight(u32 heldDmac);
char *maizoIGR_ProfileArg(void);
#ifdef MAIZO_IGR_LIVE
int maizoIGR_LiveRun(int (*buttons)(u16 *,u32 *),int (*yield)(void));
#endif
#endif
