"""Static integration guard; runtime hardware validation remains separate."""
from pathlib import Path


def read(name: str) -> str:
    return Path(name).read_text(encoding="utf-8")


def require(name: str, value: str) -> None:
    assert value, f"Missing functional integration: {name}"


menu = read("src/menusys.c")
gui = read("src/gui.c")
dia = read("src/dia.c")
art = read("src/themes.c")
ui = read("src/maizo_ui.c")
library = read("src/maizo_library.c")
driver = read("modules/ds34usb/iop/ds34usb.c")
license_text = read("assets/maizo/tabler/LICENSE")

assert "MIT License" in license_text
assert "maizo_tabler_strokes.h" in ui
assert "maizoDrawHeader(dashboard.tab)" in menu
assert "MAIZO_SETTING_CATEGORIES 10" in menu
assert "maizo_sort_mode" in menu and "maizoLibrarySortMode" in menu
assert "maizo_cover_mode" in menu and "maizo_cover_mode" in art
assert "maizo_animation_mode" in ui
assert "guiFrameId - focusStartFrame" in menu
assert "cachePrefetchTexture" in art
assert "thmRenderSupremeMain" in menu
assert "rmDrawPixmap(cover" in art
assert "hid_stream_thread" in read("modules/ds34usb/iop/ds34usb.c")
assert "hid_stream_thread" in read("modules/pademu/ds34usb.c")
assert "maizoPademuSetSlot" in read("src/guigame.c")
assert "saveConfig(CONFIG_GAME, 0)" in read("src/guigame.c")
assert "maizoSio2SecondCommand" in read("modules/pademu/pademu.c")
assert "MAIZO_BLACK" in gui and "SUPREME" in gui
assert "maizoDrawPanel" in dia and "maizoDrawBrand" in dia
assert "maizoDrawPanel" in menu
assert "maizoSettingsFooter" in menu
assert "guiGameShowUsbDiagnostics" in menu
assert "ds34usb_get_usb_diag" in read("src/guigame.c")
assert "maizohid_report_length" in driver
assert "maizo_usb_stage" in driver
assert "original" not in read("assets/maizo/tabler/README.md").lower().replace("original logo", "")
required_callbacks = (
    "guiShowUIConfig", "guiShowAudioConfig", "guiShowControllerConfig",
    "guiShowStorageConfig", "guiShowNetConfig", "guiShowNetCompatUpdate",
    "guiGameShowBluetoothManager", "guiGameShowPadEmuConfig",
    "guiShowParentalLockConfig", "guiShowSupremeDiagnostics",
    "guiGameShowCompatConfig", "guiGameShowGSConfig",
    "guiGameShowCheatConfig", "guiGameShowVMCMenu",
    "guiGameShowOSDLanguageConfig",
    "guiGameSaveConfig", "guiGameTestSettings", "menuDeleteGame",
    "menuRenameGame", "menuCheckParentalLock",
)
for callback in required_callbacks:
    assert callback in menu, f"Lost callback: {callback}"
assert "previous->owner == entry->owner && previous->item == entry->item" in library
print("MAIZO Supreme: menu wiring, licensed glyphs, HID, and settings contract passed")

# One shared MAIZO renderer for original dialog models and callbacks.
assert "maizoDialogLayout" in dia
assert "maizoSettingsFooter" in menu
assert "maizoSettingsActionIcon" in menu
assert '"SELECIONADO"' not in art
assert "rmDrawOverlayPixmap(caseBox" not in art
assert "rmDrawPixmap(caseBox" not in art
assert "ds34usb_get_menu_state" in read("src/pad.c")
assert "DS34USB_MENU_STATE" in driver
assert "maizoMenuPadMask" in read("src/pad.c")
assert "maizoMenuPadMask" in read("tests/maizo_menu_input_test.c")

