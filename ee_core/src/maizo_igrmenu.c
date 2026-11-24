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
#include "cheat_api.h"
#include "maizo_igr_profile.h"
#include "maizo_igr_filter.h"
#include "maizo_gs_runtime.h"
#include "maizo_gs_probe.h"
#include "maizo_gs_480i_bridge.h"
#include "maizo_igr_monitor.h"
#include "maizo_igr_gsm_layout.h"
#include "maizo_igr_pipeline.h"
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
extern volatile u32 MaizoNativeMask;
extern volatile u64 MaizoNativeSource[2],MaizoNativeTarget[2];
extern volatile u32 MaizoGsMonitorOnly,MaizoNativeFieldInvert;
extern volatile u32 MaizoGs480iMode,MaizoGs480iArmed;
extern u32 MaizoGs480iOriginalFix;
extern u64 MaizoGs480iOriginalSMODE2;
extern volatile u32 MaizoGsFilterMode;
extern volatile u32 MaizoGsFilterPrimary;
extern volatile u32 MaizoGsFilterSourceEN,MaizoNativeSmoothMaxFBP;
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
static vu32 *mzDmaRegs;
static int mzOverlayPath;
static maizo_igr_state_t mzState;
static int mzStateReady;
static u64 mzOriginalDisp[2];
static int mzGameHasGsm;
static int mzProfilePending;
static char mzProfileReturn[MZ_PROFILE_ARG_SIZE];
char *maizoIGR_ProfileArg(void)
{
    return mzProfileReturn[0]?mzProfileReturn:0;
}
static int mzProfileExitOK(const char *path)
{
    const char *base=path,*p=path;
    for(;*p;p++)if(*p=='/'||*p=='\\'||*p==':')base=p+1;
    /* Only explicitly selected loader return paths; never Browser or
     * arbitrary homebrew. ASCII case-insensitive first five letters. */
    if((base[0]|32)=='o' && (base[1]|32)=='p' && (base[2]|32)=='l' && base[3]=='.')return 1;
    return (base[0]|32)=='m' && (base[1]|32)=='a' &&
        (base[2]|32)=='i' && (base[3]|32)=='z' && (base[4]|32)=='o';
}

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
static void __attribute__((noinline)) mzPage(const maizo_igr_state_t *s)
{
    int i, count=maizoIgrPageCount(s->page);
    static const char *const mainNames[6] = {
        "CONTINUAR", "IMAGEM E VIDEO", "CONTROLES", "JOGO E CHEATS",
        "SISTEMA", "VOLTAR AO MAIZO"
    };
    static const char *const systemNames[5] = {
        "CONTINUAR", "SAIR DO JOGO", "DESLIGAR PS2", "VOLTAR", "SALVAR E SAIR"
    };
    USE_LOCAL_EECORE_CONFIG;
    mzLineCount=0;
    mzSelectedY=25+s->selected*10;
    mzLine(4,8,3,"MAIZO IN-GAME PRO");
    mzLine(15,8,1,"------------------");
    if(s->page==MZ_PAGE_MAIN){
        for(i=0;i<count;i++)mzLine(25+i*10,11,4,mainNames[i]);
    }else if(s->page==MZ_PAGE_VIDEO){
        if(!s->video_caps){
            static const char *const labels[9]={
                "STRETCH X: GSM OFF","STRETCH Y: GSM OFF",
                "POS. X: GSM OFF","POS. Y: GSM OFF",
                "PRESET: GSM OFF","FIELD: GSM OFF",
                "FILTROS >","RESTAURAR: GSM OFF","VOLTAR"
            };
            for(i=0;i<9;i++)
                if(i==5 && s->native_field_caps)
                    mzLine(75,11,4,s->field_fix?"FIELD INV ON":"FIELD INV OFF");
                else mzLine(25+i*10,11,(i==6||i==8)?4:5,labels[i]);
        }else if(s->video_caps==MZ_VIDEO_NATIVE){
            mzValue(0,"TAMANHO X",s->width,0);
            mzValue(1,"TAMANHO Y",s->height,0);
            mzValue(2,"POSICAO X",s->x,0);
            mzValue(3,"POSICAO Y",s->y,0);
            mzLine(65,11,4,"PRESET RECORTE");
            mzLine(75,11,s->native_field_caps?4:5,
                   s->native_field_caps?(s->field_fix?"FIELD INV ON":"FIELD INV OFF"):"FIELD: N/D");
            mzLine(85,11,4,"FILTROS >");
            mzLine(95,11,4,"RESTAURAR IMAGEM");
            mzLine(105,11,4,"VOLTAR");
        }else{
        mzValue(0,"STRETCH X",s->width*2,1);
        mzValue(1,"STRETCH Y",s->height*2,1);
        mzValue(2,"POSICAO X",s->x*2,0);
        mzValue(3,"POSICAO Y",s->y*2,0);
        mzLine(65,11,4,"PRESET 4:3/EXPAND.");
        mzLine(75,11,4,s->field_fix?"FIELD FIX ON":"FIELD FIX OFF");
        mzLine(85,11,4,"FILTROS >");
        mzLine(95,11,4,"RESTAURAR IMAGEM");
        mzLine(105,11,4,"VOLTAR");
        }
    }else if(s->page==MZ_PAGE_FILTERS){
        mzLine(25,11,s->filter_caps&2?4:5,
               s->filter_caps&2?(s->filter_mode==2?"DEFLICKER ON":"DEFLICKER OFF"):
                                "DEFLICKER N/D");
        mzLine(35,11,s->filter_caps&1?4:5,
               s->filter_caps&1?(s->filter_mode==1?"SHARP ON":"SHARP OFF"):
                                "SHARP N/D");
        mzLine(45,11,s->filter_caps&2?4:5,
               s->filter_caps&2?(s->filter_mode==2?"SMOOTH ON":"SMOOTH OFF"):
                                "SMOOTH N/D");
        mzLine(55,11,s->draw_caps&1?4:5,
               !(s->draw_caps&1)?"AA1: N/D":
               ((s->draw_modes&3)==1?"AA1: ON":
                (s->draw_modes&3)==2?"AA1: OFF":"AA1: JOGO"));
        mzLine(65,11,s->draw_caps&2?4:5,
               !(s->draw_caps&2)?"BILINEAR: N/D":
               (((s->draw_modes>>2)&3)==1?"BILINEAR: ON":
                ((s->draw_modes>>2)&3)==2?"BILINEAR: OFF":"BILINEAR: JOGO"));
        mzLine(75,11,s->draw_caps&4?4:5,
               !(s->draw_caps&4)?"DITHER: N/D":
               (((s->draw_modes>>4)&3)==1?"DITHER: ON":
                ((s->draw_modes>>4)&3)==2?"DITHER: OFF":"DITHER: JOGO"));
        mzLine(85,11,4,"GS AO VIVO >");
        mzLine(95,11,s->frame480i_caps?4:5,
               s->frame480i_caps?
               (s->frame480i_mode?(MaizoGs480iMode?"480P>480I ATIVO":"480P>480I ARMADO"):"480P>480I OFF"):
               "480P>480I N/D");
        mzLine(105,11,4,"VOLTAR");
    }else if(s->page==MZ_PAGE_GS_STATUS){
        mzLine(25,11,4,"VOLTAR");
        if(s->draw_caps&MZ_DRAW_SCANLINES)
            mzValue(1,"SCANLINES",((s->draw_modes>>6)&3u)*25,1);
        else mzLine(35,11,5,"SCANLINES: N/D");
        mzValue(2,"AMOSTRAS",mzGsProbe.samples,0);
        mzValue(3,"PATH1",mzGsProbe.path1_active,0);
        mzValue(4,"PATH2",mzGsProbe.path2_active,0);
        mzValue(5,"PATH3",mzGsProbe.path3_active,0);
        mzValue(6,"GIF DMA",mzGsProbe.gif_dma_active,0);
        /* Visibility is evidence-based: count only actual GS writes
         * changed by a verified pre-DMA adapter, NOT mere hook presence. */
        mzValue(7,"GS ALTERADOS",
                mzRuntimeMetrics.edits.primitive_changes +
                mzRuntimeMetrics.edits.texture_changes +
                mzRuntimeMetrics.edits.dither_changes +
                mzRuntimeMetrics.edits.scanmask_changes,0);
        mzLine(105,8,5,mzGsRuntimeCaps()?
            (mzOverlayPath==3?"HOOK ON / MENU GIF":"HOOK ON / MENU VIF"):
            (mzOverlayPath==3?"HOOK N/D / MENU GIF":"HOOK N/D / MENU VIF"));
    }else if(s->page==MZ_PAGE_PADS){
        mzLine(25,11,4,"PAD EMU: CONFIG.");
        mzLine(35,11,4,"VOLTAR");
        mzLine(56,8,5,"DRIVER: NAO AO VIVO");
        mzLine(66,8,5,"COMPAT. LIMITADA");
    }else if(s->page==MZ_PAGE_GAME){
        mzLine(25,11,s->cheat_caps?4:5,s->cheat_caps?
            (s->cheat_paused?"CHEATS: PAUSADOS":"CHEATS: ATIVOS"):"CHEATS: N/D");
        mzLine(35,11,s->live_count?4:5,s->live_count?"LISTA DE CHEATS >":"LISTA: N/D");
        mzLine(45,11,4,"VOLTAR");
        if(s->cheat_caps){
            mzValue(5,"CODIGOS RAW",numcodes,0);
            mzLine(95,8,5,"PAUSA NAO DESFAZ");
        }else{
            mzLine(66,8,5,"ATIVE NO OPL ANTES");
            mzLine(76,8,5,"HOOK DEVE SER VALIDO");
        }
    }else if(s->page==MZ_PAGE_CHEATS){
        for(i=0;i<(int)s->live_count;i++){
            char label[MZ_LIVE_NAME+4];
            label[0]=(MaizoLiveMask&(1u<<i))?'+':'-';label[1]=' ';
            mzCopy(label+2,config->MaizoLiveCheats.name[i]);
            mzLine(25+i*10,11,(MaizoLiveMask&(1u<<i))?4:5,label);
        }
        mzLine(25+i*10,11,4,"VOLTAR");
    }else{
        for(i=0;i<count;i++)mzLine(25+i*10,11,i==4&&!s->save_caps?5:4,
            i==4&&!s->save_caps?"SALVAR: N/D":systemNames[i]);
        if(s->confirm)mzLine(96,8,5,"CONFIRME DE NOVO");
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
/*
 * The passive GS write observer can miss CRTC setup done by a title
 * before its first trapped write (or when a game clears the debug
 * breakpoint). Read back the GS privileged CRTC registers only after
 * the recorder fails. No hardware writes or video mode changes.
 *
 * Two enabled read circuits may display independent game buffers;
 * do not reject the entire menu for this common layout. Resolve one
 * actually enabled, VRAM-bounded circuit, preferring read circuit 1.
 */
static int mzFrame(void)
{
    const u64 *r=GSMSourceGSRegs;
    mz_igr_frame_t f;
    if(!mzIgrResolveFrame(&f,r[0],r[7],r[9],r[8],r[10],r[2],mzGameHasGsm)){
        /* GS privileged registers support MMIO access on a real EE.
         * Treat an unsupported/zero read as failure, never assume a
         * guessed framebuffer address. */
        if(!mzIgrResolveFrame(&f,*(vu64 *)0x12000000,
                       *(vu64 *)0x12000070,*(vu64 *)0x12000090,
                       *(vu64 *)0x12000080,*(vu64 *)0x120000a0,
                       *(vu64 *)0x12000020,mzGameHasGsm))
            return 0;
    }
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
/* Non-destructive preflight while game threads keep running. It is
 * intentionally separate from GSM: it only checks the GS readout
 * and that VIF1/PATH2 can accept a fresh DIRECT command. */
int maizoIGR_Preflight(void)
{
    USE_LOCAL_EECORE_CONFIG;
    mzGameHasGsm=!!config->EnableGSMOp;
    return !(*MZ_D_CTRL&0xcu) &&
           mzIgrReadyPath(*MZ_D1_CHCR,*MZ_D2_CHCR,*MZ_VIF1_STAT,
                          *MZ_GIF_STAT,*MZ_D_STAT) && mzFrame();
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
    mzDmaRegs[8]=qwc;
    mzDmaRegs[4]=(u32)src;
    mzDmaRegs[0]=chcr;
    asm volatile("sync.l");
    if(mzWait(mzDmaRegs,CHCR_STR,0,0))return 1;
    mzDmaRegs[0]=0;return 0;
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
    SyncDCache(p,(u8 *)p+(7+qw)*16u);
    /* The exact same A+D/IMAGE GIF list can be sent without the
     * VIF header when PATH3 is not masked by the game. */
    return mzOverlayPath==3?mzDma(p+4,6+qw,0x101):mzDma(p,7+qw,0x101);
}
static int __attribute__((noinline)) mzDraw(u32 *packet,const maizo_igr_state_t *s)
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
/* Only recorder-backed layouts; never use guessed MMIO readbacks
 * for native edits. Both active circuits must independently validate. */
static unsigned int __attribute__((noinline)) mzNativeCaps(void)
{
    const u64 *r=GSMSourceGSRegs;
    mz_igr_frame_t f;
    unsigned int en=r[0]&3u,i;
    u64 unchanged;
    if(!MaizoGsMonitorOnly || MaizoGs480iMode || !en)return 0;
    for(i=0;i<2;i++){
        unsigned int bit=1u<<i;
        if(!(en&bit))continue;
        if(!mzIgrMonitorFrame(&f,(r[0]&~3ULL)|bit,r[7],r[9],r[8],r[10],r[2],0) ||
           !mzNativeDisplayEdit(r[8+i*2],0,0,0,0,&unchanged))return 0;
    }
    return en;
}
/* Called only with game threads suspended and the GS breakpoint off.
 * Prepare ALL outputs first; an invalid request must not partially write
 * the hardware. The passive observer reapplies only exact source matches. */
static int __attribute__((noinline)) mzNativeApply(const maizo_igr_state_t *s)
{
    unsigned int en=mzNativeCaps(),i;
    u64 target[2],source[2];
    if(!en || MaizoGsFilterMode)return 0;
    for(i=0;i<2;i++){
        if(!(en&(1u<<i)))continue;
        source[i]=GSMSourceGSRegs[8+i*2];
        if(!mzNativeDisplayEdit(source[i],s->width,s->height,s->x,s->y,&target[i]))return 0;
    }
    MaizoNativeMask=0;
    for(i=0;i<2;i++){
        if(!(en&(1u<<i)))continue;
        MaizoNativeSource[i]=source[i];
        MaizoNativeTarget[i]=target[i];
        *(vu64 *)(0x12000080u+i*0x20u)=target[i];
    }
    asm volatile("sync.l");
    if(s->width||s->height||s->x||s->y)MaizoNativeMask=en;
    return 1;
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
static int __attribute__((noinline)) mzFrame480iSourceOK(void)
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
static u64 mzNative480iTemplate(void)
{
    const u64 *r=GSMSourceGSRegs;
    unsigned int en=r[0]&3u;
    u64 d=en&1u?r[8]:r[10];
    if(!mzNativeCaps()||MaizoNativeMask||MaizoGsFilterMode)return 0;
    return mzGs480iNativeTemplate(GSMSourceSetGsCrt[0],GSMSourceSetGsCrt[1],r[2],d)?d:0;
}
static int mzFrame480iCaps(void)
{
    if(!mzGameHasGsm)return MaizoGs480iArmed||mzNative480iTemplate()!=0;
    return MaizoGs480iArmed ||
        mzGs480iArmCaps(GSMSourceSetGsCrt[0],GSMSourceSetGsCrt[1],
                       GSMDestSetGsCrt[0],GSMDestSetGsCrt[1]);
}
static int __attribute__((noinline)) mzFrame480iApply(int enabled)
{
    u64 value;
    if(!mzGameHasGsm){
        if(enabled){
            if(MaizoGs480iArmed)return 1;
            value=mzNative480iTemplate();
            if(!value)return 0;
            GSMDestSetGsCrt[0]=1;GSMDestSetGsCrt[1]=2;GSMDestSetGsCrt[2]=1;
            GSMDestGSRegs[8]=GSMDestGSRegs[10]=value;
            GSMDestGSRegs[2]=GSMSourceGSRegs[2];
            MaizoGs480iArmed=1;
        }else{
            /* Do not expose 31 kHz while a CRT is receiving 15 kHz.
             * Turn off progressive scan in the title first. */
            if(MaizoGs480iMode)return 0;
            MaizoGs480iArmed=0;
        }
        return 1;
    }
    if(enabled){
        if(!mzFrame480iCaps())return 0;
        if(MaizoGs480iMode)return 1;
        if(GSMSourceSetGsCrt[0]==1 && GSMSourceSetGsCrt[1]==2){
            MaizoGs480iArmed=1;
            return 1; /* native 480i: no changes to scanout */
        }
        if(!mzFrame480iSourceOK())return 0;
        MaizoGs480iArmed=1;
        MaizoGs480iOriginalSMODE2=GSMDestGSRegs[2];
        MaizoGs480iOriginalFix=(u32)GSMFlags[MAIZO_GSM_SMODE2_FIX_OFFSET];
        value=mzGs480iFrameSMODE2(GSMDestGSRegs[2]);
        GSMDestGSRegs[2]=value;
        GSMFlags[MAIZO_GSM_SMODE2_FIX_OFFSET]=1;
        MaizoGs480iMode=1;
    }else{
        MaizoGs480iArmed=0;
        if(!MaizoGs480iMode)return 1;
        /* Stop enforcing the scanout override before restoring. */
        MaizoGs480iMode=0;
        GSMDestGSRegs[2]=MaizoGs480iOriginalSMODE2;
        GSMFlags[MAIZO_GSM_SMODE2_FIX_OFFSET]=(u8)MaizoGs480iOriginalFix;
        value=MaizoGs480iOriginalSMODE2;
    }
    *(vu64 *)0x12000020=value; /* GS SMODE2: 480i FRAME when ON */
    asm volatile("sync.l");
    return 1;
}
static int mzFilterCapabilities(void)
{
    u64 pmode=mzOutputSource(0,GSMFlags[9]);
    u64 smode=mzOutputSource(2,GSMFlags[MAIZO_GSM_SMODE2_FIX_OFFSET]);
    u64 fb1=mzOutputSource(7,GSMFlags[15]);
    u64 fb2=mzOutputSource(9,GSMFlags[15]);
    u64 d1=mzOutputSource(8,GSMFlags[16]);
    u64 d2=mzOutputSource(10,GSMFlags[16]);
    int caps;
    if(!pmode || !smode)return 0;
    caps=mzFilterCaps(pmode,smode,fb1,fb2,d1,d2);
    if(!mzGameHasGsm){
        int limit;
        u64 fb=(pmode&3)==1?fb1:fb2;
        if(MaizoNativeMask||!mzNativeCaps())return 0;
        limit=mzNativeSmoothLimit(fb,(pmode&3)==1?d1:d2);
        if(limit<0 || (int)(fb&511u)>limit)caps&=~MZ_FILTER_SMOOTH;
    }
    return caps;
}
static int __attribute__((noinline)) mzFilterApply(int mode)
{
    u64 pmode=mzOutputSource(0,GSMFlags[9]);
    u64 smode=mzOutputSource(2,GSMFlags[MAIZO_GSM_SMODE2_FIX_OFFSET]);
    u64 fb1=mzOutputSource(7,GSMFlags[15]);
    u64 fb2=mzOutputSource(9,GSMFlags[15]);
    u64 d1=mzOutputSource(8,GSMFlags[16]);
    u64 d2=mzOutputSource(10,GSMFlags[16]);
    int primary, caps;
    u64 fb,display,filteredPmode;
    if(!pmode || !smode)return 0;
    caps=mzFilterCapabilities();
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
        if(!mzGameHasGsm)
            MaizoNativeSmoothMaxFBP=(u32)mzNativeSmoothLimit(fb,display);
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
/* Controller polling sleeps on the preserved kernel alarm instead
 * of monopolizing priority zero with repeated COP0 syscalls. */
__attribute__((noinline)) int maizoIGR_MenuRun(int (*buttons)(u16 *keys, u32 *frame),void (*yield)(void))
{
    USE_LOCAL_EECORE_CONFIG;
    /* One VIF1 packet and a line's pixels: no full-screen framebuffer. */
    u32 packet[(112+PW*2*4+64)/4];
    u32 *p=(u32 *)(((u32)packet+63)&~63);
    u32 chcr,madr,qwc,tadr,statOld,statBit,oldBpc;
    u16 prev=0,now,pressed;
    u32 frame,lastFrame=0;
    unsigned int stale=0;
    int haveFrame=0,armed=0;
    int result=MAIZO_IGR_RESUME;
    int first;
    if(!config->EnableMaizoIgrMenu)return MAIZO_IGR_UNAVAILABLE;
    mzGameHasGsm=!!config->EnableGSMOp;
    if(!mzStateReady){
        maizoIgrStateInit(&mzState,config->GsmConfig.FIELD_fix);
        mzOriginalDisp[0]=GSMDestGSRegs[8];
        mzOriginalDisp[1]=GSMDestGSRegs[10];
        mzStateReady=1;
        mzProfilePending=1;
    }
    if(!mzFrame())return MAIZO_IGR_UNAVAILABLE;
    /* The menu remains navigable without GSM. Unsafe video controls
     * stay clearly disabled, not silently mapped to GS writes. */
    mzState.video_caps=mzGameHasGsm?MZ_VIDEO_GSM:
        (mzNativeCaps()?MZ_VIDEO_NATIVE:MZ_VIDEO_NONE);
    mzState.filter_caps=mzFilterCapabilities();
    /* A DRAW switch is only actionable if a verified pre-DMA game
     * adapter registered support. Stock commercial games do not. */
    mzState.draw_caps=mzGsRuntimeCaps();
    mzState.draw_modes=mzGsRuntimeMode(MZ_DRAW_AA1) |
        (mzGsRuntimeMode(MZ_DRAW_BILINEAR)<<2) |
        (mzGsRuntimeMode(MZ_DRAW_DITHER)<<4) |
        (mzGsRuntimeMode(MZ_DRAW_SCANLINES)<<6);
    mzState.draw_changed=0;
    mzState.cheat_caps=!!config->gCheatList && MaizoCheatHookActive && numcodes>0;
    mzState.cheat_paused=!!MaizoCheatPaused;
    mzState.cheat_changed=0;
    mzState.live_count=mzState.cheat_caps?MaizoLiveCount:0;
    mzState.live_choice=0;
    mzState.frame480i_caps=mzFrame480iCaps();
    mzState.frame480i_mode=!!MaizoGs480iArmed;
    mzState.frame480i_changed=0;
    if(!mzGameHasGsm){
        if(!MaizoNativeMask)
            mzState.width=mzState.height=mzState.x=mzState.y=0;
        mzState.native_field_caps=MaizoGsMonitorOnly && !MaizoGs480iMode &&
            (GSMSourceGSRegs[2]&1u) && GSMSourceSetGsCrt[0]==1 &&
            (GSMSourceSetGsCrt[1]==2 || GSMSourceSetGsCrt[1]==3);
        if(!mzState.native_field_caps)MaizoNativeFieldInvert=0;
        mzState.field_fix=!!MaizoNativeFieldInvert;
    }
    mzState.save_caps=mzProfileIdOK(config->MaizoProfileID)&&mzProfileExitOK(config->ExitPath);
    mzState.filter_mode=(int)MaizoGsFilterMode; /* Another SetGsCrt cancelled the filter. */
    if(*MZ_D_CTRL&0xcu)return MAIZO_IGR_UNAVAILABLE;
    mzOverlayPath=mzIgrReadyPath(*MZ_D1_CHCR,*MZ_D2_CHCR,*MZ_VIF1_STAT,
                                *MZ_GIF_STAT,*MZ_D_STAT);
    if(!mzOverlayPath)return MAIZO_IGR_UNAVAILABLE;
    mzDmaRegs=mzOverlayPath==3?MZ_D2_CHCR:MZ_D1_CHCR;
    statBit=mzOverlayPath==3?4u:2u;
    oldBpc=mzBpcOff();
    /* If a game changed its readout layout without SetGsCrt, restore
     * before touching more VRAM. Fail closed instead of pretending
     * a 224-line title has a native 480-line render buffer. */
    if(mzGameHasGsm && MaizoGs480iMode && !mzFrame480iSourceOK()){
        mzFrame480iApply(0);
        mzState.frame480i_mode=0;
    }
    /* Apply a saved profile once, at a verified idle/menu boundary,
     * never during boot or an active game DMA. Invalid parts stay off. */
    if(mzProfilePending){
        u32 v=config->MaizoProfile;
        mzProfilePending=0;
        if(mzProfileValid(v)&&!!(v&(1u<<26))==mzGameHasGsm){
            maizo_igr_state_t desired=mzState;
            desired.width=(int)(v&31u)-8;desired.height=(int)((v>>5)&31u)-8;
            desired.x=(int)((v>>10)&63u)-24;desired.y=(int)((v>>16)&63u)-24;
            desired.field_fix=!!(v&(1u<<22));
            if(mzGameHasGsm||mzNativeApply(&desired)){
                if(mzGameHasGsm)mzApply(&desired);
                mzState.width=desired.width;mzState.height=desired.height;
                mzState.x=desired.x;mzState.y=desired.y;
            }
            if(mzGameHasGsm||mzState.native_field_caps){
                mzState.field_fix=desired.field_fix;
                if(!mzGameHasGsm)MaizoNativeFieldInvert=desired.field_fix;
            }
            if(((v>>23)&3u)&&mzFilterApply((v>>23)&3u))
                mzState.filter_mode=(int)MaizoGsFilterMode;
            if((v&(1u<<25))&&mzFrame480iApply(1))
                mzState.frame480i_mode=!!MaizoGs480iArmed;
            mzState.filter_caps=mzFilterCapabilities();
            mzState.frame480i_caps=mzFrame480iCaps();
        }
    }
    statOld=*MZ_D_STAT&statBit;
    chcr=mzDmaRegs[0];madr=mzDmaRegs[4];qwc=mzDmaRegs[8];tadr=mzDmaRegs[12];

    mzState.page=MZ_PAGE_MAIN;mzState.selected=0;mzState.confirm=0;
    first=mzDraw(p,&mzState);
    if(!first){result=MAIZO_IGR_UNAVAILABLE;goto cleanup;}
    for(;;){
        int action;
        yield();
        if(!buttons(&now,&frame) || (haveFrame && frame==lastFrame && now==prev)){
            /* A dead pad stream must not strand the game in suspended
             * state. A stationary but live controller keeps advancing
             * its DMA frame counter and does not trigger this guard. */
            if(++stale>=240u){result=MAIZO_IGR_UNAVAILABLE;break;}
            continue;
        }
        haveFrame=1;
        lastFrame=frame;
        stale=0;
        if(!armed){
            /* Ignore the opening chord until its buttons are released.
             * Direction/confirm edges then start from a fresh sample. */
            if(now&0x0f09u)continue;
            armed=1;
            prev=now;
            continue;
        }
        pressed=now&~prev;
        prev=now;
        if(!pressed)continue;
        /* Repeating the original six-button chord is always a
         * non-destructive resume, even deep inside a submenu. */
        if((now&0x0f09u)==0x0f09u){result=MAIZO_IGR_RESUME;break;}
        {
            int oldW=mzState.width,oldH=mzState.height;
            int oldX=mzState.x,oldY=mzState.y;
            action=maizoIgrStep(&mzState,pressed);
            if(mzState.changed){
                if(mzGameHasGsm)mzApply(&mzState);
                else if(!mzNativeApply(&mzState)){
                    mzState.width=oldW;mzState.height=oldH;
                    mzState.x=oldX;mzState.y=oldY;
                }
                mzState.changed=0;
                mzState.filter_caps=mzFilterCapabilities();
            }
        }
        if(mzState.live_choice){
            if(mzState.live_count&&MaizoCheatHookActive){
                MaizoLiveMask=mzLiveToggle(&config->MaizoLiveCheats,MaizoLiveMask,mzState.live_choice-1);
                if(MaizoLiveMask&(1u<<(mzState.live_choice-1))){
                    MaizoCheatPaused=0;
                    mzState.cheat_paused=0;
                }
            }
            mzState.live_choice=0;
        }
        if(mzState.cheat_changed){
            if(mzState.cheat_caps && MaizoCheatHookActive)
                MaizoCheatPaused=!!mzState.cheat_paused;
            mzState.cheat_changed=0;
        }
        if(mzState.field_changed){
            if(mzState.native_field_caps)MaizoNativeFieldInvert=!!mzState.field_fix;
            mzState.field_changed=0;
        }
        if(mzState.filter_changed){
            int desired=mzState.filter_mode;
            mzState.filter_changed=0;
            if(!mzFilterApply(desired))
                mzState.filter_mode=(int)MaizoGsFilterMode;
            mzState.filter_caps=mzFilterCapabilities();
        }
        if(mzState.frame480i_changed){
            int desired=(int)mzState.frame480i_mode;
            mzState.frame480i_changed=0;
            if(!mzFrame480iApply(desired))
                mzState.frame480i_mode=!!MaizoGs480iArmed;
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
            if(features&MZ_DRAW_SCANLINES)
                mzGsRuntimeSetMode(MZ_DRAW_SCANLINES,(mzState.draw_modes>>6)&3u);
            mzState.draw_changed=0;
            mzState.draw_caps=mzGsRuntimeCaps();
        }
        if(action==MZ_ACTION_RESUME){result=MAIZO_IGR_RESUME;break;}
        if(action==MZ_ACTION_SAVE_EXIT){
            u32 v=mzProfilePack(mzState.width,mzState.height,mzState.x,mzState.y,
                mzState.field_fix,mzState.filter_mode,mzState.frame480i_mode,mzGameHasGsm);
            if(mzState.save_caps&&mzProfileArg(mzProfileReturn,config->MaizoProfileID,v)){
                result=MAIZO_IGR_EXIT;break;
            }
        }
        if(action==MZ_ACTION_EXIT){result=MAIZO_IGR_EXIT;break;}
        if(action==MZ_ACTION_POWEROFF){result=MAIZO_IGR_POWEROFF;break;}
        if(!mzDraw(p,&mzState)){result=MAIZO_IGR_UNAVAILABLE;break;}
    }
    if(result==MAIZO_IGR_RESUME){
        /* Consume release so button combo does not reach/reopen in game. */
        unsigned int t;
        for(t=0;t<60u;t++){
            if(buttons(&now,&frame) && !now)break;
            yield();
        }
    }
cleanup:
    /* Never treat failed drain as success. Always leave the polling
     * route alive; do not jump to destructive stock IGR on failure. */
    if(!mzWait(MZ_GIF_STAT,GIF_BUSY,0,0) |
       (mzOverlayPath==2 && !mzWait(MZ_VIF1_STAT,VIF_FQC,0,0)))
        result=MAIZO_IGR_UNAVAILABLE;
    mzDmaRegs[4]=madr;mzDmaRegs[8]=qwc;mzDmaRegs[12]=tadr;
    mzDmaRegs[0]=chcr;
    asm volatile("sync.l");
    mzBpcRestore(oldBpc);
    /* Acknowledge ONLY our own completion, never another channel. */
    *MZ_D_STAT=(*MZ_D_STAT&statBit)&~statOld;
    return result;
}
