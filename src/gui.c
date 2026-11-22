#include "include/maizo_filemgr.h"
#include "include/cheatconfig.h"
/*
 Copyright 2010, Volca
 Licenced under Academic Free License version 3.0
 Review OpenUsbLd README & LICENSE files for further details.
 */

#include "include/opl.h"
#include "include/gui.h"
#include "include/renderman.h"
#include "include/maizo_oled.h"
#include "include/maizo_ui.h"
#include "include/maizo_library.h"
#include "include/menusys.h"
#include "include/fntsys.h"
#include "include/ioman.h"
#include "include/lang.h"
#include "include/themes.h"
#include "include/pad.h"
#include "include/util.h"
#include "include/config.h"
#include "include/system.h"
#include "include/ethsupport.h"
#include "include/compatupd.h"
#include "include/pggsm.h"
#include "include/cheatman.h"
#include "include/sound.h"
#include "include/guigame.h"
#ifdef PADEMU
#include <libds34usb.h>
#include "include/maizo_profile_config.h"
#endif

#include <limits.h>
#include <stdlib.h>
#include <libvux.h>

// Last Played Auto Start
#include <time.h>

static int gScheduledOps;
static int gCompletedOps;
static int gTerminate;
static int gInitComplete;

static gui_callback_t gFrameHook;

static s32 gSemaId;
static s32 gGUILockSemaId;
static ee_sema_t gQueueSema;

static int screenWidth;
static int screenHeight;

static int showPartPopup = 0;
static int showThmPopup;
static int showLngPopup;

static clock_t popupTimer;

/* Bounded FIFO, so connecting two controllers in one frame does not
 * overwrite the first notification. No heap allocation or modal dialog. */
#define MAIZO_PAD_TOASTS 8
static char padToasts[MAIZO_PAD_TOASTS][80];
static unsigned char padToastConnected[MAIZO_PAD_TOASTS];
static unsigned int padToastHead, padToastCount;
static clock_t padToastEnd;

static void guiQueueControllerNotice(const char *name, int slot, int connected)
{
    unsigned int index;
    if (padToastCount >= MAIZO_PAD_TOASTS)
        return;
    index = (padToastHead + padToastCount) % MAIZO_PAD_TOASTS;
    snprintf(padToasts[index], sizeof(padToasts[index]),
             "Controle %s - %s (P%d)", connected ? "conectado" : "desconectado", name, slot);
    padToastConnected[index] = connected;
    padToastCount++;
}

void guiNotifyController(const char *name, int slot)
{
    guiQueueControllerNotice(name, slot, 1);
}

void guiNotifyControllerDisconnected(const char *name, int slot)
{
    guiQueueControllerNotice(name, slot, 0);
}

void guiDrawControllerNotification(void)
{
    int x, y, width;
    if (!padToastCount || !gTheme || screenWidth < 100)
        return;
    if (!padToastEnd)
        padToastEnd = clock() + 3 * CLOCKS_PER_SEC;
    if (clock() >= padToastEnd) {
        padToastHead = (padToastHead + 1) % MAIZO_PAD_TOASTS;
        padToastCount--;
        padToastEnd = 0;
        return;
    }
    width = screenWidth - 48;
    if (width > 488)
        width = 488;
    x = screenWidth - width - 24;
    y = 72;
    maizoDrawPanel(x, y, width, 42, 0);
    u64 accent = padToastConnected[padToastHead] ? MAIZO_SUCCESS : MAIZO_WARNING;
    rmDrawRect(x, y, 3, 42, accent);
    maizoDrawIcon(MAIZO_ICON_PAD, x + 10, y + 10, 22, accent);
    fntRenderString(gTheme->fonts[0], x + 40, y + 11, ALIGN_NONE,
                    width - 52, 26, padToasts[padToastHead], MAIZO_TEXT);
}

// forward decl.
static void guiShow();

#ifdef __DEBUG

// debug version displays an FPS meter
static clock_t prevtime = 0;
static clock_t curtime = 0;
static float fps = 0.0f;

extern GSGLOBAL *gsGlobal;
#endif

// Global data
int guiInactiveFrames;
int guiFrameId;

struct gui_update_list_t
{
    struct gui_update_t *item;
    struct gui_update_list_t *next;
};

struct gui_update_list_t *gUpdateList;
struct gui_update_list_t *gUpdateEnd;

typedef struct
{
    void (*handleInput)(void);
    void (*renderScreen)(void);
    short inMenu;
} gui_screen_handler_t;

static gui_screen_handler_t screenHandlers[] = {{&menuHandleInputMain, &menuRenderMain, 0},
                                                {&menuHandleInputMenu, &menuRenderMenu, 1},
                                                {&menuHandleInputInfo, &menuRenderInfo, 1},
                                                {&menuHandleInputGameMenu, &menuRenderGameMenu, 1},
                                                {&menuHandleInputAppMenu, &menuRenderAppMenu, 1}};

// default screen handler (menu screen)
static gui_screen_handler_t *screenHandler = &screenHandlers[GUI_SCREEN_MENU];

// screen transition handling
static gui_screen_handler_t *screenHandlerTarget = NULL;
static int transIndex;

// Helper perlin noise data
#define PLASMA_H              32
#define PLASMA_W              32
#define PLASMA_ROWS_PER_FRAME 6
#define FADE_SIZE             256

static GSTEXTURE gBackgroundTex;
static int pperm[512];
static float fadetbl[FADE_SIZE + 1];

static VU_VECTOR pgrad3[12] = {{1, 1, 0, 1}, {-1, 1, 0, 1}, {1, -1, 0, 1}, {-1, -1, 0, 1}, {1, 0, 1, 1}, {-1, 0, 1, 1}, {1, 0, -1, 1}, {-1, 0, -1, 1}, {0, 1, 1, 1}, {0, -1, 1, 1}, {0, 1, -1, 1}, {0, -1, -1, 1}};

void guiReloadScreenExtents()
{
    rmGetScreenExtents(&screenWidth, &screenHeight);
}

void guiInit(void)
{
    guiFrameId = 0;
    guiInactiveFrames = 0;

    gFrameHook = NULL;
    gTerminate = 0;
    gInitComplete = 0;
    gScheduledOps = 0;
    gCompletedOps = 0;

    gUpdateList = NULL;
    gUpdateEnd = NULL;

    gQueueSema.init_count = 1;
    gQueueSema.max_count = 1;
    gQueueSema.option = 0;

    gSemaId = CreateSema(&gQueueSema);
    gGUILockSemaId = CreateSema(&gQueueSema);

    guiReloadScreenExtents();

    // background texture - for perlin
    gBackgroundTex.Width = PLASMA_W;
    gBackgroundTex.Height = PLASMA_H;
    gBackgroundTex.Mem = memalign(128, PLASMA_W * PLASMA_H * 4);
    gBackgroundTex.PSM = GS_PSM_CT32;
    gBackgroundTex.Filter = GS_FILTER_LINEAR;
    gBackgroundTex.Vram = 0;
    gBackgroundTex.VramClut = 0;
    gBackgroundTex.Clut = NULL;
    gBackgroundTex.ClutStorageMode = GS_CLUT_STORAGE_CSM1;

    // Precalculate the values for the perlin noise plasma
    int i;
    for (i = 0; i < 256; ++i) {
        pperm[i] = rand() % 256;
        pperm[i + 256] = pperm[i];
    }

    for (i = 0; i <= FADE_SIZE; ++i) {
        float t = (float)(i) / FADE_SIZE;

        fadetbl[i] = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }
}

void guiEnd()
{
    if (gBackgroundTex.Mem)
        free(gBackgroundTex.Mem);

    DeleteSema(gSemaId);
    DeleteSema(gGUILockSemaId);
}

void guiLock(void)
{
    WaitSema(gGUILockSemaId);
}

void guiUnlock(void)
{
    SignalSema(gGUILockSemaId);
}

void guiStartFrame(void)
{
    guiLock();
    rmStartFrame();
    guiFrameId++;
}

void guiEndFrame(void)
{
    guiDrawControllerNotification();
    rmEndFrame();
#ifdef __DEBUG
    // Measure time directly after vsync
    prevtime = curtime;
    curtime = clock();
#endif
    guiUnlock();
}

void guiShowSupremeDiagnostics(void)
{
    char version[96], mode[96], features[96], video[96];
    snprintf(version, sizeof(version), "OPL: %s", OPL_VERSION);
    snprintf(mode, sizeof(mode), "Tema: MAIZO SUPREME");
    snprintf(video, sizeof(video), "Area da interface: %d x %d",
             screenWidth, gTheme ? gTheme->usedHeight : screenHeight);
#ifdef PADEMU
    snprintf(features, sizeof(features),
             maizoIsPhysicalRescueMode() ?
                 "Somente PS2; segure L1+R1+START para reativar USB/BT" :
                 "PadEmu: ativo; emergencia PS2: segure L1+R1+START");
#else
    snprintf(features, sizeof(features), "PadEmu: nao incluido nesta compilacao");
#endif
    struct UIItem diagnostic[] = {
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"MAIZO SUPREME - Sistema", -1}}},
        {UI_SPLITTER},
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {version, -1}}},
        {UI_BREAK},
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {mode, -1}}},
        {UI_BREAK},
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {features, -1}}},
        {UI_BREAK},
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {video, -1}}},
        {UI_BREAK},
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"Dispositivos USB: Controles > Diagnostico USB", -1}}},
        {UI_BREAK},
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"FPS e consumo de VRAM nao estimados sem medicao.", -1}}},
        {UI_BREAK},
        {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},
        {UI_TERMINATOR}};
    diaExecuteDialog(diagnostic, -1, 1, NULL);
}

void guiShowAbout()
{
    struct UIItem credits[] = {
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"CREDITOS", -1}}},
        {UI_SPLITTER},
        {UI_BUTTON, 7401, 1, 1, -1, 0, 0, {.label = {"Open PS2 Loader - equipe original", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 7402, 1, 1, -1, 0, 0, {.label = {"MAIZON - MAIZO SUPREME", -1}}},
        {UI_BREAK},
        {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},
        {UI_TERMINATOR}};
    struct UIItem maizon[] = {
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"MAIZON - MAIZO SUPREME", -1}}},
        {UI_SPLITTER},
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"Desenvolvimento e personalizacao: MAIZON", -1}}},
        {UI_BREAK},
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"GITHUB.COM/MAIZON", -1}}},
        {UI_BREAK},
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"Instagram.com/Maicon_Nasciment0", -1}}},
        {UI_BREAK},
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"YouTube.com/MaiconGameplays", -1}}},
        {UI_BREAK},
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"Base: Open PS2 Loader e seus colaboradores", -1}}},
        {UI_BREAK},
        {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},
        {UI_TERMINATOR}};
    char OPLVersion[40];
    char OPLBuildDetails[40];
    int selected;

    snprintf(OPLVersion, sizeof(OPLVersion), "Open PS2 Loader %s", OPL_VERSION);
    diaSetLabel(diaAbout, ABOUT_TITLE, OPLVersion);

    snprintf(OPLBuildDetails, sizeof(OPLBuildDetails), "GSM %s"
                                                       " - UDMA+"
#ifdef __RTL
                                                       " - RTL"
#endif
#ifdef IGS
                                                       " - IGS %s"
#endif
#ifdef PADEMU
                                                       " - PADEMU"
#endif
             // Version numbers
             ,
             GSM_VERSION
#ifdef IGS
             ,
             IGS_VERSION
#endif
    );
    diaSetLabel(diaAbout, ABOUT_BUILD_DETAILS, OPLBuildDetails);

    while ((selected = diaExecuteDialog(credits, -1, 1, NULL)) == 7401 || selected == 7402) {
        if (selected == 7401)
            diaExecuteDialog(diaAbout, -1, 1, NULL);
        else
            diaExecuteDialog(maizon, -1, 1, NULL);
    }
}

void guiCheckNotifications(int checkTheme, int checkLang)
{
    if (gEnableNotifications) {
        if (checkTheme) {
            if (thmGetGuiValue() != 0)
                showThmPopup = 1;
        }

        if (checkLang) {
            if (lngGetGuiValue() != 0)
                showLngPopup = 1;
        }
    }
}

static void guiResetNotifications(void)
{
    showThmPopup = 0;
    showLngPopup = 0;
    popupTimer = 0;
}

static void guiRenderNotifications(char *string, int y)
{
    int x;

    x = screenWidth - rmUnScaleX(fntCalcDimensions(gTheme->fonts[0], string)) - 10;
    if (thmGetGuiValue() == 0) {
        if (x < 30)
            x = 30;
        maizoDrawPanel(x - 12, y, screenWidth - x - 16, MENU_ITEM_HEIGHT + 10, 0);
        rmDrawRect(x - 12, y, 3, MENU_ITEM_HEIGHT + 10, MAIZO_ACCENT);
        fntRenderString(gTheme->fonts[0], x, y + 5, ALIGN_NONE,
                        screenWidth - x - 25, 28, string, MAIZO_TEXT);
        return;
    }

    rmDrawRect(x - 10, y, screenWidth - x, MENU_ITEM_HEIGHT + 10, gColDarker);
    fntRenderString(gTheme->fonts[0], x - 5, y + 5, ALIGN_NONE, 0, 0, string, gTheme->textColor);
}

