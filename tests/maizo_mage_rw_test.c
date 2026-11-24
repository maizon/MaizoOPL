#include <assert.h>
#include "maizo_mage_rw_client.h"
mz_gs_shared_t *mzGsShared;
int main(void)
{
    mz_gs_shared_t shared={0};
    assert(mzMageRwFilter(1,MZ_GS_FORCE_ON,1,0,0)==2);
    assert(mzMageRwFilter(3,MZ_GS_FORCE_ON,1,1,0)==4);
    assert(mzMageRwFilter(5,MZ_GS_FORCE_ON,1,1,0)==6);
    assert(mzMageRwFilter(3,MZ_GS_FORCE_ON,1,0,1)==3);
    assert(mzMageRwFilter(3,MZ_GS_FORCE_ON,1,1,1)==6);
    assert(mzMageRwFilter(1,MZ_GS_FORCE_ON,0,1,1)==1);
    assert(mzMageRwFilter(6,MZ_GS_FORCE_OFF,1,1,0)==5);
    assert(mzMageDrawRoute(2,2,2,1)==MZ_MAGE_ENGINE);
    assert(mzMageDrawRoute(2,1,2,1)==MZ_MAGE_GIF);
    assert(mzMageDrawRoute(2,2,2,0)==MZ_MAGE_ORIGINAL);
    assert(mzMageRwLiveFilter(1,1,0)==1);
    mzGsShared=&shared;shared.enabled=1;
    assert(mzGsClientAttach(MZ_DRAW_BILINEAR));
    assert(mzGsRuntimeSetMode(MZ_DRAW_BILINEAR,MZ_GS_FORCE_ON));
    assert(mzMageRwLiveFilter(1,1,0)==2);
    assert(mzMageRwLiveFilter(1,0,0)==1);
    assert(mzGsRuntimeSetMode(MZ_DRAW_BILINEAR,MZ_GS_KEEP));
    assert(mzMageRwLiveFilter(1,1,0)==1);
    return 0;
}
