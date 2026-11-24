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
    assert(mzIgrPipelineIdle(0,0,0x1000,0));
    assert(mzIgrSelectPath(0,0)==3);
    assert(mzIgrSelectPath(0,1)==2);
    assert(mzIgrSelectPath(0,2)==2);
    assert(!mzIgrSelectPath(0,8));
    assert(mzIgrSelectPath(4,0)==3); /* masked IRQ pending is preserved */
    assert(mzIgrSelectPath(0x40004,0)==2);
    assert(!mzIgrSelectPath(0x60006,0));
    assert(!mzIgrSelectPath(0x8000,0));
    assert(mzIgrReadyPath(0,0,0x800,0,0)==3);
    assert(!mzIgrReadyPath(0,0,0x800,1,0));
    assert(!mzIgrReadyPath(0,0,4,0,0));
    assert(!mzIgrReadyPath(0,0,0,8,0));
    assert(!mzIgrReadyPath(0x100,0,0,0,0));
    puts("MAIZO In-Game VIF1/GIF pipeline preflight PASS");
    return 0;
}
