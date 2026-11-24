# Beta-82 black screen: diagnostic branch B (known-good EE core)

Hardware report: OPNPS2LD-v1.2.0-Beta-39-e75bc88.ELF boots games;
OPNPS2LD-v1.2.0-Beta-82-855d948.ELF black-screens when launching
all games even with PadEmu, GSM and IGR OFF.

This experimental branch starts at Beta-82 (855d948), but restores
**exactly the six game-launch / EE-core files** from tested Beta-39:
- ee_core/Makefile
- ee_core/include/coreconfig.h
- ee_core/src/main.c
- ee_core/src/gsm_engine.S
- ee_core/src/padhook.c
- src/system.c

The rest of Beta-82 remains untouched: store, updated gamepad drivers
and front-end are retained. This is an A/B isolation build, NOT an
evidence-backed final fix.

Impact:
- MAIZO In-Game PRO overlay and its 480p>480i/AA1/Bilinear/Dither
  experiments remain in the repository but are *not linked*, so
  choosing the old MAIZO In-Game PRO setting has no runtime effect.
- Original OPL IGR is restored.
- Original GSM drawing and game-launch core are restored.
- Other Beta-82 features remain at the newer version.

First test a single same-known-good game with GSM, PadEmu, IGR OFF.
If this boots but the simpler experimental branch A (legacy IGR
without GSM/core rollback) does not, inspect the changed GS assembler
or additional EE-core work beyond the overlay, not only stack size.
If even branch B fails while Beta-39 works, examine the Store/TLS
front-end additions, input changes or packaging as next suspects.

DO NOT merge as a final fix until tested on physical PS2.
