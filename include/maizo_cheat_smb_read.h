#ifndef MAIZO_CHEAT_SMB_READ_H
#define MAIZO_CHEAT_SMB_READ_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Only compressed ZIP data is staged, before game launch. The RPC
 * buffer stays aligned even when the server returns a short read. */
#define MZ_SMB_ZIP_CACHE_MAX (4u*1024u*1024u)
typedef int (*mz_smb_read_fn)(void *,void *,int);
typedef struct { uint32_t offset; int requested, returned; } mz_smb_read_error_t;
static inline int mzSmbZipReadAll(mz_smb_read_fn read,void *ctx,
                                  unsigned char *dst,uint32_t size,
                                  unsigned char *scratch,unsigned chunk,
                                  mz_smb_read_error_t *error)
{
    uint32_t at=0;
    if(!read||!dst||!scratch||!error||!size||size>MZ_SMB_ZIP_CACHE_MAX||
       !chunk||chunk>8192u)return 0;
    while(at<size){
        unsigned want=size-at<chunk?size-at:chunk;
        int got=read(ctx,scratch,(int)want);
        if(got<=0||(unsigned)got>want){
            error->offset=at;error->requested=(int)want;error->returned=got;
            return 0;
        }
        memcpy(dst+at,scratch,(size_t)got);
        at+=(unsigned)got;
    }
    return 1;
}
typedef struct {const unsigned char *data;uint32_t size;} mz_smb_zip_memory_t;
static inline int mzSmbZipReadAt(void *ctx,uint32_t offset,void *out,size_t count)
{
    const mz_smb_zip_memory_t *m=ctx;
    if(offset>m->size||count>m->size-offset)return 0;
    memcpy(out,m->data+offset,count);
    return 1;
}
#endif
