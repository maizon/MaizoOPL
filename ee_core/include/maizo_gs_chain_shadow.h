/* Bounded source-chain flattening for verified GIF submitters.
 * Rebuild one contiguous END chain from immutable, explicitly mapped
 * original buffers. Unlike patching each tag separately this also
 * handles GIF packets split across CNT, REF, NEXT, CALL and RET.
 * Not valid for TTE, SPR, REFS stalls, IRQ tags or MFIFO.
 * The caller must own all ranges, synchronize DMA, and know that the
 * renderer does not depend on intermediate TADR/MADR/ASR values. */
#ifndef MAIZO_GS_CHAIN_SHADOW_H
#define MAIZO_GS_CHAIN_SHADOW_H
#include "maizo_gs_runtime.h"

typedef struct {
    uint32_t address;
    const void *data;
    size_t bytes;
} mz_chain_range_t;

static inline const uint8_t *mzChainResolve(const mz_chain_range_t *ranges,size_t nr,
                                            uint32_t addr,size_t bytes)
{
    size_t i;const uint8_t *found=0;
    if(!bytes||(addr&15u)||addr<0x100000u||addr>=0x02000000u||
       bytes>0x02000000u-addr)return 0;
    for(i=0;i<nr;i++){
        uint32_t start=ranges[i].address;
        if(addr>=start && (size_t)(addr-start)<=ranges[i].bytes &&
           bytes<=ranges[i].bytes-(size_t)(addr-start)){
            if(found)return 0;
            found=(const uint8_t *)ranges[i].data+(addr-start);
        }
    }
    return found;
}
static inline int mzChainRangesOK(const mz_chain_range_t *r,size_t nr,
                                  void *scratch,size_t cap)
{
    size_t i,j;uintptr_t a=(uintptr_t)scratch;
    if(!r||!nr||nr>64||!scratch||a&15u||cap<32||cap>0x100000u||
       a+cap<a)return 0;
    for(i=0;i<nr;i++){
        uintptr_t p=(uintptr_t)r[i].data;
        if(!p||(p&15u)||!r[i].bytes||(r[i].bytes&15u)||
           p+r[i].bytes<p||(p<a+cap&&a<p+r[i].bytes)||
           r[i].address<0x100000u||r[i].address>=0x02000000u||
           (r[i].address&15u)||r[i].bytes>0x02000000u-r[i].address)return 0;
        for(j=0;j<i;j++)
            if(r[i].address<r[j].address+r[j].bytes&&
               r[j].address<r[i].address+r[i].bytes)return 0;
    }
    return 1;
}
/* Outputs chain length including a single END tag. A failed parse
 * may change scratch, never original memory or caller output length. */
static inline int mzGsChainFlatten(uint32_t start,const mz_chain_range_t *r,size_t nr,
                                    void *scratch,size_t cap,size_t *out)
{
    uint32_t pc=start,stack[2];unsigned sp=0,step;
    uint8_t *dst=scratch;size_t used=16;
    if(!out||!mzChainRangesOK(r,nr,scratch,cap))return MZ_GS_DMA_UNSUPPORTED;
    for(step=0;step<256;step++){
        const uint8_t *tag=mzChainResolve(r,nr,pc,16),*p;
        uint64_t lo;unsigned id;size_t bytes;
        uint32_t next,addr,source;
        if(!tag)return MZ_GS_DMA_BAD_PACKET;
        lo=mzGifRead64(tag);id=(unsigned)((lo>>28)&7u);
        if(lo&((1ULL<<63)|(1ULL<<31))||id==4u)return MZ_GS_DMA_UNSUPPORTED;
        bytes=(size_t)(lo&65535u)*16u;
        addr=(uint32_t)(lo>>32)&0x7fffffffu;
        next=pc+16u;
        if(id==0||id==3){source=addr;}
        else{source=next;next+=(uint32_t)bytes;}
        if(bytes){
            if(bytes>cap-used || (used+bytes-16u)/16u>65535u)return MZ_GS_DMA_UNSUPPORTED;
            p=mzChainResolve(r,nr,source,bytes);
            if(!p)return MZ_GS_DMA_BAD_PACKET;
            memcpy(dst+used,p,bytes);used+=bytes;
        }
        switch(id){
        case 0:case 7:
            if(sp||used==16u)return MZ_GS_DMA_UNSUPPORTED;
            mzGifWrite64(dst,((used-16u)/16u)|(7ULL<<28));mzGifWrite64(dst+8,0);
            *out=used;return MZ_GS_DMA_OK;
        case 1:case 3:pc=next;break;
        case 2:pc=addr;break;
        case 5:
            if(sp==2)return MZ_GS_DMA_UNSUPPORTED;
            stack[sp++]=next;pc=addr;break;
        case 6:
            if(!sp)return MZ_GS_DMA_UNSUPPORTED;
            pc=stack[--sp];break;
        default:return MZ_GS_DMA_UNSUPPORTED;
        }
    }
    return MZ_GS_DMA_UNSUPPORTED; /* cycles and excessive fragmentation */
}
static inline int mzGsChainStage(uint32_t start,const mz_chain_range_t *r,size_t nr,
                                 void *scratch,size_t cap,const mz_gs_dma_guard_t *guard,
                                 size_t *out)
{
    mz_gs_gif_policy_t p;
    size_t len=0;
    int rc;
    if(!out||!guard||guard->path!=MZ_GS_PATH3_GIF_DMA||
       !guard->caller_owns_buffer||!guard->channel_idle||
       guard->chain_tag_transfer||guard->source_is_scratchpad||
       !mzGsRuntimeCaps())return MZ_GS_DMA_SKIPPED;
    mzGsRuntimeGetPolicy(&p);
    if(!mzGsDmaPolicyEnabled(&p))return MZ_GS_DMA_SKIPPED;
    rc=mzGsChainFlatten(start,r,nr,scratch,cap,&len);
    if(rc!=MZ_GS_DMA_OK)return rc;
    rc=mzGsRuntimeSubmit(MZ_SUBMIT_PATH3_NORMAL,(uint8_t *)scratch+16,len-16,guard,0,0);
    if(rc==MZ_GS_DMA_OK)*out=len;
    return rc;
}
#endif
