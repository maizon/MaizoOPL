#include <assert.h>
#include <string.h>
#include "../modules/iopcore/common/maizo_restore_codec.h"
int main(void)
{
    uint8_t raw[1280],encoded[1280],out[1280];
    unsigned seed=7;
    for(unsigned trial=0;trial<1000;trial++){
        unsigned n=16u*((trial%80u)+1u);
        for(unsigned i=0;i<n;i+=4){
            seed=1664525u*seed+1013904223u;
            uint32_t v=(trial%3==0)?0x11223344u:seed;
            if(trial%3==1&&i%32u&&i>=4)memcpy(raw+i,raw+i-4,4);
            else memcpy(raw+i,&v,4);
        }
        unsigned size=mzrEncode(raw,n,encoded,sizeof(encoded));
        if(size){
            assert(size<n && mzrDecode(encoded,size,out,n));
            assert(!memcmp(raw,out,n));
            assert(!mzrDecode(encoded,size-1,out,n));
            assert(!mzrDecode(encoded,size,out,n-4));
        }else if(trial%3==0)assert(0);
    }
    assert(!mzrEncode(raw,16,encoded,0));
    assert(!mzrDecode(encoded,0,out,16));
    return 0;
}
