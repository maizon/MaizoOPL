/*
  Copyright 2009-2010 volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/dia.h"
#include "include/gui.h"
#include "include/lang.h"
#include "include/pad.h"
#include "include/renderman.h"
#include "include/fntsys.h"
#include "include/themes.h"
#include "include/util.h"
#include "include/sound.h"
#include "include/maizo_ui.h"

// UI spacing of the dialogues (pixels between consecutive items)
#define UI_SPACING_H      10
#define UI_SPACING_V      2
// spacer ui element width (simulates tab)
#define UI_SPACER_WIDTH   50
// minimal pixel width of spacer
#define UI_SPACER_MINIMAL 30
// length of breaking line in pixels
#define UI_BREAK_LEN      600
// scroll speed (delay in ms!) when in dialogs
#define DIA_SCROLL_SPEED  300
// scroll speed (delay in ms!) when setting int value
#define DIA_INT_SET_SPEED 100

static int screenWidth;
static int screenHeight;

// Utility stuff
#define KEYB_MODE   2
#define KEYB_WIDTH  12
#define KEYB_HEIGHT 4
#define KEYB_ITEMS  (KEYB_WIDTH * KEYB_HEIGHT)

static void diaDrawBoundingBox(int x, int y, int w, int h, int focus)
{
    if (thmGetGuiValue() == 0) {
        if (focus)
            rmDrawRect(x - 5, y + 1, 3, h > 2 ? h - 2 : 2, MAIZO_ACCENT);
        else
            rmDrawRect(x - 4, y + 1, 2, h > 2 ? h - 2 : 2, MAIZO_DIVIDER);
        return;
    }
    u64 color = focus ? gTheme->selTextColor : gTheme->textColor;

    color |= GS_SETREG_RGBA(0, 0, 0, 0xFF);
    color &= gColFocus;

    rmDrawRect(x - 5, y, w + 10, h + 10, color);
}

int diaShowKeyb(char *text, int maxLen, int hide_text, const char *title)
{
    int i, j, len = strlen(text), selkeyb = 0, x, w;
    int selchar = 0, selcommand = -1;
    char c[2] = "\0\0", *mask_buffer;
    static const char keyb0[KEYB_ITEMS] = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '\\',
        'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', '`', ':'};

    static const char keyb1[KEYB_ITEMS] = {
        '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '|',
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', '~', '.'};
    const char *keyb = keyb0;

    char *commands[KEYB_HEIGHT] = {_l(_STR_BACKSPACE), _l(_STR_SPACE), _l(_STR_ENTER), _l(_STR_MODE)};
    GSTEXTURE *cmdicons[KEYB_HEIGHT];
    cmdicons[0] = thmGetTexture(SQUARE_ICON);
    cmdicons[1] = thmGetTexture(TRIANGLE_ICON);
    cmdicons[2] = thmGetTexture(START_ICON);
    cmdicons[3] = thmGetTexture(SELECT_ICON);

    rmGetScreenExtents(&screenWidth, &screenHeight);

    if (hide_text) {
        /* Never expose a password when temporary masking allocation fails. */
        if (len >= maxLen)
            len = maxLen > 0 ? maxLen - 1 : 0;
        if ((mask_buffer = malloc(maxLen > 0 ? maxLen : 1)) != NULL) {
            memset(mask_buffer, '*', len);
            mask_buffer[len] = '\0';
        }
    } else {
        mask_buffer = NULL;
    }

    while (1) {
        readPads();

        rmStartFrame();
        guiDrawBGPlasma();
        if (thmGetGuiValue() == 0) {
            rmDrawRect(0, 0, screenWidth, gTheme->usedHeight, MAIZO_BLACK);
            maizoDrawPanel(28, 88, screenWidth - 56,
                           gTheme->usedHeight - 153, 0);
            maizoDrawBrand();
            fntRenderString(gTheme->fonts[0], screenWidth - 32, 20,
                            ALIGN_RIGHT, 190, 27, "TECLADO", MAIZO_MUTED);
        } else
            rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);

        // Title
        if (title != NULL) {
            fntRenderString(gTheme->fonts[0], thmGetGuiValue() == 0 ? 35 : 25,
                            thmGetGuiValue() == 0 ? 71 : 20, ALIGN_NONE,
                            520, 30, title,
                            thmGetGuiValue() == 0 ? MAIZO_TEXT : gTheme->textColor);
            if (thmGetGuiValue() != 0)
                rmDrawLine(25, 38, 615, 38, gColWhite);
        }

        if (thmGetGuiValue() == 0) {
            rmDrawRect(41, 105, screenWidth - 82, 36, MAIZO_CARD);
            rmDrawRect(41, 105, 3, 36, MAIZO_ACCENT);
        }
        fntRenderString(gTheme->fonts[0], 51, 114, ALIGN_NONE,
                        screenWidth - 108, 28,
                        hide_text ? (mask_buffer ? mask_buffer : "") : text,
                        thmGetGuiValue() == 0 ? MAIZO_TEXT : gTheme->textColor);
        if (thmGetGuiValue() != 0)
            rmDrawLine(25, 145, 615, 145, gColWhite);

        for (j = 0; j < KEYB_HEIGHT; j++) {
            for (i = 0; i < KEYB_WIDTH; i++) {
                c[0] = keyb[i + j * KEYB_WIDTH];

                x = 50 + i * 31;
                if (thmGetGuiValue() == 0) {
                    int y = 165 + 34 * j;
                    int selected = (i + j * KEYB_WIDTH) == selchar;
                    rmDrawRect(x - 4, y - 4, 27, 29,
                               selected ? MAIZO_ACCENT : MAIZO_CARD);
                    fntRenderString(gTheme->fonts[0], x + 5, y,
                                    ALIGN_NONE, 17, 24, c,
                                    selected ? MAIZO_TEXT : MAIZO_MUTED);
                } else {
                    w = fntRenderString(gTheme->fonts[0], x,
                                        170 + 3 * UI_SPACING_H * j,
                                        ALIGN_NONE, 0, 0, c,
                                        gTheme->uiTextColor) -
                        x;
                    if ((i + j * KEYB_WIDTH) == selchar)
                        diaDrawBoundingBox(x,
                                           170 + 3 * UI_SPACING_H * j,
                                           w, UI_SPACING_H, 0);
                }
            }
        }

        // Commands
        for (i = 0; i < KEYB_HEIGHT; i++) {
            if (cmdicons[i] && thmGetGuiValue() != 0) {
                int w = (cmdicons[i]->Width * 20) / cmdicons[i]->Height;
                int h = 20;
                rmDrawPixmap(cmdicons[i], 436, 170 + 3 * UI_SPACING_H * i, ALIGN_NONE, w, h, SCALING_RATIO, gDefaultCol);
            }

            x = 477;
            if (thmGetGuiValue() == 0) {
                int y = 165 + 34 * i;
                rmDrawRect(430, y - 4, 166, 29,
                           i == selcommand ? MAIZO_CARD : MAIZO_SURFACE);
                if (i == selcommand)
                    rmDrawRect(430, y - 4, 3, 29, MAIZO_ACCENT);
                fntRenderString(gTheme->fonts[0], x, y, ALIGN_NONE,
                                116, 24, commands[i],
                                i == selcommand ? MAIZO_BRIGHT : MAIZO_MUTED);
            } else {
                w = fntRenderString(gTheme->fonts[0], x,
                                    170 + 3 * UI_SPACING_H * i,
                                    ALIGN_NONE, 0, 0, commands[i],
                                    gTheme->uiTextColor) -
                    x;
                if (i == selcommand)
                    diaDrawBoundingBox(x, 170 + 3 * UI_SPACING_H * i,
                                       w, UI_SPACING_H, 0);
            }
        }

        if (thmGetGuiValue() == 0) {
            rmDrawRect(0, gTheme->usedHeight - 62,
                       screenWidth, 62, MAIZO_SURFACE);
            fntRenderString(gTheme->fonts[0], 33,
                            gTheme->usedHeight - 39, ALIGN_NONE,
                            screenWidth - 65, 26,
                            "SELECIONAR LETRA   |   START CONFIRMAR   |   VOLTAR",
                            MAIZO_MUTED);
        } else
            guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CROSS_ICON : CIRCLE_ICON, _STR_CANCEL, gTheme->fonts[0], 500, 417, gTheme->selTextColor);

        rmEndFrame();

        if (getKey(KEY_LEFT)) {
            sfxPlay(SFX_CURSOR);
            if (selchar > -1) {
                if (selchar % KEYB_WIDTH)
                    selchar--;
                else {
                    selcommand = selchar / KEYB_WIDTH;
                    selchar = -1;
                }
            } else {
                selchar = (selcommand + 1) * KEYB_WIDTH - 1;
                selcommand = -1;
            }
        } else if (getKey(KEY_RIGHT)) {
            sfxPlay(SFX_CURSOR);
            if (selchar > -1) {
                if ((selchar + 1) % KEYB_WIDTH)
                    selchar++;
                else {
                    selcommand = selchar / KEYB_WIDTH;
                    selchar = -1;
                }
            } else {
                selchar = selcommand * KEYB_WIDTH;
                selcommand = -1;
            }
        } else if (getKey(KEY_UP)) {
            sfxPlay(SFX_CURSOR);
            if (selchar > -1)
                selchar = (selchar + KEYB_ITEMS - KEYB_WIDTH) % KEYB_ITEMS;
            else
                selcommand = (selcommand + KEYB_HEIGHT - 1) % KEYB_HEIGHT;
        } else if (getKey(KEY_DOWN)) {
            sfxPlay(SFX_CURSOR);
            if (selchar > -1)
                selchar = (selchar + KEYB_WIDTH) % KEYB_ITEMS;
            else
                selcommand = (selcommand + 1) % KEYB_HEIGHT;
        } else if (getKeyOn(gSelectButton)) {
            if (len < (maxLen - 1) && selchar > -1) {
                sfxPlay(SFX_CONFIRM);
                if (mask_buffer != NULL) {
                    mask_buffer[len] = '*';
                    mask_buffer[len + 1] = '\0';
                }

                len++;
                c[0] = keyb[selchar];
                strcat(text, c);
            } else if (selcommand == 0) {
                sfxPlay(SFX_CANCEL);
                if (len > 0) { // BACKSPACE
                    len--;
                    text[len] = 0;
                    if (mask_buffer != NULL)
                        mask_buffer[len] = '\0';
                }
            } else if (selcommand == 1) {
                sfxPlay(SFX_CONFIRM);
                if (len < (maxLen - 1)) { // SPACE
                    if (mask_buffer != NULL) {
                        mask_buffer[len] = '*';
                        mask_buffer[len + 1] = '\0';
                    }

                    len++;
                    c[0] = ' ';
                    strcat(text, c);
                }
            } else if (selcommand == 2) {
                sfxPlay(SFX_CONFIRM);
                if (mask_buffer != NULL)
                    free(mask_buffer);
                return 1; // ENTER
            } else if (selcommand == 3) {
                sfxPlay(SFX_CONFIRM);
                selkeyb = (selkeyb + 1) % KEYB_MODE; // MODE
                if (selkeyb == 0)
                    keyb = keyb0;
                if (selkeyb == 1)
                    keyb = keyb1;
            }
        } else if (getKey(KEY_SQUARE)) {
            if (len > 0) { // BACKSPACE
                sfxPlay(SFX_CANCEL);
                len--;
                text[len] = 0;
                if (mask_buffer != NULL)
                    mask_buffer[len] = '\0';
            }
        } else if (getKey(KEY_TRIANGLE)) {
            if (len < (maxLen - 1) && selchar > -1) { // SPACE
                sfxPlay(SFX_CONFIRM);
                if (mask_buffer != NULL) {
                    mask_buffer[len] = '*';
                    mask_buffer[len + 1] = '\0';
                }

                len++;
                c[0] = ' ';
                strcat(text, c);
            }
        } else if (getKeyOn(KEY_START)) {
            sfxPlay(SFX_CONFIRM);
            if (mask_buffer != NULL)
                free(mask_buffer);
            return 1; // ENTER
        } else if (getKeyOn(KEY_SELECT)) {
            selkeyb = (selkeyb + 1) % KEYB_MODE; // MODE
            sfxPlay(SFX_CONFIRM);
            if (selkeyb == 0)
                keyb = keyb0;
            if (selkeyb == 1)
                keyb = keyb1;
        }

        if (getKey(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
            sfxPlay(SFX_CANCEL);
            break;
        }
    }


    if (mask_buffer != NULL)
        free(mask_buffer);

    return 0;
}

