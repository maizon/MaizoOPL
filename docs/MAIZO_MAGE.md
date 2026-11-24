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
