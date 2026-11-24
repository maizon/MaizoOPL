/* Hardware fixture, not a commercial-game compatibility claim.
 * Three actual paths render the same packet, with separate GS finish
 * fences, and the last step applies real scanlines to a completed frame. */
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <iopcontrol.h>
#include <loadfile.h>
#include <libpad.h>
#include <graph.h>
#include <gs_psm.h>
#include <dma.h>
#include <string.h>
#include <stdint.h>
#include "maizo_gs_client_ps2.h"
#include "maizo_gs_autolink.h"

#define W 640u
#define H 448u
#define CSR (*(volatile uint64_t *)0x12001000u)
#define VIF1STAT (*(volatile uint32_t *)0x10003c00u)
#define VPUSTAT (*(volatile uint32_t *)0x10003800u)
#define TEXBASE 0x2000u /* 256-byte blocks, above both 32/16-bit frames */
#define MZ_ALIGNED __attribute__((aligned(64)))
mz_gs_shared_t *mzGsShared;
extern unsigned char mzKickStart[],mzKickEnd[];
static uint64_t packet[512] MZ_ALIGNED,shadow[512] MZ_ALIGNED;
static uint64_t vif[520] MZ_ALIGNED;
static uint64_t scanpacket[MZ_SCANLINE_QWC*2] MZ_ALIGNED;
static unsigned char padbuf[256] MZ_ALIGNED;
static mz_scanline_frame_t frame;
static unsigned pos,psm,phase;

static void tag(void){pos=1;}
static void ad(uint64_t v,unsigned r){packet[pos*2]=v;packet[pos*2+1]=r;pos++;}
static void close_tag(void){packet[0]=(pos-1u)|(1ULL<<15)|(1ULL<<60);packet[1]=14;}
static int wait_dma(int ch){return dma_channel_wait(ch,2000000)>=0;}
static int finish(void)
{
    unsigned n;
    for(n=0;n<4000000u;n++)if(CSR&2u){CSR=2u;return 1;}
    return 0;
}
static int send(int ch,void *data,unsigned qwc)
{
    return wait_dma(ch)&&dma_channel_send_normal(ch,data,qwc,0,0)>=0&&wait_dma(ch);
}
static int direct(void){close_tag();return send(DMA_CHANNEL_GIF,packet,pos);}
static int fenced(void){ad(0,0x61);return direct()&&finish();}
static uint64_t xy(unsigned x,unsigned y){return (uint64_t)(x*16u)|((uint64_t)(y*16u)<<16);}
static void rgba(unsigned r,unsigned g,unsigned b){frame.rgbaq=0x3f80000080000000ULL|r|(g<<8)|(b<<16);ad(frame.rgbaq,1);}
static void prim(unsigned p){frame.prim=p;ad(p,0);}
static void vertex(unsigned x,unsigned y){ad(xy(x,y),5);}
static int mzLabSetup(unsigned depth)
{
    psm=depth;
    graph_set_mode(GRAPH_MODE_INTERLACED,graph_get_region(),GRAPH_MODE_FRAME,0);
    graph_set_screen(0,0,W,H);
    graph_set_bgcolor(0,0,0);
    graph_set_framebuffer(0,0,W,psm,0,0);
    graph_set_output(1,0,1,0,0,0x80);
    graph_enable_output();
    memset(&frame,0,sizeof(frame));
    frame.width=W;frame.height=H;frame.complete_frame=1;frame.exclusive_frame_end=1;
    frame.frame=((uint64_t)(W/64u)<<16)|((uint64_t)psm<<24);
    frame.zbuf=1ULL<<32;frame.test=0x30000;frame.alpha=0x44;
    frame.scissor=((uint64_t)(W-1)<<16)|((uint64_t)(H-1)<<48);
    frame.prmodecont=1;frame.colclamp=1;
    tag();ad(frame.frame,0x4c);ad(frame.zbuf,0x4e);ad(frame.test,0x47);
    ad(frame.alpha,0x42);ad(frame.xyoffset,0x18);ad(frame.scissor,0x40);
    ad(0,0x4a);ad(0,0x49);ad(1,0x46);ad(0,0x45);ad(0,0x22);ad(1,0x1a);
    ad(0x71603524ULL|0x7160352400000000ULL,0x44);
    return fenced();
}
static int texture(void)
{
    unsigned i;
    tag();ad((uint64_t)TEXBASE<<32|1ULL<<48,0x50);
    ad(0,0x51);ad(16ULL|(16ULL<<32),0x52);ad(0,0x53);
    close_tag();packet[0]&=~(1ULL<<15);
    packet[pos*2]=64ULL|(1ULL<<15)|(2ULL<<58);packet[pos*2+1]=0;pos++;
    for(i=0;i<128;i++){
        unsigned x=(i*2)%16,y=(i*2)/16;
        uint32_t a=((x^y)&1)?0x80ffffff:0x80101010;
        uint32_t b=((x^y)&1)?0x80101010:0x80ffffff;
        packet[pos*2+i]=(uint64_t)a|((uint64_t)b<<32);
    }
    pos+=64;
    if(!send(DMA_CHANNEL_GIF,packet,pos))return 0;
    tag();ad(0,0x3f);return fenced();
}
static int clear_frame(void)
{
    tag();prim(6);rgba(16,20,28);vertex(0,0);vertex(W,H);
    return fenced();
}
/* All target register writes in a complete packet, so the test also
 * verifies enabling dithering after the original environment setup. */
