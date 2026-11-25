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

## Multiframe evidence and scoped adaptive AA1

The second capture package contains ten original GS streams and ten
save states (five titles, gameplay and menu). Independently checked
89 manifest hashes, 160 uncompressed save-state members and 220,611
GIF tags over 86 VSync records. All eight RenderWare captures matched
Sky signature, dispatch, table and fog code. Tekken 5 is not matched
to this Sky route. All 120 targeted instruction windows matched the
gameplay snapshots, but no packet-exact causal relation is established.

AA1 AUTO (mode 3 in the AA1 field only) now refuses unknown state,
blending, alpha/destination alpha tests, masked depth/color writes,
sprites, fixed UV, texture alpha, invalid color format, ADC and
incomplete primitives. It requires a self-contained opaque PACKED PRE
draw and an existing final original PRIM restore. The adapter still
must certify the packet's world geometry and ownership; there is no
universal commercial hook. Legacy explicit force-on API remains for
reviewed adapters. The in-game cycle now prefers AUTO, OFF, GAME.
No candidate was established in the ten recorded streams under the
strict observed-state prerequisites. This does not prove that no safe
AA1 geometry exists; the frozen initial GS state is not decoded.
No visual quality, FPS or compatibility percentage is claimed.

BLACK (SLUS-21376, 5C891FF1) gains a separate experimental reload-blur
control (shown as BLUR RECARGA in the Screen Blur slot, not global
screen blur). A publicly documented patch by Gabominated was checked
against the supplied exact ELF and both new RAM snapshots. A normalized
nine-word signature and original opcode must match before enabling;
original/disabled replace only the known instruction while paused,
with both caches flushed. No change is made at launch, and other
versions do not advertise this route. The patch has not been run in
PCSX2 or on console in this change.

Source: https://github.com/Gabominated/PCSX2/blob/main/PCSX2%20Patches/SLUS-21376_5C891FF1.pnach

## Native GS fog: engine-independent fallback

MAEC Fog OFF can now be provided by an explicitly registered draw
adapter (MZ_DRAW_FOG), regardless of the renderer's engine. The
adapter must cover and own the relevant command submissions and
must restore original state at appropriate renderer boundaries.
The packet transform clears PRIM/PRMODE.FGE bit 5, including
embedded GIF PRE, PACKED and REGLIST PRIM, without changing alpha,
depth, fog color, vertex payloads, size or DMA topology. All existing
bounded PATH3, PATH2 DIRECT and verified PATH1 staging routes reuse
the transform. Engine-owned Fog takes precedence; masked submissions
can exclude this feature. Original releases the override on subsequent
original packets, not an immediate rollback of persistent GS state.
ABI 5 clients must be rebuilt. No physical DMA trap or universal
commercial installer is introduced. Texture, baked, or custom
postprocess fog is outside native FGE control.

The Sky route now also recognizes five additional exact executable
revisions: Vice City, ObsCure, Bully, The Godfather and Silent Hill:
Shattered Memories. The five were inspected in the supplied ELF
archives, not tested in modified gameplay. Two state-14 branch layouts
are supported with 14- or 16-word normalized hashes, preserving
the original branch displacement. The four previously matched
runtime profiles remain intact. Thus nine Sky profiles have a
candidate reversible Fog patch; only four have matching capture
RAM evidence. Unsupported revisions remain N/D. No effect is
enabled automatically. See profiles/mage/sky_evidence.json.

GS register reference: https://ps2dev.github.io/ps2sdk/gs__gp_8h.html

## Passive GS recorder access scope

The normal (GSM disabled, FIELD inversion disabled) In-Game recorder
now uses BPC.DWE without DRE, and DABM 0x1FFFFF0F instead of 0x1FFFEF0F.
This retains the scanout register observations needed by the menu
without trapping the CSR page or intercepting reads during game boot,
streaming or ordinary rendering. Explicit native FIELD inversion
restores the prior full trap policy when leaving the menu; disabling
it restores the narrow policy. GSM forced modes retain their existing
policy. SetGsCrt uses the same policy selection when rearming.
Source-linked host tests check bit fields and address selection, not
actual R5900 trap behavior. No console reproduction or verified fix
for a particular commercial game is claimed.

