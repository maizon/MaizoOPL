# MAIZO SUPREME — Unified UX + menu input (2026-11-14)

## New default Supreme presentation

The dashboard retains the existing motion profile and OPL ART cache. The
selected item's **real title** replaces the old "SELECIONADO" label. The
original OPL case sprite is drawn **behind** the personalized front cover, not
over it; its artwork respects aspect ratio and a fixed 3D case front window.
Platform, source and library count are separate, factual dashboard fields.

The START menu keeps nine icon categories, callbacks and persisted settings,
but displays them in a new two-column MAIZO layout. All ordinary dialogs
(including game compatibility, GSM, cheats, VMC, networking, video, audio,
PadEmu, Bluetooth, diagnostics, OSD, parental lock, rename and delete
workflows) use the Supreme row-based UIItem renderer with contextual focus,
toggles, field values and scrolling. No legacy setting ID or save callback
is replaced merely to change visuals. External OPL themes retain their
original renderer.

## Controller path

Physical DS2 input remains handled by libpad. The dashboard now performs a
single **IOP RPC** to collect all four logical USB slots, regardless of how
many physical controllers libpad successfully opened. Each USB slot publishes
digital state and analog LX/LY, and short USB-HID press latches are consumed
once. The existing Bluetooth driver is polled on its two supported slots.
The game runtime uses its existing PadEmu / SIO2 path and is not changed by
the EE dashboard renderer.

The original OPL software functions and game/menu action callbacks remain
source-compatible. Compiling is **not** proof that every USB controller,
specific 2P game, VMC profile or multitap works on physical PS2 hardware.

## Physical validation checklist

1. Boot with the PS2 controller unplugged; verify USB HID can navigate
   dashboard (START, select/back, L1/R1 and arrows).
2. Verify P1, P2, a two-device USB receiver and menu return after gameplay.
3. Validate quick taps and simultaneous D-pad/face button combinations.
4. Confirm dashboard, settings, options and info return without freeze.
5. Verify global and per-game PadEmu values remain after saving and reboot.
6. Test box alignment on CRT with square 140x200 covers, wide PNGs and
   missing/slow network ART.
7. Measure FPS in 480i and 576i; compile success cannot establish 60 FPS.
8. Exercise VMC, POPStarter, GSM, SMB and connected controllers in games.

## CI

Format check, HID/unit tests, four feature/PadEmu variants, main ELF build
and debug variants run through GitHub Actions.
