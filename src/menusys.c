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
#include "include/bdmsupport.h"
#include "include/ethsupport.h"
#include "include/appsupport.h"
#include "include/maizo_filemgr.h"
#include "include/cheatman.h"

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
    MENU_LIBRARY_REFRESH,
    MENU_FILE_MANAGER,
    MENU_SOURCE_FILTER,
    MENU_STORAGE_PERFORMANCE,
    MENU_GAME_PREFERENCES,
    MENU_INPUT_MAPPING,
    MENU_VIEW_LAYOUT,
    MENU_QUICK_SETUP,
    MENU_RESET_APPEARANCE,
    MENU_SMB_STATUS,
    MENU_PAD_HELP,
    MENU_PROMPT_STYLE
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
    GAME_CHEAT_ADVANCED,
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
static int detailFocus, detailReturn;

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
static char settingsQuery[32];
static const char *settingsBreadcrumb;
static int settingsRemember[10]; /* menu id + 1; zero means first item */
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
const char *menuDashboardQuery(void) { return dashboard.query; }
const char *menuSettingsBreadcrumb(void) { return settingsBreadcrumb; }

int menuDashboardLayout(void)
{
    int value = 0;
    configGetInt(configGetByType(CONFIG_OPL), "maizo_layout", &value);
    return value >= 0 && value <= 2 ? value : 0;
}
maizo_entry_t *menuDashboardSlot(int offset) { return maizoLibrarySlot(&dashboard, offset); }

static int dashboardPlatform(menu_list_t *source)
{
    item_list_t *support = source->item->userdata;
    return support->mode == PS1_MODE ? MAIZO_PS1 : support->mode == APP_MODE ? MAIZO_APP :
                                                                               MAIZO_PS2;
}

static int dashboardSource(item_list_t *support)
{
    if (support->mode == ETH_MODE)
        return MAIZO_SOURCE_SMB;
    if (support->mode == HDD_MODE)
        return MAIZO_SOURCE_HDD;
    if (support->mode == APP_MODE)
        return MAIZO_SOURCE_APP;
    if (support->mode == PS1_MODE)
        return MAIZO_SOURCE_PS1;
    if (support->mode >= BDM_MODE && support->mode < ETH_MODE && support->priv) {
        switch (((bdm_device_data_t *)support->priv)->bdmDeviceType) {
            case BDM_TYPE_USB:
                return MAIZO_SOURCE_USB;
            case BDM_TYPE_SDC:
                return MAIZO_SOURCE_MX4SIO;
            case BDM_TYPE_ATA:
                return MAIZO_SOURCE_HDD;
            case BDM_TYPE_ILINK:
                return MAIZO_SOURCE_ILINK;
        }
    }
    return MAIZO_SOURCE_ALL;
}

int menuMaizoSourceAvailable(int wanted)
{
    int available = 0;
    guiLock();
    for (menu_list_t *source = menu; source; source = source->next) {
        item_list_t *support = source->item->userdata;
        if (source->item->visible && support->enabled && dashboardSource(support) == wanted) {
            available = 1;
            break;
        }
    }
    guiUnlock();
    if (wanted == MAIZO_SOURCE_SMB)
        return available && gNetworkStartup == 0;
    return available;
}

