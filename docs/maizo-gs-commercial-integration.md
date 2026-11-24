# Commercial renderer integration gate

No unmodified commercial game has an automatic GS adapter in this branch yet.
Use the hardware fixture first, then collect the main executable from a legally
owned game. Run:

    python3 scripts/maizo_gs_scan_elf.py SLUS_XXX.XX -o candidate.json

The report is read-only and includes the executable SHA-256, virtual address,
exact opcode and nearby bytes. Literal GIF/VIF1 MMIO writes are candidates,
not proof of a DMA submission function. The scanner intentionally misses
indirect address tables, overlays, computed addresses and VU-generated
XGKICK packets. It does not install patches.

Each game revision requires a verified pre-submit boundary, original-buffer
ownership, cache sync, scratch lifetime, and a proven frame-end callback.
Do not enable a feature merely because a candidate was found. Validate
PATH1 separately using VU microprogram analysis and staging data.
Measure individual features against baseline, not against the number of
candidate addresses. Unsupported games count as not supported in the
commercial compatibility denominator.

The 80% target is an acceptance criterion, not an estimate: at least 80%
of a predeclared representative commercial-game sample must pass each feature,
with no regressions in baseline boot or input. The hardware fixture is excluded
from this denominator.

The source-available adapter now interposes the actual PS2SDK GIF chain submission as well as normal DMA, with an explicit one-shot range ownership declaration. The hardware fixture uses Triangle to switch PATH3 between normal and actual CNT/REFE split chains. This is an executable integration, not evidence of a hook in stripped commercial games. Scanline submission additionally refuses masked/paused GIF and active VIF1. No additional resident EE scratch was allocated.

Offline binary installation now has a real exact-revision JAL redirection tool (`scripts/maizo_gs_install_elf.py`), tested on synthetic executable ELF. It accepts a separately linked renderer adapter and explicitly reviewed cave/site manifest, verifies both SHA-256 hashes and the original call/delay slot, and writes only a new copy. It is not an automatic adapter generator and does not ship any verified commercial profile. Zero-filled space is not proof of ownership; review is mandatory. This provides a bounded route from a verified game-specific adapter to a patched executable without blind boot-time RAM modifications.
