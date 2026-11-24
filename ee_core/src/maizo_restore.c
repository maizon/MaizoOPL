/* Optional IOP transport for a paused game. No SIF RPC requests
 * after boot. DMA sources/destinations have resident lifetime even
 * after a timeout; the transport is poisoned until the next IOP reset. */
#include "ee_core.h"
#include "modules.h"
#include "modmgr.h"
#include "maizo_restore.h"
#include "../../modules/iopcore/common/maizo_restore.h"
static mzr_req req __attribute__((aligned(64)));
static u8 ackLine[64] __attribute__((aligned(64)));
static u8 line[MZR_LINE] __attribute__((aligned(64)));
#define ACK ((volatile mzr_ack *)UNCACHED_SEG(ackLine))
static u32 request,buf,bytes,seq,last;
static int active;
static inline u32 ticks(void){u32 c;asm volatile("mfc0 %0,$9":"=r"(c));return c;}
#define MZR_TIMEOUT 29491200u
static int dma(void *src,u32 dst,u32 len)
{
    SifDmaTransfer_t d;int id;u32 t=ticks();
    d.src=src;d.dest=(void *)dst;d.size=len;d.attr=0;
    FlushCache(0);
    do{id=SifSetDma(&d,1);if(ticks()-t>MZR_TIMEOUT){request=0;return 0;}}while(!id);
    while(SifDmaStat(id)>=0)if(ticks()-t>MZR_TIMEOUT){request=0;return 0;}
    return 1;
}
static int post(u32 op,u32 off,u32 n)
{
    u32 t;
    if(!request)return -1;
    req.op=op;req.off=off;req.len=n;req.ee=(u32)line&0x1fffffffu;
    req.seq=++seq;
    if(!dma(&req,request,sizeof(req)))return -1;
    t=ticks();
    while(ACK->seq!=seq)if(ticks()-t>MZR_TIMEOUT){request=0;return -1;}
    last=ticks();
    return ACK->result;
}
void mzRestoreLink(int enable)
{
    char arg[18];static const char hex[]="0123456789ABCDEF";
    u32 addr[2]={(u32)ackLine&0x1fffffffu,(u32)line&0x1fffffffu};
    request=buf=bytes=seq=last=0;active=0;
    ACK->seq=0;ACK->result=0;
    if(!enable)return;
    for(int j=0;j<2;j++){
        for(int i=0;i<8;i++)arg[j*9+i]=hex[(addr[j]>>(28-4*i))&15u];
        arg[j*9+8]=0;
    }
    if(LoadOPLModule(OPL_MODULE_ID_MAIZO_RESTORE,0,sizeof(arg),arg)>0){
        u32 p=(u32)ACK->result;
        if(p && !(p&15u) && p<0x200000u-sizeof(req))request=p;
    }
}
int mzRestoreBegin(u32 n)
{
    buf=bytes=0;
    if(!n || n>MZR_MAX || post(MZR_BEGIN,0,n))return 0;
    buf=ACK->buf;
    if(!buf || (buf&15u) || buf>0x200000u-n){post(MZR_END,0,0);buf=0;return 0;}
    bytes=n;active=1;return 1;
}
int mzRestoreWrite(u32 off,const void *src,u32 n)
{
    if(!request || !buf || !mzrRange(off,n,bytes))return 0;
    memcpy(line,src,n);
    return dma(line,buf+off,n);
}
int mzRestoreRead(u32 off,void *dst,u32 n)
{
    if(!buf || !mzrRange(off,n,bytes) || post(MZR_READ,off,n)!=(int)n)return 0;
    memcpy(dst,(const void *)UNCACHED_SEG(line),n);return 1;
}
int mzRestoreHush(void)
{
    if(post(MZR_HUSH,0,0))return 0;
    active=1;return 1;
}
void mzRestoreKeep(void)
{
    if(active && ticks()-last>147456000u)post(MZR_KEEP,0,0);
}
void mzRestoreEnd(void)
{
    if(active || buf)post(MZR_WAKE,0,0);
    active=0;buf=bytes=0;
}
