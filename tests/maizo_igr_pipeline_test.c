#include <assert.h>
#include <stdio.h>
#include "maizo_igr_pipeline.h"
int main(void)
{
    assert(mzIgrPipelineIdle(0,0,0,0));
    assert(!mzIgrPipelineIdle(0x100,0,0,0));
    assert(!mzIgrPipelineIdle(0,0x100,0,0));
    assert(!mzIgrPipelineIdle(0,0,1,0)); /* VPS: partial VIF command */
    assert(!mzIgrPipelineIdle(0,0,4,0)); /* VEW: VU wait */
    assert(!mzIgrPipelineIdle(0,0,8,0)); /* VGW: GIF wait */
    assert(!mzIgrPipelineIdle(0,0,0x800,0)); /* VIS */
    assert(!mzIgrPipelineIdle(0,0,0,0x400)); /* APATH */
    assert(!mzIgrPipelineIdle(0,0,0,0x80)); /* PATH2 queued */
    assert(!mzIgrPipelineIdle(0,0,0,0x40)); /* PATH3 queued */
    assert(!mzIgrPipelineIdle(0,0,0x1000000,0));
    assert(!mzIgrPipelineIdle(0,0,0,0x1000000));
    assert(!mzIgrPipelineIdle(0,0,0x800000,0));
    assert(!mzIgrPipelineIdle(0,0,0,0x200));
    assert(!mzIgrPipelineIdle(0,0,0x1000,0));
    assert(mzIgrSelectPath(0,0)==3);
    assert(mzIgrSelectPath(0,1)==2);
    assert(mzIgrSelectPath(0,2)==2);
    assert(!mzIgrSelectPath(0,8));
    assert(mzIgrSelectPath(4,0)==3); /* masked IRQ pending is preserved */
    assert(mzIgrSelectPath(0x40004,0)==3);
    assert(mzIgrSelectPath(0x60006,0)==3);
    assert(!mzIgrSelectPath(0x60006,1));
    assert(!mzIgrSelectPath(0x8000,0));
    assert(mzIgrReadyPath(0,0,0x800,0,0)==3);
    assert(!mzIgrReadyPath(0,0,0x800,1,0));
    assert(!mzIgrReadyPath(0,0,4,0,0));
    assert(!mzIgrReadyPath(0,0,0,8,0));
    assert(!mzIgrReadyPath(0x100,0,0,0,0));
    assert(!mzIgrReadyPath(0,0,0,0x1000,0));
    assert(!mzIgrReadyPath(0,0,0,0x20,0));
    assert(mzIgrOriginalDstat(2,2)==0x20002);
    assert(mzIgrReadyPath(0,0,0,0,mzIgrOriginalDstat(4,4))==3);
    assert(!mzIgrReadyPath(0,0,0,1,mzIgrOriginalDstat(2,2)));
    assert(mzIgrSafePath(0,0,0,0,0)==3);
    /* PATH3 must also reject VIF STOP/FORCE/IRQ stall or pending INT. */
    for(unsigned bit=8;bit<=11;bit++)
        assert(!mzIgrSafePath(0,0,1u<<bit,0,0));
    assert(mzIgrSafePath(0,0,0,1,0)==2);
    assert(mzIgrSafePath(0,0,0,2,0)==2);
    assert(mzIgrSafePath(0,0,0,3,0x20002)==2);
    assert(!mzIgrSafePath(0,0,0x400,1,0));
    assert(!mzIgrSafePath(0x100,0,0,1,0));
    assert(!mzIgrSafePath(0,0,0,0x101,0));
    assert(!mzIgrSafePath(0,0,0,1,0x8000));
    assert(!mzIgrSafePath(0,0,4,0,0)); /* VU1 running */
    assert(!mzIgrSafePath(0,0,0x1000,0,0)); /* VIF error */
    assert(!mzIgrSafePath(0,0,0x2000,0,0));
    for(unsigned used=0;used<32;used++)
        for(unsigned n=0;n<32;n++)
            assert(mzIgrFifoFits(used<<24,n)==
                (n>0&&n<=16&&used<=16&&used+n<=16));
    assert(!mzIgrFifoFits(0x100,14)); /* PATH1 queued */
    assert(!mzIgrFifoFits(0x8,14)); /* GIF stopped */
    assert(!mzIgrFifoFits(0x1000,14)); /* readback */
    for(unsigned n=0;n<=17;n++){
        assert(mzIgrVifFifoFits(0,1,n)==(n>0&&n<=15));
        assert(!mzIgrVifFifoFits(0x1000000,1,n));
        assert(!mzIgrVifFifoFits(0x400,1,n));
        assert(!mzIgrVifFifoFits(0,0x81,n));
    }
    for(unsigned bpp=2;bpp<=4;bpp++){
        assert(6+32*bpp/16<=16);
        assert(1+6+32*bpp/16<=16);
    }
    puts("MAIZO In-Game VIF1/GIF pipeline and bounded FIFO packets PASS");
    return 0;
}