static int colPadSettings[16];

static int diaShowColSel(unsigned char *r, unsigned char *g, unsigned char *b)
{
    int selc = 0;
    int ret = 0;
    unsigned char col[3];

    padStoreSettings(colPadSettings);

    col[0] = *r;
    col[1] = *g;
    col[2] = *b;
    setButtonDelay(KEY_LEFT, 1);
    setButtonDelay(KEY_RIGHT, 1);

    while (1) {
        readPads();

        rmStartFrame();
        guiDrawBGPlasma();
        if (thmGetGuiValue() == 0) {
            rmDrawRect(0, 0, screenWidth, gTheme->usedHeight, MAIZO_BLACK);
            maizoDrawBrand();
            maizoDrawPanel(33, 100, screenWidth - 66, 285, 0);
            fntRenderString(gTheme->fonts[0], 34, 73,
                            ALIGN_NONE, 540, 28,
                            "PERSONALIZAR / COR RGB", MAIZO_TEXT);
        } else {
            rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);
            fntRenderString(gTheme->fonts[0], 50, 50, ALIGN_NONE, 0, 0,
                            _l(_STR_COLOR_SELECTION),
                            GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));
        }

        // 3 bars representing the colors...
        size_t co;
        int x, y;

        for (co = 0; co < 3; ++co) {
            unsigned char cc[3] = {0, 0, 0};
            cc[co] = col[co];

            x = 76;
            y = thmGetGuiValue() == 0 ? 162 + co * 67 : 75 + co * 25;
            u64 dcol = GS_SETREG_RGBA(cc[0], cc[1], cc[2], 0x80);
            if (thmGetGuiValue() == 0) {
                static const char *const labels[] = {"VERMELHO", "VERDE", "AZUL"};
                char value[12];
                snprintf(value, sizeof(value), "%d", col[co]);
                fntRenderString(gTheme->fonts[0], x, y - 27, ALIGN_NONE,
                                145, 24, labels[co], MAIZO_MUTED);
                fntRenderString(gTheme->fonts[0], x + 322, y - 27,
                                ALIGN_RIGHT, 50, 24, value, MAIZO_TEXT);
            }
            rmDrawRect(x, y, thmGetGuiValue() == 0 ? 328 : 200, 20,
                       selc == co ? MAIZO_CARD : MAIZO_DIVIDER);
            rmDrawRect(x + 2, y + 2, (thmGetGuiValue() == 0 ? 324 : 190) * col[co] / 255, 16, dcol);
        }

        // target color itself
        u64 dcol = GS_SETREG_RGBA(col[0], col[1], col[2], 0x80);

        x = thmGetGuiValue() == 0 ? 453 : 300;
        y = thmGetGuiValue() == 0 ? 162 : 75;

        rmDrawRect(x, y, 90, 90, MAIZO_DIVIDER);
        rmDrawRect(x + 5, y + 5, 80, 80, dcol);

        if (thmGetGuiValue() == 0) {
            rmDrawRect(0, gTheme->usedHeight - 62,
                       screenWidth, 62, MAIZO_SURFACE);
            fntRenderString(gTheme->fonts[0], 32,
                            gTheme->usedHeight - 39, ALIGN_NONE,
                            screenWidth - 64, 26,
                            "ESQUERDA/DIREITA AJUSTAR   |   CONFIRMAR / VOLTAR",
                            MAIZO_MUTED);
        } else {
            guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CIRCLE_ICON : CROSS_ICON, _STR_OK, gTheme->fonts[0], 420, 417, gTheme->selTextColor);
            guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CROSS_ICON : CIRCLE_ICON, _STR_CANCEL, gTheme->fonts[0], 500, 417, gTheme->selTextColor);
        }

        rmEndFrame();

        if (getKey(KEY_LEFT)) {
            if (col[selc] > 0) {
                col[selc]--;
                sfxPlay(SFX_CURSOR);
            }
        } else if (getKey(KEY_RIGHT)) {
            if (col[selc] < 255) {
                col[selc]++;
                sfxPlay(SFX_CURSOR);
            }
        } else if (getKey(KEY_UP)) {
            if (selc > 0) {
                selc--;
                sfxPlay(SFX_CURSOR);
            }
        } else if (getKey(KEY_DOWN)) {
            if (selc < 2) {
                selc++;
                sfxPlay(SFX_CURSOR);
            }
        } else if (getKeyOn(gSelectButton)) {
            sfxPlay(SFX_CONFIRM);
            *r = col[0];
            *g = col[1];
            *b = col[2];
            ret = 1;
            break;
        } else if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
            sfxPlay(SFX_CANCEL);
            ret = 0;
            break;
        }
    }

    padRestoreSettings(colPadSettings);
    return ret;
}



