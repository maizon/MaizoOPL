/*
 * MAIZO IN-GAME PRO - minimal GS/VIF1 overlay for the OPL EE core.
 *
 * Core rendering approach adapted from RiptOPL / RIPPS2's igrmenu.c
 * (akilluminati47, 2026) and ORBIT's Neutrino IGR. AFL-3.0.
 * The game framebuffer is write-only on the EE; we draw into its already
 * selected framebuffer using VIF1 DIRECT (PATH2), after its DMA goes idle.
 * No textures, allocation, networking, files or continuous frame updates.
 * Incompatible graphics / busy DMA -> fail closed to stock IGR reset.
 *
 * The menu can use a passive GS register observer without GSM video
 * forcing. However GSM video adjustments remain gated behind GSM ON;
 * an unsupported display/framebuffer refuses to draw (fail closed).
 */
#include "ee_core.h"
#include "coreconfig.h"
#include "maizo_igrmenu.h"
#include "maizo_igr_state.h"
#include "maizo_igr_filter.h"
#include "maizo_gs_runtime.h"
#include "maizo_gs_probe.h"
#include "maizo_gs_480i_bridge.h"
#include "maizo_igr_monitor.h"
#define IGRM_FONT_DATA
#include "maizo_igrmfont.h"

#define PW 160
#define PH 126
#define MAX_LINES 12
#define CHARS 23
#define MZ_D1_CHCR    ((vu32 *)0x10009000)
#define MZ_D1_MADR    ((vu32 *)0x10009010)
#define MZ_D1_QWC     ((vu32 *)0x10009020)
#define MZ_D1_TADR    ((vu32 *)0x10009030)
#define MZ_D2_CHCR    ((vu32 *)0x1000a000)
#define MZ_D8_MADR    ((vu32 *)0x1000d010)
#define MZ_D_RBSR     ((vu32 *)0x1000e040)
#define MZ_D_CTRL     ((vu32 *)0x1000e000)
#define MZ_D_STAT     ((vu32 *)0x1000e010)
#define MZ_D_ENABLER  ((vu32 *)0x1000f520)
#define MZ_D_ENABLEW  ((vu32 *)0x1000f590)
#define MZ_GIF_STAT   ((vu32 *)0x10003020)
#define MZ_VIF1_STAT  ((vu32 *)0x10003c00)
#define CHCR_STR 0x100
#define VIF_STALLED 0x00800f00
#define VIF_FQC 0x1f000000
#define GIF_BUSY 0x1f000c00

extern u64 GSMSourceGSRegs[11];
extern u64 GSMDestGSRegs[11];
extern u8 GSMFlags[19];
extern u16 GSMSourceSetGsCrt[3];
extern u16 GSMDestSetGsCrt[3];
extern volatile u32 MaizoGs480iMode;
extern u32 MaizoGs480iOriginalFix;
extern u64 MaizoGs480iOriginalSMODE2;
extern volatile u32 MaizoGsFilterMode;
extern volatile u32 MaizoGsFilterPrimary;
extern volatile u32 MaizoGsFilterSourceEN;
extern volatile u64 MaizoGsFilterPMODE;
extern volatile u64 MaizoGsFilterBaseFB;
extern volatile u64 MaizoGsFilterBaseDisplay;

typedef struct {
    u8 x, y, len, color;
    u8 glyph[CHARS];
} mz_line_t;
static mz_line_t mzLines[MAX_LINES];  /* ~600 bytes, no heap */
/* All these counters/GS mode flags are bounded to 0..255. Compact
 * them without truncating the 16-bit coordinates (mzX/mzY/mzBw). */
static u8 mzLineCount, mzSelectedY, mzBpp, mzSx, mzSy, mzPsm;
static int mzX, mzY, mzBw;
static u32 mzBp;
static maizo_igr_state_t mzState;
static int mzStateReady;
static u64 mzOriginalDisp[2];
static int mzGameHasGsm;

