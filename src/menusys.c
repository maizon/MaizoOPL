/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/menusys.h"
#include "include/iosupport.h"
#include "include/renderman.h"
#include "include/fntsys.h"
#include "include/lang.h"
#include "include/themes.h"
#include "include/pad.h"
#include "include/gui.h"
#include "include/guigame.h"
#include "include/system.h"
#include "include/ioman.h"
#include "include/sound.h"
#include <assert.h>
#include <time.h>
#include "include/maizo_ui.h"
#include "include/appsupport.h"

enum MENU_IDs {
    MENU_SETTINGS = 0,
    MENU_GFX_SETTINGS,
    MENU_AUDIO_SETTINGS,
    MENU_CONTROLLER_SETTINGS,
    MENU_OSD_LANGUAGE_SETTINGS,
    MENU_PARENTAL_LOCK,
    MENU_NET_CONFIG,
    MENU_NET_UPDATE,
    MENU_START_NBD,
    MENU_ABOUT,
    MENU_SAVE_CHANGES,
    MENU_EXIT,
    MENU_POWER_OFF,
    MENU_ITEM_INFO,
    MENU_ANIMATIONS,
    MENU_PADEMU,
    MENU_BLUETOOTH,
    MENU_STORAGE_SETTINGS,
    MENU_PS1_SETTINGS,
    MENU_VIDEO_SETTINGS,
    MENU_USB_DIAG,
    MENU_DIAG_SYSTEM,
    MENU_LIBRARY_SORT,
    MENU_COVER_MODE,
    MENU_LIBRARY_REFRESH
};

enum GAME_MENU_IDs {
    GAME_COMPAT_SETTINGS = 0,
    GAME_CHEAT_SETTINGS,
    GAME_GSM_SETTINGS,
    GAME_VMC_SETTINGS,
#ifdef PADEMU
    GAME_PADEMU_SETTINGS,
    GAME_PADMACRO_SETTINGS,
#endif
    GAME_OSD_LANGUAGE_SETTINGS,
    GAME_SAVE_CHANGES,
    GAME_TEST_CHANGES,
    GAME_REMOVE_CHANGES,
    GAME_RENAME_GAME,
    GAME_DELETE_GAME,
};

// global menu variables
static menu_list_t *menu;
static menu_list_t *selected_item;

static int actionStatus;
static int itemConfigId;
static config_set_t *itemConfig;
static menu_item_t *configRequestOwner;
static int configRequestId;
static int configRequestPending;
static int configForceLoad;

static u8 parentalLockCheckEnabled = 1;

// "main menu submenu"
static submenu_list_t *mainMenu;
// active item in the main menu
static submenu_list_t *mainMenuCurrent;

// "game settings submenu"
static submenu_list_t *gameMenu;
// active item in game settings
static submenu_list_t *gameMenuCurrent;

static submenu_list_t *appMenu;
static submenu_list_t *appMenuCurrent;

static s32 menuSemaId;
static s32 menuListSemaId = -1;
static ee_sema_t menuSema;

static maizo_library_t dashboard;
static unsigned int libraryRevision = 1, builtRevision;
static unsigned int librarySignature;
static int dashboardError;
static int settingsCategory;
static int settingsFocus;
static unsigned int focusStartFrame;

static void dashboardStartAnimation(void)
{
    focusStartFrame = guiFrameId;
}
static int carouselDirection;
static unsigned int dashboardLastNavFrame;

void menuDashboardInvalidate(void)
{
    libraryRevision++;
}

float menuDashboardFocus(void)
{
    unsigned int elapsed = guiFrameId - focusStartFrame;
    unsigned int duration = maizoAnimationMode() == 1 ? 4 : 5;
    float t;
    if (!maizoAnimations() || elapsed >= duration)
        return 1.0f;
    t = (float)elapsed / duration;
    return t * t * (3.0f - 2.0f * t);
}
float menuDashboardSlide(void)
{
    return carouselDirection * (1.0f - menuDashboardFocus());
}

int menuDashboardTab(void) { return dashboard.tab; }
int menuDashboardCount(void) { return dashboard.view_count; }
int menuDashboardPosition(void) { return dashboard.view_count ? dashboard.position + 1 : 0; }
maizo_entry_t *menuDashboardSlot(int offset) { return maizoLibrarySlot(&dashboard, offset); }

static int dashboardPlatform(menu_list_t *source)
{
    item_list_t *support = source->item->userdata;
    return support->mode == PS1_MODE ? MAIZO_PS1 : support->mode == APP_MODE ? MAIZO_APP :
                                                                               MAIZO_PS2;
}

static void dashboardSelect(void)
{
    maizo_entry_t *entry = menuDashboardSlot(0);
    if (entry) {
        menu_list_t *source = entry->owner;
        if (selected_item != source || source->item->current != entry->item) {
            WaitSema(menuSemaId);
            selected_item = source;
            source->item->current = entry->item;
            source->item->pagestart = entry->item;
            itemConfigId = -1;
            SignalSema(menuSemaId);
            dashboardStartAnimation();
        }
    }
}

/* Invoked while the GUI lock is held. Entries own their display strings;
 * source/item pointers are used only after the source revision is checked. */
static void dashboardRefresh(void)
{
    menu_list_t *source;
    unsigned int signature = 0;
    int tab = dashboard.tab;
    for (source = menu; source; source = source->next) {
        item_list_t *support = source->item->userdata;
        signature = signature * 33 + source->item->visible + (support->enabled << 1);
    }
    if (builtRevision == libraryRevision && signature == librarySignature)
        return;
    /* Save selection by stable identity before discarding any source references. */
    maizo_entry_t *current = menuDashboardSlot(0);
    if (current && tab < MAIZO_SETTINGS) {
        strcpy(dashboard.remembered[tab], current->identity);
        dashboard.remembered_platform[tab] = current->platform;
    }
    maizoLibraryClear(&dashboard);
    dashboardError = 0;
    for (source = menu; source; source = source->next) {
        item_list_t *support = source->item->userdata;
        submenu_list_t *item;
        if (!source->item->visible || !support->enabled)
            continue;
        for (item = source->item->submenu; item; item = item->next) {
            const char *identity = support->itemGetStartup(support, item->item.id);
            char appPath[256];
            if (support->mode == APP_MODE && appGetItemPath(item->item.id, appPath, sizeof(appPath)))
                identity = appPath;
            const char *saved = NULL;
            char key[32];
            int platform = dashboardPlatform(source);
            if (!identity)
                continue;
            maizoFavoriteKey(platform, identity, key);
            int favorite = configGetStr(configGetByType(CONFIG_OPL), key, &saved) && strcmp(saved, identity) == 0;
            if (!maizoLibraryAdd(&dashboard, source, item, platform, identity,
                                 submenuItemGetText(&item->item), favorite)) {
                dashboardError = 1;
                break;
            }
            if (!builtRevision && source == selected_item && item == source->item->current) {
                strcpy(dashboard.remembered[MAIZO_ALL], identity);
                dashboard.remembered_platform[MAIZO_ALL] = platform;
            }
        }
        if (dashboardError)
            break;
    }
    {
        int sortMode = 0;
        configGetInt(configGetByType(CONFIG_OPL), "maizo_sort_mode", &sortMode);
        maizoLibrarySortMode(&dashboard, sortMode >= 0 && sortMode <= 2 ? sortMode : 0);
    }
    if (!maizoLibraryFilter(&dashboard, tab))
        dashboardError = 1;
    builtRevision = libraryRevision;
    librarySignature = signature;
    dashboardSelect();
}

static void menuInitMainMenu(void);

static void dashboardChangeTab(int direction)
{
    /* SETTINGS is a screen opened by START, not a sixth dashboard tab. */
    int tab = (dashboard.tab + direction + MAIZO_SETTINGS) % MAIZO_SETTINGS;
    carouselDirection = 0;
    if (!maizoLibraryFilter(&dashboard, tab)) {
        dashboardError = 1;
        return;
    }
    dashboardSelect();
    if (tab == MAIZO_SETTINGS)
        menuInitMainMenu();
    dashboardStartAnimation();
    sfxPlay(SFX_CURSOR);
}

static volatile int favoriteAction;
static int favoriteResult;
static void dashboardWriteFavorite(void)
{
    favoriteResult = configWrite(configGetByType(CONFIG_OPL));
    favoriteAction = 0;
}

