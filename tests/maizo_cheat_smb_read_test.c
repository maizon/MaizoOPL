#include <assert.h>
#include "maizo_cheat_smb_read.h"
typedef struct { unsigned char data[3001]; unsigned at,max; int fail; } mock_t;
static int next(void *ctx,void *buf,int size)
{
    mock_t *m=ctx;
    unsigned n=(unsigned)size;
    if(m->fail&&m->at>=1000)return -5;
    if(n>m->max)n=m->max;
    if(n>sizeof(m->data)-m->at)n=sizeof(m->data)-m->at;
    memcpy(buf,m->data+m->at,n);m->at+=n;return (int)n;
}
int main(void)
{
    mock_t m={0};unsigned char out[3001],scratch[2048],part[13];
    mz_smb_read_error_t err={0};m.max=257;
    for(unsigned i=0;i<sizeof(m.data);i++)m.data[i]=(unsigned char)i;
    assert(mzSmbZipReadAll(next,&m,out,sizeof(out),scratch,2048,&err));
    assert(!memcmp(out,m.data,sizeof(out)));
    mz_smb_zip_memory_t view={out,sizeof(out)};
    assert(mzSmbZipReadAt(&view,2988,part,13)&&!memcmp(part,out+2988,13));
    assert(!mzSmbZipReadAt(&view,2989,part,13));
    m.at=0;m.fail=1;
    assert(!mzSmbZipReadAll(next,&m,out,sizeof(out),scratch,1024,&err));
    assert(err.offset==1028&&err.returned==-5);
    assert(!mzSmbZipReadAll(next,&m,out,MZ_SMB_ZIP_CACHE_MAX+1,scratch,512,&err));
    m.at=0;m.fail=0;
    assert(mzSmbZipReadAll(next,&m,out,sizeof(out),scratch,512,&err));
    return 0;
}