Reference: TX79 Core Architecture manual, 4.2.17 and 13.2,
https://lukasz.dk/files/tx79architecture.pdf

## Stability recovery after commercial-game loading reports

Compared the user-confirmed playable Beta-39 (e75bc88), the early
overlay revision e02fd93 (menu appeared, but not confirmed stable),
and 426b062 with 7bb68c4. The exact last stable Justiceiro/Avatar
revision is unknown. Restore the original sleeping IGR priority 127
on every backend, with priority zero only for explicit capture.
A stale input DMA sample no longer invalidates a successful installed
pad hook in PRO mode; the normal ELF transition still handles
reinstallation. Retain the menu's fresh-input arbitration and bounded
capture. This may reduce opening responsiveness in CPU-bound games.

Sky selection at launch now identifies the title only. Live memory
signature verification and saved MAIZO geometry application occur
at the suspended menu boundary, not during startup. The original
user-selected OPL GSM mode is unchanged. The GS exception vector
is flushed before arming its breakpoint. Retain the narrower native
GS recorder and memory guards; there is still a passive write trap.
These changes isolate concrete differences, not a reproduced root
cause for BLACK, GTA, Ben 10 or Justiceiro. Hardware A/B testing
with the same ISO, SMB settings, controller and game options is
required before calling the regression fixed.

## Independent live filter requests (ABI 6)

X cycles OFF / AUTO / FORC (forçado) for drawing, scanout filters and
MAEC effects, even before a compatible route is registered. OFF means
remove MAIZO's override (original game behavior), not disable a game's
own effect. ESP is a pending request; OK means a verified control route,
not proof that a particular scene has been changed. The existing GS
edit counter remains actual packet-edit telemetry, not a compatibility
percentage. LEFT on drawing options retains the former manual
alternative (AA1/dither disabled, nearest, 50% scanlines, performance LOD).

AUTO uses scoped AA1, mip-safe trilinear, dithering, mild scanlines,
quality LOD, and certified reduced/suppressed effects. FORC chooses
bilinear, stronger scanlines and complete certified optional-effect
suppression; AA1, dithering, LOD and native scanout retain the same
mandatory safety rules in both modes. No arbitrary commercial-game
hooks are installed. A pending cooperative request activates only
after a verified adapter registers; native Sky and scanout choices
are checked on the next visibly opened paused menu, never in boot.
An owned Sky patch excludes duplicate cooperative overrides until
verified restoration or ELF reset. Old ABI clients refuse to bind,
rather than assume compatibility with the changed mailbox.

No new per-frame background polling, memory scanning, graphics
breakpoints or module arena expansion. Preserve the recovery revision's
sleeping monitor and guarded capture. This is not evidence that the
commercial-game freezes have been eliminated.

## Native stability isolation (GSM OFF)

With user-selected GSM disabled, no GS debug exception vector or SetGsCrt
hook is installed at boot. The six-button menu chord starts a bounded
capture using existing attempts, then restores the previous comparator
and exception vector before ordinary play resumes unless an explicit
native scanout override remains active. Captured PMODE, SMODE2 and both
registers of each active read circuit must be observed during this
capture. No guessing, stale records or reads from write-only registers.
Partial and quadword stores preserve their actual store instruction and
invalidate the snapshot. This can refuse menu opening in titles that
do not re-emit complete CRTC settings during the capture window.
Legacy explicitly enabled GSM retains its own route. Native capture
is also torn down at a new ELF transition. No new IOP/SMB changes.
This isolates the persistent GS trap, a concrete difference from the
stable baseline; it is not a reproduced root cause for Crash Twinsanity,
BLACK, GTA or other commercial titles. Host assertions are not game tests.

## Request-scoped capture and menu input recovery

The user reports Justiceiro League starts after c91dccd, but the menu
does not open; Ben 10 can open only in low-load scenes and X/O respond
while directional navigation does not. Preserve the no-default-GS-trap
boot policy. Extend the bounded capture opportunity from 32 x 97
horizontal ticks to 180 x 256 ticks (nominally about 3 seconds, plus
scheduler/capture overhead). Seek an idle VIF/GIF boundary before
suspending game threads and revalidate after the IRQ hold. Never force
a busy DMA channel to stop to open the menu.

