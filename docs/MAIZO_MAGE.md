# MAIZO Adaptive Graphics Engine (MAGE), first integration

## Actual scope

The offline resolver selects an independent, reviewed patch for each option:
exact game > engine version > engine family > shared signature > original.
Every candidate must list the exact SHA-256 of the executable. A name string
is only a discovery hint. A shared signature is reusable after validating
each executable, not a license to patch an unreviewed game. Static changes
are boot-only; only a separately reviewed, prelinked adapter can attach to
the existing In-Game AA1/BILINEAR/DITHER/SCANLINES controls.

The RenderWare client helper uses the existing BILINEAR runtime mode to
transform an audited world's filter enum, preserving the original mip family,
protecting UI/unknown passes and requiring known mip levels. The trilinear
helper is available for an audited integration, not a new In-Game control.
A cooperating renderer can select engine vs GIF per feature using the route
helper; it must suppress duplicate GIF processing for an engine-handled
feature. The helper does not install a commercial hook. Public enum reference:
https://github.com/aap/librw/blob/master/src/rwobjects.h

Existing native deflicker/sharp and all Correction B, Setup Dash, IGR and
PadEmu code remain untouched. No new EE-core allocations. No automatic
on-console ELF patch installation yet. No commercial signatures are shipped:
the database intentionally starts empty. No unsupported effect is advertised
as applied. Color, resolution, supersampling, IGR boundary and the remaining
effects require their own reviewed implementations; a database feature name
alone is not an implementation.

## Usage

    python3 scripts/maizo_mage.py GAME.ELF profiles/mage/database.json
    python3 scripts/maizo_mage.py GAME.ELF profiles/mage/database.json --features bilinear,dither --output GAME-PATCHED.ELF

Without --output this is a dry run. Original ELF is never overwritten.
Use an owned game executable. A patched ELF is not a replacement ISO; use an
appropriate legal rebuild workflow and confirm boot separately.

A reviewed profile contains id, scope (game/engine_version/engine_family/
signature), reviewed=true, elf_sha256 (list of exact revision hashes), and
features mapping names to ordered candidate lists. Static candidates have
method=static and sites with before/after arrays of 4..64 exact uint32 words
(hex strings or integers). Address is optional; without it an executable
signature must be unique. Each changed word is checked before modifying.
Adapter candidates use method=adapter, payload (relative binary path),
live_features and manifest from maizo_gs_install_elf.py. This is a reviewed
database, not a sandbox for untrusted patches. Review code ownership, ABI,
delay slots, control flow, overlays, cache and renderer state for each entry.
Reject nearby overlapping edits conservatively (256 bytes), except identical
adapter installations shared by several options. Never apply an unverified
fallback. Live adapter installation status is not proof it attached at runtime.

## Validation

    python3 tests/maizo_mage_test.py
    cc -std=c99 -Wall -Wextra -Werror -Iee_core/include tests/maizo_mage_rw_test.c -o /tmp/mage-rw && /tmp/mage-rw

Host tests use synthetic ELF and enum cases. First commercial milestones:
legally obtained GTA III/Vice City/San Andreas, BLACK and Burnout 3 ELF
revisions; review driver Sky call sites, capture world and HUD pass behavior,
then construct signed-off database entries and compare baseline hardware.
No commercial compatibility percentage has been measured.

## Live filter integration (ABI 2)

All drawing filter choices are made in the existing In-Game PRO menu, while
the title runs. No pre-launch filter dialog or boot preference is used.
An ELF transition clears capabilities and choices. Attached verified clients
read current modes on each eligible submission, not only on startup.
Trilinear is texture mode 3; the existing GIF parser preserves mip levels
and falls back to bilinear for non-mipmapped textures. The RenderWare client
uses the same menu setting. Unknown passes remain original.

The live mzMageRwSendFallback API accepts masks for engine-handled and GIF
supported features for a verified pass, stages only the remaining eligible
GIF filters and forwards exactly one submission without changing global
menu settings. It never guesses the engine or attaches a commercial hook.
Three rejected packets disable the registration until ELF reset.
ABI 1 clients must be rebuilt; incompatible clients fail closed.

Native deflicker and sharp remain independent scanout controls. Returning
to JOGO restores original *future* packets, not retroactively the GS state.
The offline resolver remains an optional developer tool to prepare audited
hooks, not a requirement for the player's in-game workflow. No commercial
profile has been verified yet. No claim of 80% coverage is made.

## Commercial Sky state-9 experiment: supplied ELF evidence