static void dashboardToggleFavorite(void)
{
    char identity[256], key[32];
    int previous;
    config_set_t *config = configGetByType(CONFIG_OPL);
    guiLock();
    dashboardRefresh();
    maizo_entry_t *entry = menuDashboardSlot(0);
    if (!entry) {
        guiUnlock();
        return;
    }
    strcpy(identity, entry->identity);
    maizoFavoriteKey(entry->platform, identity, key);
    previous = entry->favorite;
    guiUnlock();
    if (previous)
        configRemoveKey(config, key);
    else if (!configSetStr(config, key, identity))
        return;
    favoriteAction = 1;
    guiHandleDeferedIO((int *)&favoriteAction, _l(_STR_SAVING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &dashboardWriteFavorite);
    if (!favoriteResult) {
        /* Keep the screen consistent with the last saved favorite state. */
        if (previous)
            configSetStr(config, key, identity);
        else
            configRemoveKey(config, key);
        guiMsgBox(_l(_STR_ERROR_SAVING_SETTINGS), 0, NULL);
    }
    guiLock();
    menuDashboardInvalidate();
    dashboardRefresh();
    guiUnlock();
}

static void menuRenameGame(submenu_list_t **submenu)
{
    if (!selected_item->item->current) {
        return;
    }

    if (!gEnableWrite)
        return;

    item_list_t *support = selected_item->item->userdata;

    if (support) {
        if (support->itemRename) {
            if (menuCheckParentalLock() == 0) {
                sfxPlay(SFX_MESSAGE);
                int nameLength = support->itemGetNameLength(support, selected_item->item->current->item.id);
                char newName[nameLength];
                strncpy(newName, selected_item->item->current->item.text, nameLength);
                if (guiShowKeyboard(newName, nameLength)) {
                    guiSwitchScreen(GUI_SCREEN_MAIN);
                    submenuDestroy(submenu);

                    // Only rename the file if the name changed; trying to rename a file with a file name that hasn't changed can cause the file
                    // to be deleted on certain file systems.
                    if (strcmp(newName, selected_item->item->current->item.text) != 0) {
                        support->itemRename(support, selected_item->item->current->item.id, newName);
                        ioPutRequest(IO_MENU_UPDATE_DEFFERED, &support->mode);
                    }
                }
            }
        }
    } else
        guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void menuDeleteGame(submenu_list_t **submenu)
{
    if (!selected_item->item->current)
        return;

    if (!gEnableWrite)
        return;

    item_list_t *support = selected_item->item->userdata;

    if (support) {
        if (support->itemDelete) {
            if (menuCheckParentalLock() == 0) {
                if (guiMsgBox(_l(_STR_DELETE_WARNING), 1, NULL)) {
                    guiSwitchScreen(GUI_SCREEN_MAIN);
                    submenuDestroy(submenu);
                    support->itemDelete(support, selected_item->item->current->item.id);
                    ioPutRequest(IO_MENU_UPDATE_DEFFERED, &support->mode);
                }
            }
        }
    } else
        guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void _menuLoadConfig()
{
    WaitSema(menuSemaId);
    if (selected_item && selected_item->item == configRequestOwner &&
        configRequestOwner->current && configRequestOwner->current->item.id == configRequestId &&
        itemConfigId == configRequestId) {
        item_list_t *list = configRequestOwner->userdata;
        if (!itemConfig)
            itemConfig = list->itemGetConfig(list, configRequestId);
        actionStatus = 0;
    } else {
        /* Selection changed across sources while the read was queued. */
        itemConfigId = -1;
        actionStatus = 0;
    }
    configRequestPending = 0;
    SignalSema(menuSemaId);
}

static void _menuSaveConfig()
{
    int result;

    WaitSema(menuSemaId);
    result = configWrite(itemConfig);
    itemConfigId = -1; // to invalidate cache and force reload
    actionStatus = 0;
    SignalSema(menuSemaId);

    if (!result)
        setErrorMessage(_STR_ERROR_SAVING_SETTINGS);
}

static void _menuRequestConfig()
{
    WaitSema(menuSemaId);
    if (!selected_item || !selected_item->item->current) {
        actionStatus = 0;
        SignalSema(menuSemaId);
        return;
    }
    if (itemConfigId != selected_item->item->current->item.id || configRequestOwner != selected_item->item) {
        if (itemConfig) {
            configFree(itemConfig);
            itemConfig = NULL;
        }
        item_list_t *list = selected_item->item->userdata;
        if (!configRequestPending && (configForceLoad || guiInactiveFrames >= list->delay)) {
            configRequestOwner = selected_item->item;
            configRequestId = selected_item->item->current->item.id;
            itemConfigId = configRequestId;
            configRequestPending = 1;
            ioPutRequest(IO_CUSTOM_SIMPLEACTION, &_menuLoadConfig);
        }
    } else if (!configRequestPending)
        actionStatus = 0;
    SignalSema(menuSemaId);
}

config_set_t *menuLoadConfig()
{
    menu_item_t *owner = selected_item->item;
    actionStatus = 1;
    itemConfigId = -1;
    configForceLoad = 1;
    guiHandleDeferedIO(&actionStatus, _l(_STR_LOADING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &_menuRequestConfig);
    configForceLoad = 0;
    return configRequestOwner == owner ? itemConfig : NULL;
}

// we don't want a pop up when transitioning to or refreshing Game Menu gui.
config_set_t *gameMenuLoadConfig(struct UIItem *ui)
{
    actionStatus = 1;
    itemConfigId = -1;
    configForceLoad = 1;
    guiGameHandleDeferedIO(&actionStatus, ui, IO_CUSTOM_SIMPLEACTION, &_menuRequestConfig);
    configForceLoad = 0;
    return itemConfig;
}

void menuSaveConfig()
{
    actionStatus = 1;
    guiHandleDeferedIO(&actionStatus, _l(_STR_SAVING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &_menuSaveConfig);
}

static const char *settingsNames[] = {"INTERFACE", "CONTROLES", "JOGOS", "VIDEO", "AUDIO", "ARMAZENAMENTO", "REDE", "SISTEMA", "DIAGNOSTICO"};
static const int settingsIcons[] = {
    MAIZO_ICON_SETTINGS, MAIZO_ICON_PAD, MAIZO_ICON_PS2,
    MAIZO_ICON_VIDEO, MAIZO_ICON_AUDIO, MAIZO_ICON_HDD,
    MAIZO_ICON_NETWORK, MAIZO_ICON_INFO, MAIZO_ICON_USB};
#define MAIZO_SETTING_CATEGORIES 9

static int settingsItemCategory(int id)
{
    switch (id) {
        case MENU_GFX_SETTINGS:
        case MENU_ANIMATIONS:
        case MENU_COVER_MODE:
            return 0;
        case MENU_LIBRARY_SORT:
        case MENU_LIBRARY_REFRESH:
            return 2;
        case MENU_CONTROLLER_SETTINGS:
        case MENU_PADEMU:
        case MENU_BLUETOOTH:
            return 1;
        case MENU_OSD_LANGUAGE_SETTINGS:
        case MENU_PS1_SETTINGS:
        case MENU_ITEM_INFO:
            return 2;
        case MENU_VIDEO_SETTINGS:
            return 3;
        case MENU_AUDIO_SETTINGS:
            return 4;
        case MENU_STORAGE_SETTINGS:
            return 5;
        case MENU_NET_CONFIG:
        case MENU_NET_UPDATE:
        case MENU_START_NBD:
            return 6;
        case MENU_USB_DIAG:
        case MENU_DIAG_SYSTEM:
            return 8;
        default:
            return 7;
    }
}

static void menuInitMainMenu(void)
{
    if (mainMenu)
        submenuDestroy(&mainMenu);

    // initialize the menu
    submenuAppendItem(&mainMenu, -1, NULL, MENU_SETTINGS, _STR_SETTINGS);
    if (thmGetGuiValue() == 0) {
        submenuAppendItem(&mainMenu, -1, "Organizar biblioteca", MENU_LIBRARY_SORT, -1);
        submenuAppendItem(&mainMenu, -1, "Atualizar biblioteca", MENU_LIBRARY_REFRESH, -1);
        submenuAppendItem(&mainMenu, -1, "Capas dos jogos", MENU_COVER_MODE, -1);
        submenuAppendItem(&mainMenu, -1, "Informacoes tecnicas", MENU_DIAG_SYSTEM, -1);
        submenuAppendItem(&mainMenu, -1, "Video da interface", MENU_VIDEO_SETTINGS, -1);
        submenuAppendItem(&mainMenu, -1, "Armazenamento e fontes", MENU_STORAGE_SETTINGS, -1);
        submenuAppendItem(&mainMenu, -1, "Configuracoes PlayStation 1", MENU_PS1_SETTINGS, -1);
    }
    submenuAppendItem(&mainMenu, -1, NULL, MENU_GFX_SETTINGS, _STR_GFX_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_AUDIO_SETTINGS, _STR_AUDIO_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_CONTROLLER_SETTINGS, _STR_CONTROLLER_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_OSD_LANGUAGE_SETTINGS, _STR_OSD_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_PARENTAL_LOCK, _STR_PARENLOCKCONFIG);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_NET_CONFIG, _STR_NETCONFIG);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_NET_UPDATE, _STR_NET_UPDATE);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_START_NBD, _STR_STARTNBD);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_ABOUT, _STR_ABOUT);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_SAVE_CHANGES, _STR_SAVE_CHANGES);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_EXIT, _STR_EXIT);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_POWER_OFF, _STR_POWEROFF);

    if (thmGetGuiValue() == 0) {
        submenuAppendItem(&mainMenu, -1, "Animacoes OLED", MENU_ANIMATIONS, -1);
        if (selected_item && selected_item->item->current)
            submenuAppendItem(&mainMenu, -1, "Informacoes do item", MENU_ITEM_INFO, -1);
#ifdef PADEMU
        submenuAppendItem(&mainMenu, -1, "PadEmu / macros / USB HID", MENU_PADEMU, -1);
        submenuAppendItem(&mainMenu, -1, "Gerenciar controles Bluetooth", MENU_BLUETOOTH, -1);
        submenuAppendItem(&mainMenu, -1, "Diagnostico USB HID universal", MENU_USB_DIAG, -1);
#endif
        submenu_list_t *item = mainMenu;
        while (item) {
            submenu_list_t *next = item->next;
            int category = settingsItemCategory(item->item.id);
            int keep = category == settingsCategory;
            if (!keep)
                submenuRemoveItem(&mainMenu, item->item.id);
            item = next;
        }
        /* Existing remove routine now maintains previous links for focus traversal. */
    }
    mainMenuCurrent = mainMenu;
}

void menuReinitMainMenu(void)
{
    menuInitMainMenu();
}

void menuInitGameMenu(void)
{
    if (gameMenu)
        submenuDestroy(&gameMenu);

    // initialize the menu
    submenuAppendItem(&gameMenu, -1, NULL, GAME_COMPAT_SETTINGS, _STR_COMPAT_SETTINGS);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_CHEAT_SETTINGS, _STR_CHEAT_SETTINGS);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_GSM_SETTINGS, _STR_GSCONFIG);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_VMC_SETTINGS, _STR_VMC_SCREEN);
#ifdef PADEMU
    submenuAppendItem(&gameMenu, -1, NULL, GAME_PADEMU_SETTINGS, _STR_PADEMUCONFIG);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_PADMACRO_SETTINGS, _STR_PADMACROCONFIG);