// ----------------------------------------------------------------------------
// --------------------------- Dialogue handling ------------------------------
// ----------------------------------------------------------------------------
static const char *diaGetLocalisedText(const char *def, int id)
{
    if (id >= 0)
        return _l(id);

    return def;
}

/// returns true if the item is controllable (e.g. a value can be changed on it)
static int diaIsControllable(struct UIItem *ui)
{
    return (ui->enabled && ui->visible && (ui->type >= UI_OK));
}

/// returns true if the given item should be preceded with nextline
static int diaShouldBreakLine(struct UIItem *ui)
{
    return (ui->type == UI_SPLITTER || ui->type == UI_OK || ui->type == UI_BREAK);
}

/// returns true if the given item should be superseded with nextline
static int diaShouldBreakLineAfter(struct UIItem *ui)
{
    return (ui->type == UI_SPLITTER);
}

static void diaDrawHint(int text_id)
{
    int x, y;
    char *text = _l(text_id);

    x = screenWidth - rmUnScaleX(fntCalcDimensions(gTheme->fonts[0], text)) - 10;
    y = gTheme->usedHeight - 62;

    // render hint on the lower side of the screen.
    rmDrawRect(x, y, screenWidth - x, MENU_ITEM_HEIGHT + 10, gColDarker);
    fntRenderString(gTheme->fonts[0], x + 5, y + 5, ALIGN_NONE, 0, 0, text, gTheme->textColor);
}

