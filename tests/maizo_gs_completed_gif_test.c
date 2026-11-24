#include <assert.h>
#include "../ee_core/include/maizo_gs_completed_gif.h"
int main(void)
{
    mz_gs_register_shadow_t s={0};
    uint64_t p[10]={4ull|(1ull<<15)|(1ull<<60),14,11,0x50,22,0x51,33,0x52,3,0x53};
    assert(mzGsObserveCompletedGif(&s,p,sizeof(p)));
    assert(s.value[0x50]==11 && s.value[0x53]==3);
    assert((s.known[2]&0xf0000u)==0xf0000u);
    assert(!mzGsObserveCompletedGif(&s,p,sizeof(p)-16));
    assert(!s.known[2]);
    assert(mzGsObserveCompletedGif(&s,p,sizeof(p)));
    p[0]&=~(1ull<<15);
    assert(!mzGsObserveCompletedGif(&s,p,sizeof(p)));
    assert(!s.known[2]);
    return 0;
}