Eight supplied ELF files (seven IDs) share the same six-case filter switch.
The Warriors SHA1 matches the independently documented state-setter at
0x48e250: https://ryubara.github.io/coney/research/ps2-render/
In all eight images the nested state-9 switch clears TEX1 bits 5..8,
selects constants 0, 0x60, 0x80, 0x120, 0xc0, 0x160 and emits A+D
register 0x14 (TEX1_1). The two San Andreas revisions have the same
local fingerprint. No proprietary ELF is committed. Exact SHA-256s and
local address fingerprints are in profiles/mage/sky_evidence.json.

In the experimental In-Game menu only, a validated Sky table is a second
route when no renderer adapter claims textures. It remaps existing mip
cases 3..6; the original nearest/linear non-mip cases are never touched.
JOGO restores the original six-case mapping. No change at ELF load,
no continuous DMA polling or executable code edits. No new VRAM.
Validation uses main ELF path/game ID, module-arena bounds, a 75-word
local rolling fingerprint, state-9 dispatch and all six original/current
table pointers. This is not a cryptographic runtime identity check.
If any check fails, refuse to write.

This is experimental, not tested on a console. It only affects future
calls to this state setter, not all textures or custom VU pipelines.
It assumes a request for mip filtering corresponds to valid mip levels;
it cannot classify all HUD/world materials, and it does not restore
previously submitted GS state until the game updates it. A renderer
adapter must not force texture filtering concurrently with this route.
The unmatched four supplied executables have no enabled Sky profile.
AA1, dithering and other filters are not implemented by this table.
The 80% target remains a hardware acceptance criterion, not achieved.

## Collection scan and build recovery

A read-only structural scanner is available:
`python3 scripts/maizo_mage_sky_discover.py GAME.ELF`.
It resolves the LUI/ADDIU table addresses, checks the six TEX1 selector
cases, state-9 dispatch and RwDevice setter record. It never installs
a match. Scanned 57 supplied ELF files (56 distinct SHA-256); ten files,
nine game IDs, matched. The Godfather SLES_539.67 and Silent Hill:
Shattered Memories SLUS_218.99 are new matches. 46 files did not match;
one (Up SLES_555.22) failed the conservative load-range check. Not
matching does NOT demonstrate a non-RenderWare engine. Only one specific
driver pattern is tested. Short bootstrap ELFs may not contain the renderer.

The supplied Coney runtime captures show The Warriors uses TEX1 MMIN=5
for streamed world sectors, but most textures have MXL=0; other passes
already use bilinear. Consequently a mip-table remap is not a general
visual upgrade. The driver uses two circuits in interlaced FIELD mode
and 32-bit buffers; dithering is off. Do not force DTHE for this format,
do not assume unused VRAM, and do not confuse disabling a scanout blend
with improving texture detail. References: Coney PS2 render driver,
sections GS memory/GS register shadows; Rendering: anatomy of a frame,
sections State shared by every pass/The world; supplied librw
ps2raster.cpp calcTEX1.

The preceding commercial Sky commit failed the protected stack linker
assertion and had host kernel.h / EE libc string linkage errors.
The previous successful experimental build had only 504 bytes above the
8 KiB minimum. This change restores the previous default resident path
and isolates Sky in maizo_mage_sky.c behind the developer-only
MAIZO_MAGE_SKY_EXPERIMENTAL=1 make flag. It does not weaken the linker
guard. The Sky route is NOT included in standard In-Game builds; the
optional research build still needs a verified memory solution and
successful link before it is suitable for any hardware test.
The separate source uses the EE core's _strncmp/_strcmp helpers.
No player pre-launch selection is introduced; the eventual route
remains an in-game feature.

## Compact live route and evidence expansion

The frontend selects one six-word record from the nine validated Sky IDs;
the resident core no longer embeds the ID/address database. Loaded memory
is checked against the actual game arena: a low module arena entirely below
1 MiB does not limit normal game code to that address. A high arena is
excluded conservatively, and an arena crossing 1 MiB disables Sky.
No change to the 8 KiB stack guard or protected linker boundary.
The ordinary In-Game build retains the non-Sky route; CI sky-compact
explicitly compiles the commercial route and uploads an ELF only if
the original guard passes. No boot-time graphics changes.

Static ELF analysis resolves the RwDevice standard-function table
(28 pairs) for all nine IDs. Camera begin/end and show-raster addresses
are leads for a cooperative frame-boundary hook, not installed hooks.
ELF MIPS reginfo GP + signed TEX1 shadow-store displacement identifies
eight IDs with a candidate TEX1 shadow; The Warriors address agrees
with the supplied Coney report. Silent Hill lacks that reginfo value.
These are stored under static_leads in the evidence JSON. Never write
shadows blindly: setters may overwrite them and a cached value does
not prove a new GS packet was issued.

