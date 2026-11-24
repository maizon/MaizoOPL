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
    assert(mzIgrMonitorFrame(&f,1u,fb,0,disp(640,224,4),0,1,0));
    assert(f.x==160 && f.y==49 && f.sx==2 && f.sy==1);
    assert(f.fbw==10 && f.psm==0 && f.pixel_bytes==4);
    assert(mzIgrMonitorFrame(&f,2u,0,fb,0,disp(640,224,4),1,0));
    /* Both read circuits display the SAME image: real GS deflicker. */
    assert(mzIgrMonitorFrame(&f,3u,fb,fb,disp(640,224,4),
                             disp(640,224,4),3,0));
    assert(f.x==160 && f.y==49);
    assert(mzIgrMonitorFrame(&f,3u,fb,fb|((uint64_t)1u<<43),
                             disp(640,224,4),disp(640,223,4),3,0));
    assert(f.y==49); /* choose unshifted base */
    /* Do not draw to two unrelated framebuffers or different readouts. */
    assert(!mzIgrMonitorFrame(&f,3u,fb,fb|((uint64_t)3u<<43),
                              disp(640,224,4),disp(640,224,4),3,0));
    assert(!mzIgrMonitorFrame(&f,3u,fb,fb|((uint64_t)1u<<15),
                              disp(640,224,4),disp(640,224,4),3,0));
    assert(!mzIgrMonitorFrame(&f,3u,fb,fb,
                              disp(640,224,4),disp(640,200,4),3,0));
    assert(!mzIgrMonitorFrame(&f,0u,fb,fb,disp(640,224,4),
                              disp(640,224,4),1,0));
    assert(!mzIgrMonitorFrame(&f,1u,0,fb,disp(640,224,4),0,1,0));
    assert(!mzIgrMonitorFrame(&f,1u,fb|((uint64_t)31u<<15),0,
                              disp(640,224,4),0,1,0));
    assert(!mzIgrMonitorFrame(&f,1u,fb,0,disp(640,100,4),0,1,0));
    assert(!mzIgrMonitorFrame(&f,1u,fb,0,disp(640,224,4),0,3,1));
    assert(mzIgrMonitorFrame(&f,1u,fb,0,disp(640,224,4),0,3,0));
    assert(f.height==224 && f.y==49);
    assert(!mzIgrMonitorFrame(&f,1u,fb|500u,0,disp(640,224,4),0,1,0));
    assert(!mzIgrMonitorFrame(&f,1u,0,0,0,0,0,0));
    assert(!mzIgrMonitorFrame(0,1u,fb,0,disp(640,224,4),0,1,0));
    puts("MAIZO In-Game independent observer safety PASS");
    return 0;
}
