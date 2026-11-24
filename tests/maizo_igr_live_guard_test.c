#include <assert.h>
#include "../ee_core/include/maizo_igr_live_guard.h"
int main(void)
{
    assert(mzLivePathOK(0,0,0,0,0,0,0,0x4000,14));
    assert(!mzLivePathOK(8,0,0,0,0,0,0,0x4000,14));
    assert(!mzLivePathOK(0,0x10000,0,0,0,0,0,0x4000,14));
    assert(!mzLivePathOK(0,0,0x100,0,0,0,0,0x4000,14));
    assert(!mzLivePathOK(0,0,0,0x100,0,0,0,0x4000,14));
    assert(!mzLivePathOK(0,0,0,0,4,0,0,0x4000,14));
    assert(!mzLivePathOK(0,0,0,0,0,4,0,0x4000,14));
    assert(!mzLivePathOK(0,0,0,0,0,0x100,0,0x4000,14));
    assert(!mzLivePathOK(0,0,0,0,0,0,2,0x4000,14));
    assert(!mzLivePathOK(0,0,0,0,0,0,0,0,14));
    assert(!mzLivePathOK(0,0,0,0,0,0,0,0x4000,17));
    return 0;
}
