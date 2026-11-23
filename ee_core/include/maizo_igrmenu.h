/* Minimal MAIZO GS in-game menu. AFL-3.0 / RIPPS2 / RiptOPL credits. */
#ifndef MAIZO_IGRMENU_H
#define MAIZO_IGRMENU_H
#include <tamtypes.h>
enum { MAIZO_IGR_RESUME=0, MAIZO_IGR_EXIT=1, MAIZO_IGR_POWEROFF=2, MAIZO_IGR_UNAVAILABLE=3 };
int maizoIGR_MenuRun(u16 (*buttons)(void));
#endif