The menu merges only navigation/action bits from fresh valid sources
instead of picking one entire controller report. Opening/resume chords
must still come from a single controller. Wait for START release, not
only a partially released opening chord. Directional hold repeats;
confirmation and exit do not repeat. Left-stick navigation is derived
from normalized PadEmu reports or a recognized analog libpad report.
Game input reports are untouched. Fix the stale IOP mailbox upper
limit to use the current shared EE arena constant.

The complete verified CRTC snapshot requirement remains: some games
do not rewrite all scanout registers during a request. This change
cannot guarantee menu availability in such titles. No guessing of
write-only registers, no new permanent hook, and no claimed game
reproduction. Host regressions cover simultaneous held buttons,
stick mapping, repeat and split-chord rejection.

## Repeated menu open/close recovery

The previous overlay's VIF1 DMA timeout forcibly cleared CHCR. A
partially consumed DIRECT command can leave VIF waiting for payload
after channel registers have been restored. The GS FIFO route also
previously timed out mid-IMAGE. The recovery removes VIF1 borrowing
from the overlay, uses self-contained 32-pixel GIF packets (10, 12
or 14 QW including headers), and reserves room for an entire packet
before emitting its first tag. No DMA registers, D_STAT events or VIF
commands are written by the overlay. Reject VIF errors and live VU1,
retain the native boot isolation, and tear down temporary GS capture
before restoring graphics IRQ masks. PATH3-masked games may refuse
the menu rather than fall back to unsafe VIF injection.

This fixes identifiable incomplete-packet failure paths, not every
possible cause of a game freeze. Internal GS transfer state
(BITBLTBUF/TRXPOS/TRXREG/TRXDIR) is still modified, with no universal
readback or restoration; games that split transfers across packets
remain a risk. VRAM contents behind the overlay are not saved. A true
fully isolated menu needs verified GS transfer-state ownership and
restoration. Hardware reference: https://psi-rockin.github.io/ps2tek/

## Dual FIFO overlay recovery

Restore a PATH2 option for PATH3-masked games without restoring VIF
DMA borrowing. Send one aligned DIRECT command plus a whole
10/12/14-QW GIF packet via the CPU VIF1 FIFO (maximum 16 QWs).
Only start on an empty, idle, unstalled parser with no queued GIF
producer; no FLUSHA, mask toggling, DMA register changes or VIF
reset. Prefer PATH3 otherwise. Drain VIF before GIF on PATH2.
Preserve native boot isolation, menu controls and filter intent.
VIF CODE and internal GS transfer registers are still changed;
no universal restoration is claimed. Complete CRTC capture,
pipeline boundary, and suspended title input remain required.
Host tests do not establish commercial-game compatibility.
References: https://psi-rockin.github.io/ps2tek/ and PS2SDK VIF1_FIFO.

## IRQ and first-draw isolation audit

Compared RiptOPL's IGR_CheckInputs, upstream OPL's polling worker,
PS2SDK kernel mode helpers and GS readback sequence, gsKit registers,
PS2tek VIF/GIF documentation, and RIPPS2/NULD menu design. Keep the
existing sleeping worker (no new VBLANK handler). Briefly disable EE
interrupts across the pad snapshot and temporary kernel-mode power
and comparator access; preserve prior IE. Edge-latch power status so
an uncleared CDVD condition is not counted twice. PATH3 rejects VIF
stall/interrupt conditions just as PATH2 does. Restore profiles only
after a successful first draw. No DMA borrowing, new permanent GS
trap, or MFIFO override. The previous OFF/Mode 6 legacy-core selection
is covered by a source-linked regression check.

References: https://github.com/NathanNeurotic/RiptOPL/blob/rebuild/main/ee_core/src/padhook.c
https://github.com/ps2dev/ps2sdk/blob/master/ee/kernel/include/kernel.h
https://github.com/ps2dev/ps2sdk/blob/master/ee/debug/src/screenshot.c
https://psi-rockin.github.io/ps2tek/

Framebuffer backup, exact internal GS transfer-state restoration and
MFIFO-safe overlay are not solved by these changes. Do not claim a
hardware compatibility fix until the reported titles pass on console.

## Safe transaction, stage 1 (no freeze analyzer)

