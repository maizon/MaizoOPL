#include <assert.h>
#include "../ee_core/include/maizo_restore_guard.h"
int main(void)
{
    assert(mzrRingIdle(0,0,0,0,0,0,0,0));
    assert(!mzrRingIdle(4,0,0,0,0,0,0,0x3f0));
    assert(mzrRingIdle(8,0,0,0,0,0x1000,0x1000,0x3f0));
    assert(mzrRingIdle(12,0,0,0,0,0x1000,0x1000,0x3f0));
    assert(!mzrRingIdle(8,0x100,0,0,0,0x1000,0x1000,0x3f0));
    assert(!mzrRingIdle(12,0,0x100,0,0,0x1000,0x1000,0x3f0));
    assert(!mzrRingIdle(8,0,0,0x100,0,0x1000,0x1000,0x3f0));
    assert(!mzrRingIdle(8,0,0,0,1,0x1000,0x1000,0x3f0));
    assert(!mzrRingIdle(8,0,0,0,0,0x1000,0x1010,0x3f0));
    assert(!mzrRingIdle(8,0,0,0,0,0x1000,0x1000,0));
    assert(mzrTransferReplayable(15,3));
    assert(!mzrTransferReplayable(7,3));
    assert(!mzrTransferReplayable(15,0));
    assert(!mzrTransferReplayable(15,1));
    assert(!mzrTransferReplayable(15,2));
    return 0;
}