static void guiShowNotifications(void)
{
    char notification[128];
    char *col_pos;
    int y = thmGetGuiValue() == 0 ? gTheme->usedHeight - 184 : 10;
    int yadd = 35;

    if (showPartPopup || showThmPopup || showLngPopup || showCfgPopup) {
        if (!popupTimer) {
            popupTimer = clock() + 5000 * (CLOCKS_PER_SEC / 1000);
            sfxPlay(SFX_MESSAGE);
        }

        if (showPartPopup) {
            col_pos = strchr(gOPLPart, ':');
            col_pos++;

            snprintf(notification, sizeof(notification), _l(_STR_PARTITION_NOTIFICATION), col_pos);
            guiRenderNotifications(notification, y);
            y += yadd;
        }

        if (showCfgPopup) {
            snprintf(notification, sizeof(notification), _l(_STR_CFG_NOTIFICATION), configGetDir());
            if ((col_pos = strchr(notification, ':')) != NULL)
                *(col_pos + 1) = '\0';

            guiRenderNotifications(notification, y);
            y += yadd;
        }

        if (showThmPopup) {
            snprintf(notification, sizeof(notification), _l(_STR_THM_NOTIFICATION), thmGetFilePath(thmGetGuiValue()));
            if ((col_pos = strchr(notification, ':')) != NULL)
                *(col_pos + 1) = '\0';

            guiRenderNotifications(notification, y);
            y += yadd;
        }

        if (showLngPopup) {
            snprintf(notification, sizeof(notification), _l(_STR_LNG_NOTIFICATION), lngGetFilePath(lngGetGuiValue()));
            if ((col_pos = strchr(notification, ':')) != NULL)
                *(col_pos + 1) = '\0';

            guiRenderNotifications(notification, y);
        }

        if (clock() >= popupTimer) {
            guiResetNotifications();
            showPartPopup = 0;
            showCfgPopup = 0;
        }
    }
}

static int guiNetCompatUpdRefresh(int modified)
{
    int result;
    unsigned int done, total;

    if ((result = oplGetUpdateGameCompatStatus(&done, &total)) == OPL_COMPAT_UPDATE_STAT_WIP) {
        diaSetInt(diaNetCompatUpdate, NETUPD_PROGRESS, (done == 0 || total == 0) ? 0 : (int)((float)done / total * 100.0f));
    }

    return result;
}

static void guiShowNetCompatUpdateResult(int result)
{
    switch (result) {
        case OPL_COMPAT_UPDATE_STAT_DONE:
            // Completed with no errors.
            guiMsgBox(_l(_STR_NET_UPDATE_DONE), 0, NULL);
            break;
        case OPL_COMPAT_UPDATE_STAT_ERROR:
            // Completed with errors.
            guiMsgBox(_l(_STR_NET_UPDATE_FAILED), 0, NULL);
            break;
        case OPL_COMPAT_UPDATE_STAT_CONN_ERROR:
            // Completed with errors.
            guiMsgBox(_l(_STR_NET_UPDATE_CONN_FAILED), 0, NULL);
            break;
        case OPL_COMPAT_UPDATE_STAT_ABORTED:
            // User-aborted.
            guiMsgBox(_l(_STR_NET_UPDATE_CANCELLED), 0, NULL);
            break;
    }
}

void guiShowNetCompatUpdate(void)
{
    int ret, UpdateAll;
    u8 done, started;
    void *UpdateFunction;

    diaSetVisible(diaNetCompatUpdate, NETUPD_BTN_START, 1);
    diaSetVisible(diaNetCompatUpdate, NETUPD_BTN_CANCEL, 0);
    diaSetVisible(diaNetCompatUpdate, NETUPD_PROGRESS_LBL, 0);
    diaSetVisible(diaNetCompatUpdate, NETUPD_PROGRESS_PERC_LBL, 0);
    diaSetVisible(diaNetCompatUpdate, NETUPD_PROGRESS, 0);
    diaSetInt(diaNetCompatUpdate, NETUPD_OPT_UPD_ALL, 0);
    diaSetEnabled(diaNetCompatUpdate, NETUPD_OPT_UPD_ALL, 1);

    done = 0;
    started = 0;
    UpdateFunction = NULL;
    while (!done) {
        ret = diaExecuteDialog(diaNetCompatUpdate, -1, 1, UpdateFunction);
        switch (ret) {
            case NETUPD_BTN_START:
                if (guiMsgBox(_l(_STR_CONFIRMATION_SETTINGS_UPDATE), 1, NULL)) {
                    guiRenderTextScreen(_l(_STR_PLEASE_WAIT));

                    if ((ret = ethLoadInitModules()) == 0) {
                        diaSetVisible(diaNetCompatUpdate, NETUPD_BTN_START, 0);
                        diaSetVisible(diaNetCompatUpdate, NETUPD_BTN_CANCEL, 1);
                        diaSetVisible(diaNetCompatUpdate, NETUPD_PROGRESS_LBL, 1);
                        diaSetVisible(diaNetCompatUpdate, NETUPD_PROGRESS_PERC_LBL, 1);
                        diaSetVisible(diaNetCompatUpdate, NETUPD_PROGRESS, 1);
                        diaSetEnabled(diaNetCompatUpdate, NETUPD_OPT_UPD_ALL, 0);

                        diaGetInt(diaNetCompatUpdate, NETUPD_OPT_UPD_ALL, &UpdateAll);
                        oplUpdateGameCompat(UpdateAll);
                        UpdateFunction = &guiNetCompatUpdRefresh;
                        started = 1;
                    } else {
                        ethDisplayErrorStatus();
                    }
                }
                break;
            case UIID_BTN_CANCEL: // If the user pressed the cancel button.
            case NETUPD_BTN_CANCEL:
                if (started) {
                    if (guiMsgBox(_l(_STR_CONFIRMATION_CANCEL_UPDATE), 1, NULL)) {
                        guiRenderTextScreen(_l(_STR_PLEASE_WAIT));
                        oplAbortUpdateGameCompat();
                        // The process truly ends when the UI callback gets the update from the worker thread that the process has ended.
                    }
                } else {
                    done = 1;
                    started = 0;
                }
                break;
            default:
                guiShowNetCompatUpdateResult(ret);
                done = 1;
                started = 0;
                UpdateFunction = NULL;
                break;
        }
    }
}

void guiShowNetCompatUpdateSingle(int id, item_list_t *support, config_set_t *configSet)
{
    int ConfigSource, result;

    ConfigSource = CONFIG_SOURCE_DEFAULT;
    configGetInt(configSet, CONFIG_ITEM_CONFIGSOURCE, &ConfigSource);

    if (guiMsgBox(_l(_STR_CONFIRMATION_SETTINGS_UPDATE), 1, NULL)) {
        guiRenderTextScreen(_l(_STR_PLEASE_WAIT));

        if ((ethLoadInitModules()) == 0) {
            if ((result = oplUpdateGameCompatSingle(id, support, configSet)) == OPL_COMPAT_UPDATE_STAT_DONE) {
                configSetInt(configSet, CONFIG_ITEM_CONFIGSOURCE, CONFIG_SOURCE_DLOAD);
            }
            guiShowNetCompatUpdateResult(result);
        } else {
            ethDisplayErrorStatus();
        }
    }
}

static void guiShowBlockDeviceConfig(void)
{
    int ret;

    diaSetInt(diaBlockDevicesConfig, CFG_ENABLEILK, gEnableILK);
    diaSetInt(diaBlockDevicesConfig, CFG_ENABLEMX4SIO, gEnableMX4SIO);
    diaSetEnabled(diaBlockDevicesConfig, CFG_ENABLEBDMHDD, !gHDDStartMode);
    diaSetInt(diaBlockDevicesConfig, CFG_ENABLEBDMHDD, gEnableBdmHDD);

    ret = diaExecuteDialog(diaBlockDevicesConfig, -1, 1, NULL);
    if (ret) {
        diaGetInt(diaBlockDevicesConfig, CFG_ENABLEILK, &gEnableILK);
        diaGetInt(diaBlockDevicesConfig, CFG_ENABLEMX4SIO, &gEnableMX4SIO);
        diaGetInt(diaBlockDevicesConfig, CFG_ENABLEBDMHDD, &gEnableBdmHDD);
    }
}

static int guiAdvancedSection;
static int guiAdvancedRow(int id)
{
    switch (id) {
        case CFG_DEBUG:
        case CFG_EXITTO:
            return 0;
        case CFG_HDDGAMELISTCACHE:
        case CFG_ENWRITEOP:
        case CFG_BDMPREFIX:
        case CFG_ETHPREFIX:
        case CFG_HDDSPINDOWN:
        case CFG_BDMCACHE:
        case CFG_HDDCACHE:
        case CFG_SMBCACHE:
            return 1;
        case CFG_PS2LOGO:
        case CFG_LASTPLAYED:
        case CFG_AUTOSTARTLAST:
        case CFG_LBL_AUTOSTARTLAST:
            return 2;
        default:
            return 3;
    }
}

static void guiAdvancedRows(void)
{
    struct UIItem *row = diaConfig;
    while (row->type != UI_TERMINATOR) {
        struct UIItem *end = row;
        int section = -1, keep = row == diaConfig;
        while (end->type != UI_TERMINATOR && end->type != UI_BREAK && end->type != UI_SPLITTER) {
            if (end->type == UI_OK)
                keep = 1;
            else if (end->type >= UI_OK)
                section = guiAdvancedRow(end->id);
            end++;
        }
        keep |= section == guiAdvancedSection;
        for (struct UIItem *it = row; it < end; it++)
            it->visible = keep;
        if (end->type == UI_TERMINATOR)
            break;
        end->visible = keep;
        row = end + 1;
    }
}

static int guiUpdater(int modified)
{
    int showAutoStartLast;

    if (modified) {
        diaGetInt(diaConfig, CFG_LASTPLAYED, &showAutoStartLast);
        diaSetVisible(diaConfig, CFG_LBL_AUTOSTARTLAST, showAutoStartLast && (thmGetGuiValue() != 0 || guiAdvancedSection == 2));
        diaSetVisible(diaConfig, CFG_AUTOSTARTLAST, showAutoStartLast && (thmGetGuiValue() != 0 || guiAdvancedSection == 2));

        int bdmMode;
        diaGetInt(diaConfig, CFG_BDMMODE, &bdmMode);
        diaSetVisible(diaConfig, BLOCKDEVICE_BUTTON, bdmMode && thmGetGuiValue() != 0);
    }
    return 0;
}

int guiDeviceTypeToIoMode(int deviceType)
{
    // Translates an index into deviceNames into an IO mode index used internally.
    if (deviceType == 0)
        return BDM_MODE;
    else if (deviceType == 1)
        return ETH_MODE;
    else if (deviceType == 2)
        return HDD_MODE;
    else if (deviceType == 4)
        return PS1_MODE;
    else
        return APP_MODE;
}

int guiIoModeToDeviceType(int ioMode)
{
    switch (ioMode) {
        case BDM_MODE:
        case BDM_MODE1:
        case BDM_MODE2:
        case BDM_MODE3:
        case BDM_MODE4:
            return 0;
        case ETH_MODE:
            return 1;
        case HDD_MODE:
            return 2;
        case APP_MODE:
            return 3;
        case PS1_MODE:
            return 4;
        default:
            return 0;
    }
}

static void guiShowConfigWithFocus(int focusId)
{
    // configure the enumerations
    const char *deviceNames[] = {_l(_STR_BDM_GAMES), _l(_STR_NET_GAMES), _l(_STR_HDD_GAMES), _l(_STR_APPS), _l(_STR_PS1_GAMES), NULL};
    const char *deviceModes[] = {_l(_STR_OFF), _l(_STR_MANUAL), _l(_STR_AUTO), NULL};

    diaSetEnum(diaConfig, CFG_DEFDEVICE, deviceNames);
    diaSetEnum(diaConfig, CFG_BDMMODE, deviceModes);
    diaSetEnum(diaConfig, CFG_HDDMODE, deviceModes);
    diaSetEnum(diaConfig, CFG_ETHMODE, deviceModes);
    diaSetEnum(diaConfig, CFG_APPMODE, deviceModes);
    diaSetEnum(diaConfig, CFG_PS1MODE, deviceModes);

    diaSetInt(diaConfig, CFG_BDMCACHE, bdmCacheSize);
    diaSetInt(diaConfig, CFG_HDDCACHE, hddCacheSize);
    diaSetInt(diaConfig, CFG_SMBCACHE, smbCacheSize);

    diaSetInt(diaConfig, CFG_DEBUG, gEnableDebug);
    diaSetInt(diaConfig, CFG_PS2LOGO, gPS2Logo);
    diaSetInt(diaConfig, CFG_HDDGAMELISTCACHE, gHDDGameListCache);
    diaSetString(diaConfig, CFG_EXITTO, gExitPath);
    diaSetInt(diaConfig, CFG_ENWRITEOP, gEnableWrite);
    diaSetInt(diaConfig, CFG_HDDSPINDOWN, gHDDSpindown);
    diaSetString(diaConfig, CFG_BDMPREFIX, gBDMPrefix);
    diaSetString(diaConfig, CFG_ETHPREFIX, gETHPrefix);
    diaSetInt(diaConfig, CFG_LASTPLAYED, gRememberLastPlayed);
    diaSetInt(diaConfig, CFG_AUTOSTARTLAST, gAutoStartLastPlayed);
    diaSetVisible(diaConfig, CFG_AUTOSTARTLAST, gRememberLastPlayed);
    diaSetVisible(diaConfig, CFG_LBL_AUTOSTARTLAST, gRememberLastPlayed);

    int deviceModeIndex = guiIoModeToDeviceType(gDefaultDevice);
    diaSetInt(diaConfig, CFG_DEFDEVICE, deviceModeIndex);
    diaSetInt(diaConfig, CFG_BDMMODE, gBDMStartMode);
    diaSetVisible(diaConfig, BLOCKDEVICE_BUTTON, gBDMStartMode);
    diaSetEnabled(diaConfig, CFG_HDDMODE, !gEnableBdmHDD);
    diaSetInt(diaConfig, CFG_HDDMODE, gHDDStartMode);
    diaSetInt(diaConfig, CFG_ETHMODE, gETHStartMode);
    diaSetInt(diaConfig, CFG_APPMODE, gAPPStartMode);
    diaSetInt(diaConfig, CFG_PS1MODE, gPS1StartMode);

    if (thmGetGuiValue() == 0) {
        guiAdvancedRows();
        guiUpdater(1);
    }
    int ret = diaExecuteDialog(diaConfig, focusId, 1, &guiUpdater);
    for (struct UIItem *it = diaConfig; it->type != UI_TERMINATOR; it++)
        it->visible = 1;
    if (ret) {
        diaGetInt(diaConfig, CFG_DEBUG, &gEnableDebug);
        diaGetInt(diaConfig, CFG_PS2LOGO, &gPS2Logo);
        diaGetInt(diaConfig, CFG_HDDGAMELISTCACHE, &gHDDGameListCache);
        diaGetString(diaConfig, CFG_EXITTO, gExitPath, sizeof(gExitPath));
        diaGetInt(diaConfig, CFG_ENWRITEOP, &gEnableWrite);
        diaGetInt(diaConfig, CFG_HDDSPINDOWN, &gHDDSpindown);
        diaGetString(diaConfig, CFG_BDMPREFIX, gBDMPrefix, sizeof(gBDMPrefix));
        diaGetString(diaConfig, CFG_ETHPREFIX, gETHPrefix, sizeof(gETHPrefix));
        diaGetInt(diaConfig, CFG_LASTPLAYED, &gRememberLastPlayed);
        diaGetInt(diaConfig, CFG_AUTOSTARTLAST, &gAutoStartLastPlayed);
        DisableCron = 1; // Disable Auto Start Last Played counter (we don't want to call it right after enable it on GUI)
        diaGetInt(diaConfig, CFG_DEFDEVICE, &deviceModeIndex);
        gDefaultDevice = guiDeviceTypeToIoMode(deviceModeIndex);
        diaGetInt(diaConfig, CFG_BDMMODE, &gBDMStartMode);
        diaGetInt(diaConfig, CFG_HDDMODE, &gHDDStartMode);
        diaGetInt(diaConfig, CFG_ETHMODE, &gETHStartMode);
        diaGetInt(diaConfig, CFG_APPMODE, &gAPPStartMode);
        diaGetInt(diaConfig, CFG_PS1MODE, &gPS1StartMode);
        diaGetInt(diaConfig, CFG_BDMCACHE, &bdmCacheSize);
        diaGetInt(diaConfig, CFG_HDDCACHE, &hddCacheSize);
        diaGetInt(diaConfig, CFG_SMBCACHE, &smbCacheSize);

        if (ret == BLOCKDEVICE_BUTTON)
            guiShowBlockDeviceConfig();

        applyConfig(-1, -1, 0);
        saveConfig(CONFIG_OPL, 0);
        menuReinitMainMenu();
    }
}