#endif
    submenuAppendItem(&gameMenu, -1, NULL, GAME_OSD_LANGUAGE_SETTINGS, _STR_OSD_SETTINGS);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_SAVE_CHANGES, _STR_SAVE_CHANGES);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_TEST_CHANGES, _STR_TEST);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_REMOVE_CHANGES, _STR_REMOVE_ALL_SETTINGS);
    if (gEnableWrite) {
        submenuAppendItem(&gameMenu, -1, NULL, GAME_RENAME_GAME, _STR_RENAME);
        submenuAppendItem(&gameMenu, -1, NULL, GAME_DELETE_GAME, _STR_DELETE);
    }

    gameMenuCurrent = gameMenu;
}

void menuInitAppMenu(void)
{
    if (appMenu)
        submenuDestroy(&appMenu);

    // initialize the menu
    submenuAppendItem(&appMenu, -1, NULL, 0, _STR_RENAME);
    submenuAppendItem(&appMenu, -1, NULL, 1, _STR_DELETE);

    appMenuCurrent = appMenu;
}

// -------------------------------------------------------------------------------------------
// ---------------------------------------- Menu manipulation --------------------------------
// -------------------------------------------------------------------------------------------
void menuInit()
{
    menu = NULL;
    selected_item = NULL;
    itemConfigId = -1;
    itemConfig = NULL;
    mainMenu = NULL;
    mainMenuCurrent = NULL;
    gameMenu = NULL;
    gameMenuCurrent = NULL;
    appMenu = NULL;
    appMenuCurrent = NULL;
    menuInitMainMenu();

    menuSema.init_count = 1;
    menuSema.max_count = 1;
    menuSema.option = 0;
    menuSemaId = CreateSema(&menuSema);
    if (menuListSemaId < 0) {
        menuListSemaId = sbCreateSemaphore();
    }
}

void menuEnd()
{
    maizoLibraryClear(&dashboard);
    // destroy menu
    menu_list_t *cur = menu;

    while (cur) {
        menu_list_t *td = cur;
        cur = cur->next;

        if (td->item)
            submenuDestroy(&(td->item->submenu));

        menuRemoveHints(td->item);

        free(td);
    }

    submenuDestroy(&mainMenu);
    submenuDestroy(&gameMenu);
    submenuDestroy(&appMenu);

    if (itemConfig) {
        configFree(itemConfig);
        itemConfig = NULL;
    }

    DeleteSema(menuSemaId);
    DeleteSema(menuListSemaId);
    menuListSemaId = -1;
}

static menu_list_t *AllocMenuItem(menu_item_t *item)
{
    menu_list_t *it;

    it = malloc(sizeof(menu_list_t));

    it->prev = NULL;
    it->next = NULL;
    it->item = item;

    return it;
}

void menuAppendItem(menu_item_t *item)
{
    assert(item);

    WaitSema(menuListSemaId);

    if (menu == NULL) {
        menu = AllocMenuItem(item);
        selected_item = menu;
    } else {
        menu_list_t *cur = menu;

        // traverse till the end
        while (cur->next)
            cur = cur->next;

        // create new item
        menu_list_t *newitem = AllocMenuItem(item);

        // link
        cur->next = newitem;
        newitem->prev = cur;
    }

    SignalSema(menuListSemaId);
}

static void refreshMenuPosition(void)
{
    // Find the first menu in the list that is visible and set it as the active menu.
    if (menu == NULL)
        return;

    menu_list_t *cur = menu;
    while (cur->item->visible == 0 && cur->next)
        cur = cur->next;

    if (cur->item->visible == 0) {
        // No visible menu was found, just set the current menu to the first one in the list.
        selected_item = menu;
    } else
        selected_item = cur;
}

void submenuRebuildCache(submenu_list_t *submenu)
{
    while (submenu) {
        if (submenu->item.cache_id)
            free(submenu->item.cache_id);
        if (submenu->item.cache_uid)
            free(submenu->item.cache_uid);

        int size = gTheme->gameCacheCount * sizeof(int);
        submenu->item.cache_id = malloc(size);
        memset(submenu->item.cache_id, -1, size);
        submenu->item.cache_uid = malloc(size);
        memset(submenu->item.cache_uid, -1, size);

        submenu = submenu->next;
    }
}

static submenu_list_t *submenuAllocItem(int icon_id, char *text, int id, int text_id)
{
    submenu_list_t *it = (submenu_list_t *)malloc(sizeof(submenu_list_t));

    it->prev = NULL;
    it->next = NULL;
    it->item.icon_id = icon_id;
    it->item.text = text;
    it->item.text_id = text_id;
    it->item.id = id;
    it->item.cache_id = NULL;
    it->item.cache_uid = NULL;
    submenuRebuildCache(it);

    return it;
}

submenu_list_t *submenuAppendItem(submenu_list_t **submenu, int icon_id, char *text, int id, int text_id)
{
    menuDashboardInvalidate();
    if (*submenu == NULL) {
        *submenu = submenuAllocItem(icon_id, text, id, text_id);
        return *submenu;
    }

    submenu_list_t *cur = *submenu;

    // traverse till the end
    while (cur->next)
        cur = cur->next;

    // create new item
    submenu_list_t *newitem = submenuAllocItem(icon_id, text, id, text_id);

    // link
    cur->next = newitem;
    newitem->prev = cur;

    return newitem;
}

static void submenuDestroyItem(submenu_list_t *submenu)
{
    free(submenu->item.cache_id);
    free(submenu->item.cache_uid);

    free(submenu);
}