Use the existing dual CPU FIFO routes without borrowing or stopping DMA.
Accept inactive MFIFO modes only with a stopped SPR producer and no
unhandled producer or enabled empty IRQ. An active VIF1 MFIFO drain can
use PATH3 only when QWC is zero, the producer is stopped, the tag and
producer addresses coincide, and the empty event is latched and masked.
Active GIF drains remain unsupported. See PS2tek DMAC MFIFO and RiptOPL
igrmWaitFor; no game D_STAT bits are cleared. Check the GS CSR FIFO field
before acquisition and after GIF drain without writing CSR. This is NOT
an exact GS FINISH fence or an internal GS state restore. Stage live cheat
selection in the menu and publish only after a successful resume; reverse
thread acquisition order and preserve prior IRQ enable state. Increase
PRO's private worker stack by 1 KiB without lowering the linker guard.

No analyzer, new always-on IOP service, unsafe VRAM readback or forced
SPU2 operations are added. Full pixel backup/restore, audio restoration,
exact GS internal state and the separately loaded overlay remain open;
these require a bounded IOP mailbox and hardware validation before default
enablement. No 99% claim is supported by host tests alone.

## Restoration laboratory (opt-in build only)

MAIZO_IGR_RESTORE_EXPERIMENTAL=1 adds a separate IOP module for an
on-demand panel buffer (max 322560 bytes), bounded SIF DMA requests
with sequence numbers and resident staging, and SPU2 master volume/
voice pitch hold with conditional restore and an inactivity lease.
No new RPC during pause. The normal PRO build does not include it.
GS readback is restricted to idle PATH3 without MFIFO; no DMA channel
is borrowed. Restore the panel on ordinary resume or menu abort.
A timeout during reverse FIFO transfer remains an experimental risk:
the internal GS transfer state is not readable, and the code cannot
promise exact restoration of arbitrary in-flight GS commands. Audio
AutoDMA and envelope phase are not frozen by pitch hold. The EE overlay
is still resident; separate IRX is the restoration helper, not a
separately loaded EE menu. A fixed-address EE overlay requires a
versioned ABI and verified module-arena reservation, not an arbitrary
game RAM address. No Freeze Analyzer. Hardware testing required.

References: RIPPS2/RiptOPL igrmbox and igrmenu (AFL-3.0), PS2SDK
screenshot.c, PS2tek GS transfers and VIF1 STAT.

### Separate EE menu

The restoration laboratory now also selects
MAIZO_IGR_OVERLAY_EXPERIMENTAL=1. The complete menu, font, private
state and rendering routines link at 0xCA000 in a separate 24 KiB ELF;
the resident core only exposes a typed, versioned ABI and entry gate.
The frontend reserves the overlay range before packing IRXs, validates
the single load segment and ABI, clears BSS, and extends the protected
module wipe boundary to 0xD0000. No extra RAM above the known low arena.
When the footprint does not fit or a title uses special module
placement, select the original legacy core before copying data.
The normal PRO build still uses its resident menu unchanged. The
8 KiB main-stack assertion remains mandatory. Internal write-only GS
transfer register restoration remains unsolved; this is not a
universal hardware save-state implementation.

The IOP lease must never release a target of a delayed EE DMA: uploads
now land in an aligned, permanently resident staging line and are copied
by the IOP worker after a validated sequence. No EE DMA targets the
leased dynamic allocation. The overlay ELF loader also verifies the
file-backed entry point ranges and exact low-arena placement.

### Overlay capacity correction

The de119c0 build linked the resident EE core with 36,408 bytes of
main stack, but the separately linked menu exceeded the 24 KiB region
by 6,336 bytes (30,912 bytes total). Reserve 32 KiB from 0xC8000 to
0xD0000 and compile its code with -Oz. The same layout constant gates
IRX packing and the frontend loader; the ABI is bumped. The module
arena is 8 KiB smaller than before, so configurations that cannot fit
still select the legacy core rather than touching game memory. The
resident main stack minimum remains 8 KiB. CI verifies the actual
ELF segment, file-backed entry addresses and header before upload.

The 7df406d restoration run linked the 32 KiB overlay successfully and
passed the resident main-stack gate (36,408 bytes). Its next failure was
an unresolved memcpy generated by the IOP compiler for the request
structure copy. Import memcpy explicitly from the standard IOP sysclib,
like CDVDMAN. CI now checks the IRX and overlay independently before
the complete frontend build. This is a linker fix, not hardware proof.