static int curTheme = -1;

void guiShowConfig(void)
{
    guiAdvancedSection = 0;
    guiShowConfigWithFocus(-1);
}

void guiShowStoragePerformance(void)
{
    guiAdvancedSection = 1;
    guiShowConfigWithFocus(-1);
}

void guiShowGamePreferences(void)
{
    guiAdvancedSection = 2;
    guiShowConfigWithFocus(-1);
}

/* One screen for sources and startup, backed by real OPL settings. */
static void guiConfigureSource(int source, const char *name)
{
    const char *modes[] = {"Desativado", "Manual (SELECT)", "Automático", NULL};
    const char *hddFormats[] = {"APA", "GPT / MBR", NULL};
    config_set_t *cfg = configGetByType(CONFIG_OPL);
    int mask = 255, usb = 1, visible, startup, format = gEnableBdmHDD;
    int oldBdm = gBDMStartMode, oldFormat = gEnableBdmHDD;
    int bdm = source == MAIZO_SOURCE_USB || source == MAIZO_SOURCE_MX4SIO || source == MAIZO_SOURCE_ILINK || (source == MAIZO_SOURCE_HDD && format);
    struct UIItem options[] = {
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {name, -1}}},
        {UI_SPLITTER},
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"Mostrar na tela inicial", -1}}},
        {UI_SPACER},
        {UI_BOOL, 301, 1, 1, -1, 0, 0, {.intvalue = {0, 0}}},
        {UI_BREAK},
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"Inicialização", -1}}},
        {UI_SPACER},
        {UI_ENUM, 302, 1, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 0, modes}}},
        {UI_BREAK},
        {UI_LABEL, 303, 1, 1, -1, 0, 0, {.label = {"Formato do HDD", -1}}},
        {UI_SPACER},
        {UI_ENUM, 304, 1, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 0, hddFormats}}},
        {UI_BREAK},
        {UI_LABEL, 305, 1, 1, -1, 0, 0, {.label = {"USB, MX4SIO e iLink compartilham o driver BDM.", -1}}},
        {UI_BREAK},
        {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {"Salvar e aplicar", -1}}},
        {UI_TERMINATOR}};
    configGetInt(cfg, "maizo_source_mask", &mask);
    configGetInt(cfg, "maizo_show_usb", &usb);
    if (!usb)
        mask &= ~(1 << MAIZO_SOURCE_USB);
    visible = !!(mask & (1 << source));
    startup = bdm ? gBDMStartMode : source == MAIZO_SOURCE_SMB ? gETHStartMode :
                                source == MAIZO_SOURCE_HDD     ? gHDDStartMode :
                                source == MAIZO_SOURCE_APP     ? gAPPStartMode :
                                                                 gPS1StartMode;
    if ((source == MAIZO_SOURCE_MX4SIO && !gEnableMX4SIO) || (source == MAIZO_SOURCE_ILINK && !gEnableILK))
        startup = 0;
    diaSetInt(options, 301, visible);
    diaSetInt(options, 302, startup);
    diaSetInt(options, 304, format);
    diaSetVisible(options, 303, source == MAIZO_SOURCE_HDD);
    diaSetVisible(options, 304, source == MAIZO_SOURCE_HDD);
    diaSetVisible(options, 305, bdm || source == MAIZO_SOURCE_HDD);
    if (diaExecuteDialog(options, -1, 1, NULL) != UIID_BTN_OK)
        return;
    diaGetInt(options, 301, &visible);
    diaGetInt(options, 302, &startup);
    diaGetInt(options, 304, &format);
    if (startup && (source == MAIZO_SOURCE_USB || source == MAIZO_SOURCE_MX4SIO || source == MAIZO_SOURCE_ILINK || (source == MAIZO_SOURCE_HDD && format)))
        gBDMStartMode = startup;
    if (source == MAIZO_SOURCE_USB && !startup)
        visible = 0; /* USB driver kept available for storage/configuration. */
    if (source == MAIZO_SOURCE_MX4SIO)
        gEnableMX4SIO = startup != 0;
    if (source == MAIZO_SOURCE_ILINK)
        gEnableILK = startup != 0;
    if (source == MAIZO_SOURCE_HDD) {
        gEnableBdmHDD = format && startup != 0;
        gHDDStartMode = format ? 0 : startup;
    } else if (source == MAIZO_SOURCE_SMB)
        gETHStartMode = startup;
    else if (source == MAIZO_SOURCE_APP)
        gAPPStartMode = startup;
    else if (source == MAIZO_SOURCE_PS1)
        gPS1StartMode = startup;
    if (visible)
        mask |= 1 << source;
    else
        mask &= ~(1 << source);
    configSetInt(cfg, "maizo_source_mask", mask);
    configSetInt(cfg, "maizo_show_usb", !!(mask & (1 << MAIZO_SOURCE_USB)));
    applyConfig(-1, -1, 0);
    saveConfig(CONFIG_OPL, 0);
    if (oldBdm != gBDMStartMode)
        guiMsgBox("Modo de início compartilhado pelos dispositivos USB/MX4SIO/iLink.", 0, NULL);
    if (oldFormat != gEnableBdmHDD)
        guiMsgBox("Se necessário, reinicie o OPL para trocar o formato do HDD.", 0, NULL);
}

void guiShowStorageConfig(void)
{
    static const char *const names[] = {"Todas", "USB", "Rede SMB", "MX4SIO", "HDD", "iLink", "Aplicativos", "PS1"};
    const char *home[] = {"Todos", "PS2", "PS1", "Aplicativos", "Favoritos", NULL};
    int result = -1;
    do {
        char labels[MAIZO_SOURCE_COUNT][64];
        struct UIItem options[24] = {{0}};
        int n = 0, initial = 0;
        options[n++] = (struct UIItem) {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"FONTES DE JOGOS", -1}}};
        options[n++].type = UI_SPLITTER;
        for (int source = 1; source < MAIZO_SOURCE_COUNT; source++) {
            int enabled = source == MAIZO_SOURCE_USB ? gBDMStartMode : source == MAIZO_SOURCE_MX4SIO ? gEnableMX4SIO :
                                                                   source == MAIZO_SOURCE_ILINK      ? gEnableILK :
                                                                   source == MAIZO_SOURCE_HDD        ? (gHDDStartMode || gEnableBdmHDD) :
                                                                   source == MAIZO_SOURCE_SMB        ? gETHStartMode :
                                                                   source == MAIZO_SOURCE_APP        ? gAPPStartMode :
                                                                                                       gPS1StartMode;
            snprintf(labels[source], sizeof(labels[source]), "%s - %s", names[source],
                     !enabled ? "Desativado" : menuMaizoSourceAvailable(source) ? "Disponível" :
                                                                                  "Aguardando");
            options[n++] = (struct UIItem) {UI_BUTTON, 400 + source, 1, 1, -1, 0, 0, {.label = {labels[source], -1}}};
            options[n++].type = UI_BREAK;
        }
        options[n++] = (struct UIItem) {UI_BUTTON, 420, 1, 1, -1, 0, 0, {.label = {"Aba inicial", -1}}};
        options[n++].type = UI_BREAK;
        options[n++].type = UI_TERMINATOR;
        result = diaExecuteDialog(options, result, 1, NULL);
        if (result > 400 && result < 400 + MAIZO_SOURCE_COUNT)
            guiConfigureSource(result - 400, names[result - 400]);
        else if (result == 420) {
            struct UIItem homeDialog[] = {
                {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"ABA INICIAL", -1}}},
                {UI_SPLITTER},
                {UI_ENUM, 421, 1, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 0, home}}},
                {UI_BREAK},
                {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},
                {UI_TERMINATOR}};
            configGetInt(configGetByType(CONFIG_OPL), "maizo_home_tab", &initial);
            diaSetInt(homeDialog, 421, initial >= 0 && initial < 5 ? initial : 0);
            if (diaExecuteDialog(homeDialog, -1, 1, NULL) == UIID_BTN_OK) {
                diaGetInt(homeDialog, 421, &initial);
                configSetInt(configGetByType(CONFIG_OPL), "maizo_home_tab", initial);
                saveConfig(CONFIG_OPL, 0);
            }
        }
    } while (result >= 401 && result <= 420);
}

void guiShowPS1Config(void)
{
    guiShowStorageConfig();
}

static int guiUIUpdater(int modified)
{
    if (modified) {
        int temp, x, y;
        diaGetInt(diaUIConfig, UICFG_THEME, &temp);
        if (temp != curTheme) {
            curTheme = temp;
            if (temp == 0) {
                // Display the default theme's colours.
                diaSetItemType(diaUIConfig, UICFG_BGCOL, UI_COLOUR); // Must be correctly set before doing the diaS/GetColor !!
                diaSetItemType(diaUIConfig, UICFG_UICOL, UI_COLOUR);
                diaSetItemType(diaUIConfig, UICFG_TXTCOL, UI_COLOUR);
                diaSetItemType(diaUIConfig, UICFG_SELCOL, UI_COLOUR);
                diaSetColor(diaUIConfig, UICFG_BGCOL, gDefaultBgColor);
                diaSetColor(diaUIConfig, UICFG_UICOL, gDefaultUITextColor);
                diaSetColor(diaUIConfig, UICFG_TXTCOL, gDefaultTextColor);
                diaSetColor(diaUIConfig, UICFG_SELCOL, gDefaultSelTextColor);
            } else if (temp == thmGetGuiValue()) {
                // Display the current theme's colours.
                diaSetItemType(diaUIConfig, UICFG_BGCOL, UI_COLOUR);
                diaSetItemType(diaUIConfig, UICFG_UICOL, UI_COLOUR);
                diaSetItemType(diaUIConfig, UICFG_TXTCOL, UI_COLOUR);
                diaSetItemType(diaUIConfig, UICFG_SELCOL, UI_COLOUR);
                diaSetColor(diaUIConfig, UICFG_BGCOL, gTheme->bgColor);
                diaSetU64Color(diaUIConfig, UICFG_UICOL, gTheme->uiTextColor);
                diaSetU64Color(diaUIConfig, UICFG_TXTCOL, gTheme->textColor);
                diaSetU64Color(diaUIConfig, UICFG_SELCOL, gTheme->selTextColor);
            } else {
                // When another theme is highlighted in the list, its colours are not known. Don't show any colours.
                diaSetItemType(diaUIConfig, UICFG_BGCOL, UI_SPACER);
                diaSetItemType(diaUIConfig, UICFG_UICOL, UI_SPACER);
                diaSetItemType(diaUIConfig, UICFG_TXTCOL, UI_SPACER);
                diaSetItemType(diaUIConfig, UICFG_SELCOL, UI_SPACER);
            }

            // The user cannot adjust the current theme's colours.
            temp = !temp;
            diaSetEnabled(diaUIConfig, UICFG_BGCOL, temp);
            diaSetEnabled(diaUIConfig, UICFG_UICOL, temp);
            diaSetEnabled(diaUIConfig, UICFG_TXTCOL, temp);
            diaSetEnabled(diaUIConfig, UICFG_SELCOL, temp);
            diaSetEnabled(diaUIConfig, UICFG_RESETCOL, temp);
        }

        diaGetInt(diaUIConfig, UICFG_XOFF, &x);
        diaGetInt(diaUIConfig, UICFG_YOFF, &y);
        if ((x != gXOff) || (y != gYOff)) {
            gXOff = x;
            gYOff = y;
            rmSetDisplayOffset(x, y);
        }
        diaGetInt(diaUIConfig, UICFG_OVERSCAN, &temp);
        if (temp != gOverscan) {
            gOverscan = temp;
            rmSetOverscan(gOverscan);
            guiUpdateScreenScale();
        }
        diaGetInt(diaUIConfig, UICFG_WIDESCREEN, &temp);
        if (temp != gWideScreen) {
            gWideScreen = temp;
            rmSetAspectRatio((gWideScreen == 0) ? RM_ARATIO_4_3 : RM_ARATIO_16_9);
            guiUpdateScreenScale();
        }
    }

    return 0;
}

static int guiUiVideoOnly;

