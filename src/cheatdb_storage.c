#include "include/opl.h"
#include "include/maizo_cheatdb.h"
#include "include/cheatdb_storage.h"
#include "include/ethsupport.h"
#include "include/maizo_cheat_smb_read.h"
#include <libmc.h>
#include <malloc.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

typedef struct
{
    int fd, mc, smb, ioError;
    uint32_t position;
    int positionKnown;
    uint32_t size, cacheStart;
    size_t cacheLen;
    unsigned char *scratch;
} db_file_t;

static char dbDiagnostic[96];
const char *cheatDbLastError(void){return dbDiagnostic;}
static void dbError(const char *stage,uint32_t offset,int expected,int actual)
{
    snprintf(dbDiagnostic,sizeof(dbDiagnostic),"%s off=%lu req=%d ret=%d",
             stage,(unsigned long)offset,expected,actual);
    LOG("CHEATDB: %s\n",dbDiagnostic);
}

static int mcResult(int result)
{
    int value;
    return result < 0 ? result : mcSync(MC_WAIT, NULL, &value) == 1 ? value :
                                                                      -1;
}

static int readDb(void *ctx, uint32_t offset, void *out, size_t length)
{
    db_file_t *f = ctx;
    unsigned char *p = out;
    if (offset > f->size || length > f->size - offset)
        return 0;
    while (length) {
        if (!f->cacheLen || offset < f->cacheStart || offset - f->cacheStart >= f->cacheLen) {
            /* Common tiny ZIP central-directory reads reuse the same
             * 8 KiB cache, including on USB and SMB. */
            uint32_t start = offset;
            unsigned chunk=f->smb?1024u:8192u;
            int want = f->size - start > chunk ? (int)chunk : (int)(f->size - start);
            f->cacheLen = 0;
            if(!f->positionKnown||f->position!=start){
                int seek=f->mc?mcResult(mcSeek(f->fd,start,SEEK_SET)):fileXioLseek(f->fd,start,SEEK_SET);
                if(seek!=(int)start&&f->smb){
                    s64 seek64=fileXioLseek64(f->fd,start,SEEK_SET);
                    seek=seek64==(s64)start?(int)start:-1;
                }
                if(seek!=(int)start){
                    dbError("SEEK",start,(int)start,seek);
                    goto ioError;
                }
            }
            int count = f->mc ? mcResult(mcRead(f->fd, f->scratch, want)) : fileXioRead(f->fd, f->scratch, want);
            if (count <= 0 || count > want){
                dbError("READ",start,want,count);
                goto ioError;
            }
            f->position=start+(uint32_t)count;
            f->positionKnown=1;
            f->cacheStart = start;
            f->cacheLen = count;
        }
        size_t at = offset - f->cacheStart;
        size_t n = f->cacheLen - at;
        if (n > length) n = length;
        memcpy(p, f->scratch + at, n);
        p += n;
        offset += n;
        length -= n;
    }
    return 1;
ioError:
    f->ioError = 1;
    return 0;
}

static int smbSequentialRead(void *ctx,void *buf,int size)
{
    db_file_t *f=ctx;
    return fileXioRead(f->fd,buf,size);
}

/* SMB1 servers and older IOP clients can fail random reads or SEEK_END.
 * For a small compressed archive, use a fresh open and sequential reads,
 * then use the same validated ZIP/CRC parser on the in-memory bytes.
 * No IOP changes, writes to the share, or retained game-time allocation. */