static const char *const sourceNames[] = {"Todas", "USB", "Rede SMB", "MX4SIO", "HDD", "iLink", "Aplicativos", "PS1", NULL};

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
    int showUsb = 1, showMask = 0xff, filter = 0;
    configGetInt(configGetByType(CONFIG_OPL), "maizo_show_usb", &showUsb);
    configGetInt(configGetByType(CONFIG_OPL), "maizo_source_mask", &showMask);
    configGetInt(configGetByType(CONFIG_OPL), "maizo_source_filter", &filter);
    if (!showUsb)
        showMask &= ~(1 << MAIZO_SOURCE_USB);
    if (filter < 0 || filter >= MAIZO_SOURCE_COUNT)
        filter = 0;
    signature = showMask;
    if (!builtRevision) {
        int initial = 0;
        configGetInt(configGetByType(CONFIG_OPL), "maizo_home_tab", &initial);
        if (initial >= 0 && initial < MAIZO_SETTINGS)
            tab = initial;
    }
    for (source = menu; source; source = source->next) {
        item_list_t *support = source->item->userdata;
        signature = signature * 33 + source->item->visible + (support->enabled << 1);
    }
    if (builtRevision == libraryRevision && signature == librarySignature) {
        if (dashboard.sourceFilter != filter) {
            int previous = dashboard.sourceFilter;
            dashboard.sourceFilter = filter;
            if (!maizoLibraryFilter(&dashboard, tab)) {
                dashboard.sourceFilter = previous;
                dashboardError = 1;
            } else {
                dashboardError = 0;
                dashboardSelect();
            }
        }
        return;
    }
    /* Save selection by stable identity before discarding any source references. */
    maizo_entry_t *current = menuDashboardSlot(0);
    if (current && tab < MAIZO_SETTINGS) {
        strcpy(dashboard.remembered[tab], current->identity);
        dashboard.remembered_platform[tab] = current->platform;
    }
    maizoLibraryClear(&dashboard);
    dashboard.sourceFilter = filter;
    dashboardError = 0;
    for (source = menu; source; source = source->next) {
        item_list_t *support = source->item->userdata;
        submenu_list_t *item;
        if (!source->item->visible || !support->enabled)
            continue;
        int origin = dashboardSource(support);
        if (origin && !(showMask & (1 << origin)))
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
            dashboard.entries[dashboard.count - 1].source = origin;
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

static const char *settingsNames[] = {"INTERFACE", "CONTROLES", "JOGOS", "VIDEO", "AUDIO", "ARMAZENAMENTO", "REDE", "SISTEMA", "DIAGNOSTICO", "CREDITOS"};
static const int settingsIcons[] = {
    MAIZO_ICON_SETTINGS, MAIZO_ICON_PAD, MAIZO_ICON_PS2,
    MAIZO_ICON_VIDEO, MAIZO_ICON_AUDIO, MAIZO_ICON_HDD,
    MAIZO_ICON_NETWORK, MAIZO_ICON_INFO, MAIZO_ICON_USB, MAIZO_ICON_INFO};
#define MAIZO_SETTING_CATEGORIES 10

static int settingsItemCategory(int id)
{
    switch (id) {
        case MENU_GFX_SETTINGS:
        case MENU_ANIMATIONS:
        case MENU_COVER_MODE:
        case MENU_VIEW_LAYOUT:
        case MENU_RESET_APPEARANCE:
        case MENU_PROMPT_STYLE:
            return 0;
        case MENU_LIBRARY_SORT:
        case MENU_LIBRARY_REFRESH:
        case MENU_SOURCE_FILTER:
        case MENU_GAME_PREFERENCES:
            return 2;
        case MENU_CONTROLLER_SETTINGS:
        case MENU_PADEMU:
        case MENU_BLUETOOTH:
        case MENU_INPUT_MAPPING:
        case MENU_PAD_HELP:
        case MENU_USB_DIAG:
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
        case MENU_FILE_MANAGER:
        case MENU_STORAGE_PERFORMANCE:
            return 5;
        case MENU_NET_CONFIG:
        case MENU_NET_UPDATE:
        case MENU_START_NBD:
        case MENU_SMB_STATUS:
            return 6;
        case MENU_DIAG_SYSTEM:
            return 8;
        case MENU_ABOUT:
            return 9;
        default:
            return 7;
    }
}

static void menuInitMainMenu(void)
{
    if (mainMenu)
        submenuDestroy(&mainMenu);

    // initialize the menu
    submenuAppendItem(&mainMenu, -1, "Configurações do sistema", MENU_SETTINGS, -1);
    if (thmGetGuiValue() == 0) {
        submenuAppendItem(&mainMenu, -1, "Organizar biblioteca", MENU_LIBRARY_SORT, -1);
        submenuAppendItem(&mainMenu, -1, "Filtrar por origem", MENU_SOURCE_FILTER, -1);
        submenuAppendItem(&mainMenu, -1, "Preferências dos jogos", MENU_GAME_PREFERENCES, -1);
        submenuAppendItem(&mainMenu, -1, "Atualizar biblioteca", MENU_LIBRARY_REFRESH, -1);
        submenuAppendItem(&mainMenu, -1, "Capas dos jogos", MENU_COVER_MODE, -1);
        submenuAppendItem(&mainMenu, -1, "Visualização da biblioteca", MENU_VIEW_LAYOUT, -1);
        submenuAppendItem(&mainMenu, -1, "Estilo dos botões", MENU_PROMPT_STYLE, -1);
        submenuAppendItem(&mainMenu, -1, "Restaurar aparência", MENU_RESET_APPEARANCE, -1);
        submenuAppendItem(&mainMenu, -1, "Assistente de configuração", MENU_QUICK_SETUP, -1);
        submenuAppendItem(&mainMenu, -1, "Estado da conexão SMB", MENU_SMB_STATUS, -1);
        submenuAppendItem(&mainMenu, -1, "Informacoes tecnicas", MENU_DIAG_SYSTEM, -1);
        submenuAppendItem(&mainMenu, -1, "Resolução e ajuste de tela", MENU_VIDEO_SETTINGS, -1);
        submenuAppendItem(&mainMenu, -1, "Fontes de jogos e tela inicial", MENU_STORAGE_SETTINGS, -1);
        submenuAppendItem(&mainMenu, -1, "Gerenciador de arquivos", MENU_FILE_MANAGER, -1);
        submenuAppendItem(&mainMenu, -1, "Avançado: armazenamento", MENU_STORAGE_PERFORMANCE, -1);
    }
    submenuAppendItem(&mainMenu, -1, NULL, MENU_GFX_SETTINGS, _STR_GFX_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_AUDIO_SETTINGS, _STR_AUDIO_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_CONTROLLER_SETTINGS, _STR_CONTROLLER_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_OSD_LANGUAGE_SETTINGS, _STR_OSD_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_PARENTAL_LOCK, _STR_PARENLOCKCONFIG);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_NET_CONFIG, _STR_NETCONFIG);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_NET_UPDATE, _STR_NET_UPDATE);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_START_NBD, _STR_STARTNBD);
    submenuAppendItem(&mainMenu, -1, "Creditos OPL e MAIZON", MENU_ABOUT, -1);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_SAVE_CHANGES, _STR_SAVE_CHANGES);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_EXIT, _STR_EXIT);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_POWER_OFF, _STR_POWEROFF);

    if (thmGetGuiValue() == 0) {
        submenuAppendItem(&mainMenu, -1, "Animações do menu", MENU_ANIMATIONS, -1);
        if (selected_item && selected_item->item->current)
            submenuAppendItem(&mainMenu, -1, "Informacoes do item", MENU_ITEM_INFO, -1);
#ifdef PADEMU
        submenuAppendItem(&mainMenu, -1, "Controles no menu OPL", MENU_PAD_HELP, -1);
        submenuAppendItem(&mainMenu, -1, "Emulação de controles nos jogos", MENU_PADEMU, -1);
        submenuAppendItem(&mainMenu, -1, "Mapeamento USB e teclado", MENU_INPUT_MAPPING, -1);
        submenuAppendItem(&mainMenu, -1, "Parear controle Bluetooth", MENU_BLUETOOTH, -1);
        submenuAppendItem(&mainMenu, -1, "Testar controles USB", MENU_USB_DIAG, -1);
#endif
        submenu_list_t *item = mainMenu;
        while (item) {
            submenu_list_t *next = item->next;
            int category = settingsItemCategory(item->item.id);
            int keep = settingsQuery[0] ? maizoTextMatches(submenuItemGetText(&item->item), settingsQuery) || maizoTextMatches(settingsNames[category], settingsQuery) : category == settingsCategory;
            if (!keep)
                submenuRemoveItem(&mainMenu, item->item.id);
            item = next;
        }
        /* Existing remove routine now maintains previous links for focus traversal. */
    }
    mainMenuCurrent = mainMenu;
    if (thmGetGuiValue() == 0)
        for (submenu_list_t *it = mainMenu; it; it = it->next)
            if (it->item.id + 1 == settingsRemember[settingsCategory]) {
                mainMenuCurrent = it;
                break;
            }
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
    submenuAppendItem(&gameMenu, -1, thmGetGuiValue() == 0 ? "Compatibilidade" : NULL, GAME_COMPAT_SETTINGS, _STR_COMPAT_SETTINGS);
    submenuAppendItem(&gameMenu, -1, thmGetGuiValue() == 0 ? "Cheats do jogo" : NULL, GAME_CHEAT_SETTINGS, _STR_CHEAT_SETTINGS);
    if (thmGetGuiValue() == 0)
        submenuAppendItem(&gameMenu, -1, "Cheats: avançado", GAME_CHEAT_ADVANCED, -1);
    submenuAppendItem(&gameMenu, -1, thmGetGuiValue() == 0 ? "Vídeo (GSM)" : NULL, GAME_GSM_SETTINGS, _STR_GSCONFIG);
    submenuAppendItem(&gameMenu, -1, thmGetGuiValue() == 0 ? "Memory Card virtual" : NULL, GAME_VMC_SETTINGS, _STR_VMC_SCREEN);