static void guiUiRows(int videoOnly)
{
    struct UIItem *row = diaUIConfig;
    int video = 0;
    while (row->type != UI_TERMINATOR) {
        struct UIItem *end = row;
        int isOk = 0, hasVideo = 0;
        while (end->type != UI_TERMINATOR && end->type != UI_BREAK && end->type != UI_SPLITTER) {
            if (end->id == UICFG_VMODE)
                hasVideo = 1;
            if (end->type == UI_OK)
                isOk = 1;
            end++;
        }
        if (hasVideo)
            video = 1;
        for (struct UIItem *it = row; it < end; it++)
            it->visible = isOk || videoOnly == video || row == diaUIConfig;
        if (end->type == UI_TERMINATOR)
            break;
        end->visible = isOk || videoOnly == video || row == diaUIConfig;
        row = end + 1;
    }
}

void guiShowUIConfig(void)
{
    int themeID = -1, langID = -1;
    curTheme = -1;
    showCfgPopup = 0;
    guiResetNotifications();

    // clang-format off
    const char *vmodeNames[] = {_l(_STR_AUTO)
        , "PAL 640x512i @50Hz 24bit"
        , "NTSC 640x448i @60Hz 24bit"
        , "EDTV 640x448p @60Hz 24bit"
        , "EDTV 640x512p @50Hz 24bit"
        , "VGA 640x480p @60Hz 24bit"
        , "PAL 704x576i @50Hz 24bit (HIRES)"
        , "NTSC 704x480i @60Hz 24bit (HIRES)"
        , "EDTV 704x480p @60Hz 24bit (HIRES)"
        , "EDTV 704x576p @50Hz 24bit (HIRES)"
        , "HDTV 1280x720p @60Hz 16bit (HIRES)"
        , "HDTV 1920x1080i @60Hz 16bit (HIRES)"
        , "PAL 640x256p @50Hz 24bit"
        , "NTSC 640x224p @60Hz 24bit"
        , NULL};
    // clang-format on
    int previousVMode;
    int previousTheme, previousX, previousY, previousOverscan, previousWide;

reselect_video_mode:
    previousVMode = gVMode;
    previousTheme = thmGetGuiValue();
    previousX = gXOff;
    previousY = gYOff;
    previousOverscan = gOverscan;
    previousWide = gWideScreen;
    diaSetEnum(diaUIConfig, UICFG_THEME, (const char **)thmGetGuiList());
    diaSetEnum(diaUIConfig, UICFG_LANG, (const char **)lngGetGuiList());
    diaSetEnum(diaUIConfig, UICFG_VMODE, vmodeNames);
    diaSetInt(diaUIConfig, UICFG_THEME, thmGetGuiValue());
    diaSetInt(diaUIConfig, UICFG_LANG, lngGetGuiValue());
    diaSetInt(diaUIConfig, UICFG_AUTOSORT, gAutosort);
    diaSetInt(diaUIConfig, UICFG_AUTOREFRESH, gAutoRefresh);
    diaSetInt(diaUIConfig, UICFG_NOTIFICATIONS, gEnableNotifications);
    diaSetInt(diaUIConfig, UICFG_COVERART, gEnableArt);
    diaSetInt(diaUIConfig, UICFG_WIDESCREEN, gWideScreen);
    diaSetInt(diaUIConfig, UICFG_VMODE, gVMode);
    diaSetInt(diaUIConfig, UICFG_XOFF, gXOff);
    diaSetInt(diaUIConfig, UICFG_YOFF, gYOff);
    diaSetInt(diaUIConfig, UICFG_OVERSCAN, gOverscan);
    guiUIUpdater(1);
    if (thmGetGuiValue() == 0)
        guiUiRows(guiUiVideoOnly);
    int ret = diaExecuteDialog(diaUIConfig, -1, 1, guiUIUpdater);
    for (struct UIItem *it = diaUIConfig; it->type != UI_TERMINATOR; it++)
        it->visible = 1;
    if (ret) {
        diaGetInt(diaUIConfig, UICFG_LANG, &langID);
        diaGetInt(diaUIConfig, UICFG_THEME, &themeID);
        if (themeID == 0) {
            diaGetColor(diaUIConfig, UICFG_BGCOL, gDefaultBgColor);
            diaGetColor(diaUIConfig, UICFG_UICOL, gDefaultUITextColor);
            diaGetColor(diaUIConfig, UICFG_TXTCOL, gDefaultTextColor);
            diaGetColor(diaUIConfig, UICFG_SELCOL, gDefaultSelTextColor);
        }
        diaGetInt(diaUIConfig, UICFG_AUTOSORT, &gAutosort);
        diaGetInt(diaUIConfig, UICFG_AUTOREFRESH, &gAutoRefresh);
        diaGetInt(diaUIConfig, UICFG_NOTIFICATIONS, &gEnableNotifications);
        diaGetInt(diaUIConfig, UICFG_COVERART, &gEnableArt);
        diaGetInt(diaUIConfig, UICFG_WIDESCREEN, &gWideScreen);
        diaGetInt(diaUIConfig, UICFG_VMODE, &gVMode);
        diaGetInt(diaUIConfig, UICFG_XOFF, &gXOff);
        diaGetInt(diaUIConfig, UICFG_YOFF, &gYOff);
        diaGetInt(diaUIConfig, UICFG_OVERSCAN, &gOverscan);

        if (ret == UICFG_RESETCOL)
            setDefaultColors();

        if (previousTheme != themeID && isBgmPlaying())
            bgmStop();

        applyConfig(themeID, langID, 1);
        sfxInit(0);

        if (gEnableBGM && !isBgmPlaying())
            bgmStart();
    }

    if (!ret) {
        gXOff = previousX;
        gYOff = previousY;
        gOverscan = previousOverscan;
        gWideScreen = previousWide;
        applyConfig(-1, -1, 1);
    }
    if (previousVMode != gVMode) {
        if (guiConfirmVideoMode() == 0) {
            // Restore previous video mode, without changing the theme & language settings.
            gVMode = previousVMode;
            applyConfig(themeID, langID, 1);
            goto reselect_video_mode;
        }
    }
    if (ret)
        saveConfig(CONFIG_OPL, 0);
}

void guiShowVideoConfig(void)
{
    guiUiVideoOnly = 1;
    guiShowUIConfig();
    guiUiVideoOnly = 0;
}

static int netConfigUpdater(int modified)
{
    int showAdvancedOptions, isNetBIOS, isDHCPEnabled, i;

    if (modified) {
        diaGetInt(diaNetConfig, NETCFG_SHOW_ADVANCED_OPTS, &showAdvancedOptions);

        diaGetInt(diaNetConfig, NETCFG_PS2_IP_ADDR_TYPE, &isDHCPEnabled);
        diaGetInt(diaNetConfig, NETCFG_SHARE_ADDR_TYPE, &isNetBIOS);
        diaSetVisible(diaNetConfig, NETCFG_SHARE_NB_ADDR, isNetBIOS);

        for (i = 0; i < 4; i++) {
            diaSetVisible(diaNetConfig, NETCFG_SHARE_IP_ADDR_0 + i, !isNetBIOS);

            diaSetEnabled(diaNetConfig, NETCFG_PS2_IP_ADDR_0 + i, !isDHCPEnabled);
            diaSetEnabled(diaNetConfig, NETCFG_PS2_NETMASK_0 + i, !isDHCPEnabled);
            diaSetEnabled(diaNetConfig, NETCFG_PS2_GATEWAY_0 + i, !isDHCPEnabled);
            diaSetEnabled(diaNetConfig, NETCFG_PS2_DNS_0 + i, !isDHCPEnabled);
        }

        for (i = 0; i < 3; i++)
            diaSetVisible(diaNetConfig, NETCFG_SHARE_IP_ADDR_DOT_0 + i, !isNetBIOS);

        diaSetEnabled(diaNetConfig, NETCFG_SHARE_PORT, 1);
        diaSetEnabled(diaNetConfig, NETCFG_ETHOPMODE, showAdvancedOptions);
    }

    return 0;
}

void guiShowNetConfig(void)
{
    size_t i;
    const char *ethOpModes[] = {_l(_STR_AUTO), _l(_STR_ETH_100MFDX), _l(_STR_ETH_100MHDX), _l(_STR_ETH_10MFDX), _l(_STR_ETH_10MHDX), NULL};
    const char *addrConfModes[] = {_l(_STR_ADDR_TYPE_IP), _l(_STR_ADDR_TYPE_NETBIOS), NULL};
    const char *ipAddrConfModes[] = {_l(_STR_IP_ADDRESS_TYPE_STATIC), _l(_STR_IP_ADDRESS_TYPE_DHCP), NULL};
    diaSetEnum(diaNetConfig, NETCFG_PS2_IP_ADDR_TYPE, ipAddrConfModes);
    diaSetEnum(diaNetConfig, NETCFG_SHARE_ADDR_TYPE, addrConfModes);
    diaSetEnum(diaNetConfig, NETCFG_ETHOPMODE, ethOpModes);

    // upload current values
    diaSetInt(diaNetConfig, NETCFG_SHOW_ADVANCED_OPTS, 0);
    diaSetEnabled(diaNetConfig, NETCFG_ETHOPMODE, 0);
    diaSetEnabled(diaNetConfig, NETCFG_SHARE_PORT, 1);

    diaSetInt(diaNetConfig, NETCFG_PS2_IP_ADDR_TYPE, ps2_ip_use_dhcp);
    diaSetInt(diaNetConfig, NETCFG_SHARE_ADDR_TYPE, gPCShareAddressIsNetBIOS);
    diaSetVisible(diaNetConfig, NETCFG_SHARE_NB_ADDR, gPCShareAddressIsNetBIOS);
    diaSetInt(diaNetConfig, NETCFG_SHARE_NB_ADDR, gPCShareAddressIsNetBIOS);
    diaSetString(diaNetConfig, NETCFG_SHARE_NB_ADDR, gPCShareNBAddress);

    for (i = 0; i < 4; ++i) {
        diaSetEnabled(diaNetConfig, NETCFG_PS2_IP_ADDR_0 + i, !ps2_ip_use_dhcp);
        diaSetEnabled(diaNetConfig, NETCFG_PS2_NETMASK_0 + i, !ps2_ip_use_dhcp);
        diaSetEnabled(diaNetConfig, NETCFG_PS2_GATEWAY_0 + i, !ps2_ip_use_dhcp);
        diaSetEnabled(diaNetConfig, NETCFG_PS2_DNS_0 + i, !ps2_ip_use_dhcp);

        diaSetVisible(diaNetConfig, NETCFG_SHARE_IP_ADDR_0 + i, !gPCShareAddressIsNetBIOS);
        diaSetInt(diaNetConfig, NETCFG_PS2_IP_ADDR_0 + i, ps2_ip[i]);
        diaSetInt(diaNetConfig, NETCFG_PS2_NETMASK_0 + i, ps2_netmask[i]);
        diaSetInt(diaNetConfig, NETCFG_PS2_GATEWAY_0 + i, ps2_gateway[i]);
        diaSetInt(diaNetConfig, NETCFG_PS2_DNS_0 + i, ps2_dns[i]);
        diaSetInt(diaNetConfig, NETCFG_SHARE_IP_ADDR_0 + i, pc_ip[i]);
    }

    for (i = 0; i < 3; ++i)
        diaSetVisible(diaNetConfig, NETCFG_SHARE_IP_ADDR_DOT_0 + i, !gPCShareAddressIsNetBIOS);

    diaSetInt(diaNetConfig, NETCFG_SHARE_PORT, gPCPort);
    diaSetString(diaNetConfig, NETCFG_SHARE_NAME, gPCShareName);
    diaSetString(diaNetConfig, NETCFG_SHARE_USERNAME, gPCUserName);
    diaSetString(diaNetConfig, NETCFG_SHARE_PASSWORD, gPCPassword);
    diaSetInt(diaNetConfig, NETCFG_ETHOPMODE, gETHOpMode);

    // Update the spacer item between the OK and reconnect buttons (See dialogs.c).
    if (gNetworkStartup == 0) {
        diaSetLabel(diaNetConfig, NETCFG_OK, _l(_STR_OK));
        diaSetVisible(diaNetConfig, NETCFG_RECONNECT, 1);
    } else if (gNetworkStartup >= ERROR_ETH_SMB_CONN) {
        diaSetLabel(diaNetConfig, NETCFG_OK, _l(_STR_RECONNECT));
        diaSetVisible(diaNetConfig, NETCFG_RECONNECT, 0);
    } else {
        diaSetLabel(diaNetConfig, NETCFG_OK, _l(_STR_OK));
        diaSetVisible(diaNetConfig, NETCFG_RECONNECT, 0);
    }

    int result = diaExecuteDialog(diaNetConfig, -1, 1, &netConfigUpdater);
    if (result) {
        // Store values
        diaGetInt(diaNetConfig, NETCFG_PS2_IP_ADDR_TYPE, &ps2_ip_use_dhcp);
        diaGetInt(diaNetConfig, NETCFG_SHARE_ADDR_TYPE, &gPCShareAddressIsNetBIOS);
        diaGetString(diaNetConfig, NETCFG_SHARE_NB_ADDR, gPCShareNBAddress, sizeof(gPCShareNBAddress));

        for (i = 0; i < 4; ++i) {
            diaGetInt(diaNetConfig, NETCFG_PS2_IP_ADDR_0 + i, &ps2_ip[i]);
            diaGetInt(diaNetConfig, NETCFG_PS2_NETMASK_0 + i, &ps2_netmask[i]);
            diaGetInt(diaNetConfig, NETCFG_PS2_GATEWAY_0 + i, &ps2_gateway[i]);
            diaGetInt(diaNetConfig, NETCFG_PS2_DNS_0 + i, &ps2_dns[i]);
            diaGetInt(diaNetConfig, NETCFG_SHARE_IP_ADDR_0 + i, &pc_ip[i]);
        }
        diaGetInt(diaNetConfig, NETCFG_ETHOPMODE, &gETHOpMode);

        diaGetInt(diaNetConfig, NETCFG_SHARE_PORT, &gPCPort);
        diaGetString(diaNetConfig, NETCFG_SHARE_NAME, gPCShareName, sizeof(gPCShareName));
        diaGetString(diaNetConfig, NETCFG_SHARE_USERNAME, gPCUserName, sizeof(gPCUserName));
        diaGetString(diaNetConfig, NETCFG_SHARE_PASSWORD, gPCPassword, sizeof(gPCPassword));

        if (result == NETCFG_RECONNECT && gNetworkStartup < ERROR_ETH_SMB_CONN)
            gNetworkStartup = ERROR_ETH_SMB_LOGON;

        applyConfig(-1, -1, 0);
        saveConfig(CONFIG_NETWORK | CONFIG_OPL, 0);
    }
}