static int diaItemText(int x, int y, const char *text, u64 color, int measure)
{
    if (measure)
        return x + rmUnScaleX(fntCalcDimensions(gTheme->fonts[0], text));
    return fntRenderString(gTheme->fonts[0], x, y, ALIGN_NONE, 0, 0, text, color);
}

/// renders an ui item (either selected or not)
/// sets width and height of the render into the parameters
static void diaRenderItem(int x, int y, struct UIItem *item, int selected, int haveFocus, int *w, int *h, int measure)
{
    // Don't draw controllable items that are not visible.
    if (!item->visible && item->type >= UI_LABEL)
        return;

    *h = UI_SPACING_H;

    // all texts are rendered up from the given point!
    u64 txtcol;

    if (thmGetGuiValue() == 0)
        txtcol = !item->enabled          ? MAIZO_DISABLED :
                 diaIsControllable(item) ? MAIZO_TEXT :
                                           MAIZO_MUTED;
    else if (diaIsControllable(item))
        txtcol = gTheme->uiTextColor;
    else
        txtcol = gTheme->textColor;

    // let's see what do we have here?
    switch (item->type) {
        case UI_TERMINATOR:
            return;

        case UI_BUTTON:
        case UI_LABEL: {
            // width is text length in pixels...
            const char *txt = diaGetLocalisedText(item->label.text, item->label.stringId);
            if (txt && strlen(txt))
                *w = diaItemText(x, y, txt, txtcol, measure) - x;
            else
                *w = diaItemText(x, y, _l(_STR_NOT_SET), txtcol, measure) - x;

            break;
        }

        case UI_SPLITTER: {
            // a line. Thanks to the font rendering, we need to shift up by one font line
            *w = 0;                          // nothing to render at all
            int ypos = y - UI_SPACING_V / 2; //  gsFont->CharHeight +

            // to ODD lines
            ypos &= ~1;

            if (!measure) {
                if (thmGetGuiValue() == 0)
                    maizoDrawSeparator(x, ypos, screenWidth - x - 35);
                else
                    rmDrawLine(x, ypos, x + UI_BREAK_LEN, ypos, gColWhite);
            }
            break;
        }

        case UI_BREAK:
            *w = 0; // nothing to render at all
            break;

        case UI_SPACER: {
            // next column divisible by spacer
            *w = (UI_SPACER_WIDTH - x % UI_SPACER_WIDTH);

            if (*w < UI_SPACER_MINIMAL) {
                x += *w + UI_SPACER_MINIMAL;
                *w += (UI_SPACER_WIDTH - x % UI_SPACER_WIDTH);
            }

            *h = 0;
            break;
        }

        case UI_OK: {
            const char *txt = _l(_STR_OK);
            *w = diaItemText(x, y, txt, txtcol, measure) - x;
            break;
        }

        case UI_INT: {
            char tmp[10];

            snprintf(tmp, sizeof(tmp), "%d", item->intvalue.current);
            *w = diaItemText(x, y, tmp, txtcol, measure) - x;
            break;
        }

        case UI_STRING: {
            if (strlen(item->stringvalue.text))
                *w = diaItemText(x, y, item->stringvalue.text, txtcol, measure) - x;
            else
                *w = diaItemText(x, y, _l(_STR_NOT_SET), txtcol, measure) - x;
            break;
        }

        case UI_PASSWORD: {
            char stars[32];
            int i;
            int len;

            if (strlen(item->stringvalue.text)) {
                len = min(strlen(item->stringvalue.text), sizeof(stars) - 1);
                for (i = 0; i < len; ++i)
                    stars[i] = '*';

                stars[i] = '\0';
                *w = diaItemText(x, y, stars, txtcol, measure) - x;
            } else
                *w = diaItemText(x, y, _l(_STR_NOT_SET), txtcol, measure) - x;
            break;
        }

        case UI_BOOL: {
            const char *txtval = _l((item->intvalue.current) ? _STR_ON : _STR_OFF);
            *w = diaItemText(x, y, txtval, txtcol, measure) - x;
            break;
        }

        case UI_ENUM: {
            const char *tv = item->intvalue.enumvalues[item->intvalue.current];

            if (!tv)
                tv = _l(_STR_NO_ITEMS);

            *w = diaItemText(x, y, tv, txtcol, measure) - x;
            break;
        }

        case UI_COLOUR: {
            *w = rmWideScale(25);
            *h = 17;

            // Align to the right
            x -= *w;

            if (!measure)
                rmDrawRect(x, y + 3, *w, *h, txtcol);
            u64 dcol = GS_SETREG_RGBA(item->colourvalue.r, item->colourvalue.g, item->colourvalue.b, 0x80);
            if (!measure)
                rmDrawRect(x + 2, y + 5, *w - 4, *h - 4, dcol);

            break;
        }
    }

    if (selected && !measure)
        diaDrawBoundingBox(x, y, *w, *h, haveFocus);

    if (item->fixedWidth != 0) {
        int newSize;
        if (item->fixedWidth < 0)
            newSize = item->fixedWidth * screenWidth / -100;
        else
            newSize = item->fixedWidth;
        if (*w < newSize)
            *w = newSize;
    }

    if (item->fixedHeight != 0) {
        int newSize;
        if (item->fixedHeight < 0)
            newSize = item->fixedHeight * screenHeight / -100;
        else
            newSize = item->fixedHeight;
        if (*h < newSize)
            *h = newSize;
    }
}

