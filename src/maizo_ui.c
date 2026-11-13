/* Original MAIZO line icons: GS primitives, no runtime SVG or textures. */
#include "include/opl.h"
#include "include/renderman.h"
#include "include/fntsys.h"
#include "include/themes.h"
#include "include/config.h"
#include "include/gui.h"
#include "include/maizo_library.h"
#include "include/maizo_ui.h"
#include <stdlib.h>

typedef struct
{
    signed char x1, y1, x2, y2;
} stroke_t;
#define S(a, b, c, d) \
    {                 \
        a, b, c, d    \
    }
static const stroke_t disc[] = {S(5, 2, 19, 2), S(19, 2, 22, 12), S(22, 12, 19, 22), S(19, 22, 5, 22), S(5, 22, 2, 12), S(2, 12, 5, 2), S(10, 10, 14, 10), S(14, 10, 14, 14), S(14, 14, 10, 14), S(10, 14, 10, 10)};
static const stroke_t app[] = {S(3, 3, 21, 3), S(21, 3, 21, 21), S(21, 21, 3, 21), S(3, 21, 3, 3), S(3, 8, 21, 8), S(7, 12, 11, 16), S(11, 16, 7, 19), S(14, 18, 18, 18)};
static const stroke_t star[] = {S(12, 2, 15, 8), S(15, 8, 22, 9), S(22, 9, 17, 14), S(17, 14, 18, 22), S(18, 22, 12, 18), S(12, 18, 6, 22), S(6, 22, 7, 14), S(7, 14, 2, 9), S(2, 9, 9, 8), S(9, 8, 12, 2)};
static const stroke_t settings[] = {S(3, 5, 21, 5), S(3, 12, 21, 12), S(3, 19, 21, 19), S(8, 2, 8, 8), S(16, 9, 16, 15), S(10, 16, 10, 22)};
static const stroke_t usb[] = {S(12, 22, 12, 3), S(9, 6, 12, 3), S(12, 3, 15, 6), S(12, 17, 6, 13), S(6, 13, 6, 9), S(12, 13, 18, 9), S(18, 9, 18, 6)};
static const stroke_t hdd[] = {S(5, 3, 19, 3), S(19, 3, 22, 20), S(22, 20, 2, 20), S(2, 20, 5, 3), S(3, 15, 21, 15), S(16, 18, 18, 18)};
static const stroke_t net[] = {S(8, 2, 16, 2), S(16, 2, 16, 8), S(16, 8, 8, 8), S(8, 8, 8, 2), S(12, 8, 12, 14), S(4, 14, 20, 14), S(4, 14, 4, 21), S(20, 14, 20, 21), S(1, 21, 7, 21), S(17, 21, 23, 21)};
static const stroke_t bt[] = {S(12, 2, 12, 22), S(12, 2, 19, 7), S(19, 7, 5, 17), S(5, 7, 19, 17), S(19, 17, 12, 22)};
static const stroke_t pad[] = {S(6, 5, 18, 5), S(18, 5, 22, 19), S(22, 19, 17, 16), S(17, 16, 7, 16), S(7, 16, 2, 19), S(2, 19, 6, 5), S(5, 10, 11, 10), S(8, 7, 8, 13), S(16, 10, 18, 10), S(18, 12, 20, 12)};
static const stroke_t video[] = {S(2, 3, 22, 3), S(22, 3, 22, 17), S(22, 17, 2, 17), S(2, 17, 2, 3), S(12, 17, 12, 21), S(6, 21, 18, 21)};
static const stroke_t audio[] = {S(3, 9, 7, 9), S(7, 9, 13, 3), S(13, 3, 13, 21), S(13, 21, 7, 15), S(7, 15, 3, 15), S(3, 15, 3, 9), S(17, 7, 20, 12), S(20, 12, 17, 17)};
static const stroke_t info[] = {S(4, 3, 20, 3), S(20, 3, 20, 21), S(20, 21, 4, 21), S(4, 21, 4, 3), S(12, 6, 12, 8), S(10, 11, 12, 11), S(12, 11, 12, 18), S(10, 18, 15, 18)};
static const stroke_t search[] = {S(4, 2, 13, 2), S(13, 2, 17, 7), S(17, 7, 15, 14), S(15, 14, 6, 16), S(6, 16, 2, 10), S(2, 10, 4, 2), S(15, 14, 22, 22)};
static const stroke_t library[] = {S(3, 3, 3, 21), S(8, 3, 8, 21), S(13, 3, 13, 21), S(17, 3, 22, 21), S(2, 21, 23, 21)};
static const stroke_t back[] = {S(11, 4, 3, 12), S(3, 12, 11, 20), S(3, 12, 22, 12)};
static const stroke_t confirm[] = {S(3, 12, 9, 19), S(9, 19, 22, 4)};
/* Pixel-aligned 24-unit outline glyphs drawn with GS lines. Keep this
 * compact IOP/EE-friendly set separate from the game artwork texture cache. */
