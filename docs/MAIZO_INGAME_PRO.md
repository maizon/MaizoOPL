# MAIZO In-Game PRO (experimental and opt-in)

A new **small EE in-game menu** is available when MAIZO In-Game PRO is
enabled in Settings → Games **and the specific PS2 game has GSM active**.
Its hotkey is the **existing OPL IGR combo**:
L1 + L2 + R1 + R2 + Start + Select.

When a compatible framebuffer is known, this combo pauses EE game threads
and displays a small 160×126 panel drawn with a compact bitmap font directly
into the game's framebuffer via GS/VIF1. No full-screen framebuffer,
textures, dynamic allocation, filesystem or continuous refresh loop.
RiptOPL/RIPPS2/ORBIT are credited for the low-level GS transfer technique.

## Categories and actual behavior

- **Imagem e video:** small, bounded GS DISPLAY geometry edits (image
  position X/Y and visible display width/height), one expanded preset,
  original restore, and GSM FIELD_fix (distinct from true Deflicker).
  Edits affect the active session while GSM interception is running.
  The units are device/GSM display parameters, NOT guaranteed arbitrary
  pixel-perfect percentages or resampling.
- **Controles:** status/instructions only in this first version. Do not
  change PadEmu's IOP HID modules while a game is running without a safe IPC.
- **Jogo e Cheats:** instructions/status only. Existing PS2RD cheats are
  enabled at launch as configured; on/off individual live cheats are NOT
  wired into the engine by this implementation. Existing IGS shortcut still
  works on IGS-enabled builds, not automatically on all builds.
- **Sistema:** resume, exit to existing IGR path, power off (two presses).

The menu is **disabled by default** because PS2 games can use GS modes and
VIF/DMA paths incompatible with an overlay. With GSM off, the normal OPL
IGR shortcut remains unchanged. When a safe frame cannot be determined,
the handler refuses to draw and falls back to the original IGR reset.

## Safety and limits

- Hardware tests are still required; the successful cross-compile and host
  state-machine tests are NOT a substitute for real-console validation.
- No driver mappings, cheat engine, network streaming or USB IO in the
  resident in-game code; no ongoing work while the menu is closed.
- A game may repaint the panel's pixels after RESUME. The overlay does
  not reserve 80–320 KiB just to save a copy of the underlying picture.
- Some video formats or games that use DMA MFIFO cannot show the overlay.
- A crash or wrong video mode can still occur; use per-game IGR Mode 6
  to disable hooks in an incompatible title and disable this feature
  globally if needed.
- **Not implemented yet:** universal Deflicker, arbitrary stretch, saved
  in-game presets across boots, live remapping/reconnection, live cheat
  toggling, and screenshot from the menu itself. No false success reports.
- This work cannot claim universal or 90% compatibility without test
  results from actual PS2 hardware.