/* Layout and rendering share the same measurements. Offscreen rows are only
 * measured, so long configuration pages retain their real controls and focus. */
static int diaLayout(struct UIItem *ui, struct UIItem *cur, int haveFocus, int measure, int scroll, int *focusY)
{
    int oled = thmGetGuiValue() == 0;
    int x0 = oled ? 32 : 20;
    int x = x0, y = (oled ? 110 : 20) - scroll, hmax = 0;
    struct UIItem *item;
    for (item = ui; item->type != UI_TERMINATOR; item++) {
        int w = 0, h = 0;
        /* The old first-line heading is still shown in the Supreme header. */
        if (oled && item == ui && item->type == UI_LABEL &&
            (item + 1)->type == UI_SPLITTER)
            continue;
        if (diaShouldBreakLine(item)) {
            x = x0;
            if (hmax > 0)
                y += hmax + UI_SPACING_H;
            hmax = 0;
        }
        if (item == cur)
            *focusY = y + scroll;
        int hidden = measure || (oled && (y < 103 || y > gTheme->usedHeight - 93));
        diaRenderItem(x, y, item, item == cur, haveFocus, &w, &h, hidden);
        if (w > 0)
            x += w + UI_SPACING_V;
        hmax = h > hmax ? h : hmax;
        if (diaShouldBreakLineAfter(item)) {
            x = x0;
            if (hmax > 0)
                y += hmax + UI_SPACING_H;
            hmax = 0;
        }
    }
    return y + scroll + hmax;
}

/* Every stock OPL dialog uses UIItem IDs and callbacks. Instead of
 * remapping or throwing out legacy options, Supreme uses one new row-based
 * layout for every dialog: Config, controls, PadEmu, VMC, GSM, network,
 * cheats, OSD, keyboard fields and confirmation actions. UIItem remains
 * the controller/model, so saved settings and per-game callbacks survive. */
static int maizoDialogLayout(struct UIItem *ui, struct UIItem *cur,
                             int haveFocus, int scroll, int *focusY,
                             int measure)
{
    const int x = 35, width = screenWidth - 70, rowHeight = 39;
    int y = 122 - scroll;
    struct UIItem *item = ui;

    while (item->type != UI_TERMINATOR) {
        struct UIItem *begin, *end, *p, *firstLabel = NULL;
        const char *caption = NULL;
        int controls = 0, selected = 0, visible = 0;

        if (item == ui && item->type == UI_LABEL &&
            (item + 1)->type == UI_SPLITTER) {
            item += 2; /* Heading already displayed by the header. */
            continue;
        }
        if (item->type == UI_BREAK) {
            item++;
            continue;
        }
        if (item->type == UI_SPLITTER) {
            if (!measure && y >= 112 && y < gTheme->usedHeight - 88)
                maizoDrawSeparator(x + 8, y + 5, width - 16);
            y += 16;
            item++;
            continue;
        }

        begin = item;
        while (item->type != UI_TERMINATOR &&
               item->type != UI_BREAK && item->type != UI_SPLITTER)
            item++;
        end = item;

        for (p = begin; p < end; ++p) {
            if (!p->visible || p->type == UI_SPACER)
                continue;
            visible++;
            if (p->type == UI_LABEL && !firstLabel)
                firstLabel = p;
            if (diaIsControllable(p))
                controls++;
            if (p == cur)
                selected = 1;
        }
        if (!visible)
            continue;

        if (firstLabel)
            caption = diaGetLocalisedText(firstLabel->label.text,
                                          firstLabel->label.stringId);
        else if (begin->type == UI_BUTTON)
            caption = diaGetLocalisedText(begin->label.text,
                                          begin->label.stringId);
        else if (begin->type == UI_OK)
            caption = _l(_STR_OK);

        if (selected)
            *focusY = y + scroll + 12;
        if (measure || y + rowHeight < 115 ||
            y > gTheme->usedHeight - 92) {
            y += rowHeight + 3;
            continue;
        }

        rmDrawRect(x, y, width, rowHeight,
                   selected ? MAIZO_CARD : MAIZO_SURFACE);
        if (selected) {
            rmDrawRect(x, y, 3, rowHeight, MAIZO_ACCENT);
            rmDrawRect(x + 3, y, width - 3, 1, MAIZO_DIVIDER);
        }
        if (controls == 0)
            maizoDrawIcon(MAIZO_ICON_INFO, x + 10, y + 10, 18, MAIZO_MUTED);
        else if (begin->type == UI_BUTTON || begin->type == UI_OK)
            maizoDrawIcon(MAIZO_ICON_CONFIRM, x + 10, y + 10, 18,
                          selected ? MAIZO_BRIGHT : MAIZO_MUTED);
        else
            maizoDrawIcon(MAIZO_ICON_SETTINGS, x + 10, y + 10, 18,
                          selected ? MAIZO_BRIGHT : MAIZO_MUTED);
        fntRenderString(gTheme->fonts[0], x + 39, y + 8,
                        ALIGN_NONE, controls ? width * 49 / 100 : width - 55, 25, caption ? caption : "",
                        selected ? MAIZO_TEXT : MAIZO_MUTED);

        {
            int n = 0, valueLeft = x + width * 57 / 100;
            int valueWidth = x + width - 11 - valueLeft;
            for (p = begin; p < end; ++p) {
                char numeric[24];
                char hidden[32];
                const char *value = NULL;
                int vx, segment, count = controls ? controls : 1;
                if (!p->visible || p->type < UI_OK || !diaIsControllable(p))
                    continue;
                segment = valueWidth / count;
                vx = valueLeft + segment * n;
                numeric[0] = '\0';
                hidden[0] = '\0';
                switch (p->type) {
                    case UI_BOOL:
                        value = p->intvalue.current ? "ON" : "OFF";
                        break;
                    case UI_ENUM:
                        if (p->intvalue.enumvalues)
                            value = p->intvalue.enumvalues[p->intvalue.current];
                        if (!value)
                            value = "--";
                        break;
                    case UI_INT:
                        snprintf(numeric, sizeof(numeric), "%d",
                                 p->intvalue.current);
                        value = numeric;
                        break;
                    case UI_STRING:
                        value = p->stringvalue.text[0] ?
                                    p->stringvalue.text :
                                    "--";
                        break;
                    case UI_PASSWORD: {
                        size_t k, len = strlen(p->stringvalue.text);
                        if (len >= sizeof(hidden))
                            len = sizeof(hidden) - 1;
                        for (k = 0; k < len; k++)
                            hidden[k] = '*';
                        hidden[len] = '\0';
                        value = len ? hidden : "--";
                        break;
                    }
                    case UI_COLOUR:
                        rmDrawRect(vx + segment - 35, y + 11, 26, 17,
                                   GS_SETREG_RGBA(p->colourvalue.r,
                                                  p->colourvalue.g,
                                                  p->colourvalue.b, 0x80));
                        break;
                    case UI_BUTTON:
                        value = "ABRIR";
                        break;
                    case UI_OK:
                        value = "SALVAR";
                        break;
                    default:
                        break;
                }
                if (p == cur) {
                    rmDrawRect(vx - 4, y + 5, segment - 1, 30,
                               haveFocus ? MAIZO_ACCENT : MAIZO_DIVIDER);
                    rmDrawRect(vx - 2, y + 7, segment - 5, 26,
                               MAIZO_CARD);
                }
                if (p->type == UI_BOOL) {
                    rmDrawRect(vx + segment - 47, y + 10, 37, 19,
                               p->intvalue.current ?
                                   MAIZO_ACCENT :
                                   MAIZO_DIVIDER);
                    rmDrawRect(vx + segment - (p->intvalue.current ? 27 : 43),
                               y + 13, 12, 13, MAIZO_TEXT);
                    value = NULL;
                }
                if (value)
                    fntRenderString(gTheme->fonts[0], vx, y + 8,
                                    ALIGN_NONE, segment - 7, 25,
                                    value, p == cur ? MAIZO_BRIGHT : MAIZO_TEXT);
                n++;
            }
        }
        y += rowHeight + 3;
    }
    return y + scroll + 7;
}

