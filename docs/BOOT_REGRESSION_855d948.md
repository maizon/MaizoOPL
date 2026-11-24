# MAIZO game-launch black screen: Beta-39 vs Beta-82 diagnosis

## Hardware report (not reproduced in CI)
- OPNPS2LD-v1.2.0-Beta-39-e75bc88.ELF: games boot normally.
- OPNPS2LD-v1.2.0-Beta-82-855d948.ELF: **all games hang
  on a black screen on launch** even with PadEmu, GSM and In-Game off.

The compare between the two revisions includes **43 commits** affecting
the EE core, the in-game graphics menu, DS34USB, USB input and a new
BearSSL/MAIZO Store module. Build success does not prove that the boot
path works on physical PS2 hardware.

## Diagnostic recovery branch A: isolate resident In-Game code

This branch starts **exactly** from commit 855d948. It changes ONLY
`ee_core/Makefile` to stop linking the experimental resident GS overlay
and its packet engine into **normal game builds**. This restores the
original stock OPL IGR input path and should recover memory margin
inside the protected 77 KiB EE-core region. A compile-time escape hatch
(`MAIZO_IGR_EXPERIMENTAL=1`) retains the overlay for developer testing.

**Important:** Selecting MAIZO In-Game PRO in the graphical settings has
no effect in the default diagnostic build; do not interpret it as a
functional menu. GSM, PadEmu, Store and input-driver changes from Beta-82
remain compiled, so the test isolates the IGR residency hypothesis.

The build log prints linker-map symbols for the EE-core data/stack area.
A successful cross-compile does **not** establish that the PS2 boots.

## How to interpret a real-console test
1. Keep the working Beta-39 ELF as an emergency fallback.
2. Try diagnostic branch A with GSM, PadEmu and In-Game OFF.
3. Test the **same ISO, same USB/SMB device and identical settings**.
4. **If it boots**: focus investigation on the resident overlay, low
   memory/stack overlap or its launch-time side effects.
5. **If it still black-screens**: this test rules out the linked overlay
   as the sole cause, and the next target is Beta-82's TLS/Store growth,
   input driver changes and other modified code. Do not assume this
   test has fixed the problem without hardware evidence.

Revert or retest through the working Beta-39 binary if no game starts.
Do not merge this test branch into main until hardware results are known.
