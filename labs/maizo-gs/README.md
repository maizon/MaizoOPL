# MAIZO GS hardware fixture

This is an original homebrew disc, not a commercial title or evidence of universal support.
It draws three copies of a colored triangle and a magnified checkerboard, left to right:
PATH3 GIF DMA, PATH2 VIF1 DIRECT, PATH1 VU1 XGKICK. The last path actually uploads
a complete packet to VU1 memory and starts an assembled microprogram. Every draw
has a GS FINISH fence. The optional post-render overlay uses the existing scanline
packet on the complete 32-bit frame. Square selects the 16-bit framebuffer to
exercise DTHE, and Cross cycles the shared filters when loaded by MaizoOPL.
Open the IGR overlay normally and check the GS AO VIVO counters. Without the
resident ABI all DMA submissions fall back to original packets.

Run the ISO through the experimental loader, not as an APP if testing IGR.
Observe each column with AA1 and bilinear independently; test dithering in
16-bit mode; test scanlines in 32-bit mode. Test toggling off, IGR resume, and
repeated start/exit. A compiled ISO is not a hardware test result.
The locally assigned serial is for this fixture only. Do not use it as a
commercial-game fingerprint.

Build: make -C labs/maizo-gs iso (PS2SDK, dvp-as, mkisofs).
