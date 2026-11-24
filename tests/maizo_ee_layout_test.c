#include <assert.h>
#include "maizo_ee_layout.h"
int main(void)
{
    assert(MZ_EE_EXP_TOP-MZ_EE_LEGACY_TOP==18432u);
    assert(mzEeArenaFits(MZ_EE_EXP_TOP,MZ_EE_LOW_LIMIT,210u*1024u));
    assert(!mzEeArenaFits(MZ_EE_EXP_TOP,MZ_EE_LOW_LIMIT,210u*1024u+64u));
    assert(mzEeArenaFits(MZ_EE_LEGACY_TOP,MZ_EE_LOW_LIMIT,228u*1024u));
    assert(!mzEeArenaFits(MZ_EE_LOW_LIMIT,MZ_EE_EXP_TOP,1u));
    assert(!mzEeArenaFits(MZ_EE_EXP_TOP,MZ_EE_LOW_LIMIT,0xffffffffu));
    return 0;
}