void submenuRemoveItem(submenu_list_t **submenu, int id)
{
    menuDashboardInvalidate();
    submenu_list_t *cur = *submenu;
    submenu_list_t *prev = NULL;

    while (cur) {
        if (cur->item.id == id) {
            submenu_list_t *next = cur->next;

            if (prev)
                prev->next = cur->next;

            if (*submenu == cur)
                *submenu = next;
            if (next)
                next->prev = prev;

            submenuDestroyItem(cur);

            cur = next;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
}

void submenuDestroy(submenu_list_t **submenu)
{
    menuDashboardInvalidate();
    // destroy sub menu
    submenu_list_t *cur = *submenu;

    while (cur) {
        submenu_list_t *td = cur;
        cur = cur->next;

        submenuDestroyItem(td);
    }

    *submenu = NULL;
}

void menuAddHint(menu_item_t *menu, int text_id, int icon_id)
{
    // allocate a new hint item
    menu_hint_item_t *hint = malloc(sizeof(menu_hint_item_t));

    hint->text_id = text_id;
    hint->icon_id = icon_id;
    hint->next = NULL;

    if (menu->hints) {
        menu_hint_item_t *top = menu->hints;

        // rewind to end
        for (; top->next; top = top->next)
            ;

        top->next = hint;
    } else {
        menu->hints = hint;
    }
}

void menuRemoveHints(menu_item_t *menu)
{
    while (menu->hints) {
        menu_hint_item_t *hint = menu->hints;
        menu->hints = hint->next;
        free(hint);
    }
}

char *menuItemGetText(menu_item_t *it)
{
    if (it->text_id >= 0)
        return _l(it->text_id);
    else
        return it->text;
}

char *submenuItemGetText(submenu_item_t *it)
{
    if (it->text_id >= 0)
        return _l(it->text_id);
    else
        return it->text;
}

static void swap(submenu_list_t *a, submenu_list_t *b)
{
    submenu_list_t *pa, *nb;
    pa = a->prev;
    nb = b->next;

    a->next = nb;
    b->prev = pa;
    b->next = a;
    a->prev = b;

    if (pa)
        pa->next = b;

    if (nb)
        nb->prev = a;
}

// Sorts the given submenu by comparing the on-screen titles
void submenuSort(submenu_list_t **submenu)
{
    menuDashboardInvalidate();
    // a simple bubblesort
    // *submenu = mergeSort(*submenu);
    submenu_list_t *head;
    int sorted = 0;

    if ((submenu == NULL) || (*submenu == NULL) || ((*submenu)->next == NULL))
        return;

    head = *submenu;

    while (!sorted) {
        sorted = 1;

        submenu_list_t *tip = head;

        while (tip->next) {
            submenu_list_t *nxt = tip->next;

            char *txt1 = submenuItemGetText(&tip->item);
            char *txt2 = submenuItemGetText(&nxt->item);

            int cmp = strcasecmp(txt1, txt2);

            if (cmp > 0) {
                swap(tip, nxt);

                if (tip == head)
                    head = nxt;

                sorted = 0;
            } else {
                tip = tip->next;
            }
        }
    }

    *submenu = head;
}

static void menuNextH()
{
    struct menu_list *next = selected_item->next;
    while (next != NULL && next->item->visible == 0)
        next = next->next;

    // If we found a valid menu transition to it.
    if (next != NULL) {
        selected_item = next;
        itemConfigId = -1;
        sfxPlay(SFX_CURSOR);
    }
}

static void menuPrevH()
{
    struct menu_list *prev = selected_item->prev;
    while (prev != NULL && prev->item->visible == 0)
        prev = prev->prev;

    if (prev != NULL) {
        selected_item = prev;
        itemConfigId = -1;
        sfxPlay(SFX_CURSOR);
    }
}

static void menuFirstPage()
{
    submenu_list_t *cur = selected_item->item->current;
    if (cur) {
        if (cur->prev) {
            sfxPlay(SFX_CURSOR);
        }

        selected_item->item->current = selected_item->item->submenu;
        selected_item->item->pagestart = selected_item->item->current;
    }
}

static void menuLastPage()
{
    submenu_list_t *cur = selected_item->item->current;
    if (cur) {
        if (cur->next) {
            sfxPlay(SFX_CURSOR);
        }
        while (cur->next)
            cur = cur->next; // go to end

        selected_item->item->current = cur;

        int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems;
        while (--itms && cur->prev) // and move back to have a full page
            cur = cur->prev;

        selected_item->item->pagestart = cur;
    }
}

static void menuNextV()
{
    submenu_list_t *cur = selected_item->item->current;

    if (cur && cur->next) {
        selected_item->item->current = cur->next;
        sfxPlay(SFX_CURSOR);

        // if the current item is beyond the page start, move the page start one page down
        cur = selected_item->item->pagestart;
        int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems + 1;
        while (--itms && cur)
            if (selected_item->item->current == cur)
                return;
            else
                cur = cur->next;

        selected_item->item->pagestart = selected_item->item->current;
    } else { // wrap to start
        menuFirstPage();
    }
}

static void menuPrevV()
{
    submenu_list_t *cur = selected_item->item->current;

    if (cur && cur->prev) {
        selected_item->item->current = cur->prev;
        sfxPlay(SFX_CURSOR);

        // if the current item is on the page start, move the page start one page up
        if (selected_item->item->pagestart == cur) {
            int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems + 1; // +1 because the selection will move as well
            while (--itms && selected_item->item->pagestart->prev)
                selected_item->item->pagestart = selected_item->item->pagestart->prev;
        }
    } else { // wrap to end
        menuLastPage();
    }
}

static void menuNextPage()
{
    submenu_list_t *cur = selected_item->item->pagestart;

    if (cur && cur->next) {
        int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems + 1;
        sfxPlay(SFX_CURSOR);

        while (--itms && cur->next)
            cur = cur->next;

        selected_item->item->current = cur;
        selected_item->item->pagestart = selected_item->item->current;
    } else { // wrap to start
        menuFirstPage();
    }
}

static void menuPrevPage()
{
    submenu_list_t *cur = selected_item->item->pagestart;

    if (cur && cur->prev) {
        int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems + 1;
        sfxPlay(SFX_CURSOR);

        while (--itms && cur->prev)
            cur = cur->prev;

        selected_item->item->current = cur;
        selected_item->item->pagestart = selected_item->item->current;
    } else { // wrap to end
        menuLastPage();
    }
}

void menuSetSelectedItem(menu_item_t *item)
{
    menu_list_t *itm = menu;

    while (itm) {
        if (itm->item == item) {
            selected_item = itm;
            return;
        }

        itm = itm->next;
    }
}

/* Modern action footer shared by START, game and application settings.
 * The original menu callbacks stay untouched; only presentation changes. */
static void maizoSettingsFooter(const char *context)
{
    int h = gTheme->usedHeight;
    rmDrawRect(0, h - 62, 640, 62, MAIZO_SURFACE);
    rmDrawRect(0, h - 62, 640, 1, MAIZO_DIVIDER);
    fntRenderString(gTheme->fonts[0], 28, h - 44, ALIGN_NONE, 450, 25,
                    gSelectButton == KEY_CIRCLE ?
                        "O ABRIR     X VOLTAR     L1/R1 CATEGORIA" :
                        "X ABRIR     O VOLTAR     L1/R1 CATEGORIA",
                    MAIZO_TEXT);
    if (context)
        fntRenderString(gTheme->fonts[0], 612, h - 27,
                        ALIGN_RIGHT, 220, 23, context, MAIZO_MUTED);
}

static int maizoSettingsActionIcon(int id)
{
    switch (id) {
        case MENU_PADEMU:
        case MENU_BLUETOOTH:
        case MENU_CONTROLLER_SETTINGS:
            return MAIZO_ICON_PAD;
        case MENU_USB_DIAG:
        case MENU_STORAGE_SETTINGS:
            return MAIZO_ICON_USB;
        case MENU_NET_CONFIG:
        case MENU_NET_UPDATE:
        case MENU_START_NBD:
            return MAIZO_ICON_NETWORK;
        case MENU_GFX_SETTINGS:
        case MENU_ANIMATIONS:
        case MENU_COVER_MODE:
            return MAIZO_ICON_VIDEO;
        case MENU_AUDIO_SETTINGS:
            return MAIZO_ICON_AUDIO;
        case MENU_LIBRARY_REFRESH:
            return MAIZO_ICON_REFRESH;
        case MENU_LIBRARY_SORT:
            return MAIZO_ICON_SORT;
        case MENU_SAVE_CHANGES:
            return MAIZO_ICON_CHECK;
        case MENU_EXIT:
        case MENU_POWER_OFF:
            return MAIZO_ICON_POWER;
        case MENU_DIAG_SYSTEM:
        case MENU_ITEM_INFO:
        case MENU_ABOUT:
            return MAIZO_ICON_INFO;
        default:
            return MAIZO_ICON_SETTINGS;
    }
}

static const char *const maizoSettingsDescriptions[] = {
    "TEMAS, CAPAS, CORES E TRANSICOES",
    "CONTROLES, BLUETOOTH, HID E PADEMU",
    "BIBLIOTECA, PS1, IDIOMA E JOGOS",
    "SAIDA DE VIDEO E PROPORCOES",
    "EFEITOS E REPRODUCAO DE AUDIO",
    "DISPOSITIVOS, USB E ARMAZENAMENTO",
    "ETHERNET, SMB E ATUALIZACOES",
    "SEGURANCA, SALVAMENTO E SISTEMA",
    "RELATORIOS TECNICOS E PERIFERICOS"};

void menuRenderMenu()
{
    guiDrawBGPlasma();
    if (thmGetGuiValue() == 0) {
        const int leftX = 24, rightX = 231, rightWidth = 387;
        int y, active = 0, first, pos = 0;
        submenu_list_t *it;

        rmDrawRect(0, 0, 640, gTheme->usedHeight, MAIZO_BLACK);
        maizoDrawHeader(MAIZO_SETTINGS);
        rmDrawRect(0, 66, 640, 1, MAIZO_DIVIDER);
        fntRenderString(gTheme->fonts[0], 30, 77, ALIGN_NONE,
                        345, 30, "CONFIGURACOES", MAIZO_TEXT);

        /* Fixed nine-category navigation rail; input focus and content
         * selection are independent. */
        maizoDrawPanel(leftX, 120, 194, 280, 0);
        for (int i = 0; i < MAIZO_SETTING_CATEGORIES; i++) {
            int rowY = 130 + i * 29;
            int highlighted = i == settingsCategory;
            if (highlighted)
                rmDrawRect(leftX + 3, rowY - 2, 188, 29,
                           settingsFocus ? MAIZO_DIVIDER : MAIZO_CARD);
            if (highlighted)
                rmDrawRect(leftX + 3, rowY - 2, 3, 29, MAIZO_ACCENT);
            maizoDrawIcon(settingsIcons[i], leftX + 12, rowY + 1, 18,
                          highlighted ? MAIZO_BRIGHT : MAIZO_MUTED);
            fntRenderString(gTheme->fonts[0], leftX + 38, rowY + 1,
                            ALIGN_NONE, 152, 24, settingsNames[i],
                            highlighted ? MAIZO_TEXT : MAIZO_MUTED);
        }

        maizoDrawPanel(rightX, 120, rightWidth, 280, 0);
        fntRenderString(gTheme->fonts[0], rightX + 18, 127, ALIGN_NONE,
                        rightWidth - 36, 26, settingsNames[settingsCategory],
                        MAIZO_TEXT);
        fntRenderString(gTheme->fonts[0], rightX + 18, 151, ALIGN_NONE,
                        rightWidth - 36, 22,
                        maizoSettingsDescriptions[settingsCategory],
                        MAIZO_MUTED);
        maizoDrawSeparator(rightX + 18, 178, rightWidth - 36);

        for (it = mainMenu; it; it = it->next, pos++)
            if (it == mainMenuCurrent)
                active = pos;
        first = active >= 7 ? active - 6 : 0;

        pos = 0;
        y = 188;
        for (it = mainMenu; it; it = it->next, pos++) {
            const char *value = NULL;
            int focus;
            if (pos < first || pos >= first + 6)
                continue;
            focus = it == mainMenuCurrent && settingsFocus;
            if (it->item.id == MENU_ANIMATIONS) {
                static const char *const anim[] = {
                    "OFF", "SUAVE", "COMPLETA"};
                value = anim[maizoAnimationMode()];
            } else if (it->item.id == MENU_COVER_MODE) {
                int enabled = 1;
                configGetInt(configGetByType(CONFIG_OPL),
                             "maizo_cover_mode", &enabled);
                value = enabled ? "ON" : "OFF";
            } else if (it->item.id == MENU_LIBRARY_SORT) {
                static const char *const sortNames[] = {
                    "NOME", "PLATAFORMA", "ORIGEM"};
                int sort = 0;
                configGetInt(configGetByType(CONFIG_OPL),
                             "maizo_sort_mode", &sort);
                if (sort < 0 || sort > 2)
                    sort = 0;
                value = sortNames[sort];
            }
            rmDrawRect(rightX + 10, y, rightWidth - 20, 33,
                       focus ? MAIZO_CARD : MAIZO_SURFACE);
            if (focus)
                rmDrawRect(rightX + 10, y, 3, 33, MAIZO_ACCENT);
            maizoDrawIcon(maizoSettingsActionIcon(it->item.id),
                          rightX + 18, y + 7, 20,
                          focus ? MAIZO_BRIGHT : MAIZO_MUTED);
            fntRenderString(gTheme->fonts[0], rightX + 49, y + 7,
                            ALIGN_NONE, value ? 215 : rightWidth - 93, 24,
                            submenuItemGetText(&it->item),
                            focus ? MAIZO_TEXT : MAIZO_MUTED);
            if (value)
                fntRenderString(gTheme->fonts[0], rightX + rightWidth - 20,
                                y + 7, ALIGN_RIGHT, 115, 25, value,
                                focus ? MAIZO_BRIGHT : MAIZO_MUTED);
            else
                maizoDrawIcon(MAIZO_ICON_CONFIRM, rightX + rightWidth - 40,
                              y + 8, 18, focus ? MAIZO_BRIGHT : MAIZO_DISABLED);
            y += 35;
        }
        if (!mainMenu) {
            fntRenderString(gTheme->fonts[0], rightX + 23, 220,
                            ALIGN_NONE, 330, 48,
                            "Sem opcoes nesta categoria", MAIZO_MUTED);
        }
#ifndef PADEMU
        if (settingsCategory == 1)
            fntRenderString(gTheme->fonts[0], rightX + 18, 363,
                            ALIGN_NONE, 335, 25,
                            "PadEmu nao incluido nesta compilacao.",
                            MAIZO_WARNING);
#endif
        maizoSettingsFooter(settingsFocus ?
                                "OPCOES" :
                                "SELECIONE A CATEGORIA");
        return;
    }

    if (!mainMenu)
        return;

    // draw the animated menu
    if (!mainMenuCurrent)
        mainMenuCurrent = mainMenu;

    submenu_list_t *it = mainMenu;

    // calculate the number of items
    int count = 0;
    int sitem = 0;
    for (; it; count++, it = it->next) {
        if (it == mainMenuCurrent)
            sitem = count;
    }

    int spacing = 25;
    int y = (gTheme->usedHeight >> 1) - (spacing * (count >> 1));
    int cp = 0; // current position
    for (it = mainMenu; it; it = it->next, cp++) {
        // render, advance
        fntRenderString(gTheme->fonts[0], 320, y, ALIGN_CENTER, 0, 0, submenuItemGetText(&it->item), (cp == sitem) ? gTheme->selTextColor : gTheme->textColor);
        y += spacing;
        if (cp == (MENU_ABOUT - 1))
            y += spacing / 2;
    }

    // hints
    guiDrawSubMenuHints();
}

int menuSetParentalLockCheckState(int enabled)
{
    int wasEnabled;

    wasEnabled = parentalLockCheckEnabled;
    parentalLockCheckEnabled = enabled ? 1 : 0;

    return wasEnabled;
}

// Returns the configured parental lock password, or NULL if the parental lock is not set up.
static const char *menuGetParentalLockPassword(void)
{
    const char *parentalLockPassword;
    config_set_t *configOPL = configGetByType(CONFIG_OPL);

    if (configGetStr(configOPL, CONFIG_OPL_PARENTAL_LOCK_PWD, &parentalLockPassword) && (parentalLockPassword[0] != '\0'))
        return parentalLockPassword;

    return NULL;
}

int menuIsParentalLockPasswordSet(void)
{
    return menuGetParentalLockPassword() != NULL;
}

// Prompts for the parental lock password. Returns 0 if access was granted, EACCES otherwise.
static int menuPromptParentalLockPassword(const char *parentalLockPassword)
{
    char password[CONFIG_KEY_VALUE_LEN];
    int result;

    password[0] = '\0';
    if (diaShowKeyb(password, CONFIG_KEY_VALUE_LEN, 1, _l(_STR_PARENLOCK_ENTER_PASSWORD_TITLE))) {
        if (strncmp(parentalLockPassword, password, CONFIG_KEY_VALUE_LEN) == 0) {
            result = 0;
        } else if (strncmp(OPL_PARENTAL_LOCK_MASTER_PASS, password, CONFIG_KEY_VALUE_LEN) == 0) {
            guiMsgBox(_l(_STR_PARENLOCK_DISABLE_WARNING), 0, NULL);

            configRemoveKey(configGetByType(CONFIG_OPL), CONFIG_OPL_PARENTAL_LOCK_PWD);
            saveConfig(CONFIG_OPL, 1);

            result = 0;
        } else {
            guiMsgBox(_l(_STR_PARENLOCK_PASSWORD_INCORRECT), 0, NULL);
            result = EACCES;
        }
    } else // User aborted.
        result = EACCES;

    return result;
}

int menuCheckParentalLock(void)
{
    const char *parentalLockPassword;
    int result;

    result = 0; // Default to unlocked.
    if (parentalLockCheckEnabled) {
        // Prompt for password, only if one was set.
        parentalLockPassword = menuGetParentalLockPassword();
        if (parentalLockPassword != NULL) {
            result = menuPromptParentalLockPassword(parentalLockPassword);
            if (result == 0)
                parentalLockCheckEnabled = 0; // Stop asking for the password.
        }
    }

    return result;
}

int menuCheckGameParentalLock(config_set_t *configSet)
{
    const char *parentalLockPassword;
    int locked = 0;

    if (configSet != NULL)
        configGetInt(configSet, CONFIG_ITEM_PARENTAL_LOCK, &locked);
    if (!locked)
        return 0;

    parentalLockPassword = menuGetParentalLockPassword();
    if (parentalLockPassword == NULL)
        return 0; // No password configured, so there is nothing to enforce.

    // A locked game always asks for the password, even if the settings menus were unlocked earlier in this session.
    return menuPromptParentalLockPassword(parentalLockPassword);
}

void menuHandleInputMenu()
{
    if (thmGetGuiValue() == 0) {
        if (getKeyOn(KEY_L1) || getKeyOn(KEY_R1)) {
            settingsCategory = (settingsCategory +
                                (getKeyOn(KEY_R1) ? 1 : MAIZO_SETTING_CATEGORIES - 1)) %
                               MAIZO_SETTING_CATEGORIES;
            settingsFocus = 0;
            menuInitMainMenu();
            return;
        }
        if (getKeyOn(KEY_LEFT)) {
            settingsFocus = 0;
            return;
        }
        if (getKeyOn(KEY_RIGHT)) {
            settingsFocus = 1;
            return;
        }
        if (!settingsFocus) {
            if (getKey(KEY_UP) || getKey(KEY_DOWN)) {
                settingsCategory = (settingsCategory +
                                    (getKey(KEY_UP) ? MAIZO_SETTING_CATEGORIES - 1 : 1)) %
                                   MAIZO_SETTING_CATEGORIES;
                menuInitMainMenu();
                sfxPlay(SFX_CURSOR);
            } else if (getKeyOn(gSelectButton)) {
                settingsFocus = 1;
            }
            if (!getKeyOn(KEY_START) && !getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE))
                return;
        } else if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
            settingsFocus = 0;
            return;
        }
        if (getKeyOn(KEY_START) || getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
            guiLock();
            maizoLibraryFilter(&dashboard, MAIZO_ALL);
            dashboardSelect();
            guiUnlock();
            guiSwitchScreen(GUI_SCREEN_MAIN);
            return;
        }
    }
    if (!mainMenu)
        return;

    if (!mainMenuCurrent)
        mainMenuCurrent = mainMenu;

    if (getKey(KEY_UP)) {
        sfxPlay(SFX_CURSOR);
        if (mainMenuCurrent->prev)
            mainMenuCurrent = mainMenuCurrent->prev;
        else // rewind to the last item
            while (mainMenuCurrent->next)
                mainMenuCurrent = mainMenuCurrent->next;
    }

    if (getKey(KEY_DOWN)) {
        sfxPlay(SFX_CURSOR);
        if (mainMenuCurrent->next)
            mainMenuCurrent = mainMenuCurrent->next;
        else
            mainMenuCurrent = mainMenu;
    }

    if (getKeyOn(gSelectButton)) {
        // execute the item via looking at the id of it
        int id = mainMenuCurrent->item.id;

        sfxPlay(SFX_CONFIRM);

        if (id == MENU_ITEM_INFO) {
            if (selected_item && selected_item->item->current)
                selected_item->item->execSquare(selected_item->item);
        } else if (id == MENU_LIBRARY_SORT) {
            int mode = 0;
            if (menuCheckParentalLock() == 0) {
                configGetInt(configGetByType(CONFIG_OPL), "maizo_sort_mode", &mode);
                if (mode < 0 || mode > 2)
                    mode = 0;
                configSetInt(configGetByType(CONFIG_OPL), "maizo_sort_mode", (mode + 1) % 3);
                saveConfig(CONFIG_OPL, 1);
                menuDashboardInvalidate();
            }
        } else if (id == MENU_LIBRARY_REFRESH) {
            for (menu_list_t *source = menu; source; source = source->next) {
                if (source->item->visible) {
                    item_list_t *support = source->item->userdata;
                    if (support->enabled)
                        source->item->refresh(source->item);
                }
            }
            menuDashboardInvalidate();
        } else if (id == MENU_COVER_MODE) {
            int show = 1;
            if (menuCheckParentalLock() == 0) {
                configGetInt(configGetByType(CONFIG_OPL), "maizo_cover_mode", &show);
                configSetInt(configGetByType(CONFIG_OPL), "maizo_cover_mode", !show);
                saveConfig(CONFIG_OPL, 1);
            }
        } else if (id == MENU_DIAG_SYSTEM) {
            guiShowSupremeDiagnostics();
        } else if (id == MENU_ANIMATIONS) {
            int mode = (maizoAnimationMode() + 1) % 3;
            configSetInt(configGetByType(CONFIG_OPL), "maizo_animation_mode", mode);
            configSetInt(configGetByType(CONFIG_OPL), "maizo_animations", mode != 0);
            saveConfig(CONFIG_OPL, 1);
#ifdef PADEMU
        } else if (id == MENU_PADEMU) {
            if (menuCheckParentalLock() == 0)
                guiGameShowPadEmuConfig(1);
        } else if (id == MENU_BLUETOOTH) {
            if (menuCheckParentalLock() == 0)
                guiGameShowBluetoothManager();
#endif
        } else if (id == MENU_SETTINGS) {
            if (menuCheckParentalLock() == 0) {
                guiShowConfig();
            }
        } else if (id == MENU_STORAGE_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiShowStorageConfig();
        } else if (id == MENU_PS1_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiShowPS1Config();
        } else if (id == MENU_USB_DIAG) {
#ifdef PADEMU
            if (menuCheckParentalLock() == 0)
                guiGameShowUsbDiagnostics();
#endif
        } else if (id == MENU_VIDEO_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiShowUIConfig();
        } else if (id == MENU_GFX_SETTINGS) {
            if (menuCheckParentalLock() == 0) {
                guiShowUIConfig();
                menuInitMainMenu();
            }
        } else if (id == MENU_AUDIO_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiShowAudioConfig();
        } else if (id == MENU_CONTROLLER_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiShowControllerConfig();
        } else if (id == MENU_OSD_LANGUAGE_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiGameShowOSDLanguageConfig(1);
        } else if (id == MENU_PARENTAL_LOCK) {
            if (menuCheckParentalLock() == 0)
                guiShowParentalLockConfig();
        } else if (id == MENU_NET_CONFIG) {
            if (menuCheckParentalLock() == 0)
                guiShowNetConfig();
        } else if (id == MENU_NET_UPDATE) {
            if (menuCheckParentalLock() == 0)
                guiShowNetCompatUpdate();
        } else if (id == MENU_START_NBD) {
            if (menuCheckParentalLock() == 0)
                handleLwnbdSrv();
        } else if (id == MENU_ABOUT) {
            guiShowAbout();
        } else if (id == MENU_SAVE_CHANGES) {
            if (menuCheckParentalLock() == 0) {
                guiGameSaveOSDLanguageGlobalConfig(configGetByType(CONFIG_GAME));
#ifdef PADEMU
                guiGameSavePadEmuGlobalConfig(configGetByType(CONFIG_GAME));
                guiGameSavePadMacroGlobalConfig(configGetByType(CONFIG_GAME));
#endif
                saveConfig(CONFIG_OPL | CONFIG_NETWORK | CONFIG_GAME, 1);
                menuSetParentalLockCheckState(1); // Re-enable parental lock check.
            }
        } else if (id == MENU_EXIT) {
            if (guiMsgBox(_l(_STR_CONFIRMATION_EXIT), 1, NULL))
                sysExecExit();
        } else if (id == MENU_POWER_OFF) {
            if (guiMsgBox(_l(_STR_CONFIRMATION_POFF), 1, NULL))
                sysPowerOff();
        }

        // so the exit press wont propagate twice
        readPads();
    }

    if (getKeyOn(KEY_START) || getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        // Check if there is anything to show the user, at all.
        if (gAPPStartMode || gETHStartMode || gBDMStartMode || gHDDStartMode) {
            guiSwitchScreen(GUI_SCREEN_MAIN);
            refreshMenuPosition();
        }
    }
}

