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
