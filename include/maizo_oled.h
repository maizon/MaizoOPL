#ifndef MAIZO_OLED_H
#define MAIZO_OLED_H

/* GS alpha uses 0x80 for opaque; coordinates use the renderer virtual area. */
#define MAIZO_BLACK   GS_SETREG_RGBA(0, 0, 0, 0x80)
#define MAIZO_SURFACE GS_SETREG_RGBA(20, 20, 20, 0x80)
#define MAIZO_CARD    GS_SETREG_RGBA(28, 28, 30, 0x80)
#define MAIZO_ACCENT  GS_SETREG_RGBA(74, 158, 255, 0x80)
#define MAIZO_TEXT    GS_SETREG_RGBA(255, 255, 255, 0x80)
#define MAIZO_MUTED   GS_SETREG_RGBA(160, 160, 160, 0x80)
#define MAIZO_DIVIDER GS_SETREG_RGBA(48, 48, 48, 0x80)

#endif