void guiShowParentalLockConfig(void)
{
    int result;
    char password[CONFIG_KEY_VALUE_LEN];
    config_set_t *configOPL = configGetByType(CONFIG_OPL);

    // Set current values
    configGetStrCopy(configOPL, CONFIG_OPL_PARENTAL_LOCK_PWD, password, CONFIG_KEY_VALUE_LEN); // This will return the current password, or a blank string if it is not set.
    diaSetString(diaParentalLockConfig, CFG_PARENLOCK_PASSWORD, password);

    result = diaExecuteDialog(diaParentalLockConfig, -1, 1, NULL);
    if (result) {
        diaGetString(diaParentalLockConfig, CFG_PARENLOCK_PASSWORD, password, CONFIG_KEY_VALUE_LEN);

        if (strlen(password) > 0) {
            if (strncmp(OPL_PARENTAL_LOCK_MASTER_PASS, password, CONFIG_KEY_VALUE_LEN) != 0) {
                // Store password
                configSetStr(configOPL, CONFIG_OPL_PARENTAL_LOCK_PWD, password);
            } else {
                // Password not acceptable (i.e. master password entered).
                guiMsgBox(_l(_STR_PARENLOCK_INVALID_PASSWORD), 0, NULL);
            }
        } else {
            configRemoveKey(configOPL, CONFIG_OPL_PARENTAL_LOCK_PWD);

            guiMsgBox(_l(_STR_PARENLOCK_DISABLE_WARNING), 0, diaParentalLockConfig);
        }

        menuSetParentalLockCheckState(1);
        saveConfig(CONFIG_OPL, 0);
    }
}

static void guiSetAudioSettingsState(void)
{
    diaGetInt(diaAudioConfig, CFG_SFX, &gEnableSFX);
    diaGetInt(diaAudioConfig, CFG_BOOT_SND, &gEnableBootSND);
    diaGetInt(diaAudioConfig, CFG_BGM, &gEnableBGM);
    diaGetInt(diaAudioConfig, CFG_SFX_VOLUME, &gSFXVolume);
    diaGetInt(diaAudioConfig, CFG_BOOT_SND_VOLUME, &gBootSndVolume);
    diaGetInt(diaAudioConfig, CFG_BGM_VOLUME, &gBGMVolume);
    diaGetString(diaAudioConfig, CFG_DEFAULT_BGM_PATH, gDefaultBGMPath, sizeof(gDefaultBGMPath));
    audioSetVolume();

    if (gEnableBGM && !isBgmPlaying())
        bgmStart();
}

void guiShowAudioConfig(void)
{
    diaSetInt(diaAudioConfig, CFG_SFX, gEnableSFX);
    diaSetInt(diaAudioConfig, CFG_BOOT_SND, gEnableBootSND);
    diaSetInt(diaAudioConfig, CFG_BGM, gEnableBGM);
    diaSetInt(diaAudioConfig, CFG_SFX_VOLUME, gSFXVolume);
    diaSetInt(diaAudioConfig, CFG_BOOT_SND_VOLUME, gBootSndVolume);
    diaSetInt(diaAudioConfig, CFG_BGM_VOLUME, gBGMVolume);
    diaSetString(diaAudioConfig, CFG_DEFAULT_BGM_PATH, gDefaultBGMPath);

    if (diaExecuteDialog(diaAudioConfig, -1, 1, NULL) == UIID_BTN_OK) {
        guiSetAudioSettingsState();
        if (!gEnableBGM && isBgmPlaying())
            bgmStop();
        saveConfig(CONFIG_OPL, 0);
    }
}

#ifdef PADEMU
/* Capture the physical source before remapping, without changing the
 * active PadEmu profile or consuming the menu's pending press edges. */
static int maizoCaptureInput(int keyboard, const char *target)
{
    int neutral = 0;
    unsigned int frames = 0;
    for (; frames < 900; frames++) {
        u8 snapshot[40] = {0};
        int value = -1, ready = 0, pressed = 0;
        if (ds34usb_get_capture(snapshot)) {
            for (int port = 0; port < 2; port++) {
                const u8 *slot = snapshot + port * 20;
                if (!slot[1] || slot[0] != (keyboard ? 8 : 5))
                    continue;
                ready = 1;
                if (keyboard) {
                    if (slot[10]) {
                        pressed = 1;
                        for (int b = 0; b < 8; b++)
                            if (slot[10] == (1 << b))
                                value = 224 + b;
                    }
                    for (int b = 12; b < 18; b++)
                        if (slot[b]) {
                            if (pressed)
                                value = -1;
                            else
                                value = slot[b];
                            pressed = 1;
                        }
                } else {
                    unsigned int mask = slot[8] | (slot[9] << 8);
                    if (mask) {
                        pressed = 1;
                        for (int b = 0; b < 16; b++)
                            if (mask == (1u << b))
                                value = b;
                    }
                }
            }
        }
        if (ready && !pressed)
            neutral = 1;
        if (neutral && value >= 0)
            return value;
        readPads();
        /* The button being mapped may also be BACK; reserve START+SELECT
         * for explicit cancellation. */
        if (getKeyPressed(KEY_START) && getKeyPressed(KEY_SELECT))
            return -1;
        rmStartFrame();
        rmDrawRect(0, 0, 640, gTheme->usedHeight, MAIZO_BLACK);
        maizoDrawBrand();
        fntRenderString(gTheme->fonts[0], 320, gTheme->usedHeight / 2 - 45, ALIGN_CENTER, 560, 32, target, MAIZO_TEXT);
        fntRenderString(gTheme->fonts[0], 320, gTheme->usedHeight / 2, ALIGN_CENTER, 560, 29,
                        !ready ? "Conecte o dispositivo e pressione uma tecla." : !neutral ? "Solte todos os botões." :
                                                                                             "Pressione o botão ou tecla desejada.",
                        MAIZO_MUTED);
        fntRenderString(gTheme->fonts[0], 320, gTheme->usedHeight / 2 + 42, ALIGN_CENTER, 560, 25, "START + SELECT cancela", MAIZO_MUTED);
        guiDrawControllerNotification();
        rmEndFrame();
    }
    return -1;
}

static void maizoShowMapping(void)
{
    static const char *padNames[] = {"Select", "L3", "R3", "Start", "Cima", "Direita", "Baixo", "Esquerda", "L2", "R2", "L1", "R1", "Triangulo", "Circulo", "X", "Quadrado", "Desativado", NULL};
    static const char *keyNames[] = {"Desativado", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "Enter", "Esc", "Backspace", "Tab", "Espaco", "Direita", "Esquerda", "Baixo", "Cima", "Ctrl E", "Shift E", "Alt E", "Ctrl D", "Shift D", "Alt D", NULL};
    static const unsigned char keyCodes[] = {0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 79, 80, 81, 82, 224, 225, 226, 228, 229, 230};
    struct UIItem menu[] = {
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"Mapeamento de botoes", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 7101, 1, 1, -1, 0, 0, {.label = {"USB HID", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 7102, 1, 1, -1, 0, 0, {.label = {"Teclado USB", -1}}},
        {UI_BREAK},
        {UI_TERMINATOR}};
    int result;
    while ((result = diaExecuteDialog(menu, -1, 1, NULL)) == 7101 || result == 7102) {
        int keyboard = result == 7102, offset = keyboard ? 16 : 0;
        unsigned char profiles[MAIZO_PROFILE_SIZE];
        struct UIItem ui[16 * 6 + 7] = {{0}};
        int i, n = 0, selected, j;
        maizoLoadInputProfiles(profiles);
        ui[n++] = (struct UIItem) {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {keyboard ? "Teclado > DualShock" : "USB HID > DualShock", -1}}};
        ui[n++].type = UI_BREAK;
        for (i = 0; i < 16; i++) {
            selected = profiles[offset + i];
            if (keyboard) {
                selected = 0;
                for (j = 0; j < (int)sizeof(keyCodes); j++)
                    if (keyCodes[j] == profiles[offset + i])
                        selected = j;
            }
            ui[n++] = (struct UIItem) {UI_LABEL, 0, 1, 1, -1, -40, 0, {.label = {padNames[i], -1}}};
            ui[n++].type = UI_SPACER;
            ui[n++] = (struct UIItem) {UI_ENUM, 7200 + i, 1, 1, -1, 0, 0, {.intvalue = {selected, selected, 0, 0, keyboard ? keyNames : padNames}}};
            ui[n++].type = UI_BREAK;
            ui[n++] = (struct UIItem) {UI_BUTTON, 7400 + i, 1, 1, -1, 0, 0, {.label = {"Capturar botão / tecla", -1}}};
            ui[n++].type = UI_BREAK;
        }
        ui[n++] = (struct UIItem) {UI_BUTTON, 7300, 1, 1, -1, 0, 0, {.label = {"Restaurar padrao", -1}}};
        ui[n++].type = UI_BREAK;
        ui[n++] = (struct UIItem) {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {"Salvar", -1}}};
        ui[n++].type = UI_TERMINATOR;
        result = -1;
        do {
            result = diaExecuteDialog(ui, result, 1, NULL);
            if (result >= 7400 && result < 7416) {
                i = result - 7400;
                int captured = maizoCaptureInput(keyboard, padNames[i]);
                if (captured >= 0) {
                    if (keyboard) {
                        selected = -1;
                        for (j = 0; j < (int)sizeof(keyCodes); j++)
                            if (keyCodes[j] == captured)
                                selected = j;
                        if (selected < 0)
                            guiMsgBox("Tecla fora da lista de mapeamento.", 0, NULL);
                    } else
                        selected = captured;
                    if (selected >= 0)
                        diaSetInt(ui, 7200 + i, selected);
                }
                readPads();
            } else if (result == 7300) {
                for (i = 0; i < 16; i++) {
                    selected = i;
                    if (keyboard)
                        for (j = 0; j < (int)sizeof(keyCodes); j++)
                            if (keyCodes[j] == maizoDefaultKeys[i])
                                selected = j;
                    diaSetInt(ui, 7200 + i, selected);
                }
            }
        } while (result == 7300 || (result >= 7400 && result < 7416));
        if (result == UIID_BTN_OK) {
            char key[24];
            config_set_t *config = configGetByType(CONFIG_OPL);
            for (i = 0; i < 16; i++) {
                diaGetInt(ui, 7200 + i, &selected);
                if (keyboard && (selected < 0 || selected >= (int)sizeof(keyCodes)))
                    selected = 0;
                profiles[offset + i] = keyboard ? keyCodes[selected] : selected;
                snprintf(key, sizeof(key), "maizo_input_%02d", offset + i);
                configSetInt(config, key, profiles[offset + i]);
            }
            ds34usb_set_profiles(profiles);
            if (!saveConfig(CONFIG_OPL, 0))
                guiMsgBox("Falha ao salvar mapeamento.", 0, NULL);
        }
    }
}
#endif

void guiShowInputMapping(void)
{
#ifdef PADEMU
    maizoShowMapping();
#endif
}

void guiShowControllerConfig(void)
{
    int value;

    // configure the enumerations
    const char *scrollSpeeds[] = {_l(_STR_SLOW), _l(_STR_MEDIUM), _l(_STR_FAST), NULL};
    const char *selectButtons[] = {_l(_STR_CIRCLE), _l(_STR_CROSS), NULL};
    const char *sensitivity[] = {_l(_STR_LOW), _l(_STR_MEDIUM), _l(_STR_HIGH), NULL};

    diaSetEnum(diaControllerConfig, UICFG_SCROLL, scrollSpeeds);
    diaSetEnum(diaControllerConfig, CFG_SELECTBUTTON, selectButtons);
    diaSetEnum(diaControllerConfig, CFG_XSENSITIVITY, sensitivity);
    diaSetEnum(diaControllerConfig, CFG_YSENSITIVITY, sensitivity);

    diaSetInt(diaControllerConfig, UICFG_SCROLL, gScrollSpeed);
    diaSetInt(diaControllerConfig, CFG_SELECTBUTTON, gSelectButton == KEY_CIRCLE ? 0 : 1);
    diaSetInt(diaControllerConfig, CFG_XSENSITIVITY, gXSensitivity);
    diaSetInt(diaControllerConfig, CFG_YSENSITIVITY, gYSensitivity);

#ifdef PADEMU
    diaSetVisible(diaControllerConfig, PADEMU_GLOBAL_BUTTON, thmGetGuiValue() != 0);
    diaSetVisible(diaControllerConfig, MAIZO_MAPPING_BUTTON, thmGetGuiValue() != 0);
#endif
    int result = diaExecuteDialog(diaControllerConfig, -1, 1, NULL);
    if (result) {
        diaGetInt(diaControllerConfig, UICFG_SCROLL, &gScrollSpeed);
        diaGetInt(diaControllerConfig, CFG_XSENSITIVITY, &gXSensitivity);
        diaGetInt(diaControllerConfig, CFG_YSENSITIVITY, &gYSensitivity);

        if (diaGetInt(diaControllerConfig, CFG_SELECTBUTTON, &value))
            gSelectButton = value == 0 ? KEY_CIRCLE : KEY_CROSS;
        else
            gSelectButton = KEY_CIRCLE;
#ifdef PADEMU
        if (result == PADEMU_GLOBAL_BUTTON) {
            guiGameShowPadEmuConfig(1);
        } else if (result == MAIZO_MAPPING_BUTTON) {
            maizoShowMapping();
        }
#endif
        applyConfig(-1, -1, 1);
        saveConfig(CONFIG_OPL, 0);
    }
}