/// renders whole ui screen (for given dialog setup)
void diaRenderUI(struct UIItem *ui, short inMenu, struct UIItem *cur, int haveFocus)
{
    guiDrawBGPlasma();

    static struct UIItem *lastUI;
    static int scroll;
    int focusY = 0;
    if (thmGetGuiValue() == 0) {
        int total, maxScroll;
        const int top = 126, bottom = gTheme->usedHeight - 98;
        const char *title = "MAIZO / CONFIGURACOES";
        if (lastUI != ui) {
            scroll = 0;
            lastUI = ui;
        }
        if (ui->type == UI_LABEL) {
            const char *heading = diaGetLocalisedText(ui->label.text,
                                                      ui->label.stringId);
            if (heading && heading[0])
                title = heading;
        }
        total = maizoDialogLayout(ui, cur, haveFocus, 0, &focusY, 1);
        maxScroll = total > bottom ? total - bottom : 0;
        if (cur && focusY - scroll < top)
            scroll = focusY - top;
        if (cur && focusY - scroll > bottom - 21)
            scroll = focusY - (bottom - 21);
        if (scroll < 0)
            scroll = 0;
        if (scroll > maxScroll)
            scroll = maxScroll;

        rmDrawRect(0, 0, screenWidth, gTheme->usedHeight, MAIZO_BLACK);
        rmDrawRect(0, 70, screenWidth, 1, MAIZO_DIVIDER);
        maizoDrawBrand();
        fntRenderString(gTheme->fonts[0], 35, 79, ALIGN_NONE,
                        screenWidth - 70, 30, title, MAIZO_TEXT);
        maizoDialogLayout(ui, cur, haveFocus, scroll, &focusY, 0);
        if (maxScroll > 0) {
            int track = gTheme->usedHeight - 222;
            int thumb = track * (bottom - top) / (total - top);
            if (thumb < 12)
                thumb = 12;
            rmDrawRect(screenWidth - 25, 122, 2, track, MAIZO_DIVIDER);
            rmDrawRect(screenWidth - 25,
                       122 + scroll * (track - thumb) / maxScroll,
                       2, thumb, MAIZO_ACCENT);
        }
        rmDrawRect(0, gTheme->usedHeight - 65,
                   screenWidth, 65, MAIZO_SURFACE);
        rmDrawRect(0, gTheme->usedHeight - 65,
                   screenWidth, 1, MAIZO_DIVIDER);
        fntRenderString(gTheme->fonts[0], 38,
                        gTheme->usedHeight - 42, ALIGN_NONE,
                        screenWidth - 65, 28,
                        gSelectButton == KEY_CIRCLE ?
                            "O SELECIONAR     X VOLTAR   |   MAIZO SUPREME" :
                            "X SELECIONAR     O VOLTAR   |   MAIZO SUPREME",
                        MAIZO_MUTED);
    } else {
        diaLayout(ui, cur, haveFocus, 0, 0, &focusY);
    }

    if (thmGetGuiValue() == 0)
        return; /* Supreme has its own persistent footer and row hints. */

    if ((cur != NULL) && (!haveFocus) && (cur->hintId != -1)) {
        diaDrawHint(cur->hintId);
    }

    int uiHints[2] = {_STR_SELECT, _STR_BACK};
    int uiIcons[2] = {CIRCLE_ICON, CROSS_ICON};
    int uiY = gTheme->usedHeight - 32;
    int uiX = guiAlignSubMenuHints(2, uiHints, uiIcons, gTheme->fonts[0], 12, 2);

    uiX = guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? uiIcons[0] : uiIcons[1], uiHints[0], gTheme->fonts[0], uiX, uiY, gTheme->textColor);
    uiX += 12;
    uiX = guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? uiIcons[1] : uiIcons[0], uiHints[1], gTheme->fonts[0], uiX, uiY, gTheme->textColor);
}

