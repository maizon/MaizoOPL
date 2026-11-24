#include <assert.h>
#include "maizo_mage_sky.h"
int main(void)
{
    uint32_t table[6],original[6],state=0x350268;
    for(unsigned i=0;i<6;i++)original[i]=table[i]=mzSkyTarget(state,i,0);
    assert(mzSkyTableOK(table,state,0));
    for(unsigned mode=1;mode<=3;mode++){
        for(unsigned i=2;i<6;i++)table[i]=mzSkyTarget(state,i,mode);
        assert(mzSkyTableOK(table,state,mode));
        assert(table[0]==original[0] && table[1]==original[1]);
        assert(table[2]==original[mode==1?3:mode==2?2:5]);
        assert(table[4]==original[mode==1?5:mode==2?4:5]);
        table[2]^=4;assert(!mzSkyTableOK(table,state,mode));
        table[2]^=4;
    }
    for(unsigned i=2;i<6;i++)table[i]=mzSkyTarget(state,i,0);
    assert(mzSkyTableOK(table,state,0));
    assert(mzSkyHash(2166136261u,table,6)==mzSkyHash(2166136261u,original,6));
    return 0;
}
