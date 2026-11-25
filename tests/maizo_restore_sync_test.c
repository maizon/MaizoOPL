#include <assert.h>
#include "../ee_core/include/maizo_restore_sync.h"
int main(void)
{
    mzr_thread_identity a={0x100000,0x200000,4096},b=a;
    assert(mzrSameThread(&a,&b));
    b.stack+=16;assert(!mzrSameThread(&a,&b));b=a;
    b.entry+=4;assert(!mzrSameThread(&a,&b));b=a;
    b.size+=16;assert(!mzrSameThread(&a,&b));
    assert(mzrCanResume(8)&&mzrCanResume(12));
    assert(!mzrCanResume(1)&&!mzrCanResume(2)&&!mzrCanResume(4)&&!mzrCanResume(16));
    assert(mzrHelloOK(0x1000,0x2000,32,1280));
    assert(!mzrHelloOK(0x1000,0x1010,32,1280));
    assert(!mzrHelloOK(0x1001,0x2000,32,1280));
    assert(!mzrHelloOK(0,0x2000,32,1280));
    assert(!mzrHelloOK(0x1ffff0,0x2000,32,1280));
    assert(!mzrHelloOK(0x1000,0x200000,32,1280));
    assert(mzrWithin(100,90,10));
    assert(!mzrWithin(101,90,10));
    assert(mzrWithin(5,UINT32_MAX-4u,10));
    assert(!mzrWithin(6,UINT32_MAX-4u,10));
    assert(mzrDiscQuiet(MZR_CD_IDLE));
    assert(!mzrDiscQuiet(MZR_CD_BUSY)&&!mzrDiscQuiet(MZR_CD_UNKNOWN));
    assert(!mzrDiscQuiet(1)&&!mzrDiscQuiet(0)&&!mzrDiscQuiet(UINT32_MAX));
    return 0;
}
