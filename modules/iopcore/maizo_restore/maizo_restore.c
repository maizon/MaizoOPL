/* Optional IOP-side buffer and reversible SPU2 pause.
 * Audio technique from RIPPS2/RiptOPL, AFL-3.0.
 * No reset, no key-on/off, no SPU DMA changes. Lease releases audio
 * even if the EE stops sending commands. */
#include "irx_imports.h"
#include "maizo_restore.h"
#include "maizo_restore_codec.h"
#include "cdvdman_opl.h"
#include <spu2regs.h>
IRX_ID("maizo_restore",1,0);
static volatile mzr_req req;
static mzr_ack ack;
static u32 eeAck,eeLine,bufSize,written;
static int sealed;
/* On-demand compact pages; dynamic storage is never a DMA target. */
#define MZR_SLAB 4096u
#define MZR_SLABS ((MZR_MAX+MZR_SLAB-1u)/MZR_SLAB)
static u8 *slab[MZR_SLABS];
static u32 stored,lineBytes,rows;
static struct {u32 offset,hash;u16 size;u8 compressed;} row[MZR_ROWS];
static u8 upload[MZR_LINE] __attribute__((aligned(16)));
static u8 packed[MZR_LINE] __attribute__((aligned(16)));
static u8 download[MZR_LINE] __attribute__((aligned(16)));
static u16 vol[2][2];
static unsigned muteMask;
static int hush,dmaBusy;
static void wake(void)
{
    int c,s;
    if(!hush)return;
    CpuSuspendIntr(&s);
    for(c=0;c<2;c++){
        if((muteMask&(1u<<(c*2))) && !*SD_P_MVOLL(c))*SD_P_MVOLL(c)=vol[c][0];
        if((muteMask&(2u<<(c*2))) && !*SD_P_MVOLR(c))*SD_P_MVOLR(c)=vol[c][1];
    }
    hush=0;muteMask=0;
    CpuResumeIntr(s);
}
static void silence(void)
{
    int c,s;
    if(hush)return;
    CpuSuspendIntr(&s);
    for(c=0;c<2;c++){
        vol[c][0]=*SD_P_MVOLL(c);vol[c][1]=*SD_P_MVOLR(c);
        /* Static master volume only. Never stop voice phase, ADSR,
         * pitch, AutoDMA, KON/KOFF or an IOP audio worker. Preserve
         * sweep-mode registers instead of restarting their envelope. */
        if(!(vol[c][0]&0x8000u) && vol[c][0]){*SD_P_MVOLL(c)=0;muteMask|=1u<<(c*2);}
        if(!(vol[c][1]&0x8000u) && vol[c][1]){*SD_P_MVOLR(c)=0;muteMask|=2u<<(c*2);}
    }
    hush=1;
    CpuResumeIntr(s);
}
static void release(void)
{
    if(dmaBusy)return;
    for(unsigned i=0;i<MZR_SLABS;i++)if(slab[i]){FreeSysMemory(slab[i]);slab[i]=0;}
    bufSize=written=stored=lineBytes=rows=0;sealed=0;ack.buf=0;
}
static int send(void *src,u32 dst,u32 len)
{
    SifDmaTransfer_t d;
    int i,s;
    if(dmaBusy)return 0;
    d.src=src;d.dest=(void *)dst;d.size=len;d.attr=0;
    for(i=0;i<100;i++){
        CpuSuspendIntr(&s);dmaBusy=sceSifSetDma(&d,1);CpuResumeIntr(s);
        if(dmaBusy)break;
        DelayThread(500);
    }
    if(!dmaBusy)return 0;
    for(i=0;i<100;i++){
        if(sceSifDmaStat(dmaBusy)<0){dmaBusy=0;return 1;}
        DelayThread(500);
    }
    return 0; /* retain DMA backing storage until hardware completes */
}
static int storeBytes(const u8 *src,u32 n)
{
    if(n>MZR_MAX-stored)return 0;
    u32 at=stored;
    while(n){
        unsigned index=at/MZR_SLAB,off=at%MZR_SLAB,take=MZR_SLAB-off;
        if(index>=MZR_SLABS)return 0;
        if(take>n)take=n;
        if(!slab[index])slab[index]=AllocSysMemory(ALLOC_FIRST,MZR_SLAB,0);
        if(!slab[index])return 0;
        for(unsigned j=0;j<take;j++)slab[index][off+j]=src[j];
        src+=take;at+=take;n-=take;
    }
    stored=at;return 1;
}
static int loadBytes(u32 at,u8 *dst,u32 n)
{
    if(at>stored||n>stored-at)return 0;
    while(n){
        unsigned index=at/MZR_SLAB,off=at%MZR_SLAB,take=MZR_SLAB-off;
        if(index>=MZR_SLABS||!slab[index])return 0;
        if(take>n)take=n;
        for(unsigned j=0;j<take;j++)dst[j]=slab[index][off+j];
        dst+=take;at+=take;n-=take;
    }
    return 1;
}
static int discIdle(void)
{
    int param=0;
    return sceCdSC(CDSC_MAIZO_IO_STATE,&param);
}
static int execute(const mzr_req *r)
{
    switch(r->op){
    case MZR_BEGIN:
        if(sealed || dmaBusy || discIdle()!=CDSC_MAIZO_IO_STATE)return -1;
        release();
        if(!r->len || r->len>MZR_MAX || (r->len&15u))return -1;
        bufSize=r->len;return 0;
    case MZR_WRITE: {
        if(!bufSize || sealed || !mzrRange(r->off,r->len,bufSize) ||
           !mzrAppendOK(r->off,r->len,written,bufSize) ||
           mzrHash(upload,r->len)!=r->pad[0] || rows>=MZR_ROWS)return -1;
        if(!lineBytes)lineBytes=r->len;
        if(lineBytes!=r->len || bufSize%lineBytes)return -1;
        unsigned encoded=mzrEncode(upload,r->len,packed,sizeof(packed));
        if(encoded && (!mzrDecode(packed,encoded,download,r->len) ||
                       mzrHash(download,r->len)!=r->pad[0]))return -1;
        unsigned n=encoded?encoded:r->len;
        row[rows].offset=stored;row[rows].hash=r->pad[0];
        row[rows].size=n;row[rows].compressed=!!encoded;
        if(!storeBytes(encoded?packed:upload,n))return -1;
        /* Verify the actual committed slabs, not just the encoder's
         * scratch output. Do not seal a corrupt or incomplete backup. */
        if(encoded){
            if(!loadBytes(row[rows].offset,packed,n) || !mzrDecode(packed,n,download,r->len))return -1;
        }else if(!loadBytes(row[rows].offset,download,n))return -1;
        if(mzrHash(download,r->len)!=row[rows].hash)return -1;
        rows++;written+=r->len;return r->len;
    }
    case MZR_READ: {
        if(!bufSize || !sealed || r->ee!=eeLine || !mzrRange(r->off,r->len,bufSize) ||
           !lineBytes || r->len!=lineBytes || r->off%lineBytes)return -1;
        unsigned index=r->off/lineBytes;
        if(index>=rows || !row[index].size || row[index].size>r->len)return -1;
        if(row[index].compressed){
            if(!loadBytes(row[index].offset,packed,row[index].size) ||
               !mzrDecode(packed,row[index].size,download,r->len))return -1;
        }else if(row[index].size!=r->len ||
                 !loadBytes(row[index].offset,download,r->len))return -1;
        if(mzrHash(download,r->len)!=row[index].hash)return -1;
        return send(download,eeLine,r->len)?(int)r->len:-1;
    }
    case MZR_END: wake();release();return 0;
    case MZR_HUSH:
        if(!bufSize || sealed || !mzrSealable(written,bufSize) ||
           discIdle()!=CDSC_MAIZO_IO_STATE)return -1;
        sealed=1;silence();return 0;
    case MZR_WAKE: wake();release();return 0;
    case MZR_KEEP: return bufSize && discIdle()==CDSC_MAIZO_IO_STATE?0:-1;
    case MZR_PING: return 0x4d5a5253;
    case MZR_PROBE: return discIdle();
    case MZR_UNHUSH: wake();return sealed?0:-1;
    default:return -1;
    }
}
static void worker(void *arg)
{
    u32 seen=0,idle=0;
    int pendingAck=0;
    unsigned ackRetries=0;
    (void)arg;
    /* Announce from the worker, never from module _start / LOADFILE.
     * Retry only if the original DMA is no longer pending. */
    for(unsigned attempt=0;attempt<3u&&!req.seq;attempt++){
        if(dmaBusy){if(sceSifDmaStat(dmaBusy)>=0){DelayThread(20000);continue;}dmaBusy=0;break;}
        if(send(&ack,eeAck,sizeof(ack)))break;
        DelayThread(20000);
    }
    for(;;){
        u32 seq=req.seq;
        if(dmaBusy){
            wake();
            if(sceSifDmaStat(dmaBusy)<0)dmaBusy=0;
        }else if(pendingAck && seq==seen){
            /* Retry acknowledgement only; never execute an operation
             * twice. If EE advanced, the previous ACK was received. */
            if(++ackRetries>2u || send(&ack,eeAck,sizeof(ack)))pendingAck=0;
        }else if(seq && seq!=seen){
            pendingAck=0;ackRetries=0;
            mzr_req r=req;
            if(seq==r.seq && seq==req.seq){
                seen=seq;idle=0;
                ack.result=execute(&r);ack.seq=seen;
                if(!send(&ack,eeAck,sizeof(ack))){pendingAck=1;wake();}
            }
        }else if((bufSize || hush) && ++idle>=20000u){
            /* A sealed backup is the only copy of overwritten pixels.
             * Never free it behind the EE's back. Audio still wakes on
             * timeout. Explicit WAKE/END or IOP reset releases storage. */
            wake();if(!sealed)release();idle=0;
        }
        DelayThread((bufSize || hush || dmaBusy)?500:20000);
    }
}
static u32 hex(const char *s)
{
    u32 v=0;int i,c;
    for(i=0;i<8;i++){
        c=s[i];
        if(c>='0'&&c<='9')c-='0';
        else if(c>='A'&&c<='F')c=c-'A'+10;
        else return 0;
        v=(v<<4)|c;
    }
    return s[8]?0:v;
}
int _start(int argc,char **argv)
{
    iop_thread_t t;int id;
    if(argc!=3 || !(eeAck=hex(argv[1])) || !(eeLine=hex(argv[2])) ||
       (eeAck&63u) || (eeLine&63u) || eeAck>=0x2000000u ||
       eeLine>0x2000000u-MZR_LINE)return MODULE_NO_RESIDENT_END;
    t.attr=TH_C;t.option=0;t.thread=worker;t.stacksize=0x800;t.priority=0x50;
    id=CreateThread(&t);
    if(id<0)return MODULE_NO_RESIDENT_END;
    ack.result=(s32)&req;
    ack.buf=(u32)upload;ack.pad=MZR_HELLO;ack.seq=0;
    StartThread(id,0);
    return MODULE_RESIDENT_END;
}