static const stroke_t ps2[] = {S(3, 5, 21, 5), S(21, 5, 21, 18), S(21, 18, 3, 18), S(3, 18, 3, 5), S(6, 8, 12, 8), S(12, 8, 12, 11), S(12, 11, 6, 11), S(6, 11, 6, 14), S(6, 14, 12, 14), S(15, 8, 20, 8), S(15, 8, 15, 11), S(15, 11, 19, 11), S(19, 11, 19, 14), S(15, 14, 20, 14)};
static const stroke_t ps1[] = {S(12, 2, 18, 5), S(18, 5, 22, 12), S(22, 12, 19, 19), S(19, 19, 12, 22), S(12, 22, 5, 19), S(5, 19, 2, 12), S(2, 12, 5, 5), S(5, 5, 12, 2), S(12, 9, 15, 12), S(15, 12, 12, 15), S(12, 15, 9, 12), S(9, 12, 12, 9)};
static const stroke_t play[] = {S(6, 3, 20, 12), S(20, 12, 6, 21), S(6, 21, 6, 3)};
static const stroke_t folder[] = {S(2, 6, 9, 6), S(9, 6, 12, 9), S(12, 9, 22, 9), S(22, 9, 22, 20), S(22, 20, 2, 20), S(2, 20, 2, 6)};
static const stroke_t file[] = {S(5, 2, 15, 2), S(15, 2, 21, 8), S(21, 8, 21, 22), S(21, 22, 5, 22), S(5, 22, 5, 2), S(15, 2, 15, 8), S(15, 8, 21, 8), S(8, 13, 18, 13), S(8, 17, 18, 17)};
static const stroke_t lock[] = {S(5, 10, 19, 10), S(19, 10, 19, 22), S(19, 22, 5, 22), S(5, 22, 5, 10), S(8, 10, 8, 6), S(8, 6, 12, 2), S(12, 2, 16, 6), S(16, 6, 16, 10), S(12, 14, 12, 18)};
static const stroke_t card[] = {S(3, 2, 19, 2), S(19, 2, 22, 5), S(22, 5, 22, 21), S(22, 21, 3, 21), S(3, 21, 3, 2), S(6, 9, 18, 9), S(6, 12, 18, 12), S(6, 16, 9, 16)};
static const stroke_t refresh[] = {S(5, 7, 9, 3), S(9, 3, 16, 3), S(16, 3, 20, 7), S(20, 7, 20, 11), S(17, 8, 20, 11), S(20, 11, 23, 8), S(19, 17, 15, 21), S(15, 21, 8, 21), S(8, 21, 4, 17), S(4, 17, 4, 13), S(1, 16, 4, 13), S(4, 13, 7, 16)};
static const stroke_t warning[] = {S(12, 2, 23, 22), S(23, 22, 1, 22), S(1, 22, 12, 2), S(12, 9, 12, 15), S(12, 19, 12, 20)};
static const stroke_t filter[] = {S(2, 4, 22, 4), S(22, 4, 14, 13), S(14, 13, 14, 21), S(14, 21, 10, 18), S(10, 18, 10, 13), S(10, 13, 2, 4)};
static const stroke_t sort[] = {S(4, 5, 20, 5), S(4, 11, 16, 11), S(4, 17, 12, 17), S(18, 14, 18, 22), S(15, 19, 18, 22), S(18, 22, 21, 19)};
static const stroke_t power[] = {S(12, 2, 12, 13), S(7, 5, 3, 10), S(3, 10, 3, 16), S(3, 16, 8, 21), S(8, 21, 16, 21), S(16, 21, 21, 16), S(21, 16, 21, 10), S(21, 10, 17, 5)};
static const stroke_t device[] = {S(4, 3, 20, 3), S(20, 3, 20, 21), S(20, 21, 4, 21), S(4, 21, 4, 3), S(8, 7, 16, 7), S(8, 11, 16, 11), S(8, 15, 16, 15)};
static const stroke_t download[] = {S(12, 2, 12, 16), S(7, 11, 12, 16), S(12, 16, 17, 11), S(4, 17, 4, 22), S(4, 22, 20, 22), S(20, 22, 20, 17)};
static const stroke_t del[] = {S(5, 6, 19, 6), S(8, 3, 16, 3), S(7, 6, 8, 21), S(8, 21, 16, 21), S(16, 21, 17, 6), S(11, 10, 11, 18), S(14, 10, 14, 18)};
static const stroke_t clock_icon[] = {S(12, 2, 19, 5), S(19, 5, 22, 12), S(22, 12, 19, 19), S(19, 19, 12, 22), S(12, 22, 5, 19), S(5, 19, 2, 12), S(2, 12, 5, 5), S(5, 5, 12, 2), S(12, 6, 12, 12), S(12, 12, 17, 15)};
static const stroke_t cheats[] = {S(4, 3, 20, 3), S(20, 3, 20, 21), S(20, 21, 4, 21), S(4, 21, 4, 3), S(8, 8, 16, 8), S(8, 13, 16, 13), S(8, 18, 12, 18)};
static const stroke_t shield[] = {S(12, 2, 21, 6), S(21, 6, 20, 16), S(20, 16, 12, 22), S(12, 22, 4, 16), S(4, 16, 3, 6), S(3, 6, 12, 2), S(8, 12, 11, 15), S(11, 15, 16, 9)};
static const stroke_t home[] = {S(2, 12, 12, 3), S(12, 3, 22, 12), S(5, 10, 5, 21), S(5, 21, 19, 21), S(19, 21, 19, 10), S(10, 21, 10, 14), S(10, 14, 14, 14), S(14, 14, 14, 21)};
static const stroke_t keyboard[] = {S(2, 5, 22, 5), S(22, 5, 22, 19), S(22, 19, 2, 19), S(2, 19, 2, 5), S(5, 9, 7, 9), S(10, 9, 12, 9), S(15, 9, 17, 9), S(5, 13, 7, 13), S(10, 13, 12, 13), S(15, 13, 17, 13), S(7, 16, 17, 16)};
#undef S
#include "include/maizo_tabler_strokes.h"

