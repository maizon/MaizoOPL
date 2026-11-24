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
 return 0;
}
