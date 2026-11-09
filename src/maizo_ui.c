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
#ifdef PADEMU
#include <libds34bt.h>
#endif

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
#undef S

int maizoAnimations(void)
{
    int enabled = 1;
    configGetInt(configGetByType(CONFIG_OPL), "maizo_animations", &enabled);
    return enabled != 0;
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
        case MAIZO_ICON_APPS:
            USE(app);
            break;
        case MAIZO_ICON_STAR:
            USE(star);
            break;
        case MAIZO_ICON_SETTINGS:
            USE(settings);
            break;
        case MAIZO_ICON_USB:
            USE(usb);
            break;
        case MAIZO_ICON_HDD:
            USE(hdd);
            break;
        case MAIZO_ICON_NETWORK:
            USE(net);
            break;
        case MAIZO_ICON_BLUETOOTH:
            USE(bt);
            break;
        case MAIZO_ICON_PAD:
            USE(pad);
            break;
        case MAIZO_ICON_VIDEO:
            USE(video);
            break;
        case MAIZO_ICON_AUDIO:
            USE(audio);
            break;
        case MAIZO_ICON_INFO:
            USE(info);
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

void maizoDrawBrand(void)
{
    /* Retain the original OPL wallpaper: only a small quiet brand plaque. */
    rmDrawRect(24, 16, 111, 30, MAIZO_SURFACE);
    rmDrawRect(24, 16, 3, 30, MAIZO_ACCENT);
    fntRenderString(gTheme->fonts[0], 34, 20, ALIGN_NONE, 95, 24, "MAIZO", MAIZO_TEXT);
}

void maizoDrawPanel(int x, int y, int width, int height, int focused)
{
    rmDrawRect(x, y, width, height, focused ? MAIZO_CARD : MAIZO_SURFACE);
    if (focused)
        rmDrawRect(x, y, 3, height, MAIZO_ACCENT);
}

#ifdef PADEMU
/* Throttle Bluetooth IOP RPCs; the indicator is live, never guessed from pairing. */
static unsigned int maizoBtStatusFrame;
static int maizoBtPortStatus[2];

static void maizoDrawBluetoothStatus(void)
{
    int i, ready = -1, linking = -1;
    if (!maizoBtStatusFrame || guiFrameId - maizoBtStatusFrame >= 60) {
        maizoBtStatusFrame = guiFrameId ? guiFrameId : 1;
        for (i = 0; i < 2; i++)
            maizoBtPortStatus[i] = ds34bt_get_status(i);
    }
    for (i = 0; i < 2; i++) {
        if (maizoBtPortStatus[i] & DS34BT_STATE_RUNNING) {
            ready = i;
            break;
        }
        if (maizoBtPortStatus[i] & DS34BT_STATE_CONNECTED)
            linking = i;
    }
    if (ready >= 0) {
        maizoDrawIcon(MAIZO_ICON_PAD, 496, 20, 21, MAIZO_ACCENT);
        fntRenderString(gTheme->fonts[0], 525, 22, ALIGN_NONE, 91, 21,
                        ready == 0 ? "DS4 P1" : "DS4 P2", MAIZO_TEXT);
    } else if (linking >= 0) {
        maizoDrawIcon(MAIZO_ICON_BLUETOOTH, 496, 20, 21, MAIZO_MUTED);
        fntRenderString(gTheme->fonts[0], 525, 22, ALIGN_NONE, 91, 21,
                        linking == 0 ? "BT P1..." : "BT P2...", MAIZO_MUTED);
    }
}
#endif

void maizoDrawHeader(int tab)
{
    static int underline;
    static const int positions[] = {92, 175, 254, 329, 436, 551};
    static const char *labels[] = {"TODOS", "PS2", "PS1", "APPS", "FAVORITOS", "AJUSTES"};
    int i, target;
    if (tab < 0 || tab >= MAIZO_TAB_COUNT)
        return;
    /* Single slim strip; no tall, overlapping navbar or new background asset. */
    rmDrawRect(24, 66, 592, 41, MAIZO_SURFACE);
    for (i = 0; i < MAIZO_TAB_COUNT; i++)
        fntRenderString(gTheme->fonts[0], positions[i], 75, ALIGN_CENTER,
                        i == MAIZO_FAVORITES ? 106 : 80, 25,
                        labels[i], i == tab ? MAIZO_TEXT : MAIZO_MUTED);
    target = positions[tab] - 20;
    if (!underline || !maizoAnimations() || abs(target - underline) < 4)
        underline = target;
    else
        underline += (target - underline) / 3;
    rmDrawRect(underline, 104, 40, 3, MAIZO_ACCENT);
    maizoDrawBrand();
#ifdef PADEMU
    maizoDrawBluetoothStatus();
#endif
}
