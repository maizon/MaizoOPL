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
    {
        /* Synthetic immutable branch body; acceptance must cover all
         * 14 words, normalize ONLY the known first branch. */
        uint32_t code[14]={MZ_SKY_FOG_ORIGINAL};
        for(unsigned i=1;i<14;i++)code[i]=0x01234567u+i;
        uint32_t h=mzSkyHash(2166136261u,code,14);
        assert(mzSkyFogCodeOK(code,h));
        code[0]=MZ_SKY_FOG_OFF;
        assert(mzSkyFogCodeOK(code,h));
        code[2]^=1u;assert(!mzSkyFogCodeOK(code,h));code[2]^=1u;
        code[0]^=1u;assert(!mzSkyFogCodeOK(code,h));
        code[0]=MZ_SKY_FOG_ORIGINAL;assert(mzSkyFogCodeOK(code,h));
    }
    {
        /* Test exact-word normalization with a synthetic surrounding
         * instruction window; the real profile is separately checked. */
        uint32_t code[9]={0,1,2,3,MZ_SKY_RELOAD_ORIGINAL,5,6,7,8};
        uint32_t h=mzSkyHash(2166136261u,code,9);
        assert(mzSkyReloadCodeOK(code,h));
        code[4]=MZ_SKY_RELOAD_OFF;
        assert(mzSkyReloadCodeOK(code,h));
        code[8]^=1u;assert(!mzSkyReloadCodeOK(code,h));code[8]^=1u;
        code[4]=0;assert(!mzSkyReloadCodeOK(code,h));
        code[4]=MZ_SKY_RELOAD_ORIGINAL;assert(mzSkyReloadCodeOK(code,h));
    }
    return 0;
}