#ifdef PADEMU
    submenuAppendItem(&gameMenu, -1, thmGetGuiValue() == 0 ? "Controles (PadEmu)" : NULL, GAME_PADEMU_SETTINGS, _STR_PADEMUCONFIG);
#endif
    submenuAppendItem(&gameMenu, -1, thmGetGuiValue() == 0 ? "Idioma e região" : NULL, GAME_OSD_LANGUAGE_SETTINGS, _STR_OSD_SETTINGS);
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
        submenu->item.cache_id = size ? malloc(size) : NULL;
        submenu->item.cache_uid = size ? malloc(size) : NULL;
        if (submenu->item.cache_id && submenu->item.cache_uid) {
            memset(submenu->item.cache_id, -1, size);
            memset(submenu->item.cache_uid, -1, size);
        } else {
            free(submenu->item.cache_id);
            free(submenu->item.cache_uid);
            submenu->item.cache_id = submenu->item.cache_uid = NULL;
        }

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
    maizoDrawPromptHint(gSelectButton == KEY_CIRCLE ? CIRCLE_ICON : CROSS_ICON, 28, h - 45, "Abrir");
    maizoDrawPromptHint(gSelectButton == KEY_CIRCLE ? CROSS_ICON : CIRCLE_ICON, 144, h - 45, "Voltar");
    fntRenderString(gTheme->fonts[0], 263, h - 43, ALIGN_NONE, 190, 25, "L1/R1 Categoria", MAIZO_MUTED);
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
        case MENU_FILE_MANAGER:
            return MAIZO_ICON_FOLDER;
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
    "RELATORIOS TECNICOS E PERIFERICOS",
    "EQUIPE ORIGINAL DO OPL E MAIZON"};