static int smbReadZip(db_file_t *f,const char *path,const char *serial,
                      const char *title,char **text,char heading[256])
{
    unsigned char *compressed;
    int result=MZDB_IO;
    iox_stat_t st __attribute__((aligned(64)));
    memset(&st,0,sizeof(st));
    int statResult=fileXioGetStat(path,&st);
    if(statResult>=0 && !st.hisize && st.size>0)
        f->size=st.size;
    else{
        s64 size=fileXioLseek64(f->fd,0,SEEK_END);
        if(size<=0){
            int legacy=fileXioLseek(f->fd,0,SEEK_END);
            size=legacy;
        }
        if(size<=0||size>512u*1024u*1024u){
            dbError("SMB SIZE",0,statResult,(int)size);
            return size>0?MZDB_LIMIT:MZDB_IO;
        }
        f->size=(uint32_t)size;
    }
    if(f->size>512u*1024u*1024u)return MZDB_LIMIT;
    f->positionKnown=0;f->cacheLen=0;
    compressed=f->size<=MZ_SMB_ZIP_CACHE_MAX?malloc(f->size):NULL;
    if(compressed){
        static const unsigned chunks[]={2048,1024,512};
        for(unsigned pass=0;pass<sizeof(chunks)/sizeof(chunks[0]);pass++){
            mz_smb_read_error_t error={0,0,0};
            fileXioClose(f->fd);
            f->fd=fileXioOpen(path,O_RDONLY,0);
            if(f->fd<0){
                dbError("SMB REOPEN",0,0,f->fd);
                break;
            }
            if(mzSmbZipReadAll(smbSequentialRead,f,compressed,f->size,
                               f->scratch,chunks[pass],&error)){
                mz_smb_zip_memory_t mem={compressed,f->size};
                dbDiagnostic[0]=0;
                result=maizoCheatZipFind(mzSmbZipReadAt,&mem,f->size,serial,title,text,heading);
                free(compressed);
                return result;
            }
            dbError("SMB SEQ",error.offset,error.requested,error.returned);
        }
        free(compressed);
        if(f->fd<0)return MZDB_IO;
    }
    /* Larger archives (or low heap) keep the original bounded streaming
     * reader, with smaller SMB chunks and seek64 fallback. */
    f->ioError=0;f->positionKnown=0;f->cacheLen=0;
    result=maizoCheatZipFind(readDb,f,f->size,serial,title,text,heading);
    if(f->ioError)return MZDB_IO;
    if(result>=0)dbDiagnostic[0]=0;
    return result;
}

int cheatDbLoad(const char *serial, const char *title, char **text, char heading[256], char source[80])
{
    static const char *const paths[] = {
        "mass0:/Cheats/CheatDatabase.zip", "mass1:/Cheats/CheatDatabase.zip",
        "mass2:/Cheats/CheatDatabase.zip", "mass3:/Cheats/CheatDatabase.zip",
        "mass4:/Cheats/CheatDatabase.zip", "mass5:/Cheats/CheatDatabase.zip",
        "mass6:/Cheats/CheatDatabase.zip", "mass7:/Cheats/CheatDatabase.zip",
        "mass8:/Cheats/CheatDatabase.zip", "mass9:/Cheats/CheatDatabase.zip",
        "mc0:/Cheats/CheatDatabase.zip", "mc1:/Cheats/CheatDatabase.zip",
        "smb0:\\Cheats\\CheatDatabase.zip"};
    db_file_t f;
    int result = MZDB_NONE;
    *text = NULL;
    source[0] = heading[0] = dbDiagnostic[0] = 0;
    f.scratch = memalign(64, 8192);
    if (!f.scratch)
        return MZDB_MEMORY;
    for (unsigned int i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        const char *path = paths[i];
        if (!strncmp(path, "smb", 3) && (!ethGetObject(1) || gNetworkStartup != 0 || !gPCShareName[0]))
            continue;
        f.mc = !strncmp(path, "mc", 2);
        f.smb = !strncmp(path, "smb", 3);
        f.fd = f.mc ? mcResult(mcOpen(path[2] - '0', 0, path + 4, O_RDONLY)) : fileXioOpen(path, O_RDONLY, 0);
        if (f.fd < 0)
            continue;
        snprintf(source, 80, "%s", path);
        f.cacheLen=0;f.ioError=0;f.positionKnown=0;
        if(f.smb)
            result=smbReadZip(&f,path,serial,title,text,heading);
        else{
            int size=f.mc?mcResult(mcSeek(f.fd,0,SEEK_END)):fileXioLseek(f.fd,0,SEEK_END);
            f.size=size>0?(uint32_t)size:0;
            if(size<0)dbError("SIZE",0,0,size);
            result=size>0?maizoCheatZipFind(readDb,&f,size,serial,title,text,heading):
                   size<0?MZDB_IO:MZDB_INVALID;
            if(f.ioError&&result<0)result=MZDB_IO;
        }
        if (f.mc)
            mcResult(mcClose(f.fd));
        else if(f.fd>=0)
            fileXioClose(f.fd);
        break;
    }
    free(f.scratch);
    return result;
}