ABI 3 adds independent mip LOD in the existing In-Game GS status page.
A verified packet adapter adjusts signed TEX1.K by +/-0.5 levels only
for an existing mip filter, nonzero MXL, LCM=0. No missing mip levels
are invented; the caller must classify eligible passes. Texture and LOD
masks can be delegated separately to engine or GIF. Unhooked games are
not affected; ABI 2 clients must rebuild. Menu updates use one feature
loop and current packed modes. The guarded FIELD sharp path accepts an
existing fixed-blend same-image pair with different DBY and matching
display geometry; FRAME smooth is unchanged. Native scanout may cancel
the override on subsequent incompatible writes.
No universal AA1, DTHE, frame hook or arbitrary restoration claim.

## Fine In-Game geometry

New sessions use one DISPLAY clock/line per GSM width, height and position
step; R1 multiplies the logical step by four. These are output-window
adjustments, not arbitrary fractional scaling. Native position uses one
output clock/line, native size arrows crop one source pixel/row and return
towards the original size, never increase MAGH/MAGV. X on a native size
row explicitly toggles an integer zoom for that axis; the existing
all-circuit geometry validation rejects unsupported requests. The original
crop preset remains separate. No extra VRAM, DMA or timing changes.

Profile V3 uses the same packed size and transport as V2 with a separate
version tag. V1/V2 profiles keep their previous numeric meaning, including
integer magnification and GSM clock increments; restore switches to fine
mode. A failed native edit also restores the previous geometry version.
New GSM window range is intentionally limited to +/-32 logical units
rather than the old larger coarse range. Native cropping does not resample.
No change to the protected EE memory boundary or stack requirement.

## MAEC optional effects and captured runtime evidence

Four supplied PCSX2 2.8.2 captures each include a GS dump and a gameplay
save state. The 32 MiB EE RAM snapshots confirm the complete original
Sky fingerprints, dispatch entries and texture tables for SA, BLACK,
Burnout 3 and The Warriors. A dump's transfer index is not a measured
physical GIF/DMA path. Counts are observed register writes, not FPS or
compatibility. See profiles/mage/capture_evidence.json.

MAEC exposes 14 effect rows on two pages: Motion Blur, Screen Blur,
Bloom, Glow, Depth of Field, Film Grain, Fog, Heat Haze, Color Filter,
Lens Flare, Screen Distortion, Shadows, Particles and Reflections.
Each has Original/Off and optionally Reduced if a verified adapter
registers it. Unsupported entries are N/D and immutable. The ABI 4
mailbox separates draw filters and effects. Cooperative effect adapters
must identify an optional renderer-owned pass *before* work is emitted,
never skip a generic GIF packet based on alpha, texture or sprite shape.
Restoration uses the original current engine strength, not a stale
value from a previous scene. Critical cleanup and gameplay state remain
the engine's responsibility. The adapter passes certified_optional=0
for UI, unknown or dependent work. New ELF or repeated malformed draw
submissions clear capabilities and requests. Existing clients must
rebuild for ABI 4.

The Sky opt-in build contains a real, experimental Fog control for
four exact revisions. State-14's 14-word body matched the supplied
ELF and gameplay RAM in each case. The validated conditional branch
is replaced only with the unconditional branch to the *existing*
fog-off path, while paused, and the data/instruction caches are flushed.
Original restores the original instruction. No arbitrary NOP, global
blend disable, GS state guess, or frame packet suppression. This affects
future state-14 calls; it cannot promise immediate removal of cached
or custom fog or change visibility-dependent gameplay safely in every
scene. Other effects need title-specific optional-pass hooks; candidate
feedback passes are not certified effect IDs. No commercial adapter
for the other 13 effects is claimed.

The guarded experimental core now borrows 10 KiB from the low module
arena (0x9B800 -> 0x9E000); the upper limit stays 0xD0000 and the
legacy arena remains unchanged. The frontend uses the same constant,
checks the complete aligned module footprint against 200 KiB, and
falls back to the unchanged legacy EE core if it does not fit. The
linker and independent map check still require 8 KiB stack; clients
use the matching shared layout bound. No growth into ramD0. CI tests
and physical boot with USB/SMB/HDD/VMC/PadEmu are required before
declaring the experimental variant stable.