static void menuRenderElements(theme_element_t *elem)
{
    // selected_item can't be NULL here as we only allow to switch to "Main" rendering when there is at least one device activated
    _menuRequestConfig();

    WaitSema(menuSemaId);

    while (elem) {
        if (elem->drawElem)
            elem->drawElem(selected_item, selected_item->item->current, itemConfig, elem);

        elem = elem->next;
    }
    SignalSema(menuSemaId);
}

void menuRenderMain(void)
{
    if (thmGetGuiValue() == 0) {
        dashboardRefresh();
        if (dashboard.tab == MAIZO_SETTINGS) {
            menuRenderMenu();
            return;
        }
        if (!menuDashboardSlot(0)) {
            guiDrawBGPlasma();
            maizoDrawHeader(dashboard.tab);
            fntRenderString(gTheme->fonts[0], 320, 224, ALIGN_CENTER, 560, 48,
                            dashboardError ? "Memoria insuficiente para a biblioteca" : "Nenhum item nesta aba", MAIZO_MUTED);
            fntRenderString(gTheme->fonts[0], 320, 290, ALIGN_CENTER, 560, 48,
                            "Start: configuracoes  |  Select: atualizar fontes", MAIZO_MUTED);
            return;
        }
    }
    item_list_t *list = selected_item->item->userdata;

    if (thmGetGuiValue() == 0) {
        /* Do not draw the old ItemsList, MenuText or legacy hints over the
         * Supreme carrousel. Keep the original OPL menu data and callbacks. */
        _menuRequestConfig();
        WaitSema(menuSemaId);
        thmRenderSupremeMain(selected_item, selected_item->item->current, itemConfig);
        SignalSema(menuSemaId);
        return;
    }

    if (list->mode == APP_MODE) {
        menuRenderElements(gTheme->appsMainElems.first);
        gTheme->itemsList = gTheme->appsItemsList;
    } else {
        menuRenderElements(gTheme->mainElems.first);
        gTheme->itemsList = gTheme->gamesItemsList;
    }
}