/// sets the ui item value to the default again
static void diaResetValue(struct UIItem *item)
{
    switch (item->type) {
        case UI_INT:
        case UI_BOOL:
            item->intvalue.current = item->intvalue.def;
            return;
        case UI_STRING:
        case UI_PASSWORD:
            strncpy(item->stringvalue.text, item->stringvalue.def, sizeof(item->stringvalue.text));
            return;
        default:
            return;
    }
}

static int diaHandleInput(struct UIItem *item, int *modified)
{
    // circle loses focus, sets old values first
    if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        diaResetValue(item);
        sfxPlay(SFX_CONFIRM);
        return 0;
    }

    // cross loses focus without setting default
    if (getKeyOn(gSelectButton)) {
        sfxPlay(SFX_CONFIRM);
        *modified = 0;
        return 0;
    }

    // UI item type dependant part:
    if (item->type == UI_BOOL) {
        // a trick. Set new value, lose focus
        item->intvalue.current = !item->intvalue.current;
        return 0;
    }
    if (item->type == UI_INT) {
        // to be sure
        setButtonDelay(KEY_UP, DIA_INT_SET_SPEED);
        setButtonDelay(KEY_DOWN, DIA_INT_SET_SPEED);

        // up and down
        if (getKey(KEY_UP)) {
            sfxPlay(SFX_CURSOR);
            if (item->intvalue.current < item->intvalue.max) {
                item->intvalue.current++;
            } else {
                item->intvalue.current = item->intvalue.min; // was "= 0;"
            }
        } else if (getKey(KEY_DOWN)) {
            sfxPlay(SFX_CURSOR);
            if (item->intvalue.current > item->intvalue.min) {
                item->intvalue.current--;
            } else {
                item->intvalue.current = item->intvalue.max;
            }
        } else
            *modified = 0;
    } else if ((item->type == UI_STRING) || (item->type == UI_PASSWORD)) {
        char tmp[32];
        strncpy(tmp, item->stringvalue.text, sizeof(tmp));

        if (item->stringvalue.handler) {
            if (item->stringvalue.handler(tmp, sizeof(tmp)))
                strncpy(item->stringvalue.text, tmp, sizeof(item->stringvalue.text));
        } else {
            if (diaShowKeyb(tmp, sizeof(tmp), item->type == UI_PASSWORD, NULL))
                strncpy(item->stringvalue.text, tmp, sizeof(item->stringvalue.text));
        }

        return 0;
    } else if (item->type == UI_ENUM) {
        int cur = item->intvalue.current;

        if (item->intvalue.enumvalues[cur] == NULL) {
            if (cur > 0)
                item->intvalue.current--;
            else
                return 0;
        }

        if (getKey(KEY_UP) && (item->intvalue.current > 0)) {
            item->intvalue.current--;
            sfxPlay(SFX_CURSOR);
        } else if (getKey(KEY_DOWN) && (item->intvalue.enumvalues[item->intvalue.current + 1] != NULL)) {
            item->intvalue.current++;
            sfxPlay(SFX_CURSOR);
        }

        else {
            *modified = 0;
        }

    } else if (item->type == UI_COLOUR) {
        if (!diaShowColSel(&item->colourvalue.r, &item->colourvalue.g, &item->colourvalue.b))
            *modified = 0;

        return 0;
    }

    return 1;
}

static struct UIItem *diaGetFirstControl(struct UIItem *ui)
{
    struct UIItem *cur = ui;

    while (!diaIsControllable(cur)) {
        if (cur->type == UI_TERMINATOR)
            return ui;

        cur++;
    }

    return cur;
}

static struct UIItem *diaGetLastControl(struct UIItem *ui)
{
    struct UIItem *last = diaGetFirstControl(ui);
    struct UIItem *cur = last;

    while (cur->type != UI_TERMINATOR) {
        cur++;

        if (diaIsControllable(cur))
            last = cur;
    }

    return last;
}

static struct UIItem *diaGetNextControl(struct UIItem *cur, struct UIItem *dflt)
{
    while (cur->type != UI_TERMINATOR) {
        cur++;

        if (diaIsControllable(cur))
            return cur;
    }

    return dflt;
}

static struct UIItem *diaGetPrevControl(struct UIItem *cur, struct UIItem *ui)
{
    struct UIItem *newf = cur;

    while (newf != ui) {
        newf--;

        if (diaIsControllable(newf))
            return newf;
    }

    return cur;
}

/// finds first control on previous line...
static struct UIItem *diaGetPrevLine(struct UIItem *cur, struct UIItem *ui)
{
    struct UIItem *newf = cur;

    int lb = 0;
    int hadCtrl = 0; // had the scanned line any control?

    while (newf != ui) {
        newf--;

        if ((lb > 0) && (diaIsControllable(newf)))
            hadCtrl = 1;

        if (diaShouldBreakLine(newf)) { // is this a line break?
            if (hadCtrl || lb == 0) {
                lb++;
                hadCtrl = 0;
            }
        }

        // twice the break? find first control
        if (lb == 2)
            return diaGetFirstControl(newf);
    }

    return cur;
}

static struct UIItem *diaGetNextLine(struct UIItem *cur, struct UIItem *ui)
{
    struct UIItem *newf = cur;

    int lb = 0;

    while (newf->type != UI_TERMINATOR) {
        newf++;

        if (diaShouldBreakLine(newf)) { // is this a line break?
            lb++;
        }

        if (lb == 1)
            return diaGetNextControl(newf, cur);
    }

    return cur;
}

static int diaPadSettings[16];

static void diaStoreScrollSpeed(void)
{
    padStoreSettings(diaPadSettings);
}

static void diaRestoreScrollSpeed(void)
{
    padRestoreSettings(diaPadSettings);
}