static const u8 mzColors[6][3] = {
    {9, 15, 27},      /* panel */
    {73, 104, 168},   /* border */
    {44, 57, 87},     /* selection */
    {238, 240, 253},  /* title */
    {169, 189, 221},  /* regular */
    {252, 182, 101}   /* warning */
};

static void mzLine(int y, int x, int color, const char *s)
{
    mz_line_t *l;
    int i;
    if(mzLineCount>=MAX_LINES)return;
    l=&mzLines[mzLineCount++];
    l->y=y;l->x=x;l->color=color;
    for(i=0;i<CHARS&&s[i];i++){
        unsigned char c=(unsigned char)s[i];
        if(c>='a'&&c<='z')c-=32;
        l->glyph[i]=(c>=32&&c<=95)?c-32:('?' - 32);
    }
    l->len=i;
}
static char *mzCopy(char *d, const char *s)
{
    while(*s)*d++=*s++;
    *d=0;return d;
}
static char *mzNumber(char *dst, int n)
{
    char temp[12];
    int len=0;
    if(n<0){*dst++='-';n=-n;}
    do{temp[len++]='0'+n%10;n/=10;}while(n&&len<10);
    while(len)*dst++=temp[--len];
    *dst=0;return dst;
}
static void mzValue(int row,const char *name,int number,int suffix)
{
    char temp[CHARS + 1],*p=mzCopy(temp,name);
    p=mzCopy(p," ");
    p=mzNumber(p,number);
    if(suffix)p=mzCopy(p,"%");
    mzLine(25+row*10,11,4,temp);
}
static void mzPage(const maizo_igr_state_t *s)
{
    int i, count=maizoIgrPageCount(s->page);
    static const char *const mainNames[6] = {
        "CONTINUAR", "IMAGEM E VIDEO", "CONTROLES", "JOGO E CHEATS",
        "SISTEMA", "VOLTAR AO MAIZO"
    };
    static const char *const systemNames[4] = {
        "CONTINUAR", "SAIR DO JOGO", "DESLIGAR PS2", "VOLTAR"
    };
    mzLineCount=0;
    mzSelectedY=25+s->selected*10;
    mzLine(4,8,3,"MAIZO IN-GAME PRO");
    mzLine(15,8,1,"-----------------------");
    if(s->page==MZ_PAGE_MAIN){
        for(i=0;i<count;i++)mzLine(25+i*10,11,4,mainNames[i]);
    }else if(s->page==MZ_PAGE_VIDEO){
        if(!s->video_caps){
            mzLine(25,11,5,"STRETCH X: REQUER GSM");
            mzLine(35,11,5,"STRETCH Y: REQUER GSM");
            mzLine(45,11,5,"POSICAO X: REQUER GSM");
            mzLine(55,11,5,"POSICAO Y: REQUER GSM");
            mzLine(65,11,5,"PRESET: REQUER GSM");
            mzLine(75,11,5,"FIELD FIX: REQUER GSM");
            mzLine(85,11,4,"FILTROS GRAFICOS >");
            mzLine(95,11,5,"RESTAURAR: REQUER GSM");
            mzLine(105,11,4,"VOLTAR");
        }else{
        mzValue(0,"STRETCH X",s->width*2,1);
        mzValue(1,"STRETCH Y",s->height*2,1);
        mzValue(2,"POSICAO X",s->x*2,0);
        mzValue(3,"POSICAO Y",s->y*2,0);
        mzLine(65,11,4,"PRESET 4:3 / EXPANDIDO");
        mzLine(75,11,4,s->field_fix?"FIELD FIX    ON":"FIELD FIX    OFF");
        mzLine(85,11,4,"FILTROS GRAFICOS >");
        mzLine(95,11,4,"RESTAURAR IMAGEM");
        mzLine(105,11,4,"VOLTAR");
        }
    }else if(s->page==MZ_PAGE_FILTERS){
        mzLine(25,11,s->filter_caps&2?4:5,
               s->filter_caps&2?(s->filter_mode==2?"DEFLICKER     ON":"DEFLICKER    OFF"):
                                "DEFLICKER INDISP.");
        mzLine(35,11,s->filter_caps&1?4:5,
               s->filter_caps&1?(s->filter_mode==1?"SHARP         ON":"SHARP        OFF"):
                                "SHARP     INDISP.");
        mzLine(45,11,s->filter_caps&2?4:5,
               s->filter_caps&2?(s->filter_mode==2?"SMOOTH        ON":"SMOOTH       OFF"):
                                "SMOOTH    INDISP.");
        mzLine(55,11,s->draw_caps&1?4:5,
               !(s->draw_caps&1)?"AA1: INDISPONIVEL":
               ((s->draw_modes&3)==1?"AA1: ON":
                (s->draw_modes&3)==2?"AA1: OFF":"AA1: JOGO"));
        mzLine(65,11,s->draw_caps&2?4:5,
               !(s->draw_caps&2)?"BILINEAR: INDISP.":
               (((s->draw_modes>>2)&3)==1?"BILINEAR: ON":
                ((s->draw_modes>>2)&3)==2?"BILINEAR: OFF":"BILINEAR: JOGO"));
        mzLine(75,11,s->draw_caps&4?4:5,
               !(s->draw_caps&4)?"DITHER: INDISP.":
               (((s->draw_modes>>4)&3)==1?"DITHER: ON":
                ((s->draw_modes>>4)&3)==2?"DITHER: OFF":"DITHER: JOGO"));
        mzLine(85,11,4,"GS AO VIVO >");
        mzLine(95,11,s->frame480i_caps?4:5,
               s->frame480i_caps?
               (s->frame480i_mode?"480P>480I FRAME ON":"480P>480I FRAME OFF"):
               "480P>480I INDISP.");
        mzLine(105,11,4,"VOLTAR");
    }else if(s->page==MZ_PAGE_GS_STATUS){
        mzLine(25,11,4,"VOLTAR");
        mzValue(1,"AMOSTRAS",mzGsProbe.samples,0);
        mzValue(2,"PATH1",mzGsProbe.path1_active,0);
        mzValue(3,"PATH2",mzGsProbe.path2_active,0);
        mzValue(4,"PATH3",mzGsProbe.path3_active,0);
        mzValue(5,"VIF1 DMA",mzGsProbe.vif1_dma_active,0);
        mzValue(6,"GIF DMA",mzGsProbe.gif_dma_active,0);
        /* Visibility is evidence-based: count only actual GS writes
         * changed by a verified pre-DMA adapter, NOT mere hook presence. */
        mzValue(7,"GS ALTERADOS",
                mzRuntimeMetrics.edits.primitive_changes +
                mzRuntimeMetrics.edits.texture_changes +
                mzRuntimeMetrics.edits.dither_changes,0);
        mzLine(105,8,5,mzGsRuntimeCaps()?"HOOK: REGISTRADO":"HOOK: INDISPONIVEL");
    }else if(s->page==MZ_PAGE_PADS){
        mzLine(25,11,4,"PAD EMU: CONFIG. MAIZO");
        mzLine(35,11,4,"VOLTAR");
        mzLine(56,8,5,"MUDAR DRIVER AO JOGAR");
        mzLine(66,8,5,"NAO E UNIVERSAL.");
    }else if(s->page==MZ_PAGE_GAME){
        mzLine(25,11,4,"CHEATS: CONFIG. MAIZO");
        mzLine(35,11,4,"IGS: L1+L2+R1+R2+UP");
        mzLine(45,11,4,"VOLTAR");
        mzLine(66,8,5,"CHEATS EM TEMPO REAL");
        mzLine(76,8,5,"AINDA NAO SUPORTADOS");
    }else{
        for(i=0;i<count;i++)mzLine(25+i*10,11,4,systemNames[i]);
        if(s->confirm)mzLine(78,8,5,"CONFIRME NOVAMENTE");
    }
    mzLine(114,8,3,"X OK  O VOLTAR START");
}
static int mzPixelBase(int x,int y)
{
    if(!x||!y||x==PW-1||y==PH-1)return 1;
    if(y>=mzSelectedY-2 && y<mzSelectedY+8)return 2;
    return 0;
}
static void mzPut(u8 *dest,int color)
{
    const u8 *c=mzColors[color];
    if(mzBpp==2) {
        u32 p=(c[0]>>3)|((c[1]>>3)<<5)|((c[2]>>3)<<10)|0x8000;
        dest[0]=p;dest[1]=p>>8;
    } else {
        dest[0]=c[0];dest[1]=c[1];dest[2]=c[2];
        if(mzBpp==4)dest[3]=0x80;
    }
}
static void mzRenderLine(u8 *dest,int y)
{
    const mz_line_t *active[4];int n=0,i,x;
    int uy=y/mzSy;
    for(i=0;i<mzLineCount&&n<4;i++)
        if(uy>=mzLines[i].y&&uy<mzLines[i].y+7)
            active[n++]=&mzLines[i];
    for(x=0;x<PW*mzSx;x++,dest+=mzBpp){
        int ux=x/mzSx,k,color=-1;
        for(k=0;k<n&&color<0;k++){
            const mz_line_t *l=active[k];
            int gx=ux-l->x,pos,g;
            if(gx<0)continue;
            pos=gx/6;
            if(pos>=l->len || gx%6==5)continue;
            g=l->glyph[pos];
            if(g>=0&&g<IGRM_FONT_GLYPHS &&
               (igrmFontData[g][uy-l->y]&(0x10>>(gx%6))))
                color=l->color;
        }
        mzPut(dest,color<0?mzPixelBase(ux,uy):color);
    }
}
static int mzFrame(void)
{
    const u64 *r=GSMSourceGSRegs;
    mz_igr_frame_t f;
    /* Both recording modes retain the original game's GS writes.
     * No guessing or readback of potentially write-only GS registers. */
    if(!mzIgrMonitorFrame(&f,r[0],r[7],r[9],r[8],r[10],
                          r[2],mzGameHasGsm))
        return 0;
    mzPsm=(int)f.psm;
    mzBpp=(int)f.pixel_bytes;
    mzBw=(int)f.fbw;
    mzBp=f.fbbase;
    mzSx=(int)f.sx;
    mzSy=(int)f.sy;
    mzX=(int)f.x;
    mzY=(int)f.y;
    return 1;
}
static int mzWait(vu32 *reg,u32 mask,u32 want,int mfifo)
{
    int i;
    for(i=0;i<2000000;i++)
        if((*reg&mask)==want || (mfifo && !reg[8] &&
            !((reg[12]^*MZ_D8_MADR)&*MZ_D_RBSR)))return 1;
    return 0;
}
static int mzDma(void *src,u32 qwc,u32 chcr)
{
    *MZ_D1_QWC=qwc;
    *MZ_D1_MADR=(u32)src;
    *MZ_D1_CHCR=chcr;
    asm volatile("sync.l");
    if(mzWait(MZ_D1_CHCR,CHCR_STR,0,0))return 1;
    *MZ_D1_CHCR=0;return 0;
}
static int mzSendLine(u32 *p,int line)
{
    u32 qw=PW*mzSx*mzBpp/16;
    u64 *q=(u64 *)(p+4);
    p[0]=0;p[1]=0;p[2]=0x13<<24;p[3]=(0x50<<24)|(6+qw);
    q[0]=(u64)4|((u64)1<<60);q[1]=0x0e;
    q[2]=(u64)mzBp<<32|(u64)mzBw<<48|(u64)mzPsm<<56;
    q[3]=0x50;
    q[4]=(u64)mzX<<32|(u64)(mzY+line)<<48;
    q[5]=0x51;
    q[6]=(u64)(PW*mzSx)|((u64)1<<32);
    q[7]=0x52;
    q[8]=0;q[9]=0x53;
    q[10]=(u64)qw|((u64)1<<15)|((u64)2<<58);
    q[11]=0;
    FlushCache(0);
    return mzDma(p,7+qw,0x101);
}
static int mzDraw(u32 *packet,const maizo_igr_state_t *s)
{
    int y;
    mzPage(s);
    for(y=0;y<PH*mzSy;y++){
        mzRenderLine((u8 *)(packet+28),y);
        if(!mzSendLine(packet,y))return 0;
    }
    return 1;
}
/* Writable GS DISPLAY parameters are not free-form resampling. Keep small
 * offsets, clamp fields, and preserve the original MAGH/MAGV / mode bits. */
