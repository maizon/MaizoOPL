#include <assert.h>
#include "maizo_cheat_live.h"
int main(void){
 mz_live_cheats_t p={0};
 p.count=3;p.variant[0]=p.variant[1]=1;
 assert(mzLiveWriteOK(0x20100004)&&mzLiveWriteOK(0x10100002)&&mzLiveWriteOK(0x00100001));
 assert(!mzLiveWriteOK(0x20100002)&&!mzLiveWriteOK(0x20090000)&&!mzLiveWriteOK(0x40100004));
 assert(mzLiveToggle(&p,1,1)==2);
 assert(mzLiveToggle(&p,2,2)==6);
 assert(mzLiveToggle(&p,6,1)==4);
 assert(mzLiveToggle(&p,4,8)==4);
 p.lines=3;
 p.owner[0]=0;p.owner[1]=1;p.owner[2]=2;
 p.name[0][0]='A';p.name[1][0]='B';p.name[2][0]='C';
 p.mask=1;assert(mzLiveCatalogOK(&p));
 p.mask=3;assert(!mzLiveCatalogOK(&p));
 p.mask=1;p.owner[2]=8;assert(!mzLiveCatalogOK(&p));
 uint32_t a=0xe1020034u,v=0x00100001u;
 mzLiveNormalize(&a,&v);
 assert(a==0xd0100001u&&v==0x02010034u&&mzLiveLineSpan(a,v)==2);
 assert(mzLiveLineSpan(0xd0100001u,0)==-1);
 assert(mzLiveLineSpan(0xd0100002u,0)==1);
 assert(mzLiveLineSpan(0x70100002u,0x001000ffu)==0);
 assert(mzLiveLineSpan(0x70100001u,0x001000ffu)==-1);
 assert(mzLiveLineSpan(0xc0100000u,0)==-1);
 return 0;
}
