/*
 * MAIZO GS scanout filter - no shaders, allocations or GIF packet patching.
 * Derived from the two-read-circuit deflicker method in ps2sdk graph_mode.c.
 * Independent host tests verify PMODE, DISPFB and DISPLAY transformations.
 *
 * SMOOTH: two circuits showing the SAME framebuffer with a 1-line DBY
 * separation; blend ALP=0x70 (for interlaced FRAME mode only).
 * SHARP: turn off existing two-circuit blend ONLY when source framebuffers
 * are provably the same image with DBY difference <=1.
 * ORIGINAL: no interception; restore the game's output.
 */
#ifndef MAIZO_IGR_FILTER_H
#define MAIZO_IGR_FILTER_H
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned long long mz_u64;
enum { MZ_FILTER_NATIVE=0, MZ_FILTER_SHARP=1, MZ_FILTER_SMOOTH=2 };
static int mzFilterModeValid(mz_u64 smode2)
{
    return (smode2 & 3ULL) == 3ULL; /* Interlace FRAME mode only. */
}
static int mzFilterOneCircuit(mz_u64 pmode)
{
    int en=(int)(pmode & 3ULL);
    return en==1||en==2;
}
static int mzFilterSameBuffer(mz_u64 first,mz_u64 second)
{
    int y1=(int)((first>>43)&2047ULL),y2=(int)((second>>43)&2047ULL);
    /* FB base/width/PSM, X and all other settings must match exactly.
       Only a one-line Y displacement is accepted. */
    mz_u64 dyMask=(mz_u64)2047<<43;
    if(!first||!second)return 0;
    return ((first&~dyMask)==(second&~dyMask)) &&
           (y1==y2 || y1+1==y2 || y2+1==y1);
}
static int mzFilterCaps(mz_u64 pmode,mz_u64 smode2,
                        mz_u64 fb1,mz_u64 fb2,
                        mz_u64 display1,mz_u64 display2)
{
    int en=(int)(pmode&3ULL);
    int caps=0;
    if(!mzFilterModeValid(smode2) || ((pmode>>2)&7ULL)!=1ULL)
        return 0;
    if(mzFilterOneCircuit(pmode)){
        mz_u64 fb=en==1?fb1:fb2;
        mz_u64 display=en==1?display1:display2;
        int y=(int)((fb>>43)&2047ULL);
        int dh=(int)((display>>44)&2047ULL);
        int psm=(int)((fb>>15)&31ULL);
        if(fb && display && y<2047 && dh>0 &&
            (psm==0 || psm==1 || psm==2 || psm==10))
            caps|=MZ_FILTER_SMOOTH;
    }
    if(en==3 && mzFilterSameBuffer(fb1,fb2) && display1 && display2){
        /* No arbitrary side effects on independently mixed read circuits. */
        mz_u64 dhmask=(mz_u64)2047<<44;
        int h1=(int)((display1>>44)&2047ULL);
        int h2=(int)((display2>>44)&2047ULL);
        if((display1&~dhmask)==(display2&~dhmask) &&
           (h1==h2 || h1+1==h2 || h2+1==h1))
            caps|=MZ_FILTER_SHARP;
    }
    return caps;
}
static mz_u64 mzFilterPmode(mz_u64 original,int mode,int primary)
{
    if(mode==MZ_FILTER_NATIVE)return original;
    if(mode==MZ_FILTER_SHARP)
        return (original&~3ULL)|(primary==2?2ULL:1ULL);
    if(mode==MZ_FILTER_SMOOTH) {
        /* EN1,EN2=1; MMOD=1; AMOD=0; SLBG=0; ALP=0x70. */
        return (original & ~0xffe3ULL)|3ULL|0x20ULL|((mz_u64)0x70<<8);
    }
    return original;
}
static mz_u64 mzFilterOffsetOneLine(mz_u64 fb)
{
    int y=(int)((fb>>43)&2047ULL);
    mz_u64 mask=(mz_u64)2047<<43;
    return (fb&~mask)|((mz_u64)(y<2047?y+1:y)<<43);
}
static mz_u64 mzFilterShortDisplay(mz_u64 display)
{
    int height=(int)((display>>44)&2047ULL);
    mz_u64 mask=(mz_u64)2047<<44;
    return (display&~mask)|((mz_u64)(height>0?height-1:height)<<44);
}
#ifdef __cplusplus
}
#endif
#endif