### Beta-173 boot handoff regression

Physical report: launch remained at the last Loading Settings screen.
All USB/BDM, SMB and HDD launchers deinitialize the GUI, input, audio,
I/O and configuration before entering sysLaunchLoaderElf. The prior
restoration code could call guiMsgBox in that phase (including ordinary
arena fallback) and read the already cleared configuration. Snapshot
IGR enable during sysMaizoPrepareProfile, validate the embedded overlay
before packing IRXs, and never open a dialog after deinit. If the fixed
overlay cannot fit or validate, omit its optional IRX and select the
embedded conventional PRO core. Only if the ordinary arena cannot fit
or IGR is explicitly off use the legacy core. If even legacy packing
fails, exit rather than returning to a destroyed frontend. Optional IOP
loading bypasses fatal module-loader diagnostics. Actual hardware boot
must be retested; no claim that this is the only possible regression.

### LIVE laboratory (opt-in)
Based on 56b81be. No suspension of title threads, long graphics IRQ hold,
SPU2 pause, restoration IRX, reverse VIF or DMA resets on the menu chord.
Complete PATH3 packets only, with a brief atomic check and enqueue,
bounded slices and alarm sleep. Busy, stale pad or alarm failure closes
without reset. Native scanout only, no MFIFO. Cooperative filter/effect
and existing cheat requests remain; native geometry, direct game code
patches, scanout filters and save/exit are not applied live. Controls
remain shared with the game. Direct drawing into displayed framebuffer
is not an independent VRAM plane; pixels may persist, flicker, or be
redrawn. Idle hardware does not prove internal GS transfer state is safe
to replace. Not a universal or 100% compatible solution. Requires PS2
hardware tests, including Ben 10 during loading. Regular PRO and
Restoration workflows unchanged.

### LIVE native video adapter (second iteration)
Reuses existing native CRTC validation and transformations: geometry,
position/crop/zoom, field inversion, sharp/smooth/deflicker, and native
480i bridge arming. Each requested class is queued for up to 120 sleeping
slices; only recorder-backed, valid scanout and idle hardware may enter
a short IRQ-preserving CRTC transaction. Both circuits must have been
observed before rewriting/restoring a filter. No waits or game thread
suspension inside a transaction; restore GS comparator before IRQs.
Conflicting geometry/filter/bridge requests fail closed and the UI
reports N/D rather than pretending to apply them. Mode changes remain
owned by the original SetGsCrt hook. Existing cooperative drawing and
effect intents remain supported; no unsynchronized executable Sky patch,
no GSM live paint, no forced unsupported effects or profile exit.
The live painter now renders a line once per slice instead of per chunk.
Real hardware validation remains necessary; regular builds are unchanged.

### Restoration priority / mandatory backup transaction

LIVE is no longer built automatically on branch pushes. Its source and
manual workflow remain for reproducibility. Restoration-only changes:

* Ready service gate before pausing; conservative PATH3-only readback
  boundary, two quiet scheduler samples, no active IPU, VIF0, or SPR DMA,
  no pending graphics completions or MFIFO. Partial thread suspension
  is immediately reversed. These gates do not establish that all title
  semaphores or streaming operations are safe to pause.
* IOP accepts contiguous checksummed writes only; EE retains the
  original per-line hashes and validates each returned line. All bytes
  must be received and sealed before audio hush or the first draw.
  No unbacked drawing when PATH2, MFIFO, allocation or handshake fails.
* A sealed buffer is pinned until explicit release or IOP reset, not
  freed by the old inactivity lease; the lease still unmutes audio.
  A poisoned transport can retain the allocation until IOP reset.
  Heartbeat failures close the menu and partial first draws attempt
  rollback. A hardware/transport failure during rollback remains an
  unresolved failure mode; no claim of guaranteed recovery.
* If the Restoration overlay cannot fit, the conventional PRO core
  still boots but its unbacked pause menu is disabled in this build;
  if even that core cannot fit, legacy boot uses mode 6 when PRO was
  requested. Separate conventional build remains unchanged.