static u64 mzDisplayEdit(u64 base,const maizo_igr_state_t *s)
{
    int olddx=base&0xfff,olddy=(base>>12)&0x7ff;
    int olddw=(base>>32)&0xfff,olddh=(base>>44)&0x7ff;
    int dw=maizoIgrBound(olddw+s->width*12,0,4095);
    int dh=maizoIgrBound(olddh+s->height*6,0,2047);
    int dx=maizoIgrBound(olddx+s->x*2-(dw-olddw)/2,0,4095);
    int dy=maizoIgrBound(olddy+s->y*2-(dh-olddh)/2,0,2047);
    u64 clear=((u64)0xfff)|((u64)0x7ff<<12)|
              ((u64)0xfff<<32)|((u64)0x7ff<<44);
    return (base & ~clear)|(u64)dx|((u64)dy<<12)|
           ((u64)dw<<32)|((u64)dh<<44);
}
static void mzApply(const maizo_igr_state_t *s)
{
    u64 display1,display2;
    if(!mzGameHasGsm)return;
    display1=mzDisplayEdit(mzOriginalDisp[0],s);
    display2=mzDisplayEdit(mzOriginalDisp[1],s);
    GSMDestGSRegs[8]=display1;
    GSMDestGSRegs[10]=display2;
    GSMFlags[17]=s->field_fix?4:0;
    /* GS privileged regs: write only while GSM's breakpoint is disabled. */
    *(vu64 *)0x12000080=display1;
    *(vu64 *)0x120000a0=display2;
    asm volatile("sync.l");
}

