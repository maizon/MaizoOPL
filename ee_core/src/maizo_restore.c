/* Optional IOP transport for a paused game. No SIF RPC requests
 * after boot. DMA sources/destinations have resident lifetime even
 * after a timeout; the transport is poisoned until the next IOP reset. */
#include "ee_core.h"
#include "modules.h"
#include "modmgr.h"
#include "maizo_restore.h"
#include "maizo_restore_sync.h"
#include "../../modules/iopcore/common/maizo_restore.h"
static mzr_req req __attribute__((aligned(64)));
static u8 ackLine[64] __attribute__((aligned(64)));
static u8 line[MZR_LINE] __attribute__((aligned(64)));
#define ACK ((volatile mzr_ack *)UNCACHED_SEG(ackLine))
static u32 request,upload,bytes,seq,last;
static u32 written,lineBytes,lineHash[MZR_ROWS];
static int active,sealed,awaitHello;
volatile u32 mzRestoreTransition;
static inline u32 ticks(void){u32 c;asm volatile("mfc0 %0,$9":"=r"(c));return c;}
#define MZR_TIMEOUT 29491200u
static int dma(SifDmaTransfer_t *d,int count)
{
    int id;u32 t=ticks();
    FlushCache(0);
    do{id=SifSetDma(d,count);if(ticks()-t>MZR_TIMEOUT){request=0;return 0;}}while(!id);
    /* A queued transfer can complete late. Keep the backing storage
     * untouched, allow a second bounded window, and never submit it twice. */
    while(SifDmaStat(id)>=0)if(ticks()-t>2u*MZR_TIMEOUT){request=0;return 0;}
    return 1;
}
static int post(u32 op,u32 off,u32 n)
{
    u32 t;
    SifDmaTransfer_t d[2];
    int count=0;
    if(!request)return -1;
    req.op=op;req.off=off;req.len=n;req.ee=(u32)line&0x1fffffffu;
    req.pad[0]=op==MZR_WRITE?mzrHash(line,n):0;
    if(++seq==0)++seq;
    req.pad[1]=mzrRequestTag(op,off,n,req.ee,req.pad[0],seq);
    req.pad[2]=0;req.seq=seq;
    /* Ordered permanent upload and request, one SIF submission. */
    if(op==MZR_WRITE){
        d[count].src=line;d[count].dest=(void *)upload;
        d[count].size=n;d[count++].attr=0;
    }
    d[count].src=&req;d[count].dest=(void *)request;
    d[count].size=sizeof(req);d[count++].attr=0;
    if(!dma(d,count))return -1;
    t=ticks();
    for(;;){
        if(ACK->seq==seq){
            s32 result=ACK->result;u32 tag=ACK->pad;
            asm volatile("sync.l" ::: "memory");
            if(ACK->seq==seq && tag==mzrAckTag(seq,(u32)result)){
                last=ticks();return result;
            }
        }
        if(ticks()-t>3u*MZR_TIMEOUT){request=0;return -1;}
    }
}
void mzRestoreLink(int enable)
{
    char arg[18];static const char hex[]="0123456789ABCDEF";
    u32 addr[2]={(u32)ackLine&0x1fffffffu,(u32)line&0x1fffffffu};
    request=upload=bytes=seq=last=written=lineBytes=0;active=sealed=awaitHello=0;
    /* Flush any cached BSS initialization before the IOP writes here. */
    FlushCache(0);
    ACK->seq=0;ACK->result=0;ACK->buf=0;ACK->pad=0;
    if(!enable)return;
    for(int j=0;j<2;j++){
        for(int i=0;i<8;i++)arg[j*9+i]=hex[(addr[j]>>(28-4*i))&15u];
        arg[j*9+8]=0;
    }
    /* Optional service must not invoke LoadOPLModule's fatal/debug
     * error policy when a game's IOP has insufficient memory. */
    void *module=NULL;unsigned int size=0;
    if(GetOPLModInfo(OPL_MODULE_ID_MAIZO_RESTORE,&module,&size)==0 && size &&
       LoadMemModule(0,module,size,sizeof(arg),arg)>0)
        awaitHello=1;
    /* No additional SIF handshake wait in the boot path. The optional
     * IRX worker advertises its permanent addresses asynchronously. */
}
int mzRestoreReady(void)
{
    if(mzRestoreTransition || active)return 0;
    if(awaitHello && ACK->pad==MZR_HELLO && !ACK->seq){
        u32 p=(u32)ACK->result,u=ACK->buf;
        asm volatile("sync.l" ::: "memory");
        if(ACK->pad==MZR_HELLO && !ACK->seq){
            awaitHello=0;
            if(mzrHelloOK(p,u,sizeof(req),MZR_LINE)){request=p;upload=u;}
        }
    }
    return request && upload;
}
int mzRestoreDiscIdle(void)
{
    return mzRestoreReady() && mzrDiscQuiet((u32)post(MZR_PROBE,0,0));
}
int mzRestoreBegin(u32 n)
{
    if(!mzRestoreReady() || !n || n>MZR_MAX || (n&15u))return 0;
    bytes=written=lineBytes=0;sealed=0;
    if(post(MZR_BEGIN,0,n))return 0;
    bytes=n;active=1;return 1;
}
int mzRestoreWrite(u32 off,const void *src,u32 n)
{
    if(!active || sealed || !request || !upload || !mzrRange(off,n,bytes) ||
       !mzrAppendOK(off,n,written,bytes))return 0;
    if(!lineBytes)lineBytes=n;
    if(n!=lineBytes || off/n>=MZR_ROWS)return 0;
    memcpy(line,src,n);
    if(post(MZR_WRITE,off,n)!=(int)n)return 0;
    lineHash[off/n]=mzrHash(line,n);written+=n;
    return 1;
}
int mzRestoreRead(u32 off,void *dst,u32 n)
{
    if(!sealed || !lineBytes || n!=lineBytes || off%n || off/n>=MZR_ROWS ||
       !mzrRange(off,n,bytes))return 0;
    /* Retry a corrupt line once, never retry a poisoned DMA transport. */
    for(unsigned i=0;i<2;i++){
        if(post(MZR_READ,off,n)!=(int)n){if(!request)return 0;continue;}
        memcpy(dst,(const void *)UNCACHED_SEG(line),n);
        if(mzrHash(dst,n)==lineHash[off/n])return 1;
    }
    return 0;
}
int mzRestoreHush(void)
{
    if(!active || sealed || !mzrSealable(written,bytes) || post(MZR_HUSH,0,0))return 0;
    sealed=1;return 1;
}
int mzRestoreKeep(void)
{
    if(!active || !request)return 0;
    if(ticks()-last>147456000u && post(MZR_KEEP,0,0))return 0;
    return 1;
}
void mzRestoreEnd(int retain)
{
    if(active || bytes)post(retain?MZR_UNHUSH:MZR_WAKE,0,0);
    /* Keep the sole sealed copy after failed rollback; stop reopening
     * until next IOP reset. The IOP lease still releases the audio. */
    if(retain)request=0;
    active=sealed=0;bytes=written=lineBytes=0;
}
