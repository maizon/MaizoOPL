/* Separate, statically linked EE menu. This range is reserved by the
 * frontend before any IRX data is copied; never borrow game RAM. */
#ifndef MZ_OVERLAY_LAYOUT_H
#define MZ_OVERLAY_LAYOUT_H
#include <stdint.h>
#define MZ_OVL_BASE 0x000c8000u
#define MZ_OVL_END  0x000d0000u
#define MZ_OVL_MAGIC 0x4d5a4f56u
#define MZ_OVL_ABI 3u /* restoration End(retain) ABI */
typedef struct {uint32_t magic,abi,end,preflight,run,profile,api_size;} mz_ovl_header;
static inline int mzOvlHeaderOK(const mz_ovl_header *h)
{
    return h->magic==MZ_OVL_MAGIC && h->abi==MZ_OVL_ABI &&
        h->end>MZ_OVL_BASE+sizeof(*h) && h->end<=MZ_OVL_END &&
        h->preflight>=MZ_OVL_BASE+sizeof(*h) && h->preflight<h->end &&
        h->run>=MZ_OVL_BASE+sizeof(*h) && h->run<h->end &&
        h->profile>=MZ_OVL_BASE+sizeof(*h) && h->profile<h->end &&
        !((h->preflight|h->run|h->profile)&3u);
}
#endif
