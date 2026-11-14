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
assert "MAIZO_SETTING_CATEGORIES 9" in menu
assert "maizo_sort_mode" in menu and "maizoLibrarySortMode" in menu
assert "maizo_cover_mode" in menu and "maizo_cover_mode" in art
assert "maizo_animation_mode" in ui
assert "guiFrameId - focusStartFrame" in menu
assert "cachePrefetchTexture" in art
assert "thmRenderSupremeMain" in menu
assert "CASE_OVERLAY" in art
assert "hid_stream_thread" in read("modules/ds34usb/iop/ds34usb.c")
assert "hid_stream_thread" in read("modules/pademu/ds34usb.c")
assert "maizoPademuSetSlot" in read("src/guigame.c")
assert "saveConfig(CONFIG_GAME, 0)" in read("src/guigame.c")
assert "maizoSio2SecondCommand" in read("modules/pademu/pademu.c")
assert "MAIZO_BLACK" in gui and "SUPREME" in gui
assert "maizoDrawPanel" in dia and "maizoDrawBrand" in dia
assert "maizoDrawRow" in menu
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
    "guiGameShowPadMacroConfig", "guiGameShowOSDLanguageConfig",
    "guiGameSaveConfig", "guiGameTestSettings", "menuDeleteGame",
    "menuRenameGame", "menuCheckParentalLock",
)
for callback in required_callbacks:
    assert callback in menu, f"Lost callback: {callback}"
assert "if (previous->owner == entry->owner)" in library
print("MAIZO Supreme: menu wiring, licensed glyphs, HID, and settings contract passed")

# One shared MAIZO renderer for original dialog models and callbacks.
assert "maizoDialogLayout" in dia
assert "maizoSettingsFooter" in menu
assert "maizoSettingsActionIcon" in menu
assert '"SELECIONADO"' not in art
assert "rmDrawPixmap(caseBox" in art
assert "rmDrawOverlayPixmap(caseBox" not in art
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