int maizoAnimationMode(void)
{
    int mode = 2;
    config_set_t *config = configGetByType(CONFIG_OPL);
    if (configGetInt(config, "maizo_animation_mode", &mode) && mode >= 0 && mode <= 2)
        return mode;
    mode = 1;
    configGetInt(config, "maizo_animations", &mode);
    return mode ? 2 : 0;
}

int maizoAnimations(void)
{
    return maizoAnimationMode() != 0;
}

void maizoDrawIcon(int icon, int x, int y, int size, u64 color)
{
    const stroke_t *lines = disc;
    unsigned int count = sizeof(disc) / sizeof(*disc), i;
#define USE(name)                             \
    do {                                      \
        lines = name;                         \
        count = sizeof(name) / sizeof(*name); \
    } while (0)
    switch (icon) {
        case MAIZO_ICON_PS2:
            USE(ps2);
            break;
        case MAIZO_ICON_PS1:
            USE(ps1);
            break;
        case MAIZO_ICON_PLAY:
            USE(play);
            break;
        case MAIZO_ICON_FOLDER:
            USE(tabler_folder);
            break;
        case MAIZO_ICON_FILE:
            USE(file);
            break;
        case MAIZO_ICON_LOCK:
            USE(lock);
            break;
        case MAIZO_ICON_CARD:
            USE(card);
            break;
        case MAIZO_ICON_STAR_OUTLINE:
            USE(star);
            break;
        case MAIZO_ICON_REFRESH:
            USE(refresh);
            break;
        case MAIZO_ICON_WARNING:
            USE(warning);
            break;
        case MAIZO_ICON_CHECK:
            USE(confirm);
            break;
        case MAIZO_ICON_FILTER:
            USE(filter);
            break;
        case MAIZO_ICON_SORT:
            USE(sort);
            break;
        case MAIZO_ICON_POWER:
            USE(power);
            break;
        case MAIZO_ICON_DEVICE:
            USE(device);
            break;
        case MAIZO_ICON_DOWNLOAD:
            USE(download);
            break;
        case MAIZO_ICON_DELETE:
            USE(del);
            break;
        case MAIZO_ICON_CLOCK:
            USE(clock_icon);
            break;
        case MAIZO_ICON_CHEATS:
            USE(cheats);
            break;
        case MAIZO_ICON_SHIELD:
            USE(shield);
            break;
        case MAIZO_ICON_HOME:
            USE(home);
            break;
        case MAIZO_ICON_KEYBOARD:
            USE(keyboard);
            break;
        case MAIZO_ICON_APPS:
            USE(app);
            break;
        case MAIZO_ICON_STAR:
            USE(tabler_star);
            break;
        case MAIZO_ICON_SETTINGS:
            USE(tabler_settings);
            break;
        case MAIZO_ICON_USB:
            USE(tabler_usb);
            break;
        case MAIZO_ICON_HDD:
            USE(hdd);
            break;
        case MAIZO_ICON_NETWORK:
            USE(tabler_network);
            break;
        case MAIZO_ICON_BLUETOOTH:
            USE(tabler_bt);
            break;
        case MAIZO_ICON_PAD:
            USE(tabler_pad);
            break;
        case MAIZO_ICON_VIDEO:
            USE(tabler_video);
            break;
        case MAIZO_ICON_AUDIO:
            USE(tabler_audio);
            break;
        case MAIZO_ICON_INFO:
            USE(tabler_info);
            break;
        case MAIZO_ICON_SEARCH:
            USE(search);
            break;
        case MAIZO_ICON_LIBRARY:
            USE(library);
            break;
        case MAIZO_ICON_BACK:
            USE(back);
            break;
        case MAIZO_ICON_CONFIRM:
            USE(confirm);
            break;
        default:
            break;
    }
#undef USE
    for (i = 0; i < count; i++)
        rmDrawLine(x + rmWideScale(lines[i].x1 * size / 24), y + lines[i].y1 * size / 24,
                   x + rmWideScale(lines[i].x2 * size / 24), y + lines[i].y2 * size / 24, color);
}

