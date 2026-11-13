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
    MAIZO_ICON_CONFIRM,
    MAIZO_ICON_PLAY,
    MAIZO_ICON_FOLDER,
    MAIZO_ICON_FILE,
    MAIZO_ICON_LOCK,
    MAIZO_ICON_CARD,
    MAIZO_ICON_STAR_OUTLINE,
    MAIZO_ICON_REFRESH,
    MAIZO_ICON_WARNING,
    MAIZO_ICON_CHECK,
    MAIZO_ICON_FILTER,
    MAIZO_ICON_SORT,
    MAIZO_ICON_POWER,
    MAIZO_ICON_DEVICE,
    MAIZO_ICON_DOWNLOAD,
    MAIZO_ICON_DELETE,
    MAIZO_ICON_CLOCK,
    MAIZO_ICON_CHEATS,
    MAIZO_ICON_SHIELD,
    MAIZO_ICON_HOME,
    MAIZO_ICON_KEYBOARD
};
int maizoAnimations(void);
int maizoAnimationMode(void);
void maizoDrawIcon(int icon, int x, int y, int size, u64 color);
void maizoDrawBrand(void);
void maizoDrawHeader(int tab);
void maizoDrawPanel(int x, int y, int width, int height, int focused);
void maizoDrawRow(int x, int y, int width, int icon, const char *label, int selected);
void maizoDrawSeparator(int x, int y, int width);
#endif