void menuRenderMenu()
{
    settingsBreadcrumb = NULL;
    guiDrawBGPlasma();
    if (thmGetGuiValue() == 0) {
        const int leftX = 24, rightX = 231, rightWidth = 387;
        int y, active = 0, first, pos = 0;
        int panelHeight = gTheme->usedHeight - 165;
        int categoryStep = (panelHeight - 12) / MAIZO_SETTING_CATEGORIES;
        int categoryRows = MAIZO_SETTING_CATEGORIES;
        if (categoryStep < 24) {
            categoryStep = 24;
            categoryRows = (panelHeight - 12) / categoryStep;
            if (categoryRows < 1)
                categoryRows = 1;
        }
        int categoryFirst = settingsCategory >= categoryRows ? settingsCategory - categoryRows + 1 : 0;
        int visibleRows = (panelHeight - 73) / 35;
        if (visibleRows < 1)
            visibleRows = 1;
        submenu_list_t *it;

        rmDrawRect(0, 0, 640, gTheme->usedHeight, MAIZO_BLACK);
        maizoDrawHeader(MAIZO_SETTINGS);
        rmDrawRect(0, 66, 640, 1, MAIZO_DIVIDER);
        fntRenderString(gTheme->fonts[0], 30, 77, ALIGN_NONE,
                        345, 30, "CONFIGURACOES", MAIZO_TEXT);

        /* Fixed category navigation rail; input focus and content
         * selection are independent. */
        maizoDrawPanel(leftX, 120, 194, panelHeight, 0);
        for (int i = categoryFirst; i < MAIZO_SETTING_CATEGORIES && i < categoryFirst + categoryRows; i++) {
            int rowY = 130 + (i - categoryFirst) * categoryStep;
            int highlighted = i == settingsCategory;
            if (highlighted)
                rmDrawRect(leftX + 3, rowY - 2, 188, categoryStep,
                           settingsFocus ? MAIZO_DIVIDER : MAIZO_CARD);
            if (highlighted)
                rmDrawRect(leftX + 3, rowY - 2, 3, categoryStep, MAIZO_ACCENT);
            maizoDrawIcon(settingsIcons[i], leftX + 12, rowY + 1, 18,
                          highlighted ? MAIZO_BRIGHT : MAIZO_MUTED);
            fntRenderString(gTheme->fonts[0], leftX + 38, rowY + 1,
                            ALIGN_NONE, 152, 24, settingsNames[i],
                            highlighted ? MAIZO_TEXT : MAIZO_MUTED);
        }

        maizoDrawPanel(rightX, 120, rightWidth, panelHeight, 0);
        fntRenderString(gTheme->fonts[0], rightX + 18, 127, ALIGN_NONE,
                        rightWidth - 36, 26, settingsQuery[0] ? "RESULTADOS DA PESQUISA" : settingsNames[settingsCategory],
                        MAIZO_TEXT);
        fntRenderString(gTheme->fonts[0], rightX + 18, 151, ALIGN_NONE,
                        rightWidth - 36, 22,
                        settingsQuery[0] ? settingsQuery : maizoSettingsDescriptions[settingsCategory],
                        MAIZO_MUTED);
        maizoDrawSeparator(rightX + 18, 178, rightWidth - 36);

        for (it = mainMenu; it; it = it->next, pos++)
            if (it == mainMenuCurrent)
                active = pos;
        first = active >= visibleRows ? active - visibleRows + 1 : 0;

        pos = 0;
        y = 188;
        for (it = mainMenu; it; it = it->next, pos++) {
            const char *value = NULL;
            int focus;
            if (pos < first || pos >= first + visibleRows)
                continue;
            focus = it == mainMenuCurrent && settingsFocus;
            if (it->item.id == MENU_PROMPT_STYLE) {
                static const char *const promptNames[] = {"PLAYSTATION", "XBOX", "SWITCH"};
                value = promptNames[maizoPromptStyle()];
            } else if (it->item.id == MENU_VIEW_LAYOUT) {
                static const char *const layouts[] = {"CARROSSEL", "GRADE", "LISTA"};
                value = layouts[menuDashboardLayout()];
            } else if (it->item.id == MENU_SOURCE_FILTER) {
                int source = 0;
                configGetInt(configGetByType(CONFIG_OPL), "maizo_source_filter", &source);
                value = sourceNames[source >= 0 && source < MAIZO_SOURCE_COUNT ? source : 0];
            } else if (it->item.id == MENU_ANIMATIONS) {
                static const char *const anim[] = {
                    "DESLIGADO", "SUAVE", "COMPLETA"};
                value = anim[maizoAnimationMode()];
            } else if (it->item.id == MENU_COVER_MODE) {
                int enabled = 1;
                configGetInt(configGetByType(CONFIG_OPL),
                             "maizo_cover_mode", &enabled);
                value = enabled ? "SIM" : "NÃO";
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
                            "Nenhuma opção encontrada", MAIZO_MUTED);
        }
#ifndef PADEMU
        if (settingsCategory == 1)
            fntRenderString(gTheme->fonts[0], rightX + 18, 363,
                            ALIGN_NONE, 335, 25,
                            "PadEmu nao incluido nesta compilacao.",
                            MAIZO_WARNING);
#endif
        maizoSettingsFooter(settingsQuery[0] ? "VOLTAR: LIMPAR BUSCA" : "SELECT: PESQUISAR");
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
        if (getKeyOn(KEY_SELECT)) {
            char query[32];
            snprintf(query, sizeof(query), "%s", settingsQuery);
            if (guiShowKeyboard(query, sizeof(query))) {
                snprintf(settingsQuery, sizeof(settingsQuery), "%s", query);
                settingsFocus = 1;
                menuInitMainMenu();
            }
            return;
        }
        if (settingsQuery[0] && getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
            settingsQuery[0] = 0;
            settingsFocus = 0;
            menuInitMainMenu();
            return;
        }
        if (mainMenuCurrent && !settingsQuery[0])
            settingsRemember[settingsCategory] = mainMenuCurrent->item.id + 1;
        if (getKeyOn(KEY_L1) || getKeyOn(KEY_R1)) {
            settingsQuery[0] = 0;
            settingsCategory = (settingsCategory +
                                (getKeyOn(KEY_R1) ? 1 : MAIZO_SETTING_CATEGORIES - 1)) %
                               MAIZO_SETTING_CATEGORIES;
            settingsFocus = 0;
            menuInitMainMenu();
            return;
        }
        if (getKeyOn(KEY_LEFT)) {
            settingsQuery[0] = 0;
            menuInitMainMenu();
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
            dashboardRefresh();
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
        if (thmGetGuiValue() == 0) {
            settingsRemember[settingsCategory] = id + 1;
            settingsBreadcrumb = settingsNames[settingsItemCategory(id)];
        }

        sfxPlay(SFX_CONFIRM);

        if (id == MENU_ITEM_INFO) {
            if (selected_item && selected_item->item->current)
                selected_item->item->execSquare(selected_item->item);
        } else if (id == MENU_PROMPT_STYLE) {
            if (menuCheckParentalLock() == 0)
                guiShowPromptStyle();
        } else if (id == MENU_VIEW_LAYOUT) {
            const char *layouts[] = {"Carrossel", "Grade", "Lista", NULL};
            struct UIItem ui[] = {
                {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"Visualização da biblioteca", -1}}},
                {UI_SPLITTER},
                {UI_ENUM, 9010, 1, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 0, layouts}}},
                {UI_BREAK},
                {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},
                {UI_TERMINATOR}};
            int layout = menuDashboardLayout();
            diaSetInt(ui, 9010, layout);
            if (diaExecuteDialog(ui, -1, 1, NULL) == UIID_BTN_OK) {
                diaGetInt(ui, 9010, &layout);
                configSetInt(configGetByType(CONFIG_OPL), "maizo_layout", layout);
                saveConfig(CONFIG_OPL, 0);
            }
        } else if (id == MENU_RESET_APPEARANCE) {
            if (menuCheckParentalLock() == 0 && guiMsgBox("Restaurar o estilo da biblioteca? Jogos, controles e rede serão preservados.", 1, NULL)) {
                config_set_t *cfg = configGetByType(CONFIG_OPL);
                configSetInt(cfg, "maizo_layout", 0);
                configSetInt(cfg, "maizo_animation_mode", 2);
                configSetInt(cfg, "maizo_animations", 1);
                configSetInt(cfg, "maizo_cover_mode", 1);
                configSetInt(cfg, "maizo_sort_mode", 0);
                menuDashboardInvalidate();
                saveConfig(CONFIG_OPL, 0);
            }
        } else if (id == MENU_QUICK_SETUP) {
            if (menuCheckParentalLock() == 0) {
                struct UIItem setup[] = {
                    {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"Bem-vindo ao MAIZO", -1}}},
                    {UI_SPLITTER},
                    {UI_BUTTON, 9020, 1, 1, -1, 0, 0, {.label = {"1. Escolher fontes de jogos", -1}}},
                    {UI_BREAK},
                    {UI_BUTTON, 9021, 1, 1, -1, 0, 0, {.label = {"2. Ajustar vídeo e margens", -1}}},
                    {UI_BREAK},
                    {UI_BUTTON, 9022, 1, 1, -1, 0, 0, {.label = {"3. Configurar rede SMB", -1}}},
                    {UI_BREAK},
                    {UI_BUTTON, 9023, 1, 1, -1, 0, 0, {.label = {"4. Configurar controles", -1}}},
                    {UI_BREAK},
                    {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {"Concluir", -1}}},
                    {UI_TERMINATOR}};
                int result = -1;
                do {
                    result = diaExecuteDialog(setup, result, 1, NULL);
                    if (result == 9020)
                        guiShowStorageConfig();
                    else if (result == 9021)
                        guiShowVideoConfig();
                    else if (result == 9022)
                        guiShowNetConfig();
                    else if (result == 9023)
                        guiShowControllerConfig();
                } while (result >= 9020 && result <= 9023);
            }
        } else if (id == MENU_SMB_STATUS) {
            char status[192];
            int connected = menuMaizoSourceAvailable(MAIZO_SOURCE_SMB);
            snprintf(status, sizeof(status), "SMB: %s | código %d | porta %d | pasta %.31s", connected ? "disponível" : "não conectado", gNetworkStartup, gPCPort, gPCShareName);
            guiMsgBox(status, 0, NULL);
            if (menuCheckParentalLock() == 0 && guiMsgBox("Abrir configuração de rede para conectar ou corrigir?", 1, NULL))
                guiShowNetConfig();
        } else if (id == MENU_PAD_HELP) {
#ifdef PADEMU
            guiMsgBox("USB e Bluetooth ativos no menu, sem iniciar jogo. A emulação nos jogos é independente. Segure L1+R1+START no controle original para resgate temporário; solte para voltar.", 0, NULL);
            guiGameShowUsbDiagnostics();
#endif
        } else if (id == MENU_SOURCE_FILTER || id == MENU_LIBRARY_SORT || id == MENU_COVER_MODE || id == MENU_ANIMATIONS) {
            const char *sortNames[] = {"Nome", "Plataforma", "Origem", NULL};
            const char *animationNames[] = {"Desligadas", "Suaves", "Completas", NULL};
            const char *coverNames[] = {"Ocultar", "Mostrar", NULL};
            const char **names = id == MENU_LIBRARY_SORT ? sortNames : id == MENU_ANIMATIONS ? animationNames :
                                                                   id == MENU_COVER_MODE     ? coverNames :
                                                                                               (const char **)sourceNames;
            const char *key = id == MENU_LIBRARY_SORT ? "maizo_sort_mode" : id == MENU_ANIMATIONS ? "maizo_animation_mode" :
                                                                        id == MENU_COVER_MODE     ? "maizo_cover_mode" :
                                                                                                    "maizo_source_filter";
            int value = id == MENU_COVER_MODE ? 1 : id == MENU_ANIMATIONS ? maizoAnimationMode() :
                                                                            0;
            int max = id == MENU_SOURCE_FILTER ? MAIZO_SOURCE_COUNT : id == MENU_COVER_MODE ? 2 :
                                                                                              3;
            struct UIItem choices[] = {
                {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {submenuItemGetText(&mainMenuCurrent->item), -1}}},
                {UI_SPLITTER},
                {UI_ENUM, 9001, 1, 1, -1, 0, 0, {.intvalue = {0, 0, 0, 0, names}}},
                {UI_BREAK},
                {UI_OK, 0, 1, 1, -1, 0, 0, {.label = {NULL, _STR_OK}}},
                {UI_TERMINATOR}};
            if (menuCheckParentalLock() == 0) {
                configGetInt(configGetByType(CONFIG_OPL), key, &value);
                diaSetInt(choices, 9001, value >= 0 && value < max ? value : 0);
                if (diaExecuteDialog(choices, -1, 1, NULL) == UIID_BTN_OK) {
                    diaGetInt(choices, 9001, &value);
                    configSetInt(configGetByType(CONFIG_OPL), key, value);
                    if (id == MENU_ANIMATIONS)
                        configSetInt(configGetByType(CONFIG_OPL), "maizo_animations", value != 0);
                    saveConfig(CONFIG_OPL, 1);
                    if (id == MENU_LIBRARY_SORT)
                        menuDashboardInvalidate();
                }
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
        } else if (id == MENU_DIAG_SYSTEM) {
            guiShowSupremeDiagnostics();
#ifdef PADEMU
        } else if (id == MENU_PADEMU) {
            if (menuCheckParentalLock() == 0)
                guiGameShowPadEmuConfig(1);
        } else if (id == MENU_INPUT_MAPPING) {
            if (menuCheckParentalLock() == 0)
                guiShowInputMapping();
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
        } else if (id == MENU_STORAGE_PERFORMANCE) {
            if (menuCheckParentalLock() == 0)
                guiShowStoragePerformance();
        } else if (id == MENU_GAME_PREFERENCES) {
            if (menuCheckParentalLock() == 0)
                guiShowGamePreferences();
        } else if (id == MENU_FILE_MANAGER) {
            if (menuCheckParentalLock() == 0)
                maizoFileManagerOpen();
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
                guiShowVideoConfig();
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

        settingsBreadcrumb = NULL;
        // so the exit press wont propagate twice
        readPads();
    }

    if (getKeyOn(KEY_START) || getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        // Check if there is anything to show the user, at all.
        if (thmGetGuiValue() == 0 || gAPPStartMode || gETHStartMode || gBDMStartMode || gHDDStartMode || gPS1StartMode) {
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
    settingsBreadcrumb = NULL;
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
                            dashboardError ? "Memória insuficiente para a biblioteca" : dashboard.query[0] ? "Nenhum resultado para a pesquisa" :
                                                                                                             "Nenhum item nesta aba",
                            MAIZO_MUTED);
            fntRenderString(gTheme->fonts[0], 320, 290, ALIGN_CENTER, 560, 48,
                            "START: configuração | SELECT: atualizar | R3: pesquisar", MAIZO_MUTED);
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
        detailReturn = 0;
        source->execTriangle(source);
    } else if (result == 1003) {
        detailFocus = detailReturn = 0;
        source->execSquare(source);
    } else if (result == 1004) {
        dashboardToggleFavorite();
    } else if (result == 1005) {
        source->refresh(source);
    }
}