int guiShowKeyboard(char *value, int maxLength)
{
    char tmp[maxLength];
    strncpy(tmp, value, maxLength);

    int result = diaShowKeyb(tmp, maxLength, 0, NULL);
    if (result) {
        strncpy(value, tmp, maxLength);
        value[maxLength - 1] = '\0';
    }

    return result;
}

int guiGetOpCompleted(int opid)
{
    return gCompletedOps > opid;
}

int guiDeferUpdate(struct gui_update_t *op)
{
    WaitSema(gSemaId);

    struct gui_update_list_t *up = (struct gui_update_list_t *)malloc(sizeof(struct gui_update_list_t));
    up->item = op;
    up->next = NULL;

    if (!gUpdateList) {
        gUpdateList = up;
        gUpdateEnd = gUpdateList;
    } else {
        gUpdateEnd->next = up;
        gUpdateEnd = up;
    }

    SignalSema(gSemaId);

    return gScheduledOps++;
}

static void guiHandleOp(struct gui_update_t *item)
{
    submenu_list_t *result = NULL;

    switch (item->type) {
        case GUI_INIT_DONE:
            gInitComplete = 1;
            break;

        case GUI_OP_ADD_MENU:
            menuAppendItem(item->menu.menu);
            break;

        case GUI_OP_APPEND_MENU:
            result = submenuAppendItem(item->menu.subMenu, item->submenu.icon_id, item->submenu.text, item->submenu.id, item->submenu.text_id);
            if (!item->menu.menu->submenu) { // first subitem in list
                item->menu.menu->submenu = result;
                if (!item->submenu.selected) {
                    item->menu.menu->current = result;
                    item->menu.menu->pagestart = result;
                }
            }
            if (item->submenu.selected) { // remember last played game feature
                item->menu.menu->current = result;
                item->menu.menu->pagestart = result;
                item->menu.menu->remindLast = 1;

                // Last Played Auto Start
                if ((gAutoStartLastPlayed) && !(KeyPressedOnce))
                    DisableCron = 0; // Release Auto Start Last Played counter
            }

            break;

        case GUI_OP_SELECT_MENU:
            menuSetSelectedItem(item->menu.menu);
            screenHandler = &screenHandlers[GUI_SCREEN_MAIN];
            break;

        case GUI_OP_CLEAR_SUBMENU:
            submenuDestroy(item->menu.subMenu);
            item->menu.menu->submenu = NULL;
            item->menu.menu->current = NULL;
            item->menu.menu->pagestart = NULL;
            break;

        case GUI_OP_SORT:
            submenuSort(item->menu.subMenu);
            item->menu.menu->submenu = *item->menu.subMenu;

            if (!item->menu.menu->remindLast)
                item->menu.menu->current = item->menu.menu->submenu;

            item->menu.menu->pagestart = item->menu.menu->current;
            break;

        case GUI_OP_ADD_HINT:
            // append the hint list in the menu item
            menuAddHint(item->menu.menu, item->hint.text_id, item->hint.icon_id);
            break;

        default:
            LOG("GUI: ??? (%d)\n", item->type);
    }
}

static void guiHandleDeferredOps(void)
{

    WaitSema(gSemaId);
    while (gUpdateList) {

        guiHandleOp(gUpdateList->item);

        struct gui_update_list_t *td = gUpdateList;
        gUpdateList = gUpdateList->next;

        free(td);

        gCompletedOps++;
    }
    SignalSema(gSemaId);

    gUpdateEnd = NULL;
}

void guiExecDeferredOps(void)
{
    // Clears deferred operations list by executing them.
    guiHandleDeferredOps();
}

static void guiDrawBusy(int alpha)
{
    if (gTheme->loadingIcon) {
        GSTEXTURE *texture = thmGetTexture(LOAD0_ICON + (guiFrameId >> 1) % gTheme->loadingIconCount);
        if (texture && texture->Mem) {
            u64 mycolor = GS_SETREG_RGBA(0x80, 0x80, 0x80, alpha);
            rmDrawPixmap(texture, gTheme->loadingIcon->posX, gTheme->loadingIcon->posY, gTheme->loadingIcon->aligned, gTheme->loadingIcon->width, gTheme->loadingIcon->height, gTheme->loadingIcon->scaled, mycolor);
        }
    }
}

static void guiRenderGreeting(int alpha)
{
    u64 mycolor;
    if (thmGetGuiValue() == 0) {
        rmDrawRect(0, 0, screenWidth, screenHeight, MAIZO_BLACK);
        mycolor = GS_SETREG_RGBA(245, 246, 248, alpha);
        fntRenderString(gTheme->fonts[0], screenWidth >> 1, (gTheme->usedHeight >> 1) - 45,
                        ALIGN_CENTER, 340, 46, "MAIZO", mycolor);
        mycolor = GS_SETREG_RGBA(89, 165, 255, alpha);
        fntRenderString(gTheme->fonts[0], screenWidth >> 1, (gTheme->usedHeight >> 1) + 4,
                        ALIGN_CENTER, 320, 28, "SUPREME", mycolor);
        mycolor = GS_SETREG_RGBA(160, 164, 174, alpha);
        fntRenderString(gTheme->fonts[0], screenWidth >> 1, (gTheme->usedHeight >> 1) + 45,
                        ALIGN_CENTER, 420, 28,
                        gInitComplete ? "Pronto" : "Preparando biblioteca...", mycolor);
        return;
    }
    mycolor = GS_SETREG_RGBA(0x1C, 0x1C, 0x1C, alpha);
    rmDrawRect(0, 0, screenWidth, screenHeight, mycolor);

    GSTEXTURE *logo = thmGetTexture(LOGO_PICTURE);
    if (logo) {
        mycolor = GS_SETREG_RGBA(0x80, 0x80, 0x80, alpha);
        rmDrawPixmap(logo, screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, logo->Width, logo->Height, SCALING_RATIO, mycolor);
    }
}

static float mix(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float fade(float t)
{
    return fadetbl[(int)(t * FADE_SIZE)];
}

// The same as mix, but with 8 (2*4) values mixed at once
static void VU0MixVec(VU_VECTOR *a, VU_VECTOR *b, float mix, VU_VECTOR *res)
{
    asm volatile(
#if __GNUC__ > 3
        "lqc2           $vf1, (%[a])\n"        // load the first vector
        "lqc2           $vf2, (%[b])\n"        // load the second vector
        "qmtc2          %[mix], $vf3\n"        // move the mix value from reg to VU
        "vaddw.x        $vf5, $vf0, $vf0\n"    // vf5.x = 1
        "vsub.x         $vf4x, $vf5x, $vf3x\n" // subtract 1 - vf3,x, store the result in vf4.x
        "vmulax.xyzw    $ACC, $vf1, $vf3x\n"   // multiply vf1 by vf3.x, store the result in ACC
        "vmaddx.xyzw    $vf1, $vf2, $vf4x\n"   // multiply vf2 by vf4.x add ACC, store the result in vf1
        "sqc2           $vf1, (%[res])\n"      // transfer the result in acc to the ee
#else
        "lqc2           vf1, (%[a])\n"      // load the first vector
        "lqc2           vf2, (%[b])\n"      // load the second vector
        "qmtc2          %[mix], vf3\n"      // move the mix value from reg to VU
        "vaddw.x        vf5, vf00, vf00\n"  // vf5.x = 1
        "vsub.x         vf4x, vf5x, vf3x\n" // subtract 1 - vf3,x, store the result in vf4.x
        "vmulax.xyzw    ACC, vf1, vf3x\n"   // multiply vf1 by vf3.x, store the result in ACC
        "vmaddx.xyzw    vf1, vf2, vf4x\n"   // multiply vf2 by vf4.x add ACC, store the result in vf1
        "sqc2           vf1, (%[res])\n"    // transfer the result in acc to the ee
#endif
        : [res] "+r"(res), "=m"(*res)
        : [a] "r"(a), [b] "r"(b), [mix] "r"(mix), "m"(*a), "m"(*b));
}

static float guiCalcPerlin(float x, float y, float z)
{
    // Taken from: http://people.opera.com/patrickl/experiments/canvas/plasma/perlin-noise-classical.js
    // By Sean McCullough

    // Find unit grid cell containing point
    int X = floorf(x);
    int Y = floorf(y);
    int Z = floorf(z);

    // Get relative xyz coordinates of point within that cell
    x = x - X;
    y = y - Y;
    z = z - Z;

    // Wrap the integer cells at 255 (smaller integer period can be introduced here)
    X = X & 255;
    Y = Y & 255;
    Z = Z & 255;

    // Calculate a set of eight hashed gradient indices
    int gi000 = pperm[X + pperm[Y + pperm[Z]]] % 12;
    int gi001 = pperm[X + pperm[Y + pperm[Z + 1]]] % 12;
    int gi010 = pperm[X + pperm[Y + 1 + pperm[Z]]] % 12;
    int gi011 = pperm[X + pperm[Y + 1 + pperm[Z + 1]]] % 12;
    int gi100 = pperm[X + 1 + pperm[Y + pperm[Z]]] % 12;
    int gi101 = pperm[X + 1 + pperm[Y + pperm[Z + 1]]] % 12;
    int gi110 = pperm[X + 1 + pperm[Y + 1 + pperm[Z]]] % 12;
    int gi111 = pperm[X + 1 + pperm[Y + 1 + pperm[Z + 1]]] % 12;

    // The gradients of each corner are now:
    // g000 = grad3[gi000];
    // g001 = grad3[gi001];
    // g010 = grad3[gi010];
    // g011 = grad3[gi011];
    // g100 = grad3[gi100];
    // g101 = grad3[gi101];
    // g110 = grad3[gi110];
    // g111 = grad3[gi111];
    // Calculate noise contributions from each of the eight corners
    VU_VECTOR vec;
    vec.x = x;
    vec.y = y;
    vec.z = z;
    vec.w = 1;

    VU_VECTOR a, b;

    // float n000
    a.x = Vu0DotProduct(&pgrad3[gi000], &vec);

    vec.y -= 1;

    // float n010
    a.z = Vu0DotProduct(&pgrad3[gi010], &vec);

    vec.x -= 1;

    // float n110
    b.z = Vu0DotProduct(&pgrad3[gi110], &vec);

    vec.y += 1;

    // float n100
    b.x = Vu0DotProduct(&pgrad3[gi100], &vec);

    vec.z -= 1;

    // float n101
    b.y = Vu0DotProduct(&pgrad3[gi101], &vec);

    vec.y -= 1;

    // float n111
    b.w = Vu0DotProduct(&pgrad3[gi111], &vec);

    vec.x += 1;

    // float n011
    a.w = Vu0DotProduct(&pgrad3[gi011], &vec);

    vec.y += 1;

    // float n001
    a.y = Vu0DotProduct(&pgrad3[gi001], &vec);

    // Compute the fade curve value for each of x, y, z
    float u = fade(x);
    float v = fade(y);
    float w = fade(z);

    // TODO: Low priority... This could be done on VU0 (xyzw for the first 4 mixes)
    // The result in sw
    // Interpolate along x the contributions from each of the corners
    VU_VECTOR rv;
    VU0MixVec(&b, &a, u, &rv);

    // TODO: The VU0MixVec could as well mix the results (as follows) - might improve performance...
    // Interpolate the four results along y
    float nxy0 = mix(rv.x, rv.z, v);
    float nxy1 = mix(rv.y, rv.w, v);
    // Interpolate the two last results along z
    float nxyz = mix(nxy0, nxy1, w);

    return nxyz;
}

static float dir = 0.02;
static float perz = -100;
static int pery = 0;
static unsigned char curbgColor[3] = {0, 0, 0};

static int cdirection(unsigned char a, unsigned char b)
{
    if (a == b)
        return 0;
    else if (a > b)
        return -1;
    else
        return 1;
}

void guiDrawBGPlasma()
{
    if (thmGetGuiValue() == 0) {
        rmDrawRect(0, 0, screenWidth, screenHeight, GS_SETREG_RGBA(gTheme->bgColor[0], gTheme->bgColor[1], gTheme->bgColor[2], 0x80));
        return;
    }

    int x, y;

    // transition the colors
    curbgColor[0] += cdirection(curbgColor[0], gTheme->bgColor[0]);
    curbgColor[1] += cdirection(curbgColor[1], gTheme->bgColor[1]);
    curbgColor[2] += cdirection(curbgColor[2], gTheme->bgColor[2]);

    // it's PLASMA_ROWS_PER_FRAME rows a frame to stop being a resource hog
    if (pery >= PLASMA_H) {
        pery = 0;
        perz += dir;

        if (perz > 100.0f || perz < -100.0f)
            dir = -dir;
    }

    u32 *buf = gBackgroundTex.Mem + PLASMA_W * pery;
    int ymax = pery + PLASMA_ROWS_PER_FRAME;

    if (ymax > PLASMA_H)
        ymax = PLASMA_H;

    for (y = pery; y < ymax; y++) {
        for (x = 0; x < PLASMA_W; x++) {
            u32 fper = guiCalcPerlin((float)(2 * x) / PLASMA_W, (float)(2 * y) / PLASMA_H, perz) * 0x80 + 0x80;

            *buf = GS_SETREG_RGBA(
                (u32)(fper * curbgColor[0]) >> 8,
                (u32)(fper * curbgColor[1]) >> 8,
                (u32)(fper * curbgColor[2]) >> 8,
                0x80);

            ++buf;
        }
    }

    pery = ymax;
    rmInvalidateTexture(&gBackgroundTex);
    rmDrawPixmap(&gBackgroundTex, 0, 0, ALIGN_NONE, screenWidth, screenHeight, SCALING_NONE, gDefaultCol);
}

int guiDrawIconAndText(int iconId, int textId, int font, int x, int y, u64 color)
{
    GSTEXTURE *iconTex = thmGetTexture(iconId);
    int w = 0;
    int h = 20;

    if (iconTex) {
        w = (iconTex->Width * 20) / iconTex->Height;
    }

    if (iconTex && iconTex->Mem) {
        y += h >> 1;
        rmDrawPixmap(iconTex, x, y, ALIGN_VCENTER, w, h, SCALING_RATIO, gDefaultCol);
        x += rmWideScale(w) + 2;
    } else {
        // HACK: font is aligned to VCENTER, the default height icon height is 20
        y += 10;
    }

    x = fntRenderString(font, x, y, ALIGN_VCENTER, 0, 0, _l(textId), color);

    return x;
}

int guiAlignMenuHints(menu_hint_item_t *hint, int font, int width)
{
    int x = screenWidth;
    int w;

    for (; hint; hint = hint->next) {
        GSTEXTURE *iconTex = thmGetTexture(hint->icon_id);
        w = (iconTex->Width * 20) / iconTex->Height;
        char *text = _l(hint->text_id);

        x -= rmWideScale(w) + 2;
        x -= rmUnScaleX(fntCalcDimensions(font, text));
        if (hint->next != NULL)
            x -= width;
    }

    // align center
    x /= 2;

    return x;
}

int guiAlignSubMenuHints(int hintCount, int *textID, int *iconID, int font, int width, int align)
{
    int x = screenWidth;
    int i, w;

    for (i = 0; i < hintCount; i++) {
        GSTEXTURE *iconTex = thmGetTexture(iconID[i]);
        w = (iconTex->Width * 20) / iconTex->Height;
        char *text = _l(textID[i]);

        x -= rmWideScale(w) + 2;
        x -= rmUnScaleX(fntCalcDimensions(font, text));
        if (i != (hintCount - 1))
            x -= width;
    }

    if (align == 1) // align center
        x /= 2;

    if (align == 2) // align right
        x -= 20;

    return x;
}

void guiDrawSubMenuHints(void)
{
    int subMenuHints[2] = {_STR_SELECT, _STR_GAMES_LIST};
    int subMenuIcons[2] = {CIRCLE_ICON, CROSS_ICON};

    int x = guiAlignSubMenuHints(2, subMenuHints, subMenuIcons, gTheme->fonts[0], 12, 2);
    int y = gTheme->usedHeight - 32;

    x = guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? subMenuIcons[0] : subMenuIcons[1], subMenuHints[0], gTheme->fonts[0], x, y, gTheme->textColor);
    x += 12;
    guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? subMenuIcons[1] : subMenuIcons[0], subMenuHints[1], gTheme->fonts[0], x, y, gTheme->textColor);
}

