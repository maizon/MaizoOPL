#include <assert.h>
#include "maizo_cheat_map.h"
int main(void){
 mz_map_t m={0};uint32_t a,p,mask;
 assert(mzMapName(" weapon ]",m.name));
 assert(mzMapEntry(&m,"01 = Sword")==1);
 assert(mzMapEntry(&m,"FF: Shield")==1);
 assert(mzMapReference("20123456 ABCDEF$weapon",&m,1,&a,&p,&mask)==0);
 assert(a==0x20123456&&p==0xabcdef00&&mask==0xff);
 assert((p|m.values[1].value)==0xabcdefff);
 assert(mzMapReference("20123456 $weapon",&m,1,&a,&p,&mask)==0&&mask==0xffffffffu&&p==0);
 assert(mzMapReference("20123456 ABC$weapon",&m,1,&a,&p,&mask)<0);
 assert(mzMapReference("20123456 $unknown",&m,1,&a,&p,&mask)<0);
 assert(mzMapEntry(&m,"100000000 = Invalid")==-1);
 assert(mzMapReference("20123456 $weapon",&m,1,&a,&p,&mask)<0);
 return 0;
}
