# Maizo OPL — PS1/VCD tab through POPStarter

The dedicated **PS1 Games** page is based on the ELM/POPStarter behavior
from Jay-Jay-OPL/OPL-Daily-Builds (Academic Free License 3.0).
It scans `mass0:POPS/` to `mass4:POPS/` for `*.VCD` files. PS1 page is set
to Auto by default; press Select to refresh if a USB drive is reconnected.

## Installation — no per-game configuration

USB (FAT32):
```text
POPS/
  POPSTARTER.ELF
  POPS_IOX.PAK
  Game One.VCD
  SLUS_123.45.Game Two.VCD
```

**Choose a title in the PS1 tab and press X.** No conf_apps.cfg,
title.cfg, renamed XX.*.ELF, or manually entered game path is required.
The external runtime is selected from the same mass unit as the VCD;
the OPL Daily Builds ELF handoff synthesizes the
`massN:POPS/XX.<basename>.ELF` argv[0] used by POPStarter.

The ZIP uploaded with the request **does not contain POPSTARTER.ELF**
and **does not contain POPS_IOX.PAK**. It contains utility IRX modules and
network configuration. The menu will give a specific error for missing
runtime files; they cannot be created from the ZIP and are not bundled
or redistributed by this repository.

## Scope / limits

- Integrated PS1 tab, direct click-to-launch USB/FAT32 `*.VCD` titles.
- Game file names need not begin with SLES_/SLUS_; ID-prefixed files
  can still use artwork metadata. Standard existing PS2 and USB HID paths
  remain intact.
- Current automatic scanner covers USB mass0..mass4. SMB/APA and raw
  `BIN/CUE` are out of scope in this integration.
- `POPS_IOX.PAK` is proprietary and must be supplied by the user from
  legally acquired files. POPStarter is a separately installed runtime.
- Launch success and real-console compatibility have not been confirmed.
- Based on Jay-Jay-OPL OPL-Daily-Builds, retaining the existing
  Maizo OPL USB HID / Bluetooth code.

## Artwork and known limits

The PS1 browser uses OPL's existing PNG texture loader directly, searching
`mass0:ART/` through `mass4:ART/`. A serial-prefixed VCD uses its serial
as the artwork key; plain filenames use a stable 8-digit hash, avoiding
collisions between different PS1 titles. The existing OPL cover display
settings and themes apply.

**Important:** This integration does **not** embed Sony POPS runtime or
POPStarter. Neither can be treated as a redistributable dependency without
checking its actual license and provenance. A true `POPS/*.VCD`-only
installation is **not yet supported**. PS1 USB exFAT operation is conditional
on POPStarter's compatible runtime drivers; OPL's own BDM exFAT support
does not by itself provide that functionality. Launch and controls still
require testing on actual PS2 hardware.