static int endIntro = 0; // Break intro loop and start 'Last Played Auto Start' countdown
static void guiDrawOverlays()
{
    // are there any pending operations?
    int pending = ioHasPendingRequests();
    static int busyAlpha = 0x00; // Fully transparant

    if (!pending) {
        // Fade out
        if (busyAlpha > 0x00)
            busyAlpha -= 0x02;
    } else {
        // Fade in
        if (busyAlpha < 0x80)
            busyAlpha += 0x02;
    }

    if (busyAlpha > 0x00)
        guiDrawBusy(busyAlpha);

#ifdef __DEBUG
    char text[20];
    int x = screenWidth - 120;
    int y = 15;
    int yadd = 15;

    snprintf(text, sizeof(text), "VRAM:");
    fntRenderString(gTheme->fonts[0], x, y, ALIGN_LEFT, 0, 0, text, GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));
    y += yadd;

    snprintf(text, sizeof(text), "%dKiB FIXED", gsGlobal->CurrentPointer / 1024);
    fntRenderString(gTheme->fonts[0], x, y, ALIGN_LEFT, 0, 0, text, GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));
    y += yadd;

    snprintf(text, sizeof(text), "%dKiB TEXMAN", ((4 * 1024 * 1024) - gsGlobal->CurrentPointer) / 1024);
    fntRenderString(gTheme->fonts[0], x, y, ALIGN_LEFT, 0, 0, text, GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));
    y += yadd;
    y += yadd; // Empty line

    if (prevtime != 0) {
        clock_t diff = curtime - prevtime;
        if (diff == 0)
            diff = 1;

        // Raw FPS value with 2 decimal places
        float rawfps = ((100 * CLOCKS_PER_SEC) / diff) / 100.0f;

        if (fps == 0.0f)
            fps = rawfps;
        else
            fps = fps * 0.9f + rawfps / 10.0f; // Smooth FPS value

        snprintf(text, sizeof(text), "%.1f FPS", fps);
        fntRenderString(gTheme->fonts[0], x, y, ALIGN_LEFT, 0, 0, text, GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));
        y += yadd;
    }
#endif

    // Last Played Auto Start
    if (!pending && DisableCron == 0 && endIntro) {
        if (CronStart == 0) {
            CronStart = clock() / CLOCKS_PER_SEC;
        } else {
            char strAutoStartInNSecs[21];
            clock_t CronCurrent;

            CronCurrent = clock() / CLOCKS_PER_SEC;
            RemainSecs = gAutoStartLastPlayed - (CronCurrent - CronStart);
            snprintf(strAutoStartInNSecs, sizeof(strAutoStartInNSecs), _l(_STR_AUTO_START_IN_N_SECS), RemainSecs);
            fntRenderString(gTheme->fonts[0], screenWidth / 2, screenHeight / 2, ALIGN_CENTER, 0, 0, strAutoStartInNSecs, GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));
        }
    }

    // BLURT output
    // if (gEnableDebug)
    //     fntRenderString(gTheme->fonts[0], 0, screenHeight - 24, ALIGN_NONE, 0, 0, blurttext, GS_SETREG_RGBA(255, 255, 0, 128));
}

static void guiReadPads()
{
    if (readPads())
        guiInactiveFrames = 0;
    else
        guiInactiveFrames++;
}

// renders the screen and handles inputs. Also handles screen transitions between numerous
// screen handlers. Fade transition code written by Maximus32
static void guiShow()
{
    /* Disabled effects must never delay navigation or launch callbacks. */
    if (screenHandlerTarget && thmGetGuiValue() == 0 && !maizoAnimations()) {
        screenHandler = screenHandlerTarget;
        screenHandlerTarget = NULL;
        transIndex = 0;
    }

    // is there a transmission effect going on or are
    // we in a normal rendering state?
    if (screenHandlerTarget) {
        u8 alpha;
        const u8 transition_frames = thmGetGuiValue() == 0 ?
                                         (maizoAnimationMode() == 1 ? 6 : 8) :
                                         26;
        if (transIndex < (transition_frames / 2)) {
            // Fade-out old screen
            // index: 0..7
            // alpha: 1..8 * transition_step
            screenHandler->renderScreen();
            alpha = fade((float)(transIndex + 1) / (transition_frames / 2)) * 0x80;
        } else {
            // Fade-in new screen
            // index: 8..15
            // alpha: 8..1 * transition_step
            screenHandlerTarget->renderScreen();
            alpha = fade((float)(transition_frames - transIndex) / (transition_frames / 2)) * 0x80;
        }

        // Overlay the actual "fade"
        rmDrawRect(0, 0, screenWidth, screenHeight, GS_SETREG_RGBA(0x00, 0x00, 0x00, alpha));

        // Advance the effect
        transIndex++;
        if (transIndex >= transition_frames) {
            screenHandler = screenHandlerTarget;
            screenHandlerTarget = NULL;
        }
    } else
        // render with the set screen handler
        screenHandler->renderScreen();

    /* Each Supreme screen draws its own header exactly once. Repeating the
     * brand here doubled font rendering on every frame. */
}

void guiIntroLoop(void)
{
    int greetingAlpha = 0x80;
    const int fadeFrameCount = 0x80 / 2;
    const int fadeDuration = (fadeFrameCount * 1000) / 55; // Average between 50 and 60 fps
    clock_t tFadeDelayEnd = 0;

    while (!endIntro) {
        guiStartFrame();

        if (greetingAlpha < 0x80)
            guiShow();

        if (greetingAlpha > 0)
            guiRenderGreeting(greetingAlpha);

        // Initialize boot sound
        if (gInitComplete && !tFadeDelayEnd && gEnableBootSND) {
            // Start playing sound
            sfxPlay(SFX_BOOT);
            // Calculate transition delay
            tFadeDelayEnd = clock() + (sfxGetSoundDuration(SFX_BOOT) - fadeDuration) * (CLOCKS_PER_SEC / 1000);
        }

        if (gInitComplete && clock() >= tFadeDelayEnd)
            greetingAlpha -= 2;

        if (greetingAlpha <= 0)
            endIntro = 1;

        guiDrawOverlays();

        guiHandleDeferredOps();

        guiEndFrame();

        if (!screenHandlerTarget && screenHandler)
            screenHandler->handleInput();
    }
}

void guiMainLoop(void)
{
    guiResetNotifications();
    guiCheckNotifications(1, 1);

    if (gOPLPart[0] != '\0')
        showPartPopup = 1;

    if (gEnableBGM)
        bgmStart();

    while (!gTerminate) {
        guiStartFrame();

        // Read the pad states to prepare for input processing in the screen handler
        guiReadPads();

        // handle inputs and render screen
        guiShow();

        // Render overlaying gui thingies :)
        guiDrawOverlays();

        if (gEnableNotifications)
            guiShowNotifications();

        // handle deferred operations
        guiHandleDeferredOps();

        guiEndFrame();

        // if not transiting, handle input
        // done here so we can use renderman if needed
        if (!screenHandlerTarget && screenHandler)
            screenHandler->handleInput();

        if (gFrameHook)
            gFrameHook();
    }
}

void guiSetFrameHook(gui_callback_t cback)
{
    gFrameHook = cback;
}

void guiSwitchScreen(int target)
{
    // Only initiate the transition once or else we could get stuck in an infinite loop.
    if (screenHandlerTarget != NULL) {
        return;
    }
    sfxPlay(SFX_TRANSITION);
    transIndex = 0;
    screenHandlerTarget = &screenHandlers[target];
}

struct gui_update_t *guiOpCreate(gui_op_type_t type)
{
    struct gui_update_t *op = (struct gui_update_t *)malloc(sizeof(struct gui_update_t));
    memset(op, 0, sizeof(struct gui_update_t));
    op->type = type;
    return op;
}

void guiUpdateScrollSpeed(void)
{
    // sanitize the settings
    if ((gScrollSpeed < 0) || (gScrollSpeed > 2))
        gScrollSpeed = 1;

    // update the pad delays for KEY_UP and KEY_DOWN
    // default delay is 7
    // fast - 100 ms
    // medium - 300 ms
    // slow - 500 ms
    setButtonDelay(KEY_UP, 500 - gScrollSpeed * 200); // 0,1,2 -> 500, 300, 100
    setButtonDelay(KEY_DOWN, 500 - gScrollSpeed * 200);
}

void guiUpdateScreenScale(void)
{
    fntUpdateAspectRatio();
}

int guiMsgBox(const char *text, int addAccept, struct UIItem *ui)
{
    int terminate = 0;

    sfxPlay(SFX_MESSAGE);

    while (!terminate) {
        guiStartFrame();

        readPads();

        if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE))
            terminate = 1;
        else if (getKeyOn(gSelectButton))
            terminate = 2;

        if (ui)
            diaRenderUI(ui, screenHandler->inMenu, NULL, 0);
        else
            guiShow();

        if (thmGetGuiValue() == 0) {
            rmDrawRect(0, 0, screenWidth, screenHeight,
                       GS_SETREG_RGBA(0, 0, 0, 0x60));
            maizoDrawPanel(58, (gTheme->usedHeight >> 1) - 86,
                           screenWidth - 116, 172, 0);
            maizoDrawIcon(MAIZO_ICON_WARNING, 82,
                          (gTheme->usedHeight >> 1) - 55, 28, MAIZO_WARNING);
            fntRenderString(gTheme->fonts[0], screenWidth >> 1,
                            (gTheme->usedHeight >> 1) - 32,
                            ALIGN_CENTER, screenWidth - 154, 95, text, MAIZO_TEXT);
            guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CROSS_ICON : CIRCLE_ICON,
                               _STR_BACK, gTheme->fonts[0], 470,
                               (gTheme->usedHeight >> 1) + 58, MAIZO_MUTED);
            if (addAccept)
                guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CIRCLE_ICON : CROSS_ICON,
                                   _STR_ACCEPT, gTheme->fonts[0], 85,
                                   (gTheme->usedHeight >> 1) + 58, MAIZO_ACCENT);
        } else {
            rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);
            rmDrawLine(50, 75, screenWidth - 50, 75, gColWhite);
            rmDrawLine(50, 410, screenWidth - 50, 410, gColWhite);
            fntRenderString(gTheme->fonts[0], screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, 0, 0, text, gTheme->textColor);
            guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CROSS_ICON : CIRCLE_ICON, _STR_BACK, gTheme->fonts[0], 500, 417, gTheme->selTextColor);
            if (addAccept)
                guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CIRCLE_ICON : CROSS_ICON, _STR_ACCEPT, gTheme->fonts[0], 70, 417, gTheme->selTextColor);
        }

        guiEndFrame();
    }

    if (terminate == 1) {
        sfxPlay(SFX_CANCEL);
    }
    if (terminate == 2) {
        sfxPlay(SFX_CONFIRM);
    }

    return terminate - 1;
}

void guiHandleDeferedIO(int *ptr, const char *message, int type, void *data)
{
    ioPutRequest(type, data);

    while (*ptr)
        guiRenderTextScreen(message);
}

void guiGameHandleDeferedIO(int *ptr, struct UIItem *ui, int type, void *data)
{
    ioPutRequest(type, data);

    while (*ptr) {
        guiStartFrame();
        if (ui)
            diaRenderUI(ui, screenHandler->inMenu, NULL, 0);
        else
            guiShow();
        guiEndFrame();
    }
}

void guiRenderTextScreen(const char *message)
{
    guiStartFrame();

    guiShow();

    if (thmGetGuiValue() == 0) {
        rmDrawRect(0, 0, screenWidth, screenHeight,
                   GS_SETREG_RGBA(0, 0, 0, 0x5A));
        maizoDrawPanel(54, (gTheme->usedHeight >> 1) - 53,
                       screenWidth - 108, 106, 0);
        fntRenderString(gTheme->fonts[0], screenWidth >> 1,
                        (gTheme->usedHeight >> 1) - 6, ALIGN_CENTER,
                        screenWidth - 142, 65, message, MAIZO_TEXT);
    } else {
        rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);
        fntRenderString(gTheme->fonts[0], screenWidth >> 1, gTheme->usedHeight >> 1,
                        ALIGN_CENTER, 0, 0, message, gTheme->textColor);
    }

    guiDrawOverlays();

    guiEndFrame();
}

