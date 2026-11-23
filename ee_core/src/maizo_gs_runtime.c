/*
 * GS runtime shared state. All policy helpers are static inline, because
 * the protected EE-core ram84 region is already near its fixed boundary.
 * Parser code is emitted ONLY by a cooperating renderer-specific adapter.
 */
#include "maizo_gs_runtime.h"
#include "maizo_gs_probe.h"

volatile unsigned int mzRuntimeCapabilities;
volatile unsigned int mzRuntimeModes;
unsigned int mzRuntimeFailures;
unsigned int mzRuntimeAccepted;
unsigned int mzRuntimeConsecutiveFailures;
mz_gs_gif_state_t mzRuntimeState;
mz_gs_dma_metrics_t mzRuntimeMetrics;

/* Read-only live activity sampling shared with the paused IGR screen. */
mz_gs_probe_t mzGsProbe;
