#include <assert.h>
#include "../ee_core/include/maizo_igr_live_guard.h"
int main(void)
{
    assert(mzLiveSeenOK(1,0x185,0));
    assert(!mzLiveSeenOK(1,0x185,1));
    assert(mzLiveSeenOK(1,0x785,1));
    assert(mzLiveSeenOK(2,0x605,0));
    assert(!mzLiveSeenOK(3,0x605,0));
    assert(!mzLiveSeenOK(0,0x785,1));
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
