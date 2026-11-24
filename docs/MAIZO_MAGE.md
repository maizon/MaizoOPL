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

## Boot preference integration (ABI 2)

Experimental frontend: Settings > VIDEO > Filtros graficos automaticos.
Requests AA1, texture bilinear/trilinear, dithering, and scanlines before
launch. The existing In-Game switches still control attached adapters.
Every ELF transition clears capabilities and seeds the requested modes.
An adapter receives only the subset of requests it actually supports.
No adapter means original graphics. Three consecutive rejected packets
disable registration until an ELF reset. Clients compiled against ABI 1
must be rebuilt; incompatible ABI markers fail closed.

Trilinear is texture mode 3, with the existing TEX1 parser and RenderWare
world-texture helper. For GIF, only an already mipmapped sampler with
nonzero MXL may change to MMIN=5; no new mip levels, addresses, or K/LOD
values are fabricated. Non-mipmapped textures fall back to bilinear;
reserved MMIN values stay untouched. RenderWare requires the adapter's
proof of existing mipmaps. This does not fix texture upload or filtering
in unhooked commercial binaries.

Native deflicker and sharp remain independent scanout controls. No
commercial adapter or signature is invented. The offline database
remains empty until real ELF evidence is supplied. No claim of 80%
commercial compatibility is made.