static void dashboardShowContext(void)
{
    char title[256], identity[256];
    int platform, canOptions;
    guiLock();
    dashboardRefresh();
    maizo_entry_t *entry = menuDashboardSlot(0);
    if (!entry) {
        guiUnlock();
        return;
    }
    snprintf(title, sizeof(title), "%s", entry->title);
    strcpy(identity, entry->identity);
    platform = entry->platform;
    item_list_t *support = selected_item->item->userdata;
    canOptions = !(support->flags & MODE_FLAG_NO_COMPAT) || (platform == MAIZO_APP && gEnableWrite);
    guiUnlock();
    struct UIItem choices[] = {
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {title, -1}}},
        {UI_BREAK},
        {UI_BUTTON, 1001, 1, 1, -1, 0, 0, {.label = {"Iniciar", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 1002, canOptions, canOptions, -1, 0, 0, {.label = {"Configuracoes do item", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 1003, 1, 1, -1, 0, 0, {.label = {"Informacoes", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 1004, 1, 1, -1, 0, 0, {.label = {"Alternar favorito", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 1005, 1, 1, -1, 0, 0, {.label = {"Atualizar fonte", -1}}},
        {UI_BREAK},
        {UI_TERMINATOR}};
    int result = diaExecuteDialog(choices, -1, 1, NULL);
    guiLock();
    dashboardRefresh();
    entry = menuDashboardSlot(0);
    int valid = entry && entry->platform == platform && strcmp(entry->identity, identity) == 0;
    menu_item_t *source = selected_item ? selected_item->item : NULL;
    guiUnlock();
    if (!valid || !source || !source->current)
        return; /* Removed while the panel was open. */
    if (result == 1001) {
        if (gSelectButton == KEY_CIRCLE)
            source->execCircle(source);
        else
            source->execCross(source);
    } else if (result == 1002) {
        source->execTriangle(source);
    } else if (result == 1003) {
        source->execSquare(source);
    } else if (result == 1004) {
        dashboardToggleFavorite();
    } else if (result == 1005) {
        source->refresh(source);
    }
}

static void dashboardHandleInput(void)
{
    guiLock();
    dashboardRefresh();
    if (getKeyOn(KEY_L1) || getKeyOn(KEY_R1)) {
        dashboardChangeTab(getKeyOn(KEY_R1) ? 1 : -1);
        guiUnlock();
        return;
    }
    if (dashboard.tab == MAIZO_SETTINGS) {
        guiUnlock();
        menuHandleInputMenu();
        return;
    }
    /* The classic OPL key-repeat delay is deliberately preserved in
     * legacy themes. Supreme navigates instantly on a new press, then
     * repeats every five rendered frames while held. No blocking sleep. */
    int pressLeft = getKeyPressed(KEY_LEFT) || getKeyPressed(KEY_UP);
    int pressRight = getKeyPressed(KEY_RIGHT) || getKeyPressed(KEY_DOWN);
    int pressEdge = getKeyOn(KEY_LEFT) || getKeyOn(KEY_UP) ||
                    getKeyOn(KEY_RIGHT) || getKeyOn(KEY_DOWN);
    if ((pressLeft || pressRight) &&
        (pressEdge || guiFrameId - dashboardLastNavFrame >= 5)) {
        dashboardLastNavFrame = guiFrameId;
        carouselDirection = pressLeft ? -1 : 1;
        maizoLibraryMove(&dashboard, carouselDirection);
        dashboardSelect();
        dashboardStartAnimation();
        sfxPlay(SFX_CURSOR);
    } else if (getKeyOn(KEY_L2) || getKeyOn(KEY_R2)) {
        if (dashboard.view_count)
            dashboard.position = getKeyOn(KEY_L2) ? 0 : dashboard.view_count - 1;
        dashboardSelect();
    }
    int hasItem = menuDashboardSlot(0) != NULL;
    guiUnlock();
    if (getKeyOn(KEY_START)) {
        menuInitMainMenu();
        guiSwitchScreen(GUI_SCREEN_MENU);
    } else if (getKeyOn(KEY_SELECT)) {
        /* Includes manually configured, not-yet-started sources. */
        for (menu_list_t *source = menu; source; source = source->next) {
            if (source->item->visible) {
                item_list_t *support = source->item->userdata;
                if (support->enabled)
                    source->item->refresh(source->item);
                else if (gSelectButton == KEY_CIRCLE)
                    source->item->execCircle(source->item);
                else
                    source->item->execCross(source->item);
            }
        }
    } else if (hasItem && getKeyOn(KEY_SQUARE)) {
        selected_item->item->execSquare(selected_item->item);
    } else if (hasItem && getKeyOn(KEY_TRIANGLE)) {
        dashboardShowContext();
    } else if (hasItem && getKeyOn(gSelectButton)) {
        if (gSelectButton == KEY_CIRCLE)
            selected_item->item->execCircle(selected_item->item);
        else
            selected_item->item->execCross(selected_item->item);
    } else if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        if (dashboard.tab != MAIZO_ALL) {
            guiLock();
            maizoLibraryFilter(&dashboard, MAIZO_ALL);
            dashboardSelect();
            guiUnlock();
        }
    } else if (hasItem && RemainSecs < 0) {
        DisableCron = 1;
        RemainSecs = 0;
        if (gSelectButton == KEY_CIRCLE)
            selected_item->item->execCircle(selected_item->item);
        else
            selected_item->item->execCross(selected_item->item);
    }
}

void menuHandleInputMain()
{
    if (thmGetGuiValue() == 0) {
        dashboardHandleInput();
        return;
    }
    if (getKey(KEY_LEFT)) {
        if (thmGetGuiValue() == 0)
            menuPrevV();
        else
            menuPrevH();
    } else if (getKey(KEY_RIGHT)) {
        if (thmGetGuiValue() == 0)
            menuNextV();
        else
            menuNextH();
    } else if (getKey(KEY_UP)) {
        menuPrevV();
    } else if (getKey(KEY_DOWN)) {
        menuNextV();
    } else if (getKeyOn(KEY_CROSS)) {
        selected_item->item->execCross(selected_item->item);
    } else if (getKeyOn(KEY_TRIANGLE)) {
        selected_item->item->execTriangle(selected_item->item);
    } else if (getKeyOn(KEY_CIRCLE)) {
        selected_item->item->execCircle(selected_item->item);
    } else if (getKeyOn(KEY_SQUARE)) {
        selected_item->item->execSquare(selected_item->item);
    } else if (getKeyOn(KEY_START)) {
        // reinit main menu - show/hide items valid in the active context
        menuInitMainMenu();
        guiSwitchScreen(GUI_SCREEN_MENU);
    } else if (getKeyOn(KEY_SELECT)) {
        selected_item->item->refresh(selected_item->item);
    } else if (getKey(KEY_L1)) {
        if (thmGetGuiValue() == 0)
            menuPrevH();
        else
            menuPrevPage();
    } else if (getKey(KEY_R1)) {
        if (thmGetGuiValue() == 0)
            menuNextH();
        else
            menuNextPage();
    } else if (getKeyOn(KEY_L2)) { // home
        menuFirstPage();
    } else if (getKeyOn(KEY_R2)) { // end
        menuLastPage();
    }

    // Last Played Auto Start
    if (RemainSecs < 0) {
        DisableCron = 1; // Disable Counter
        RemainSecs = 0;  // Reset, so a launch that returns (e.g. a cancelled parental lock prompt) does not retrigger every frame
        if (gSelectButton == KEY_CIRCLE)
            selected_item->item->execCircle(selected_item->item);
        else
            selected_item->item->execCross(selected_item->item);
    }
}

void menuRenderInfo(void)
{
    item_list_t *list = selected_item->item->userdata;

    if (thmGetGuiValue() == 0) {
        const char *identity = NULL;
        char path[256];
        int mode = list->mode;
        submenu_list_t *current = selected_item->item->current;
        if (!current || !selected_item->item->visible) {
            guiSwitchScreen(GUI_SCREEN_MAIN);
            return;
        }
        if (list->itemGetStartup)
            identity = list->itemGetStartup(list, current->item.id);
        if (mode == APP_MODE && appGetItemPath(current->item.id, path, sizeof(path)))
            identity = path;
        guiDrawBGPlasma();
        maizoDrawBrand();
        fntRenderString(gTheme->fonts[0], 608, 20, ALIGN_RIGHT, 160, 28,
                        "INFORMACOES", MAIZO_MUTED);
        maizoDrawPanel(30, 110, 580, 285, 0);
        maizoDrawIcon(mode == APP_MODE ? MAIZO_ICON_APPS :
                      mode == PS1_MODE ? MAIZO_ICON_PS1 :
                                         MAIZO_ICON_PS2,
                      44, 126, 40, MAIZO_BRIGHT);
        fntRenderString(gTheme->fonts[0], 98, 132, ALIGN_NONE, 486, 52,
                        submenuItemGetText(&current->item), MAIZO_TEXT);
        maizoDrawSeparator(44, 198, 548);
        fntRenderString(gTheme->fonts[0], 48, 215, ALIGN_NONE, 520, 30,
                        mode == APP_MODE ? "Aplicativo ELF" :
                        mode == PS1_MODE ? "PlayStation 1" :
                                           "PlayStation 2",
                        MAIZO_MUTED);
        if (identity && identity[0]) {
            fntRenderString(gTheme->fonts[0], 48, 264, ALIGN_NONE, 520, 26,
                            "Identificador / origem:", MAIZO_MUTED);
            fntRenderString(gTheme->fonts[0], 48, 294, ALIGN_NONE, 520, 50,
                            identity, MAIZO_TEXT);
        } else
            fntRenderString(gTheme->fonts[0], 48, 270, ALIGN_NONE, 520, 35,
                            "Identificador nao informado", MAIZO_MUTED);
        guiDrawSubMenuHints();
        return;
    }

    if (list->mode == APP_MODE) {
        menuRenderElements(gTheme->appsInfoElems.first);
        gTheme->itemsList = gTheme->appsItemsList;
    } else {
        menuRenderElements(gTheme->infoElems.first);
        gTheme->itemsList = gTheme->gamesItemsList;
    }
}

void menuHandleInputInfo()
{
    if (getKeyOn(KEY_CROSS)) {
        if (gSelectButton == KEY_CIRCLE)
            guiSwitchScreen(GUI_SCREEN_MAIN);
        else
            selected_item->item->execCross(selected_item->item);
    } else if (getKey(KEY_UP)) {
        menuPrevV();
    } else if (getKey(KEY_DOWN)) {
        menuNextV();
    } else if (getKeyOn(KEY_CIRCLE)) {
        if (gSelectButton == KEY_CROSS)
            guiSwitchScreen(GUI_SCREEN_MAIN);
        else
            selected_item->item->execCircle(selected_item->item);
    } else if (getKey(KEY_L1)) {
        menuPrevPage();
    } else if (getKey(KEY_R1)) {
        menuNextPage();
    } else if (getKeyOn(KEY_L2)) {
        menuFirstPage();
    } else if (getKeyOn(KEY_R2)) {
        menuLastPage();
    }
}

void menuRenderGameMenu()
{
    guiDrawBGPlasma();

    if (!gameMenu)
        return;

    // If the device menu that has the selected game suddenly goes invisible (device was removed), switch
    // back to the game list menu.
    if (selected_item->item->visible == 0) {
        guiSwitchScreen(GUI_SCREEN_MAIN);
        return;
    }

    // If we enter the game settings menu and there's no selected item bail out. I'm not entirely sure how we get into
    // this state but it seems to happen on some consoles when transitioning from the game settings menu back to the game
    // list menu.
    if (selected_item->item->current == NULL)
        return;

    if (thmGetGuiValue() == 0) {
        submenu_list_t *it;
        int index = 0, active = 0, first, y = 164;
        const int rx = 236, rw = 382;
        const char *title = selected_item->item->current->item.text;
        if (!gameMenuCurrent)
            gameMenuCurrent = gameMenu;
        for (it = gameMenu; it; it = it->next, index++)
            if (it == gameMenuCurrent)
                active = index;
        first = active >= 7 ? active - 6 : 0;
        rmDrawRect(0, 0, 640, gTheme->usedHeight, MAIZO_BLACK);
        maizoDrawHeader(MAIZO_SETTINGS);
        rmDrawRect(0, 70, 640, 1, MAIZO_DIVIDER);

        maizoDrawPanel(25, 106, 200, 302, 0);
        maizoDrawIcon(MAIZO_ICON_PS2, 43, 125, 40, MAIZO_BRIGHT);
        fntRenderString(gTheme->fonts[0], 44, 176, ALIGN_NONE,
                        160, 70, title, MAIZO_TEXT);
        maizoDrawSeparator(41, 257, 162);
        fntRenderString(gTheme->fonts[0], 41, 276, ALIGN_NONE,
                        170, 28, "CONFIGURACAO", MAIZO_MUTED);
        fntRenderString(gTheme->fonts[0], 41, 309, ALIGN_NONE,
                        170, 58, gameConfigSource(), MAIZO_BRIGHT);

        maizoDrawPanel(rx, 106, rw, 302, 0);
        maizoDrawIcon(MAIZO_ICON_SETTINGS, rx + 13, 117, 26, MAIZO_BRIGHT);
        fntRenderString(gTheme->fonts[0], rx + 48, 118,
                        ALIGN_NONE, rw - 62, 29,
                        "OPCOES DO JOGO", MAIZO_TEXT);
        maizoDrawSeparator(rx + 13, 149, rw - 26);
        index = 0;
        for (it = gameMenu; it; it = it->next, index++) {
            int focused;
            if (index < first || index >= first + 6)
                continue;
            focused = it == gameMenuCurrent;
            rmDrawRect(rx + 10, y, rw - 20, 35,
                       focused ? MAIZO_CARD : MAIZO_SURFACE);
            if (focused)
                rmDrawRect(rx + 10, y, 3, 35, MAIZO_ACCENT);
            maizoDrawIcon(it->item.id == GAME_SAVE_CHANGES ?
                              MAIZO_ICON_CHECK :
                          it->item.id == GAME_DELETE_GAME ||
                                  it->item.id == GAME_REMOVE_CHANGES ?
                              MAIZO_ICON_DELETE :
                          it->item.id == GAME_CHEAT_SETTINGS ?
                              MAIZO_ICON_CHEATS :
                          it->item.id == GAME_VMC_SETTINGS ?
                              MAIZO_ICON_CARD :
                          it->item.id == GAME_GSM_SETTINGS ?
                              MAIZO_ICON_VIDEO :
                              MAIZO_ICON_SETTINGS,
                          rx + 22, y + 8, 19,
                          focused ? MAIZO_BRIGHT : MAIZO_MUTED);
            fntRenderString(gTheme->fonts[0], rx + 52, y + 8,
                            ALIGN_NONE, rw - 93, 25,
                            submenuItemGetText(&it->item),
                            focused ? MAIZO_TEXT : MAIZO_MUTED);
            maizoDrawIcon(MAIZO_ICON_CONFIRM, rx + rw - 35,
                          y + 9, 16,
                          focused ? MAIZO_BRIGHT : MAIZO_DISABLED);
            y += 38;
        }
        if (index > 6)
            fntRenderString(gTheme->fonts[0], rx + rw - 18, 391,
                            ALIGN_RIGHT, 230, 22, "MAIS OPCOES",
                            MAIZO_MUTED);
        maizoSettingsFooter("POR JOGO");
        return;
    }

    // draw the animated menu
    if (!gameMenuCurrent)
        gameMenuCurrent = gameMenu;

    submenu_list_t *it = gameMenu;

    // calculate the number of items
    int count = 0;
    int sitem = 0;
    for (; it; count++, it = it->next) {
        if (it == gameMenuCurrent)
            sitem = count;
    }

    int spacing = 25;
    int y = (gTheme->usedHeight >> 1) - (spacing * (count >> 1));
    int cp = 0; // current position

    // game title
    fntRenderString(gTheme->fonts[0], 320, 20, ALIGN_CENTER, 0, 0, selected_item->item->current->item.text, gTheme->selTextColor);

    // config source
    char *cfgSource = gameConfigSource();
    fntRenderString(gTheme->fonts[0], 320, 40, ALIGN_CENTER, 0, 0, cfgSource, gTheme->textColor);

    // settings list
    for (it = gameMenu; it; it = it->next, cp++) {
        // render, advance
        fntRenderString(gTheme->fonts[0], 320, y, ALIGN_CENTER, 0, 0, submenuItemGetText(&it->item), (cp == sitem) ? gTheme->selTextColor : gTheme->textColor);
        y += spacing;
        if (cp == (GAME_SAVE_CHANGES - 1) || cp == (GAME_REMOVE_CHANGES - 1))
            y += spacing / 2;
    }

    // hints
    guiDrawSubMenuHints();
}

void menuHandleInputGameMenu()
{
    if (!gameMenu)
        return;

    if (!gameMenuCurrent)
        gameMenuCurrent = gameMenu;

    if (getKey(KEY_UP)) {
        sfxPlay(SFX_CURSOR);
        if (gameMenuCurrent->prev)
            gameMenuCurrent = gameMenuCurrent->prev;
        else // rewind to the last item
            while (gameMenuCurrent->next)
                gameMenuCurrent = gameMenuCurrent->next;
    }

    if (getKey(KEY_DOWN)) {
        sfxPlay(SFX_CURSOR);
        if (gameMenuCurrent->next)
            gameMenuCurrent = gameMenuCurrent->next;
        else
            gameMenuCurrent = gameMenu;
    }

    if (getKeyOn(gSelectButton)) {
        // execute the item via looking at the id of it
        int menuID = gameMenuCurrent->item.id;

        sfxPlay(SFX_CONFIRM);

        if (menuID == GAME_COMPAT_SETTINGS) {
            guiGameShowCompatConfig(selected_item->item->current->item.id, selected_item->item->userdata, itemConfig);
        } else if (menuID == GAME_CHEAT_SETTINGS) {
            guiGameShowCheatConfig();
        } else if (menuID == GAME_GSM_SETTINGS) {
            guiGameShowGSConfig();
        } else if (menuID == GAME_VMC_SETTINGS) {
            guiGameShowVMCMenu(selected_item->item->current->item.id, selected_item->item->userdata);
#ifdef PADEMU
        } else if (menuID == GAME_PADEMU_SETTINGS) {
            guiGameShowPadEmuConfig(0);
        } else if (menuID == GAME_PADMACRO_SETTINGS) {
            guiGameShowPadMacroConfig(0);
#endif
        } else if (menuID == GAME_OSD_LANGUAGE_SETTINGS) {
            guiGameShowOSDLanguageConfig(0);
        } else if (menuID == GAME_SAVE_CHANGES) {
            if (guiGameSaveConfig(itemConfig, selected_item->item->userdata))
                configSetInt(itemConfig, CONFIG_ITEM_CONFIGSOURCE, CONFIG_SOURCE_USER);
            menuSaveConfig();
            saveConfig(CONFIG_GAME, 0);
            menuSetParentalLockCheckState(1); // Re-enable parental lock check.
            guiMsgBox(_l(_STR_GAME_SETTINGS_SAVED), 0, NULL);
            guiGameLoadConfig(selected_item->item->userdata, gameMenuLoadConfig(NULL));
        } else if (menuID == GAME_TEST_CHANGES) {
            guiGameTestSettings(selected_item->item->current->item.id, selected_item->item->userdata, itemConfig);
        } else if (menuID == GAME_REMOVE_CHANGES) {
            if (guiGameShowRemoveSettings(itemConfig, configGetByType(CONFIG_GAME))) {
                menuSetParentalLockCheckState(1); // Re-enable parental lock check.
                guiGameLoadConfig(selected_item->item->userdata, gameMenuLoadConfig(NULL));
            }
        } else if (menuID == GAME_RENAME_GAME) {
            menuRenameGame(&gameMenu);
        } else if (menuID == GAME_DELETE_GAME) {
            menuDeleteGame(&gameMenu);
        }
        // so the exit press wont propagate twice
        readPads();
    }

    if (getKeyOn(KEY_START) || getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        guiSwitchScreen(GUI_SCREEN_MAIN);
    }
}

void menuRenderAppMenu()
{
    guiDrawBGPlasma();

    if (!appMenu)
        return;

    if (thmGetGuiValue() == 0) {
        submenu_list_t *it;
        int pos = 0, selected = 0, first, y = 177;
        if (!appMenuCurrent)
            appMenuCurrent = appMenu;
        for (it = appMenu; it; it = it->next, pos++)
            if (it == appMenuCurrent)
                selected = pos;
        first = selected >= 6 ? selected - 5 : 0;
        rmDrawRect(0, 0, 640, gTheme->usedHeight, MAIZO_BLACK);
        maizoDrawHeader(MAIZO_APPS_TAB);
        maizoDrawPanel(28, 110, 202, 278, 0);
        maizoDrawIcon(MAIZO_ICON_APPS, 45, 128, 40, MAIZO_BRIGHT);
        fntRenderString(gTheme->fonts[0], 45, 183,
                        ALIGN_NONE, 165, 90,
                        selected_item->item->current ?
                            selected_item->item->current->item.text :
                            "APLICATIVO",
                        MAIZO_TEXT);
        maizoDrawSeparator(43, 295, 168);
        fntRenderString(gTheme->fonts[0], 45, 317,
                        ALIGN_NONE, 168, 26,
                        "APLICATIVO / ELF", MAIZO_MUTED);
        maizoDrawPanel(241, 110, 371, 278, 0);
        fntRenderString(gTheme->fonts[0], 257, 128,
                        ALIGN_NONE, 329, 30,
                        "GERENCIAR APLICATIVO", MAIZO_TEXT);
        maizoDrawSeparator(254, 161, 338);
        pos = 0;
        for (it = appMenu; it; it = it->next, pos++) {
            int focused = it == appMenuCurrent;
            if (pos < first || pos >= first + 5)
                continue;
            rmDrawRect(252, y, 350, 39,
                       focused ? MAIZO_CARD : MAIZO_SURFACE);
            if (focused)
                rmDrawRect(252, y, 3, 39, MAIZO_ACCENT);
            maizoDrawIcon(it->item.id == 1 ?
                              MAIZO_ICON_DELETE : MAIZO_ICON_FILE,
                          266, y + 9, 22,
                          focused ? MAIZO_BRIGHT : MAIZO_MUTED);
            fntRenderString(gTheme->fonts[0], 300, y + 10,
                            ALIGN_NONE, 277, 28,
                            submenuItemGetText(&it->item),
                            focused ? MAIZO_TEXT : MAIZO_MUTED);
            y += 42;
        }
        maizoSettingsFooter("APLICATIVO");
        return;
    }

    // draw the animated menu
    if (!appMenuCurrent)
        appMenuCurrent = appMenu;

    submenu_list_t *it = appMenu;

    // calculate the number of items
    int count = 0;
    int sitem = 0;
    for (; it; count++, it = it->next) {
        if (it == appMenuCurrent)
            sitem = count;
    }

    int spacing = 25;
    int y = (gTheme->usedHeight >> 1) - (spacing * (count >> 1));
    int cp = 0; // current position

    // app title
    fntRenderString(gTheme->fonts[0], 320, 20, ALIGN_CENTER, 0, 0, selected_item->item->current->item.text, gTheme->selTextColor);

    for (it = appMenu; it; it = it->next, cp++) {
        // render, advance
        fntRenderString(gTheme->fonts[0], 320, y, ALIGN_CENTER, 0, 0, submenuItemGetText(&it->item), (cp == sitem) ? gTheme->selTextColor : gTheme->textColor);
        y += spacing;
    }

    // hints
    guiDrawSubMenuHints();
}

void menuHandleInputAppMenu()
{
    if (!appMenu)
        return;

    if (!appMenuCurrent)
        appMenuCurrent = appMenu;

    if (getKey(KEY_UP)) {
        sfxPlay(SFX_CURSOR);
        if (appMenuCurrent->prev)
            appMenuCurrent = appMenuCurrent->prev;
        else // rewind to the last item
            while (appMenuCurrent->next)
                appMenuCurrent = appMenuCurrent->next;
    }

    if (getKey(KEY_DOWN)) {
        sfxPlay(SFX_CURSOR);
        if (appMenuCurrent->next)
            appMenuCurrent = appMenuCurrent->next;
        else
            appMenuCurrent = appMenu;
    }

    if (getKeyOn(gSelectButton)) {
        // execute the item via looking at the id of it
        int menuID = appMenuCurrent->item.id;

        sfxPlay(SFX_CONFIRM);

        if (menuID == 0) {
            menuRenameGame(&appMenu);
        } else if (menuID == 1) {
            menuDeleteGame(&appMenu);
        }
        // so the exit press wont propagate twice
        readPads();
    }

    if (getKeyOn(KEY_START) || getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        guiSwitchScreen(GUI_SCREEN_MAIN);
    }
}