static void dashboardHandleInput(void)
{
    if (getKeyOn(KEY_R3)) {
        char query[32];
        snprintf(query, sizeof(query), "%s", dashboard.query);
        if (guiShowKeyboard(query, sizeof(query))) {
            guiLock();
            snprintf(dashboard.query, sizeof(dashboard.query), "%s", query);
            if (!maizoLibraryFilter(&dashboard, dashboard.tab))
                dashboardError = 1;
            else {
                dashboardError = 0;
                dashboardSelect();
            }
            guiUnlock();
        }
        return;
    }
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
        int layout = menuDashboardLayout();
        carouselDirection = pressLeft ? -1 : 1;
        if (layout == 1 && (getKeyPressed(KEY_UP) || getKeyPressed(KEY_DOWN))) {
            for (int col = 0; col < 4; col++)
                maizoLibraryMove(&dashboard, carouselDirection);
        } else
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
        detailFocus = detailReturn = 0;
        selected_item->item->execSquare(selected_item->item);
    } else if (hasItem && getKeyOn(KEY_TRIANGLE)) {
        dashboardShowContext();
    } else if (hasItem && getKeyOn(gSelectButton)) {
        if (gSelectButton == KEY_CIRCLE)
            selected_item->item->execCircle(selected_item->item);
        else
            selected_item->item->execCross(selected_item->item);
    } else if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        if (dashboard.query[0] || dashboard.tab != MAIZO_ALL) {
            guiLock();
            int tab = dashboard.query[0] ? dashboard.tab : MAIZO_ALL;
            dashboard.query[0] = 0;
            maizoLibraryFilter(&dashboard, tab);
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
    if (!selected_item || !selected_item->item || !selected_item->item->userdata) {
        detailFocus = detailReturn = 0;
        guiSwitchScreen(GUI_SCREEN_MAIN);
        return;
    }
    item_list_t *list = selected_item->item->userdata;

    if (thmGetGuiValue() == 0) {
        submenu_list_t *current = selected_item->item->current;
        if (!current || !selected_item->item->visible) {
            detailFocus = detailReturn = 0;
            guiSwitchScreen(GUI_SCREEN_MAIN);
            return;
        }
        const char *identity = list->itemGetStartup ? list->itemGetStartup(list, current->item.id) : "";
        int mode = list->mode;
        int h = gTheme->usedHeight;
        const int coverX = 32, coverY = 126, coverW = 150, coverH = 203;
        const char *platform = mode == APP_MODE ? "Homebrew" : mode == PS1_MODE ? "PlayStation 1" :
                                                                                  "PlayStation 2";
        int canConfigure = !(list->flags & MODE_FLAG_NO_COMPAT) || (mode == APP_MODE && gEnableWrite);
        int buttons = canConfigure ? 2 : 1;
        if (detailFocus >= buttons)
            detailFocus = 0;
        rmDrawRect(0, 0, 640, h, MAIZO_BLACK);
        maizoDrawBrand();
        fntRenderString(gTheme->fonts[0], 608, 20, ALIGN_RIGHT, 180, 26, "DETALHES DO JOGO", MAIZO_MUTED);
        fntRenderString(gTheme->fonts[0], 32, 66, ALIGN_NONE, 576, 43, submenuItemGetText(&current->item), MAIZO_TEXT);
        maizoDrawPanel(coverX, coverY, coverW, coverH, 0);
        GSTEXTURE *cover = thmSupremeDetailArt(list, current->item.id, 0);
        if (cover && cover->Mem && cover->Width && cover->Height) {
            int ch = coverH - 10, cw = rmWideScale(ch * cover->Width / cover->Height);
            if (cw > coverW - 10) {
                ch = ch * (coverW - 10) / cw;
                cw = coverW - 10;
            }
            rmDrawPixmap(cover, coverX + coverW / 2, coverY + coverH / 2, ALIGN_CENTER, cw, ch, SCALING_NONE, gDefaultCol);
        } else {
            maizoDrawIcon(mode == APP_MODE ? MAIZO_ICON_APPS : mode == PS1_MODE ? MAIZO_ICON_PS1 :
                                                                                  MAIZO_ICON_PS2,
                          coverX + 52, coverY + 64, 48, MAIZO_BRIGHT);
            fntRenderString(gTheme->fonts[0], coverX + coverW / 2, coverY + 126, ALIGN_CENTER, coverW - 14, 24, "Sem capa", MAIZO_MUTED);
        }
        fntRenderString(gTheme->fonts[0], 198, 126, ALIGN_NONE, 410, 25, platform, MAIZO_BRIGHT);
        char metadata[160];
        snprintf(metadata, sizeof(metadata), "%s  |  %s", identity ? identity : "", sourceNames[dashboardSource(list)]);
        fntRenderString(gTheme->fonts[0], 198, 155, ALIGN_NONE, 410, 24, metadata, MAIZO_MUTED);
        _menuRequestConfig();
        char genre[64] = {0}, players[32] = {0}, year[24] = {0};
        if (itemConfig && configRequestOwner == selected_item->item && itemConfigId == current->item.id) {
            configGetStrCopy(itemConfig, "#Genre", genre, sizeof(genre));
            configGetStrCopy(itemConfig, "#Players", players, sizeof(players));
            configGetStrCopy(itemConfig, "#Release", year, sizeof(year));
        }
        snprintf(metadata, sizeof(metadata), "%s%s%s%s%s", genre, genre[0] && players[0] ? " | " : "", players, (genre[0] || players[0]) && year[0] ? " | " : "", year);
        if (metadata[0])
            fntRenderString(gTheme->fonts[0], 198, 182, ALIGN_NONE, 410, 24, metadata, MAIZO_MUTED);
        for (int slot = 1; slot <= 2; slot++) {
            const int x = 198 + (slot - 1) * 208, y = 213, w = 200, sh = 116;
            maizoDrawPanel(x, y, w, sh, 0);
            GSTEXTURE *shot = thmSupremeDetailArt(list, current->item.id, slot);
            if (shot && shot->Mem && shot->Width && shot->Height) {
                int ch = sh - 8, cw = rmWideScale(ch * shot->Width / shot->Height);
                if (cw > w - 8) {
                    ch = ch * (w - 8) / cw;
                    cw = w - 8;
                }
                rmDrawPixmap(shot, x + w / 2, y + sh / 2, ALIGN_CENTER, cw, ch, SCALING_NONE, gDefaultCol);
            } else {
                maizoDrawIcon(MAIZO_ICON_VIDEO, x + 85, y + 27, 30, MAIZO_DISABLED);
                fntRenderString(gTheme->fonts[0], x + w / 2, y + 67, ALIGN_CENTER, w - 12, 22, slot == 1 ? "Sem imagem" : "Sem segunda imagem", MAIZO_MUTED);
            }
        }
        for (int i = 0; i < buttons; i++) {
            int x = 32 + i * 292;
            maizoDrawPanel(x, h - 129, canConfigure ? 280 : 576, 51, i == detailFocus);
            maizoDrawIcon(i == 0 ? MAIZO_ICON_PLAY : MAIZO_ICON_SETTINGS, x + 13, h - 116, 25, MAIZO_BRIGHT);
            fntRenderString(gTheme->fonts[0], x + 48, h - 114, ALIGN_NONE, 225, 28, i == 0 ? "Iniciar" : "Configurar jogo", MAIZO_TEXT);
        }
        rmDrawRect(0, h - 62, 640, 62, MAIZO_SURFACE);
        maizoDrawPromptHint(gSelectButton == KEY_CIRCLE ? CIRCLE_ICON : CROSS_ICON, 32, h - 38, "Selecionar");
        maizoDrawPromptHint(gSelectButton == KEY_CIRCLE ? CROSS_ICON : CIRCLE_ICON, 202, h - 38, "Voltar");
        if (canConfigure)
            maizoDrawPromptHint(TRIANGLE_ICON, 355, h - 38, "Configurar");
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
    if (thmGetGuiValue() == 0) {
        if (!selected_item || !selected_item->item->current)
            return;
        item_list_t *support = selected_item->item->userdata;
        int canConfigure = !(support->flags & MODE_FLAG_NO_COMPAT) || (support->mode == APP_MODE && gEnableWrite);
        if (canConfigure && (getKeyOn(KEY_LEFT) || getKeyOn(KEY_RIGHT)))
            detailFocus ^= 1;
        if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
            detailFocus = detailReturn = 0;
            guiSwitchScreen(GUI_SCREEN_MAIN);
        } else if (canConfigure && (getKeyOn(KEY_TRIANGLE) || (detailFocus && getKeyOn(gSelectButton)))) {
            detailReturn = 1;
            selected_item->item->execTriangle(selected_item->item);
        } else if (getKeyOn(gSelectButton) && !detailFocus) {
            if (gSelectButton == KEY_CIRCLE)
                selected_item->item->execCircle(selected_item->item);
            else
                selected_item->item->execCross(selected_item->item);
        }
        return;
    }
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
        int panelHeight = gTheme->usedHeight - 178;
        int rows = (panelHeight - 76) / 38;
        if (rows < 1)
            rows = 1;
        const int rx = 236, rw = 382;
        const char *title = submenuItemGetText(&selected_item->item->current->item);
        if (!gameMenuCurrent)
            gameMenuCurrent = gameMenu;
        for (it = gameMenu; it; it = it->next, index++)
            if (it == gameMenuCurrent)
                active = index;
        first = active >= rows ? active - rows + 1 : 0;
        if (first > index - rows && index > rows)
            first = index - rows;
        rmDrawRect(0, 0, 640, gTheme->usedHeight, MAIZO_BLACK);
        maizoDrawHeader(MAIZO_SETTINGS);
        rmDrawRect(0, 70, 640, 1, MAIZO_DIVIDER);

        maizoDrawPanel(25, 106, 200, panelHeight, 0);
        GSTEXTURE *cover = thmSupremeDetailArt(selected_item->item->userdata, selected_item->item->current->item.id, 0);
        if (cover && cover->Mem && cover->Width && cover->Height) {
            int h = 108, w = rmWideScale(h * cover->Width / cover->Height);
            if (w > 152) {
                h = h * 152 / w;
                w = 152;
            }
            rmDrawPixmap(cover, 125, 185, ALIGN_CENTER, w, h, SCALING_NONE, gDefaultCol);
        } else
            maizoDrawIcon(MAIZO_ICON_PS2, 100, 147, 42, MAIZO_BRIGHT);
        fntRenderString(gTheme->fonts[0], 125, 246, ALIGN_CENTER,
                        164, 25, title, MAIZO_TEXT);
        maizoDrawSeparator(41, 275, 162);
        fntRenderString(gTheme->fonts[0], 41, 289, ALIGN_NONE,
                        170, 28, "CONFIGURACAO", MAIZO_MUTED);
        fntRenderString(gTheme->fonts[0], 41, 322, ALIGN_NONE,
                        170, 58, gameConfigSource(), MAIZO_BRIGHT);

        maizoDrawPanel(rx, 106, rw, panelHeight, 0);
        maizoDrawIcon(MAIZO_ICON_SETTINGS, rx + 13, 117, 26, MAIZO_BRIGHT);
        fntRenderString(gTheme->fonts[0], rx + 48, 118,
                        ALIGN_NONE, rw - 62, 29,
                        "OPCOES DO JOGO", MAIZO_TEXT);
        maizoDrawSeparator(rx + 13, 149, rw - 26);
        index = 0;
        for (it = gameMenu; it; it = it->next, index++) {
            int focused;
            if (index < first || index >= first + rows)
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
        if (index > rows) {
            char position[32];
            snprintf(position, sizeof(position), "%d / %d", active + 1, index);
            fntRenderString(gTheme->fonts[0], rx + rw - 18, 106 + panelHeight - 22,
                            ALIGN_RIGHT, 100, 20, position, MAIZO_MUTED);
        }
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

        settingsBreadcrumb = "POR JOGO";
        if (menuID == GAME_COMPAT_SETTINGS) {
            guiGameShowCompatConfig(selected_item->item->current->item.id, selected_item->item->userdata, itemConfig);
        } else if (menuID == GAME_CHEAT_SETTINGS) {
            if (thmGetGuiValue() == 0)
                guiGameShowDatabaseCheats(selected_item->item->userdata, selected_item->item->current->item.id, itemConfig);
            else
                guiGameShowCheatConfig();
        } else if (menuID == GAME_CHEAT_ADVANCED) {
            guiGameShowCheatConfig();
        } else if (menuID == GAME_GSM_SETTINGS) {
            guiGameShowGSConfig();
        } else if (menuID == GAME_VMC_SETTINGS) {
            guiGameShowVMCMenu(selected_item->item->current->item.id, selected_item->item->userdata);
#ifdef PADEMU
        } else if (menuID == GAME_PADEMU_SETTINGS) {
            guiGameShowPadEmuConfig(0);

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
        guiSwitchScreen(detailReturn ? GUI_SCREEN_INFO : GUI_SCREEN_MAIN);
        detailReturn = 0;
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
                              MAIZO_ICON_DELETE :
                              MAIZO_ICON_FILE,
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
