/* Optional IOP-side buffer and reversible SPU2 pause.
 * Audio technique from RIPPS2/RiptOPL, AFL-3.0.
 * No reset, no key-on/off, no SPU DMA changes. Lease releases audio
 * even if the EE stops sending commands. */
#include "irx_imports.h"
#include "maizo_restore.h"
#include <spu2regs.h>
IRX_ID("maizo_restore",1,0);
static volatile mzr_req req;
static mzr_ack ack;
static u32 eeAck,eeLine,bufSize;
static u8 *buf;
static u8 upload[MZR_LINE] __attribute__((aligned(16)));
static u16 vol[2][2],pitch[2][24];
static int hush,dmaBusy;
static void wake(void)
{
    int c,v,s;
    if(!hush)return;
    CpuSuspendIntr(&s);
    for(c=0;c<2;c++){
        for(v=0;v<24;v++)if(!*SD_VP_PITCH(c,v))*SD_VP_PITCH(c,v)=pitch[c][v];
        if(!*SD_P_MVOLL(c))*SD_P_MVOLL(c)=vol[c][0];
        if(!*SD_P_MVOLR(c))*SD_P_MVOLR(c)=vol[c][1];
    }
    hush=0;
    CpuResumeIntr(s);
}
static void silence(void)
{
    int c,v,s;
    if(hush)return;
    CpuSuspendIntr(&s);
    for(c=0;c<2;c++){
        vol[c][0]=*SD_P_MVOLL(c);vol[c][1]=*SD_P_MVOLR(c);
        *SD_P_MVOLL(c)=0;*SD_P_MVOLR(c)=0;
        for(v=0;v<24;v++){pitch[c][v]=*SD_VP_PITCH(c,v);*SD_VP_PITCH(c,v)=0;}
    }
    hush=1;
    CpuResumeIntr(s);
}
static void release(void)
{
    if(buf && !dmaBusy)FreeSysMemory(buf);
    if(!dmaBusy){buf=0;bufSize=0;ack.buf=0;}
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
static int execute(const mzr_req *r)
{
    switch(r->op){
    case MZR_BEGIN:
        release();
        if(!r->len || r->len>MZR_MAX || (r->len&15u))return -1;
        buf=AllocSysMemory(ALLOC_FIRST,r->len,0);
        if(!buf)return -1;
        bufSize=r->len;return 0;
    case MZR_WRITE:
        /* DMA only targets permanent upload storage, never a buffer
         * which may be freed by the inactivity lease. */
        if(!buf || !mzrRange(r->off,r->len,bufSize))return -1;
        for(u32 j=0;j<r->len;j++)buf[r->off+j]=upload[j];
        return r->len;
    case MZR_READ:
        if(!buf || r->ee!=eeLine || !mzrRange(r->off,r->len,bufSize))return -1;
        return send(buf+r->off,eeLine,r->len)?(int)r->len:-1;
    case MZR_END: release();return 0;
    case MZR_HUSH: silence();return 0;
    case MZR_WAKE: wake();release();return 0;
    case MZR_KEEP: return 0;
    default:return -1;
    }
}
static void worker(void *arg)
{
    u32 seen=0,idle=0;
    (void)arg;
    for(;;){
        u32 seq=req.seq;
        if(dmaBusy){
            wake();
            if(sceSifDmaStat(dmaBusy)<0){dmaBusy=0;release();}
        }else if(seq && seq!=seen){
            mzr_req r=req;
            if(seq==r.seq && seq==req.seq){
                seen=seq;idle=0;
                ack.result=execute(&r);ack.seq=seen;
                if(!send(&ack,eeAck,sizeof(ack)))wake();
            }
        }else if((buf || hush) && ++idle>=20000u){
            wake();release();idle=0;
        }
        /* 10-second lease while allocated or muted. */
        DelayThread((buf || hush || dmaBusy)?500:20000);
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
    t.attr=TH_C;t.option=0;t.thread=worker;t.stacksize=0x800;t.priority=0x30;
    id=CreateThread(&t);
    if(id<0)return MODULE_NO_RESIDENT_END;
    ack.result=(s32)&req;
    ack.buf=(u32)upload;
    if(!send(&ack,eeAck,sizeof(ack)))return MODULE_RESIDENT_END;
    StartThread(id,0);
    return MODULE_RESIDENT_END;
}