static struct UIItem *diaFindByID(struct UIItem *ui, int id)
{
    while (ui->type != UI_TERMINATOR) {
        if (ui->id == id)
            return ui;

        ui++;
    }

    return NULL;
}

int diaExecuteDialog(struct UIItem *ui, int uiId, short inMenu, int (*updater)(int modified))
{
    rmGetScreenExtents(&screenWidth, &screenHeight);

    struct UIItem *cur = NULL;
    if (uiId != -1)
        cur = diaFindByID(ui, uiId);

    if (!cur)
        cur = diaGetFirstControl(ui);

    // what? no controllable item? Exit!
    if (cur == ui)
        return -1;

    int haveFocus = 0, modified;

    diaStoreScrollSpeed();

    // slower controls for dialogs
    setButtonDelay(KEY_UP, DIA_SCROLL_SPEED);
    setButtonDelay(KEY_DOWN, DIA_SCROLL_SPEED);

    // okay, we have the first selectable item
    // we can proceed with rendering etc. etc.
    while (1) {
        rmStartFrame();
        diaRenderUI(ui, inMenu, cur, haveFocus);
        rmEndFrame();

        readPads();

        if (haveFocus) {
            modified = 1;
            haveFocus = diaHandleInput(cur, &modified);

            if (!haveFocus) {
                setButtonDelay(KEY_UP, DIA_SCROLL_SPEED);
                setButtonDelay(KEY_DOWN, DIA_SCROLL_SPEED);
            }
        } else {
            modified = 0;
            struct UIItem *newf = cur;

            if (getKey(KEY_LEFT)) {
                newf = diaGetPrevControl(cur, ui);
                if (newf == cur)
                    newf = diaGetLastControl(ui);
            }

            if (getKey(KEY_RIGHT)) {
                newf = diaGetNextControl(cur, cur);
                if (newf == cur)
                    newf = diaGetFirstControl(ui);
            }

            if (getKey(KEY_UP)) {
                newf = diaGetPrevLine(cur, ui);
                if (newf == cur)
                    newf = diaGetLastControl(ui);
            }

            if (getKey(KEY_DOWN)) {
                newf = diaGetNextLine(cur, ui);
                if (newf == cur)
                    newf = diaGetFirstControl(ui);
            }

            if (newf != cur) {
                // Navigation change detected
                sfxPlay(SFX_CURSOR);
                cur = newf;
            }

            // Cancel button breaks focus or exits with false result
            if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
                diaRestoreScrollSpeed();
                sfxPlay(SFX_CANCEL);
                return UIID_BTN_CANCEL;
            }

            // see what key events we have
            if (getKeyOn(gSelectButton)) {
                haveFocus = 1;
                sfxPlay(SFX_CONFIRM);

                if (cur->type == UI_BUTTON) {
                    diaRestoreScrollSpeed();
                    return cur->id;
                }

                if (cur->type == UI_OK) {
                    diaRestoreScrollSpeed();
                    return UIID_BTN_OK;
                }
            }
        }

        if (updater) {
            int updResult = updater(modified);
            if (updResult)
                return updResult;
        }
    }
}

void diaSetEnabled(struct UIItem *ui, int id, int enabled)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return;

    item->enabled = enabled;
}

void diaSetVisible(struct UIItem *ui, int id, int visible)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return;

    item->visible = visible;
}

void diaSetItemType(struct UIItem *ui, int id, UIItemType type)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return;

    item->type = type;
}

int diaGetInt(struct UIItem *ui, int id, int *value)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if ((item->type == UI_INT) || (item->type == UI_BOOL) || (item->type == UI_ENUM)) {
        *value = item->intvalue.current;
        return 1;
    }

    return 0;
}

int diaSetInt(struct UIItem *ui, int id, int value)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if ((item->type == UI_INT) || (item->type == UI_BOOL) || (item->type == UI_ENUM)) {
        item->intvalue.def = value;
        item->intvalue.current = value;
        return 1;
    }

    return 0;
}

int diaGetString(struct UIItem *ui, int id, char *value, int length)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if ((item->type == UI_STRING) || (item->type == UI_PASSWORD)) {
        strncpy(value, item->stringvalue.text, length);
        return 1;
    }

    return 0;
}

int diaSetString(struct UIItem *ui, int id, const char *text)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if ((item->type == UI_STRING) || (item->type == UI_PASSWORD)) {
        strncpy(item->stringvalue.def, text, sizeof(item->stringvalue.def));
        item->stringvalue.def[sizeof(item->stringvalue.def) - 1] = '\0';
        strncpy(item->stringvalue.text, text, sizeof(item->stringvalue.text));
        item->stringvalue.text[sizeof(item->stringvalue.text) - 1] = '\0';
        return 1;
    }

    return 0;
}

int diaGetColor(struct UIItem *ui, int id, unsigned char *col)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if (item->type != UI_COLOUR)
        return 0;

    col[0] = item->colourvalue.r;
    col[1] = item->colourvalue.g;
    col[2] = item->colourvalue.b;
    return 1;
}

int diaSetColor(struct UIItem *ui, int id, const unsigned char *col)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if (item->type != UI_COLOUR)
        return 0;

    item->colourvalue.r = col[0];
    item->colourvalue.g = col[1];
    item->colourvalue.b = col[2];
    return 1;
}

int diaSetU64Color(struct UIItem *ui, int id, u64 col)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if (item->type != UI_COLOUR)
        return 0;

    item->colourvalue.r = col & 0xFF;
    col >>= 8;
    item->colourvalue.g = col & 0xFF;
    col >>= 8;
    item->colourvalue.b = col & 0xFF;
    return 1;
}

int diaSetLabel(struct UIItem *ui, int id, const char *text)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if ((item->type == UI_LABEL) || (item->type == UI_BUTTON)) {
        item->label.text = text;
        return 1;
    }

    return 0;
}

int diaSetEnum(struct UIItem *ui, int id, const char **enumvals)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if (item->type != UI_ENUM)
        return 0;

    item->intvalue.enumvalues = enumvals;
    return 1;
}
