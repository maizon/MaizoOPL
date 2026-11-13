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
assert "clock() - focusStartClock" in menu
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
