#include <assert.h>
#include <string.h>
#include "maizo_gs_scanlines.h"
int main(void){
 uint64_t packet[MZ_SCANLINE_QWC*2] __attribute__((aligned(16)));
 mz_scanline_frame_t f={0};
 unsigned q;
 f.frame=(10ULL<<16);f.width=640;f.height=480;f.complete_frame=1;f.exclusive_frame_end=1;
 f.prim=0x123;f.scanmsk=3;f.rgbaq=0x9988;f.prmodecont=0;f.zbuf=123;
 memset(packet,0,sizeof(packet));
 q=mzScanlineBuild(packet,sizeof(packet),&f,2);
 assert(q==MZ_SCANLINE_QWC);
 assert((packet[0]&0x7fff)==q-1 && packet[1]==14);
 assert(packet[2]==(f.frame|0xff00000000000000ULL)&&packet[3]==0x4c);
 assert(packet[4]==(f.zbuf|(1ULL<<32))&&packet[5]==0x4e);
 assert(packet[8]==(0x64ULL|(64ULL<<32))&&packet[9]==0x42);
 assert(packet[22]==MZ_GS_SCAN_ODD_ONLY&&packet[23]==0x22);
 assert(packet[32]==(640ULL*16u|(480ULL*16u<<16))&&packet[33]==5);
 assert(packet[34]==f.frame&&packet[35]==0x4c);
 assert(packet[54]==f.scanmsk&&packet[55]==0x22);
 assert(packet[58]==f.prim&&packet[59]==0);
 assert(packet[60]==f.prmodecont&&packet[61]==0x1a); /* corrected below */
 f.complete_frame=0;assert(!mzScanlineBuild(packet,sizeof(packet),&f,2));
 f.complete_frame=1;f.frame|=2ULL<<24;assert(!mzScanlineBuild(packet,sizeof(packet),&f,2));
 f.frame=(10ULL<<16)|500u;assert(!mzScanlineBuild(packet,sizeof(packet),&f,2));
 f.frame=10ULL<<16;assert(!mzScanlineBuild(packet,sizeof(packet)-16,&f,2));
 f.context=1;assert(mzScanlineBuild(packet,sizeof(packet),&f,3)==q&&packet[3]==0x4d);
 return 0;
}
