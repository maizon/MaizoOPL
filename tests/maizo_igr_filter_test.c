#include <assert.h>
#include <stdio.h>
#include "maizo_igr_filter.h"
static mz_u64 fb(int base,int w,int psm,int y)
{
    return (mz_u64)base|((mz_u64)w<<9)|((mz_u64)psm<<15)|((mz_u64)y<<43);
}
int main(void)
{
    mz_u64 one=fb(96,10,0,6);
    mz_u64 disp=(mz_u64)479<<44|((mz_u64)620<<32)|((mz_u64)75<<12)|632;
    assert(mzFilterModeValid(3));
    assert(!mzFilterModeValid(1) && !mzFilterModeValid(2));
    assert(mzFilterCaps(0x05,3,one,0,disp,0)==MZ_FILTER_SMOOTH);
    assert(mzFilterCaps(0x05,1,one,0,disp,0)==0); /* progressive */
    assert(mzFilterCaps(0x06,3,0,one,0,disp)==MZ_FILTER_SMOOTH);
    assert(mzFilterCaps(0x07,3,one,mzFilterOffsetOneLine(one),
                        disp,mzFilterShortDisplay(disp))==MZ_FILTER_SHARP);
    assert(mzFilterCaps(0x07,3,one,fb(100,10,0,6),disp,disp)==0);
    assert(mzFilterCaps(0x8067,1,one,mzFilterOffsetOneLine(one),disp,mzFilterShortDisplay(disp))==MZ_FILTER_SHARP);
    assert(mzFilterCaps(0x8065,1,one,0,disp,0)==0);
    assert(mzFilterCaps(0x8067,1,one,one,disp,disp)==0);
    assert(mzFilterCaps(0x80e7,1,one,mzFilterOffsetOneLine(one),disp,mzFilterShortDisplay(disp))==0);
    assert(mzFilterOffsetOneLine(one)==fb(96,10,0,7));
    assert(mzNativeSmoothLimit(one,disp)==360);
    assert(mzNativeSmoothLimit(one,disp|(1ULL<<27))==-1);
    assert(mzNativeSmoothLimit(fb(96,1,0,6),disp)==-1);
    assert(mzNativeSmoothLimit(fb(96,10,31,6),disp)==-1);
    assert((mzFilterPmode(0x05,MZ_FILTER_SMOOTH,1)&0xffe3ULL)==0x7023);
    assert((mzFilterPmode(0x07,MZ_FILTER_SHARP,1)&3ULL)==1);
    assert((mzFilterPmode(0x07,MZ_FILTER_SHARP,2)&3ULL)==2);
    assert(mzFilterPmode(0x81,MZ_FILTER_NATIVE,1)==0x81);
    assert((mzFilterShortDisplay(disp)>>44&2047ULL)==478);
    puts("MAIZO GS filters: interlaced gates, PMODE, DISPFB and DISPLAY verified");
    return 0;
}