* Full internal GS write-only transfer-state tracking, validated
  reverse-transfer DMA and MFIFO restoration, complete SPU2/AutoDMA
  state, safe title-specific thread selection and full VRAM snapshot
  remain open. No FreezeAnalyzer and no arbitrary game-RAM reservation.
  Hardware tests, especially Ben 10 during loading, repeated open/close,
  and low-memory configurations, are release blockers.

### Restoration recovery boundary (follow-up)

Inactive configured MFIFO (VIF1/GIF) is admitted only with both DMA
drains and producer stopped, zero selected QWC, equal ring pointers,
valid ring size and no pending error/graphics event. Active MFIFO is
still rejected, not reset. The new pure guard has host tests. Readback
failures after starting reverse transfer permanently inhibit opening
again in the current overlay. On failed dirty-pixel rollback the IOP
keeps the sealed backup, unmutes, and EE poisons further requests.
Remaining valid lines can be restored after a checksum failure; no
unverified pixels are sent. Boot handshake now checks the actual worker.
SPU2 now changes only static master volumes, not voice pitch/ADSR or
AutoDMA; this is mute/unmute, not a playback-position snapshot. Sweep
volumes are untouched. A partial suspension stops acquiring more threads.
Overlay ABI 3 reflects End(retain). These are bounded mitigations, not a
hardware failure guarantee. The pure TRXDIR replay predicate is a future
cooperative snapshot prerequisite, not actual register restoration.
Full GS write-only register state, active MFIFO, full SPU2 checkpoint,
4 MiB VRAM storage and universal thread classification remain unimplemented.
The panel backup is at most 322560 bytes; IOP has only 2 MiB total RAM.
No speculative allocation in game RAM, no FreezeAnalyzer, no LIVE.

### Cooperative full-VRAM checkpoint (ABI 7 / overlay ABI 4)

The new renderer checkpoint is an opt-in execution route, not automatic
commercial game integration. A renderer allocates and exclusively owns
4 MiB of aligned EE memory, registers after attaching, tracks completed
register writes (128-entry software shadow), and publishes only at a
verified frame/streaming boundary with TRXDIR explicitly deactivated.
It must park every graphics producer until the request is cleared.
The resident waits for this acknowledgement before pausing, and rejects
registered clients that cannot provide it. Full backup uses 512 physical
PSMCT32 pages of 8192 bytes each, with an integrity hash; on resume it
restores those pages and the four known transfer registers. A failed
rollback marks the checkpoint faulted, and the client must retain the
buffer until ELF reset. No guessed game memory is used. Full mode leaves
SPU2/IOP audio streaming untouched rather than forcing a mute without the
panel backup service. Unregistered titles retain the previous guarded
panel-only path. Readback and page mapping require real PS2 validation;
full backup/restore may be slow. Core transition gates explicitly block
menu capture during New_Reset_Iop and sysLoadElf, but do not recognize
arbitrary in-game asset-loading code. Active MFIFO still needs drain;
no universal safe thread classification, complete hidden GS/SPU2 state,
AutoDMA checkpoint, or guaranteed hardware-fault recovery exists here.
No LIVE or FreezeAnalyzer. Tests cover buffer ownership contract,
checkpoint state and source-level integration, not real PS2 hardware.

### Restoration synchronization hardening (commercial titles)
The IOP service now queries an explicitly tagged, read-only CDVDMAN SC
status: high-level I/O semaphore count/waiters and low-level sync flag.
No semaphore acquisition, blocking game RPC, stop/read cancellation, or
storage reset. Check before pause and again after suspension; if not
idle, immediately release threads and retry in the existing bounded
window. This improves the Ben 10 loading boundary but cannot identify
arbitrary game logic or all streaming I/O; continuously busy titles may
not open the menu. Cooperative clients use their declared safe point.
The EE remembers each suspended thread entry/stack/size and will not
resume a reused ID with a different identity or non-suspended state.
This is not universal thread role discovery. SIF permits a bounded late
completion window; IOP may resend the same ACK twice without replaying
its operation. No DMA buffer reuse until completion. Failed line reads
may retry while the transport is still usable. Final graphics drain
must pass before releasing backup, including cooperative full VRAM.
No changes to LIVE, no FreezeAnalyzer. Hidden GS/SPU2 state, active
MFIFO checkpoint, universal 4 MiB storage, universal input isolation and
hardware failure recovery remain open. Hardware tests still required.