static void scene(unsigned col,unsigned y)
{
    unsigned x=col*208u+12u;
    tag();ad(frame.frame,0x4c);ad(1,0x1a);
    prim(3|8);ad(0x71603524ULL|0x7160352400000000ULL,0x44);ad(0,0x45);
    rgba(240,40,30);vertex(x+10,y+15);
    rgba(30,240,60);vertex(x+180,y+31);
    rgba(30,80,240);vertex(x+63,y+170);
    /* Scaled nearest texture becomes bilinear when the filter is ON. */
    ad(TEXBASE|(1ULL<<14)|(4ULL<<26)|(4ULL<<30)|(1ULL<<34)|(1ULL<<35),6);
    ad(0,0x14);
    prim(6|16|256);rgba(128,128,128);ad(0,3);vertex(x+12,y+180);
    ad(256ULL|(256ULL<<16),3);vertex(x+186,y+290);
    /* Reset the primitive attributes before the next caller. */
    prim(6);ad(0,0x61);close_tag();
}
static int path3(void)
{
    scene(0,30);
    if(!wait_dma(DMA_CHANNEL_GIF))return 0;
    mzGsAutoEligible(1);
    if(dma_channel_send_normal(DMA_CHANNEL_GIF,packet,pos,0,0)<0)return 0;
    return wait_dma(DMA_CHANNEL_GIF)&&finish();
}
static int path2(void)
{
    uint32_t *v=(uint32_t *)vif;
    scene(1,30);memset(v,0,16);
    v[3]=(0x50u<<24)|pos;
    memcpy(v+4,packet,pos*16u);
    if(!wait_dma(DMA_CHANNEL_VIF1))return 0;
    mzGsAutoEligible(1);
    if(dma_channel_send_normal(DMA_CHANNEL_VIF1,vif,pos+1u,0,0)<0)return 0;
    return wait_dma(DMA_CHANNEL_VIF1)&&finish();
}
static int upload_vu(void)
{
    uint32_t *v=(uint32_t *)vif;
    unsigned n=(unsigned)(mzKickEnd-mzKickStart);
    if(!n||n>2048u||(n&7u))return 0;
    memset(vif,0,16+n+16);v[3]=(0x4au<<24)|((n/8u)<<16);
    memcpy(v+4,mzKickStart,n);
    return send(DMA_CHANNEL_VIF1,vif,(16u+n+15u)/16u);
}
static int path1(void)
{
    uint32_t *v=(uint32_t *)vif;
    void *selected=packet;
    unsigned n;
    scene(2,30);
    if(!wait_dma(DMA_CHANNEL_VIF1)||pos>240u)return 0;
    mzGsPs2VuStage(packet,shadow,sizeof(shadow),pos*16u,1,1,&selected);
    memset(vif,0,16);v[0]=0x01000101u; /* STCYCL 1,1 */
    v[3]=0x6c000000u|(pos<<16); /* V4-32, absolute VU address zero */
    memcpy(v+4,selected,pos*16u);
    n=4+pos*4u;
    v[n++]=0x14000000u; /* MSCAL 0: actual XGKICK microprogram */
    v[n++]=0x11000000u; /* FLUSH */
    v[n++]=0;v[n++]=0;
    return send(DMA_CHANNEL_VIF1,vif,n/4u)&&finish();
}
static int end_frame(void)
{
    int r;
    /* No pending primitive, all three paths have returned a FINISH.
     * The scene ends on PRIM=6 and last RGBAQ is known. */
    frame.prim=6;
    r=mzGsPs2Scanlines(scanpacket,sizeof(scanpacket),&frame,1);
    if(r<0&&r!=MZ_GS_DMA_UNSUPPORTED)return 0;
    if(!wait_dma(DMA_CHANNEL_GIF))return 0;
    tag();return fenced(); /* fence the post-process before next frame */
}
static int init_pad(void)
{
    unsigned n;
    SifInitRpc(0);
    SifIopReset("",0);
    for(n=0;n<2000000u&&!SifIopSync();n++){}
    if(n==2000000u)return 0;
    SifInitRpc(0);
    SifLoadModule("rom0:SIO2MAN",0,0);
    SifLoadModule("rom0:PADMAN",0,0);
    padInit(0);
    return padPortOpen(0,0,padbuf)>0;
}
int main(void)
{
    unsigned prev=0;
    struct padButtonStatus p;
    int has_pad=init_pad();
    dma_channel_initialize(DMA_CHANNEL_GIF,0,0);
    dma_channel_initialize(DMA_CHANNEL_VIF1,0,0);
    if(mzGsClientBind())mzGsClientAttach(MZ_DRAW_ALL);
    if(!mzLabSetup(GS_PSM_32)||!texture()||!upload_vu())goto stop;
    for(;;){
        unsigned now=0;
        if(has_pad&&padRead(0,0,&p))now=(~p.btns)&0xffffu;
        /* Square switches actual PSMCT32/16. Cross cycles filters when
         * the MAIZO ABI is present; IGR also controls the same ABI. */
        if((now&~prev)&PAD_SQUARE){
            if(!mzLabSetup(psm==GS_PSM_32?GS_PSM_16:GS_PSM_32))goto stop;
        }
        if((now&~prev)&PAD_CROSS && mzGsShared){
            phase=(phase+1u)&3u;
            mzGsRuntimeSetMode(MZ_DRAW_AA1,phase?MZ_GS_FORCE_ON:MZ_GS_KEEP);
            mzGsRuntimeSetMode(MZ_DRAW_BILINEAR,phase?MZ_GS_FORCE_ON:MZ_GS_KEEP);
            mzGsRuntimeSetMode(MZ_DRAW_DITHER,phase?MZ_GS_FORCE_ON:MZ_GS_KEEP);
            mzGsRuntimeSetMode(MZ_DRAW_SCANLINES,phase);
        }
        prev=now;
        if(!clear_frame()||!path3()||!path2()||!path1()||!end_frame())goto stop;
        graph_wait_vsync();
    }
stop:
    graph_set_bgcolor(120,0,0);
    SleepThread();
    return 1;
}