void guiWarning(const char *text, int count)
{
    guiStartFrame();

    guiShow();

    rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);

    rmDrawLine(50, 75, screenWidth - 50, 75, gColWhite);
    rmDrawLine(50, 410, screenWidth - 50, 410, gColWhite);

    fntRenderString(gTheme->fonts[0], screenWidth >> 1, gTheme->usedHeight >> 1, ALIGN_CENTER, screenWidth, screenHeight, text, gTheme->textColor);

    guiEndFrame();

    delay(count);
}

int guiConfirmVideoMode(void)
{
    clock_t timeEnd;
    int terminate = 0;

    sfxPlay(SFX_MESSAGE);

    timeEnd = clock() + OPL_VMODE_CHANGE_CONFIRMATION_TIMEOUT_MS * (CLOCKS_PER_SEC / 1000);
    while (!terminate) {
        guiStartFrame();

        readPads();

        if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE))
            terminate = 1;
        else if (getKeyOn(gSelectButton))
            terminate = 2;

        // If the user fails to respond within the timeout period, deem it as a cancel operation.
        if (clock() > timeEnd)
            terminate = 1;

        guiShow();

        if (thmGetGuiValue() == 0) {
            int top = (gTheme->usedHeight >> 1) - 95;
            rmDrawRect(0, 0, screenWidth, gTheme->usedHeight, MAIZO_BLACK);
            maizoDrawBrand();
            maizoDrawPanel(52, top, screenWidth - 104, 191, 0);
            maizoDrawIcon(MAIZO_ICON_VIDEO, 73, top + 22, 31, MAIZO_ACCENT);
            fntRenderString(gTheme->fonts[0], 112, top + 29,
                            ALIGN_NONE, screenWidth - 174, 32,
                            "CONFIRMAR VIDEO", MAIZO_TEXT);
            fntRenderString(gTheme->fonts[0], 74, top + 88,
                            ALIGN_NONE, screenWidth - 150, 46,
                            _l(_STR_CFM_VMODE_CHG), MAIZO_MUTED);
            fntRenderString(gTheme->fonts[0], 74, top + 154,
                            ALIGN_NONE, screenWidth - 151, 29,
                            "CONFIRMAR     |     VOLTAR / CANCELAR",
                            MAIZO_BRIGHT);
        } else {
            rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);
            rmDrawLine(50, 75, screenWidth - 50, 75, gColWhite);
            rmDrawLine(50, 410, screenWidth - 50, 410, gColWhite);
            fntRenderString(gTheme->fonts[0], screenWidth >> 1,
                            gTheme->usedHeight >> 1, ALIGN_CENTER, 0, 0,
                            _l(_STR_CFM_VMODE_CHG), gTheme->textColor);
            guiDrawIconAndText(gSelectButton == KEY_CIRCLE ?
                                   CROSS_ICON :
                                   CIRCLE_ICON,
                               _STR_BACK,
                               gTheme->fonts[0], 500, 417, gTheme->selTextColor);
            guiDrawIconAndText(gSelectButton == KEY_CIRCLE ?
                                   CIRCLE_ICON :
                                   CROSS_ICON,
                               _STR_ACCEPT,
                               gTheme->fonts[0], 70, 417, gTheme->selTextColor);
        }

        guiEndFrame();
    }

    if (terminate == 1) {
        sfxPlay(SFX_CANCEL);
    }
    if (terminate == 2) {
        sfxPlay(SFX_CONFIRM);
    }

    return terminate - 1;
}

int guiGameShowRemoveSettings(config_set_t *configSet, config_set_t *configGame)
{
    int terminate = 0;
    char message[256];

    sfxPlay(SFX_MESSAGE);

    while (!terminate) {
        guiStartFrame();

        readPads();

        if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE))
            terminate = 1;
        else if (getKeyOn(gSelectButton))
            terminate = 2;
        else if (getKeyOn(KEY_SQUARE))
            terminate = 3;
        else if (getKeyOn(KEY_TRIANGLE))
            terminate = 4;

        guiShow();

        if (thmGetGuiValue() == 0) {
            int top = (gTheme->usedHeight >> 1) - 146;
            const char *const choices[] = {
                "X/O: remover configuracoes deste jogo",
                "QUADRADO: remover configuracoes globais",
                "TRIANGULO: remover todas as configuracoes",
                "VOLTAR: cancelar sem alterar arquivos"};
            rmDrawRect(0, 0, screenWidth, gTheme->usedHeight, MAIZO_BLACK);
            maizoDrawBrand();
            maizoDrawPanel(36, top, screenWidth - 72, 290, 0);
            maizoDrawIcon(MAIZO_ICON_DELETE, 57, top + 23,
                          31, MAIZO_WARNING);
            fntRenderString(gTheme->fonts[0], 103, top + 26,
                            ALIGN_NONE, screenWidth - 147, 33,
                            "REMOVER CONFIGURACOES", MAIZO_TEXT);
            fntRenderString(gTheme->fonts[0], 53, top + 65,
                            ALIGN_NONE, screenWidth - 109, 46,
                            _l(_STR_GAME_SETTINGS_PROMPT), MAIZO_MUTED);
            for (int j = 0; j < 4; ++j) {
                int y = top + 120 + j * 39;
                rmDrawRect(53, y, screenWidth - 106, 34,
                           j == 2 ? MAIZO_CARD : MAIZO_SURFACE);
                maizoDrawIcon(j == 2 ? MAIZO_ICON_WARNING :
                              j == 3 ? MAIZO_ICON_BACK :
                                       MAIZO_ICON_SETTINGS,
                              63, y + 7, 20,
                              j == 2 ? MAIZO_WARNING : MAIZO_BRIGHT);
                fntRenderString(gTheme->fonts[0], 95, y + 6,
                                ALIGN_NONE, screenWidth - 153, 26,
                                choices[j], j == 2 ? MAIZO_WARNING : MAIZO_TEXT);
            }
        } else {
            rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);
            rmDrawLine(50, 75, screenWidth - 50, 75, gColWhite);
            rmDrawLine(50, 410, screenWidth - 50, 410, gColWhite);
            fntRenderString(gTheme->fonts[0], screenWidth >> 1,
                            gTheme->usedHeight >> 1, ALIGN_CENTER, 0, 0,
                            _l(_STR_GAME_SETTINGS_PROMPT), gTheme->textColor);
            guiDrawIconAndText(gSelectButton == KEY_CIRCLE ?
                                   CROSS_ICON :
                                   CIRCLE_ICON,
                               _STR_BACK,
                               gTheme->fonts[0], 500, 417, gTheme->selTextColor);
            guiDrawIconAndText(SQUARE_ICON, _STR_GLOBAL_SETTINGS,
                               gTheme->fonts[0], 213, 417, gTheme->selTextColor);
            guiDrawIconAndText(TRIANGLE_ICON, _STR_ALL_SETTINGS,
                               gTheme->fonts[0], 356, 417, gTheme->selTextColor);
            guiDrawIconAndText(gSelectButton == KEY_CIRCLE ?
                                   CIRCLE_ICON :
                                   CROSS_ICON,
                               _STR_PERGAME_SETTINGS,
                               gTheme->fonts[0], 70, 417, gTheme->selTextColor);
        }

        guiEndFrame();
    }

    if (terminate == 1) {
        sfxPlay(SFX_CANCEL);
        return 0;
    } else if (terminate == 2) {
        guiGameRemoveSettings(configSet);
        snprintf(message, sizeof(message), _l(_STR_GAME_SETTINGS_REMOVED), _l(_STR_PERGAME_SETTINGS));
    } else if (terminate == 3) {
        guiGameRemoveGlobalSettings(configGame);
        snprintf(message, sizeof(message), _l(_STR_GAME_SETTINGS_REMOVED), _l(_STR_GLOBAL_SETTINGS));
    } else if (terminate == 4) {
        guiGameRemoveSettings(configSet);
        guiGameRemoveGlobalSettings(configGame);
        snprintf(message, sizeof(message), _l(_STR_GAME_SETTINGS_REMOVED), _l(_STR_ALL_SETTINGS));
    }
    sfxPlay(SFX_CONFIRM);
    guiMsgBox(message, 0, NULL);

    return 1;
}

void guiManageCheats(config_set_t *configSet)
{
    int offset = 0;
    int terminate = 0;
    int cheatCount = 0;
    int selectedCheat = 0;
    int visibleCheats = thmGetGuiValue() == 0 ? 7 : 10;

    while (cheatCount < MAX_CODES && strlen(gCheats[cheatCount].name) > 0)
        cheatCount++;

    sfxPlay(SFX_MESSAGE);

    while (!terminate) {
        guiStartFrame();
        readPads();

        if (getKeyOn(KEY_UP) && selectedCheat > 0) {
            selectedCheat -= 1;
            if (selectedCheat < offset)
                offset = selectedCheat;
        }

        if (getKeyOn(KEY_DOWN) && selectedCheat < cheatCount - 1) {
            selectedCheat += 1;
            if (selectedCheat >= offset + visibleCheats)
                offset = selectedCheat - visibleCheats + 1;
        }

        if (getKeyOn(gSelectButton) && cheatCount > 0) {
            if (!cheatConfigIsMasterCode(gCheats[selectedCheat].name))
                gCheats[selectedCheat].enabled = !gCheats[selectedCheat].enabled;
        }

        if (getKeyOn(KEY_START))
            terminate = 1;

        guiShow();

        if (thmGetGuiValue() == 0) {
            rmDrawRect(0, 0, screenWidth, gTheme->usedHeight, MAIZO_BLACK);
            maizoDrawBrand();
            maizoDrawPanel(29, 102, screenWidth - 58, 300, 0);
            maizoDrawIcon(MAIZO_ICON_CHEATS, 44, 112, 31, MAIZO_BRIGHT);
            fntRenderString(gTheme->fonts[0], 87, 119, ALIGN_NONE,
                            screenWidth - 121, 29, _l(_STR_CHEAT_SELECTION),
                            MAIZO_TEXT);
            maizoDrawSeparator(47, 148, screenWidth - 94);
            if (cheatCount == 0)
                fntRenderString(gTheme->fonts[0], 52, 211, ALIGN_NONE,
                                screenWidth - 104, 33,
                                "Nenhum cheat encontrado.", MAIZO_MUTED);
        } else {
            rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);
            rmDrawLine(50, 75, screenWidth - 50, 75, gColWhite);
            rmDrawLine(50, 410, screenWidth - 50, 410, gColWhite);
            fntRenderString(gTheme->fonts[0], screenWidth >> 1, 60,
                            ALIGN_CENTER, 0, 0,
                            _l(_STR_CHEAT_SELECTION), gTheme->textColor);
        }

        int renderedCheats = 0;
        for (int i = offset; renderedCheats < visibleCheats && i < cheatCount; i++) {
            if (strlen(gCheats[i].name) == 0)
                continue;

            if (thmGetGuiValue() == 0) {
                int y = 162 + renderedCheats * 33;
                int focused = i == selectedCheat;
                rmDrawRect(45, y, screenWidth - 90, 31,
                           focused ? MAIZO_CARD : MAIZO_SURFACE);
                if (focused)
                    rmDrawRect(45, y, 3, 31, MAIZO_ACCENT);
                fntRenderString(gTheme->fonts[0], 60, y + 6,
                                ALIGN_NONE, screenWidth - 205, 25,
                                gCheats[i].name,
                                focused ? MAIZO_TEXT : MAIZO_MUTED);
                rmDrawRect(screenWidth - 139, y + 7, 70, 19,
                           gCheats[i].enabled ? MAIZO_ACCENT :
                                                MAIZO_DIVIDER);
                fntRenderString(gTheme->fonts[0], screenWidth - 104, y + 5,
                                ALIGN_CENTER, 63, 22,
                                gCheats[i].enabled ? "ON" : "OFF",
                                MAIZO_TEXT);
            } else {
                int enabled = gCheats[i].enabled;
                int boxX = 50;
                int boxY = 100 + (renderedCheats * 30);
                int boxWidth = rmWideScale(25);
                int boxHeight = 17;
                if (enabled) {
                    rmDrawRect(boxX, boxY + 3,
                               boxWidth, boxHeight, gTheme->textColor);
                    rmDrawRect(boxX + 2, boxY + 5,
                               boxWidth - 4, boxHeight - 4,
                               gTheme->selTextColor);
                }
                u32 textColour = (i == selectedCheat) ?
                                     gTheme->selTextColor :
                                     gTheme->textColor;
                fntRenderString(gTheme->fonts[0], boxX + 35, boxY + 3,
                                ALIGN_LEFT, 0, 0, gCheats[i].name,
                                textColour);
            }
            renderedCheats++;
        }

        if (thmGetGuiValue() == 0) {
            rmDrawRect(0, gTheme->usedHeight - 60,
                       screenWidth, 60, MAIZO_SURFACE);
            fntRenderString(gTheme->fonts[0], 34,
                            gTheme->usedHeight - 38,
                            ALIGN_NONE, screenWidth - 70, 25,
                            "X/O ATIVAR   |   START SALVAR E VOLTAR",
                            MAIZO_MUTED);
        } else {
            guiDrawIconAndText(gSelectButton == KEY_CIRCLE ?
                                   CIRCLE_ICON :
                                   CROSS_ICON,
                               _STR_SELECT,
                               gTheme->fonts[0], 70, 417, gTheme->selTextColor);
            guiDrawIconAndText(START_ICON, _STR_RUN,
                               gTheme->fonts[0], 500, 417,
                               gTheme->selTextColor);
        }
        guiEndFrame();
    }

    cheatConfigSaveSelections(configSet);
    configWrite(configSet);
    sfxPlay(SFX_CONFIRM);
}