/* This function is called ONLY in the paused IGR thread after GSM's GS
 * breakpoint has been suspended. Every active state change is applied to the
 * real GS registers, and persistent framebuffer swaps remain intercepted by
 * gsm_engine.S. No CPU image blur or fabricated AA passes.
 *
 * Scope: interlaced FRAME scanout only, with a single original read circuit.
 * AA1, bilinear texture filtering, dithering and scanlines require intercepting
 * full GS drawing packets and therefore are NOT enabled here.
 */
static u64 mzOutputSource(int index, int fix)
{
    u64 target=GSMDestGSRegs[index],source=GSMSourceGSRegs[index];
    return fix && target ? target : source;
}

/* The game must NATIVELY render progressive 480p into a full-height
 * frame buffer while GSM produces NTSC 480i. A field-mode renderer
 * cannot be converted to a 480p renderer by changing SMODE2 alone. */
static int mzFrame480iCaps(void)
{
    const u64 *r=GSMSourceGSRegs;
    return mzGs480iBridgeCaps(
        GSMSourceSetGsCrt[0],GSMSourceSetGsCrt[1],
        GSMDestSetGsCrt[0],GSMDestSetGsCrt[1],
        r[0],r[7],r[9],r[8],r[10]);
}

/* The GSM breakpoint is disabled while the IGR menu runs. Change only
 * GS scanout mode and its GSM shadow; never touch per-game primitives,
 * frame buffer layout, DISPLAY/DISPFB or GS timing mode (NTSC stays
 * NTSC). Hook_SetGsCrt rolls back this opt-in on CRT mode changes. */
