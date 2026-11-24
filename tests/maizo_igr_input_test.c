#include <assert.h>
#include "maizo_igr_input.h"
int main(void){
 uint32_t p[4]={1,0x40000010,0,0};
 p[3]=mzPadCheck(p[0],p[1],p[2],3);
 assert(mzPadPacketOK(p));p[0]++;assert(!mzPadPacketOK(p));
 assert(mzPadPick(0x0f00,9)==0x0f00);
 assert(mzPadPick(0x4000,0x0f19)==MZ_PAD_CHORD);
 assert(mzPadPick(0,0x40)==0x40);
 unsigned rank=0,best=0;
 best=mzPadMenuChoose(best,&rank,0x0800,0x0800);
 best=mzPadMenuChoose(best,&rank,0x0040,0);
 assert(best==0x0040);
 rank=best=0;
 best=mzPadMenuChoose(best,&rank,0x4000,0x4000);
 best=mzPadMenuChoose(best,&rank,0x0010,0);
 assert(best==0x0010);
 rank=best=0;
 best=mzPadMenuChoose(best,&rank,0x0f00,0);
 best=mzPadMenuChoose(best,&rank,9,0);
 assert(best!=MZ_PAD_CHORD);
 best=mzPadMenuChoose(best,&rank,MZ_PAD_CHORD,0);
 assert(best==MZ_PAD_CHORD);
 return 0;
}
