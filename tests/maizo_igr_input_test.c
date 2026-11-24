#include <assert.h>
#include "maizo_igr_input.h"
int main(void)
{
    uint32_t p[4]={1,0x40000010,0,0};
    p[3]=mzPadCheck(p[0],p[1],p[2],3);
    assert(mzPadPacketOK(p));p[0]++;assert(!mzPadPacketOK(p));
    assert(mzPadPick(0x0f00,9)==0x0f00);
    assert(mzPadPick(0x4000,0x0f19)==MZ_PAD_CHORD);
    assert(mzPadPick(0,0x40)==0x40);
    /* Previously one selected report could hide other live controls.
     * Keep held X and R1 independent of a fresh UP or DOWN. */
    unsigned prev=0,held=0,combined;
    combined=mzPadMenuMerge(0,0x4800);
    assert(mzPadMenuStep(combined,&prev,&held)==0x4800);
    combined=mzPadMenuMerge(combined,0x0040);
    assert(mzPadMenuStep(combined,&prev,&held)==0x0040);
    assert(mzPadMenuStep(mzPadMenuMerge(0x4800,0x0010),&prev,&held)==0x0010);
    /* A split chord cannot be manufactured by aggregation. */
    assert(mzPadMenuMerge(mzPadMenuMerge(0,0x0f00),9)!=MZ_PAD_CHORD);
    assert(mzPadMenuMerge(0x4000,MZ_PAD_CHORD)==MZ_PAD_CHORD);
    assert(mzPadMenuMerge(MZ_PAD_CHORD,0x40)==MZ_PAD_CHORD);
    /* Left stick navigation, deadzone, digital precedence. */
    assert(mzPadMenuAnalog(0,128,128)==0);
    assert(mzPadMenuAnalog(0,127,60)==0);
    assert(mzPadMenuAnalog(0,128,0)==0x10);
    assert(mzPadMenuAnalog(0,128,255)==0x40);
    assert(mzPadMenuAnalog(0,0,128)==0x80);
    assert(mzPadMenuAnalog(0,255,128)==0x20);
    assert(mzPadMenuAnalog(0x40,128,0)==0x40);
    assert(mzPadMenuAnalog(MZ_PAD_CHORD,128,0)==MZ_PAD_CHORD);
    assert(mzPadMenuAnalog(0x0f06,128,0)==0x0f06);
    /* Held movement repeats, confirmation never repeats. */
    prev=held=0;
    assert(mzPadMenuStep(0x4040,&prev,&held)==0x4040);
    for(unsigned i=0;i<16;i++)assert(!mzPadMenuStep(0x4040,&prev,&held));
    assert(mzPadMenuStep(0x4040,&prev,&held)==0x40);
    for(unsigned i=0;i<3;i++)assert(!mzPadMenuStep(0x4040,&prev,&held));
    assert(mzPadMenuStep(0x4040,&prev,&held)==0x40);
    assert(!mzPadMenuStep(0,&prev,&held));
    assert(mzPadMenuStep(0x10,&prev,&held)==0x10);
    return 0;
}
