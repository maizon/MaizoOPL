/*
 * GS runtime shared state. All policy helpers are static inline, because
 * the protected EE-core ram84 region is already near its fixed boundary.
 * Parser code is emitted ONLY by a cooperating renderer-specific adapter.
 */
#include "maizo_gs_runtime.h"
#include "maizo_gs_probe.h"

mz_gs_shared_t mzGsResident __attribute__((aligned(16),used)) = {
    .magic0=MZ_GS_SHARED_MAGIC0,.magic1=MZ_GS_SHARED_MAGIC1,
    .abi=MZ_GS_SHARED_ABI,.bytes=sizeof(mz_gs_shared_t)
};

/* Read-only live activity sampling shared with the paused IGR screen. */
mz_gs_probe_t mzGsProbe;