### Compact IOP snapshot and completed GIF observer
Panel backup now uses lossless word RLE with raw fallback, packed into
on-demand 4 KiB IOP slabs. No worst-case contiguous allocation. Each
encoded row is decoded and checked before commit, then checked again on
read. EE also verifies the original hash. DMA uses permanent aligned
upload/download staging, not the slabs. Incompressible data may need the
same payload plus slab rounding and metadata. Allocation failure still
prevents drawing. Cooperative renderer clients can observe ordered,
completed GIF A+D writes from decoded PATH1/2/3 streams; invalid or
incomplete streams invalidate the shadow. This is not a universal
commercial hook, does not decode executing VU programs, and does not
capture hidden state. After rollback, wait for two fresh neutral merged
raw pad samples with a finite bound; no modification of title pad buffers.
This reduces input carryover, not total isolation. No LIVE or FreezeAnalyzer.
Active MFIFO, universal thread classification, full GS/VIF/VU/SPU state,
universal 4 MiB storage and hardware recovery remain unresolved.

### SMB and boot stability hardening
The optional restoration IRX no longer sends its handshake from
_start or requires a synchronous PING during game boot. LOADFILE still
loads the optional module normally; the worker advertises permanent
addresses and a tagged protocol version asynchronously, validated for
alignment/range/non-overlap by the first menu request. Its priority is
lowered from 0x30 to 0x50. The existing CDVD status probe now also
checks the actual SMB socket semaphore (including VMC traffic); it
never takes a storage lock or resets TCP. Backup begin/seal and periodic
heartbeat reject observed busy storage. The snapshot verifies bytes
read back from the actual allocated slabs before accepting each row.
After a failed reverse transfer, future preflight refuses capture;
reverse FIFO must drain before changing direction. The thread set is
revalidated after suspension. The final drain precedes backup release;
no redundant later drain can invalidate that decision. These changes
preserve the normal menu features, legacy fallback, PadEmu, and existing
cooperative full VRAM route. Busy titles may defer or refuse an open.
No universal 90% hardware success claim: SMB/USB physical comparisons,
repeated opens and boot tests remain required. No LIVE or FreezeAnalyzer.

### Minimal-overhead stability pass
The 32 KiB overlay is deliberately untouched (previous measured image
32272 bytes). Each panel write queues the permanent upload and request
in one ordered SIF DMA list, with one cache flush and completion wait.
Request metadata and ACK result/sequence are tagged. The original
per-row payload hashes and actual slab verification remain. Before
allocating each slab, check largest free block and total free IOP RAM
with a conservative 64 KiB reserve for other services. This is not a
hard partition; concurrent game allocations can race. A missing or
poisoned worker cancels before GS monitor and capture attempts. One
checked retry on transient EE ResumeThread failure, never on a different
thread identity. No additional worker, no default boot/core changes,
no LIVE or FreezeAnalyzer. Busy/low-memory games can refuse the menu;
universal GS/MFIFO restoration and physical hardware testing remain open.

### Loading-friendly capture and input cleanup
The pre-suspension CDVD/SMB probe now submits at most one SIF request
and polls it across the existing capture scheduler gaps, without a
busy-wait for the IOP at priority zero. Request memory is immutable
until DMA and tagged ACK complete. Old acknowledgements are discarded;
a post-suspension fresh probe is limited to approximately 40 ms, after
which the game is immediately resumed and pending DMA retained until
safe completion. This avoids long speculative pause periods, not all
possible game-specific loading dependencies. Protocol hello version is
3. Require enough free IOP memory for the first slab before reverse
GS transfer, retain the per-slab reserve checks, remove duplicate
temporary RLE decode and duplicate EE row hash. Check actual committed
slabs and original row hashes as before. In Restoration, consume pad
release only once after rollback (not before AND after), use actual
raw button state for release, and age invalid input reports so a dead
controller cannot retain a stale report indefinitely. No new permanent
worker or graphics hooks, no LIVE or FreezeAnalyzer. The normal core
and menu options are unchanged; busy or incompatible games may defer
opening. Physical SMB/USB and repeated close/resume tests remain needed.