/* The original MaizoWii logo PNG is not present in either repository.
 * Keep this transparent text fallback until the approved asset is imported. */
void maizoDrawBrand(void)
{
    fntRenderString(gTheme->fonts[0], 32, 20, ALIGN_NONE, 112, 24, "MAIZO", MAIZO_TEXT);
}

void maizoDrawPanel(int x, int y, int width, int height, int focused)
{
    if (width <= 0 || height <= 0)
        return;
    rmDrawRect(x, y, width, height, focused ? MAIZO_CARD : MAIZO_SURFACE);
    if (focused)
        rmDrawRect(x, y, 3, height, MAIZO_ACCENT);
}

void maizoDrawRow(int x, int y, int width, int icon, const char *label, int selected)
{
    if (selected)
        rmDrawRect(x, y + 1, 3, 23, MAIZO_ACCENT);
    if (icon >= 0)
        maizoDrawIcon(icon, x + 10, y + 2, 20, selected ? MAIZO_BRIGHT : MAIZO_MUTED);
    fntRenderString(gTheme->fonts[0], x + (icon >= 0 ? 38 : 12), y + 3,
                    ALIGN_NONE, width - (icon >= 0 ? 44 : 16), 26,
                    label ? label : "", selected ? MAIZO_TEXT : MAIZO_MUTED);
}

void maizoDrawSeparator(int x, int y, int width)
{
    if (width > 0)
        rmDrawRect(x, y, width, 1, MAIZO_DIVIDER);
}

void maizoDrawHeader(int tab)
{
    static const char *const names[] = {
        "TODOS", "PS2", "PS1", "APPS", "FAVORITOS", "CONFIGURACOES"};
    if (tab < 0 || tab >= MAIZO_TAB_COUNT)
        return;

    /* Supreme header: only the MAIZO wordmark and current page, not tabs.
     * L1/R1 still switch pages via the original input callbacks. */
    maizoDrawBrand();
    {
        int width, height;
        rmGetScreenExtents(&width, &height);
        fntRenderString(gTheme->fonts[0], width - 28, 20, ALIGN_RIGHT, 165, 26,
                        names[tab], MAIZO_MUTED);
    }
}
