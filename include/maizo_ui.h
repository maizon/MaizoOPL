#ifndef MAIZO_UI_H
#define MAIZO_UI_H
#include <tamtypes.h>
#include "include/maizo_oled.h"

enum maizo_icon {
    MAIZO_ICON_PS2,
    MAIZO_ICON_PS1,
    MAIZO_ICON_APPS,
    MAIZO_ICON_STAR,
    MAIZO_ICON_SETTINGS,
    MAIZO_ICON_USB,
    MAIZO_ICON_HDD,
    MAIZO_ICON_NETWORK,
    MAIZO_ICON_BLUETOOTH,
    MAIZO_ICON_PAD,
    MAIZO_ICON_VIDEO,
    MAIZO_ICON_AUDIO,
    MAIZO_ICON_INFO,
    MAIZO_ICON_SEARCH,
    MAIZO_ICON_LIBRARY,
    MAIZO_ICON_BACK,
    MAIZO_ICON_CONFIRM
};
int maizoAnimations(void);
int maizoAnimationMode(void);
void maizoDrawIcon(int icon, int x, int y, int size, u64 color);
void maizoDrawBrand(void);
void maizoDrawHeader(int tab);
void maizoDrawPanel(int x, int y, int width, int height, int focused);
#endif