static int mzFrame480iApply(int enabled)
{
    u64 value;
    if(enabled){
        if(!mzFrame480iCaps())return 0;
        if(MaizoGs480iMode)return 1;
        MaizoGs480iOriginalSMODE2=GSMDestGSRegs[2];
        MaizoGs480iOriginalFix=(u32)GSMFlags[10];
        value=mzGs480iFrameSMODE2(GSMDestGSRegs[2]);
        GSMDestGSRegs[2]=value;
        GSMFlags[10]=1;
        MaizoGs480iMode=1;
    }else{
        if(!MaizoGs480iMode)return 1;
        /* Stop enforcing the scanout override before restoring. */
        MaizoGs480iMode=0;
        GSMDestGSRegs[2]=MaizoGs480iOriginalSMODE2;
        GSMFlags[10]=(u8)MaizoGs480iOriginalFix;
        value=MaizoGs480iOriginalSMODE2;
    }
    *(vu64 *)0x12000020=value; /* GS SMODE2: 480i FRAME when ON */
    asm volatile("sync.l");
    return 1;
}
static int mzFilterCapabilities(void)
{
    u64 pmode=mzOutputSource(0,GSMFlags[9]);
    u64 smode=mzOutputSource(2,GSMFlags[10]);
    u64 fb1=mzOutputSource(7,GSMFlags[15]);
    u64 fb2=mzOutputSource(9,GSMFlags[15]);
    u64 d1=mzOutputSource(8,GSMFlags[16]);
    u64 d2=mzOutputSource(10,GSMFlags[16]);
    if(!pmode || !smode)return 0;
    return mzFilterCaps(pmode,smode,fb1,fb2,d1,d2);
}
static int mzFilterApply(int mode)
{
    u64 pmode=mzOutputSource(0,GSMFlags[9]);
    u64 smode=mzOutputSource(2,GSMFlags[10]);
    u64 fb1=mzOutputSource(7,GSMFlags[15]);
    u64 fb2=mzOutputSource(9,GSMFlags[15]);
    u64 d1=mzOutputSource(8,GSMFlags[16]);
    u64 d2=mzOutputSource(10,GSMFlags[16]);
    int primary, caps;
    u64 fb,display,filteredPmode;
    if(!pmode || !smode)return 0;
    caps=mzFilterCaps(pmode,smode,fb1,fb2,d1,d2);
    if(mode && !(caps&mode))return 0;

    /* Turn off the assembler hook FIRST while returning to the game's
     * settings. Never leave two read circuits on different game frames. */
    MaizoGsFilterMode=0;
    asm volatile("sync.l");
    if(mode==MZ_FILTER_NATIVE){
        *(vu64 *)0x12000070=fb1;
        *(vu64 *)0x12000090=fb2;
        *(vu64 *)0x12000080=d1;
        *(vu64 *)0x120000a0=d2;
        *(vu64 *)0x12000000=pmode;
        asm volatile("sync.l");
        return 1;
    }
    if(mode==MZ_FILTER_SMOOTH){
        primary=(pmode&3)==1?1:2;
    }else{
        /* The game already uses the two-read-circuit anti-flicker path.
         * Choose the unshifted read circuit, keeping the same image. */
        primary=((fb1>>43)&2047)<((fb2>>43)&2047)?1:2;
    }
    fb=primary==1?fb1:fb2;
    display=primary==1?d1:d2;
    filteredPmode=mzFilterPmode(pmode,mode,primary);
    MaizoGsFilterSourceEN=(u32)GSMSourceGSRegs[0]&3;
    MaizoGsFilterPrimary=primary;
    MaizoGsFilterBaseFB=fb;
    MaizoGsFilterBaseDisplay=display;
    MaizoGsFilterPMODE=filteredPmode;

    if(mode==MZ_FILTER_SMOOTH){
        if(primary==1){
            *(vu64 *)0x12000070=fb;
            *(vu64 *)0x12000090=mzFilterOffsetOneLine(fb);
        }else{
            *(vu64 *)0x12000070=mzFilterOffsetOneLine(fb);
            *(vu64 *)0x12000090=fb;
        }
        if(primary==1){
            *(vu64 *)0x12000080=display;
            *(vu64 *)0x120000a0=mzFilterShortDisplay(display);
        }else{
            *(vu64 *)0x12000080=mzFilterShortDisplay(display);
            *(vu64 *)0x120000a0=display;
        }
    }
    *(vu64 *)0x12000000=filteredPmode;
    asm volatile("sync.l");
    MaizoGsFilterMode=mode; /* Enable persistence AFTER both circuits match. */
    asm volatile("sync.l");
    return 1;
}

