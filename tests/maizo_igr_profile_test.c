#include <assert.h>
#include <string.h>
#include "maizo_igr_profile.h"
int main(void){
 char arg[MZ_PROFILE_ARG_SIZE],id[12];uint32_t v,o;
 v=mzProfilePack(-3,2,24,-24,1,2,1,0);
 assert(mzProfileValid(v)&&mzProfileArg(arg,"SLUS_123.45",v));
 assert(mzProfileParse(arg,id,&o)&&o==v&&!strcmp(id,"SLUS_123.45"));
 assert(!mzProfileParse("MZP1:",id,&o));
 assert(!mzProfileArg(arg,"../x",v));
 assert(!mzProfilePack(3,0,0,0,0,0,0,0));
 assert(mzProfilePack(31,-32,31,-32,0,0,0,1));
 uint32_t old=0x80000000u|6u|(5u<<5)|(24u<<10)|(24u<<16);
 assert(mzProfileUpgrade(old)==mzProfilePack(-4,-6,0,0,0,0,0,0));
 assert(mzProfileUpgrade(v)==v);
 mz_profile_record_t r={0x4d5a5031u,9,v,mzProfileCheck(9,v)};
 assert(mzProfileRecordOK(&r));r.value^=1;assert(!mzProfileRecordOK(&r));
 return 0;
}
