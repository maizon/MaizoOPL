# MAIZO GS GIF drawing filters — technical prototype

This release includes a **real, zero-allocation GIF packet rewriter** in
`ee_core/include/maizo_gs_gif_filter.h`, and host unit tests.

**Status: the transformer works on caller-owned GIF packets, but there is
NO universal game GIF DMA interception yet. These are NOT functioning
in-game ON/OFF switches in the current OPL build.** Existing GSM
Deflicker/Sharp/Smooth functionality is unaffected.

## Technical implementation

- **AA1:** rewrites PRIM/PRMODE bit 7 only for lines/triangles that already
  enable alpha blending; supports GIFtag PRE, PACKED and REGLIST.
- **Bilinear:** rewrites TEX1_1/TEX1_2 MMAG and MMIN, preserving mipmap
  family rather than forcibly discarding level-of-detail settings.
- **Dithering:** rewrites DTHE only when 16-bit FRAME_1/FRAME_2 and DIMX
  matrix were observed. Prevents arbitrary activation in 32-bit modes.
- **GS SCANMSK:** optional experimental even/odd scanline DRAW masking.
  This is NOT a proper scanlines postprocess. It can leave old-frame
  pixels or produce graphical errors; do not expose as a user feature.
- **Safety:** two-pass validation before mutation, maximum 1MiB input,
  no untrusted pointer arithmetic beyond supplied packet length,
  no dynamic allocation, no background thread or resident game hook.
- **Performance:** O(number of GIF packet entries), only when caller
  explicitly submits a mutable GIF packet.

## Why not simply turn these ON globally?

GSM traps a few privileged GS *output* register writes; AA1, TEX1,
DTHE and SCANMSK are **drawing GIF commands**. To apply a filter throughout
a game, its GIF DMA packets (PATH1/PATH2/PATH3, with DMA chains) need to
be intercepted and safely patched *before* GS submission, preserving DMA
ownership, cache consistency, and exact old state for turning OFF.
Games can overwrite these registers many times every frame. Writing
them once in the In-Game menu will generally not remain effective.

This patcher is **the tested transform core**, NOT the missing global
renderer hook. No per-game support or universal PS2 compatibility is
claimed. The current in-game UI continues to say that AA1 and textures
are controlled by the game, and does not expose fake toggles.

## Technical references and credit

PS2 GS User's Manual supplement (AA1), ps2tek (GIF/GS), PCSX2 GSRegs.h
(register identifiers and texture modes), ps2dev/gsKit (dithering).


## Atualização: pré-envio PATH3 e validação extra

Leia [MAIZO_GS_DMA_PREFLIGHT.md](MAIZO_GS_DMA_PREFLIGHT.md). O código
aceita **apenas** pacotes de um chamador cooperativo antes da DMA, não
instala interceptação universal de jogos.