static u32 mzBpcOff(void)
{
    u32 old;
    ee_kmode_enter();
    asm volatile("sync.l\n\tmfbpc %0\n\tsync.p":"=r"(old));
    asm volatile("mtbpc %0\n\tsync.p"::"r"(0x8000));
    ee_kmode_exit();
    return old;
}
static void mzBpcRestore(u32 old)
{
    ee_kmode_enter();
    asm volatile("sync.l\n\tmtbpc %0\n\tsync.p"::"r"(old));
    ee_kmode_exit();
}
int maizoIGR_MenuRun(u16 (*buttons)(void))
{
    USE_LOCAL_EECORE_CONFIG;
    /* One VIF1 packet and a line's pixels: no full-screen framebuffer. */
    u32 packet[(112+PW*2*4+64)/4];
    u32 *p=(u32 *)(((u32)packet+63)&~63);
    u32 chcr,madr,qwc,tadr,ctrl,statOld,suspend,oldBpc;
    u16 prev,now,pressed;
    int mfifo,result=MAIZO_IGR_RESUME;
    int first;
    if(!config->EnableMaizoIgrMenu)return MAIZO_IGR_UNAVAILABLE;
    mzGameHasGsm=!!config->EnableGSMOp;
    if(!mzStateReady){
        maizoIgrStateInit(&mzState,config->GsmConfig.FIELD_fix);
        mzOriginalDisp[0]=GSMDestGSRegs[8];
        mzOriginalDisp[1]=GSMDestGSRegs[10];
        mzStateReady=1;
    }
    if(!mzFrame())return MAIZO_IGR_UNAVAILABLE;
    /* The menu remains navigable without GSM. Unsafe video controls
     * stay clearly disabled, not silently mapped to GS writes. */
    mzState.video_caps=(unsigned int)mzGameHasGsm;
    mzState.filter_caps=mzGameHasGsm?mzFilterCapabilities():0;
    /* A DRAW switch is only actionable if a verified pre-DMA game
     * adapter registered support. Stock commercial games do not. */
    mzState.draw_caps=mzGsRuntimeCaps();
    mzState.draw_modes=mzGsRuntimeMode(MZ_DRAW_AA1) |
        (mzGsRuntimeMode(MZ_DRAW_BILINEAR)<<2) |
        (mzGsRuntimeMode(MZ_DRAW_DITHER)<<4);
    mzState.draw_changed=0;
    mzState.frame480i_caps=mzGameHasGsm?mzFrame480iCaps():0;
    mzState.frame480i_mode=!!MaizoGs480iMode;
    mzState.frame480i_changed=0;
    if(!mzGameHasGsm){
        mzState.width=mzState.height=mzState.x=mzState.y=0;
        mzState.field_fix=0;
        mzState.filter_mode=0;
    }
    if(mzState.filter_mode!=(int)MaizoGsFilterMode)
        mzState.filter_mode=0; /* Another SetGsCrt cancelled the filter. */
    ctrl=*MZ_D_CTRL;
    mfifo=(ctrl>>2)&3;
    if(!mzWait(MZ_D1_CHCR,CHCR_STR,0,mfifo==2) ||
       !mzWait(MZ_D2_CHCR,CHCR_STR,0,mfifo==3) ||
       (*MZ_VIF1_STAT & VIF_STALLED))return MAIZO_IGR_UNAVAILABLE;
    oldBpc=mzBpcOff();
    /* If a game changed its readout layout without SetGsCrt, restore
     * before touching more VRAM. Fail closed instead of pretending
     * a 224-line title has a native 480-line render buffer. */
    if(MaizoGs480iMode && !mzFrame480iCaps()){
        mzFrame480iApply(0);
        mzState.frame480i_mode=0;
    }
    suspend=0;
    if(mfifo==2){
        suspend=*MZ_D_ENABLER;
        *MZ_D_ENABLEW=suspend|0x10000;
    }
    statOld=*MZ_D_STAT&0x4002;
    chcr=*MZ_D1_CHCR;madr=*MZ_D1_MADR;qwc=*MZ_D1_QWC;tadr=*MZ_D1_TADR;
    if(mfifo==2){
        *MZ_D1_CHCR=chcr & ~CHCR_STR;
        *MZ_D_ENABLEW=suspend;
        *MZ_D_CTRL=ctrl & ~0xc;
    }

    mzState.page=MZ_PAGE_MAIN;mzState.selected=0;mzState.confirm=0;
    first=mzDraw(p,&mzState);
    if(!first){result=MAIZO_IGR_UNAVAILABLE;goto cleanup;}
    prev=buttons();
    for(;;){
        int action;
        now=buttons();
        pressed=now&~prev;
        prev=now;
        if(!pressed)continue;
        action=maizoIgrStep(&mzState,pressed);
        if(mzState.changed && mzGameHasGsm){
            mzApply(&mzState);
            mzState.changed=0;
        }
        if(mzState.filter_changed && mzGameHasGsm){
            int desired=mzState.filter_mode;
            mzState.filter_changed=0;
            if(!mzFilterApply(desired))
                mzState.filter_mode=(int)MaizoGsFilterMode;
            mzState.filter_caps=mzFilterCapabilities();
        }
        if(mzState.frame480i_changed && mzGameHasGsm){
            int desired=(int)mzState.frame480i_mode;
            mzState.frame480i_changed=0;
            if(!mzFrame480iApply(desired))
                mzState.frame480i_mode=!!MaizoGs480iMode;
            mzState.frame480i_caps=mzFrame480iCaps();
        }
        if(mzState.draw_changed){
            unsigned int features=mzState.draw_changed;
            if(features&MZ_DRAW_AA1)
                mzGsRuntimeSetMode(MZ_DRAW_AA1,mzState.draw_modes&3u);
            if(features&MZ_DRAW_BILINEAR)
                mzGsRuntimeSetMode(MZ_DRAW_BILINEAR,(mzState.draw_modes>>2)&3u);
            if(features&MZ_DRAW_DITHER)
                mzGsRuntimeSetMode(MZ_DRAW_DITHER,(mzState.draw_modes>>4)&3u);
            mzState.draw_changed=0;
            mzState.draw_caps=mzGsRuntimeCaps();
        }
        if(action==MZ_ACTION_RESUME){result=MAIZO_IGR_RESUME;break;}
        if(action==MZ_ACTION_EXIT){result=MAIZO_IGR_EXIT;break;}
        if(action==MZ_ACTION_POWEROFF){result=MAIZO_IGR_POWEROFF;break;}
        if(!mzDraw(p,&mzState)){result=MAIZO_IGR_UNAVAILABLE;break;}
    }
    if(result==MAIZO_IGR_RESUME){
        /* Consume release so button combo does not reach/reopen in game. */
        u32 t=0;
        while(buttons()&&t++<25000000u){asm volatile("nop");}
    }
cleanup:
    mzWait(MZ_VIF1_STAT,VIF_FQC,0,0);
    mzWait(MZ_GIF_STAT,GIF_BUSY,0,0);
    *MZ_D1_MADR=madr;*MZ_D1_QWC=qwc;*MZ_D1_TADR=tadr;
    *MZ_D_CTRL=ctrl;*MZ_D1_CHCR=chcr;
    asm volatile("sync.l");
    mzBpcRestore(oldBpc);
    *MZ_D_STAT=(*MZ_D_STAT&0x4002)&~statOld;
    return result;
}
