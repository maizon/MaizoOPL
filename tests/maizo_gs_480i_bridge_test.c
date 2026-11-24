/* Native 480p renderer scanned out safely over NTSC 480i. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "maizo_gs_480i_bridge.h"
static uint64_t display(unsigned int height,unsigned int magv)
{
    return ((uint64_t)(height-1u)<<44)|
           ((uint64_t)(640u-1u)<<32)|
           ((uint64_t)magv<<27)|((uint64_t)35u<<12);
}
int main(void)
{
    uint64_t pmode=1,fb=((uint64_t)10u<<9)|(2u<<15);
    uint64_t d=display(480,0);
    assert(mzGs480iNativeTemplate(1,2,3,((uint64_t)2559<<32)|((uint64_t)3<<23)|((uint64_t)479<<44)));
    assert(!mzGs480iNativeTemplate(0,0x50,0,d));
    assert(!mzGs480iNativeTemplate(1,3,3,d));
    assert(!mzGs480iNativeTemplate(1,2,3,display(224,0)));
    assert(mzGs480iArmCaps(1,2,1,2)); /* Arm in ordinary NTSC 480i. */
    assert(mzGs480iArmCaps(0,0x50,1,2));
    assert(!mzGs480iArmCaps(1,3,1,2));
    assert(!mzGs480iArmCaps(0,0x50,0,0x50));
    assert(!mzGs480iArmCaps(0,2,1,2));
    assert(mzGs480iBridgeCaps(0,0x50,1,2,pmode,fb,0,d,0));
    assert(mzGs480iBridgeCaps(0,0x50,1,2,2,0,fb,0,d));
    assert(!mzGs480iBridgeCaps(1,0x50,1,2,pmode,fb,0,d,0));
    assert(!mzGs480iBridgeCaps(0,2,1,2,pmode,fb,0,d,0));
    assert(!mzGs480iBridgeCaps(0,0x50,0,2,pmode,fb,0,d,0));
    assert(!mzGs480iBridgeCaps(0,0x50,1,0x50,pmode,fb,0,d,0));
    assert(!mzGs480iBridgeCaps(0,0x50,1,2,3,fb,fb,d,d));
    assert(!mzGs480iBridgeCaps(0,0x50,1,2,pmode,fb,0,display(224,0),0));
    assert(!mzGs480iBridgeCaps(0,0x50,1,2,pmode,fb,0,display(480,1),0));
    assert(!mzGs480iBridgeCaps(0,0x50,1,2,pmode,fb,0,display(481,0),0));
    assert(mzGs480iBridgeCaps(0,0x50,1,2,pmode,fb | (31ULL<<43),
                               0,d,0)); /* still valid if within 2048 */
    assert(!mzGs480iBridgeCaps(0,0x50,1,2,pmode,0,0,d,0));
    assert(!mzGs480iBridgeCaps(0,0x50,1,2,pmode,(fb & ~(31ULL<<9)),0,d,0));
    assert(!mzGs480iBridgeCaps(0,0x50,1,2,pmode,fb | (7ULL<<15),0,d,0));
    assert(!mzGs480iBridgeCaps(0,0x50,1,2,pmode,
                               (500ULL | (10ULL<<9)),0,d,0)); /* beyond VRAM */
    assert(!mzGs480iBridgeCaps(0,0x50,1,2,pmode,
                               fb | (100ULL<<32),0,d,0)); /* clipped horizontal */
    assert(mzGs480iBridgeCaps(0,0x50,1,2,pmode,
                              fb,0,display(448,0),0)); /* 448-line frame */
    assert(mzGs480iFrameSMODE2(0)==3);
    assert(mzGs480iFrameSMODE2(1)==3);
    assert(mzGs480iFrameSMODE2(2)==3);
    assert(mzGs480iFrameSMODE2(0xc)==0xf); /* DPMS bits untouched */
    puts("Native 480p -> standard NTSC 480i FRAME bridge: conservative gates PASS");
    return 0;
}
