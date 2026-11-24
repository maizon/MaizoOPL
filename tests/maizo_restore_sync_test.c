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
    assert(mzrDiscQuiet(MZR_CD_IDLE));
    assert(!mzrDiscQuiet(MZR_CD_BUSY)&&!mzrDiscQuiet(MZR_CD_UNKNOWN));
    assert(!mzrDiscQuiet(1)&&!mzrDiscQuiet(0)&&!mzrDiscQuiet(UINT32_MAX));
    return 0;
}
