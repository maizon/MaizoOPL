#include <assert.h>
#include "maizo_mage_profiles.h"
int main(void)
{
    mz_sky_record_t r;
    mzSkyChoose("SLUS_209.46",&r);
    assert(r.state==0x00350268u && r.hash==0x7fd09a3bu);
    assert(r.fog==0x0035056cu && r.fog_hash==0x023909aau);
    mzSkyChoose("SLUS_212.69",&r);
    assert(r.state && !r.fog && !r.fog_hash);
    mzSkyChoose("UNKNOWN",&r);
    assert(!r.state && !r.hash);
    assert(mzSkyLimit(0x9b800,0xd0000)==0x02000000);
    assert(mzSkyLimit(0x1700000,0x1800000)==0x1700000);
    assert(!mzSkyLimit(0xf0000,0x110000));
    assert(mzSkyRange(0x350268,212,0x02000000));
    assert(!mzSkyRange(0x1fffffc,8,0x02000000));
    assert(!mzSkyRange(0x350269,4,0x02000000));
    return 0;
}
