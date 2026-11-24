#include <assert.h>
#include "maizo_igr_input.h"
int main(void){
 uint32_t p[4]={1,0x40000010,0,0};
 p[3]=mzPadCheck(p[0],p[1],p[2],3);
 assert(mzPadPacketOK(p));p[0]++;assert(!mzPadPacketOK(p));
 assert(mzPadPick(0x0f00,9)==0x0f00);
 assert(mzPadPick(0x4000,0x0f19)==MZ_PAD_CHORD);
 assert(mzPadPick(0,0x40)==0x40);
 return 0;
}
