#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "maizo_igr_monitor.h"
static uint64_t disp(unsigned int w,unsigned int h,unsigned int magnification)
{
    return ((uint64_t)(w*magnification-1u)<<32)|
           ((uint64_t)(magnification-1u)<<23)|
           ((uint64_t)(h-1u)<<44);
}
int main(void)
{
    mz_igr_frame_t f={0};
    uint64_t fb=(uint64_t)(10u<<9); /* PSMCT32, 640 px stride */
    assert(mzIgrMonitorFrame(&f,1u,fb,0,disp(640,224,4),0,1));
    assert(f.x==160 && f.y==49 && f.sx==2 && f.sy==1);
    assert(f.fbw==10 && f.psm==0 && f.pixel_bytes==4);
    assert(mzIgrMonitorFrame(&f,2u,0,fb,0,disp(640,224,4),1));
    assert(!mzIgrMonitorFrame(&f,3u,fb,fb,disp(640,224,4),
                              disp(640,224,4),1));
    assert(!mzIgrMonitorFrame(&f,0u,fb,fb,disp(640,224,4),
                              disp(640,224,4),1));
    assert(!mzIgrMonitorFrame(&f,1u,0,fb,disp(640,224,4),0,1));
    assert(!mzIgrMonitorFrame(&f,1u,fb|((uint64_t)31u<<15),0,
                              disp(640,224,4),0,1));
    assert(!mzIgrMonitorFrame(&f,1u,fb,0,disp(640,100,4),0,1));
    assert(!mzIgrMonitorFrame(&f,1u,fb,0,disp(640,224,4),0,3));
    assert(!mzIgrMonitorFrame(&f,1u,fb|500u,0,disp(640,224,4),0,1));
    assert(!mzIgrMonitorFrame(&f,1u,0,0,0,0,0));
    assert(!mzIgrMonitorFrame(0,1u,fb,0,disp(640,224,4),0,1));
    puts("MAIZO In-Game independent observer safety PASS");
    return 0;
}
