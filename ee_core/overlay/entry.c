#include "maizo_overlay_api.h"
#include "maizo_igrmenu.h"
const mz_ovl_api *mzOvlApi;
mz_gs_shared_t *mzGsShared;
extern u8 _mz_overlay_end[];
static void bind(const mz_ovl_api *api){mzOvlApi=api;mzGsShared=api->shared;}
static int pre(const mz_ovl_api *api,u32 mask){bind(api);return maizoIGR_Preflight(mask);}
static int run(const mz_ovl_api *api,int (*buttons)(u16 *,u32 *),void (*yield)(void),u32 mask)
{bind(api);return maizoIGR_MenuRun(buttons,yield,mask);}
static char *profile(const mz_ovl_api *api){bind(api);return maizoIGR_ProfileArg();}
const mz_ovl_header mzOverlayHeader __attribute__((section(".mzovl.hdr"),used))={
    MZ_OVL_MAGIC,MZ_OVL_ABI,(u32)_mz_overlay_end,(u32)pre,(u32)run,(u32)profile,sizeof(mz_ovl_api)
};
/* No resident libc/gp dependence. */
void *memcpy(void *d,const void *s,size_t n)
{
    u8 *a=d;const u8 *b=s;while(n--)*a++=*b++;return d;
}
void *memset(void *d,int c,size_t n)
{
    u8 *a=d;while(n--)*a++=(u8)c;return d;
}