# Regression: an all-zero SIF RPC buffer must never synthesize DOWN/START
# or suppress real PS2 Port 1 / Port 2 controls.
menu_input = read("include/maizo_menu_input.h")
menu_driver = read("modules/ds34usb/iop/ds34usb.c")
pad_input = read("src/pad.c")
assert "MAIZO_MENU_SLOTS         2" in menu_input
assert "MAIZO_MENU_MAGIC0" in menu_input
assert "maizoMenuPadValidMask" in menu_input
assert "maizoMenuPadNeutralMask" in menu_input
assert "hid_report_ready[port]" in menu_driver
assert "snapshot[11] |= 1u << port" in menu_driver
assert "maizoUsbArmed &= valid" in pad_input
assert "maizoPhysicalRescueMode" in pad_input
assert "PAD_L1 | PAD_R1 | PAD_START" in pad_input
assert "maizoIsPhysicalRescueMode" in gui
assert "GERENCIAR APLICATIVO" in menu
assert "OPCOES DO JOGO" in menu
assert "CONFIRMAR VIDEO" in gui
assert "REMOVER CONFIGURACOES" in gui
assert "Nenhum cheat encontrado." in gui

# Supreme Storage > file manager and wLaunchELF-inspired native browser.
assert "maizoFileManagerOpen" in menu
assert "MENU_FILE_MANAGER" in menu
fm = read("src/maizo_filemgr.c")
assert "fileXioDopen" in fm
assert "fileXioDread" in fm
assert "fileXioMkdir" in fm
assert "fileXioRename" in fm
assert "fm_copy_tree" in fm
assert "fm_remove_tree" in fm
assert "LoadELFFromFileWithPartition" in fm
assert "ethGetObject(1)" in fm
assert "gNetworkStartup == 0" in fm
assert "gPCPort" in fm and "gPCShareName" in fm
assert "fm_exists(dest)" in fm
assert "maizo_filemgr.o" in read("Makefile")

assert "maizoUsbCenters" in read("src/pad.c")
assert "rmDrawOverlayPixmap(caseBox" not in read("src/themes.c")
assert "snapshot[12] |= 1u << port" in read("modules/ds34usb/iop/ds34usb.c")
assert "READY:%02X SLOT:%02X" in read("src/guigame.c")

# Flat cover only, plus shared keyboard/USB mapping.
assert "caseBox" not in art
assert "rmDrawPixmap(cover" in art
assert 'badgeX - 47, 32, current->title' not in art
assert "DS34,XB360,USB,KEYBOARD" in read("src/guigame.c")
assert "maizoPademuAssignPorts" in read("src/guigame.c")
assert "maizoPademuPortChoice" in read("src/guigame.c")
assert "maizoShowMapping" in gui
assert "maizoDecodeKeyboard" in menu_driver
assert "maizoDecodeKeyboard" in read("modules/pademu/ds34usb.c")
assert "MaizoInputProfiles" in read("ee_core/src/iopmgr.c")
assert "maizoRemapHid" in menu_driver
assert "maizoRemapHid" in read("modules/pademu/ds34usb.c")

# Menu USB has an independent SIF client and an always-available pump.
assert "menuusb" in read("modules/ds34usb/ee/libds34usb.c")
assert "hid_stream_pump();" in menu_driver
assert "ds34usb_reinit_ports(0x03)" in read("src/system.c")
startup = read("src/opl.c").split("static void init(void)\n{", 1)[1].split("static void deferredInit", 1)[0]
assert "while (!padStatus)" not in startup
assert "maizoReadUsbSnapshot" in pad_input
assert "rpc_transaction" in read("modules/ds34usb/ee/libds34usb.c")
assert '"CREDITOS"' in menu
assert "Instagram.com/Maicon_Nasciment0" in gui

# Native card backend, direct source preferences, embedded Brazilian locale.
assert "mcOpen(" in fm and "mcGetDir(" in fm and "mcSync(" in fm
assert "mcFlush(" in fm and "fm_paste(pane)" in fm
assert "maizo_show_usb" in gui and "maizo_show_usb" in menu
assert "maizo_home_tab" in gui and "maizo_home_tab" in menu
assert "GAME_PADMACRO_SETTINGS, _STR_PADMACROCONFIG" not in menu
assert "config->PadMacroSettings = 0" in read("src/system.c")
assert "Portuguese_BR.yml" in read("Makefile")
