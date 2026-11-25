#include <assert.h>
#include <stdint.h>
#include "../modules/iopcore/common/maizo_restore_integrity.h"
int main(void)
{
    unsigned char a[16]={0},b[16]={0};
    assert(mzrHash(a,16)==mzrHash(b,16));
    b[15]=1;assert(mzrHash(a,16)!=mzrHash(b,16));
    assert(mzrAppendOK(0,16,0,32));
    assert(!mzrAppendOK(16,16,0,32));
    assert(!mzrAppendOK(0,16,16,32));
    assert(mzrAppendOK(16,16,16,32));
    assert(!mzrAppendOK(32,16,32,32));
    assert(!mzrAppendOK(UINT32_MAX,16,UINT32_MAX,32));
    assert(!mzrSealable(0,32));
    assert(!mzrSealable(16,32));
    assert(mzrSealable(32,32));
    assert(mzrMayAllocate(MZR_IOP_RESERVE+4096,4096,4096));
    assert(!mzrMayAllocate(MZR_IOP_RESERVE+4095,4096,4096));
    assert(!mzrMayAllocate(MZR_IOP_RESERVE+8192,4095,4096));
    assert(!mzrMayAllocate(0,4096,4096));
    assert(!mzrMayAllocate(UINT32_MAX,UINT32_MAX,0));
    assert(mzrRequestTag(7,0,16,0x1000,123,1)!=mzrRequestTag(7,0,16,0x1000,123,2));
    assert(mzrRequestTag(7,0,16,0x1000,123,1)!=mzrRequestTag(7,16,16,0x1000,123,1));
    assert(mzrAckTag(1,16)!=mzrAckTag(1,0));
    assert(mzrAckTag(1,16)!=mzrAckTag(2,16));
    assert(!mzrSealable(0,0));
    return 0;
}
