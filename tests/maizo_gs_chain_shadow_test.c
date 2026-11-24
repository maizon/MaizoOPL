#include <assert.h>
#include <string.h>
#include "maizo_gs_chain_shadow.h"
int main(void){
 uint8_t chain[64] __attribute__((aligned(16)))={0};
 uint8_t ref[32] __attribute__((aligned(16)))={0};
 uint8_t scratch[128] __attribute__((aligned(16)))={0};
 uint8_t saved[64];
 mz_chain_range_t ranges[]={{0x100000,chain,sizeof(chain)},{0x200000,ref,sizeof(ref)}};
 mz_gs_dma_guard_t g={MZ_GS_PATH3_GIF_DMA,1,1,0,0};
 size_t n=0;
 /* A GIF tag in a CNT and its payload in a REFE. Old parser
    rejected because the first fragment did not contain EOP data. */
 mzGifWrite64(chain,1u|(1ULL<<28));
 mzGifWrite64(chain+16,1ULL|(1ULL<<15)|(1ULL<<60));mzGifWrite64(chain+24,14);
 mzGifWrite64(chain+32,1u|(0x200000ULL<<32));
 mzGifWrite64(ref,3);mzGifWrite64(ref+8,0);
 memcpy(saved,chain,sizeof(chain));
 mzGsRuntimeReset();assert(mzGsRuntimeAttach(MZ_DRAW_AA1));
 assert(mzGsRuntimeSetMode(MZ_DRAW_AA1,MZ_GS_FORCE_ON));
 assert(mzGsChainStage(0x100000,ranges,2,scratch,sizeof(scratch),&g,&n)==0);
 assert(n==48&&mzGifRead64(scratch)==(2u|(7ULL<<28)));
 assert(mzGifRead64(scratch+32)==131&&mzGifRead64(ref)==3&&!memcmp(saved,chain,sizeof(chain)));
 /* A bounded cyclic NEXT cannot hang the game. */
 mzGifWrite64(chain,(2ULL<<28)|(0x100000ULL<<32));n=123;
 assert(mzGsChainFlatten(0x100000,ranges,2,scratch,sizeof(scratch),&n)<0&&n==123);
 /* CALL/RET transfers data then returns to the root END. */
 memset(chain,0,sizeof(chain));
 mzGifWrite64(chain,(5ULL<<28)|(0x200000ULL<<32));
 mzGifWrite64(chain+16,1u|(7ULL<<28));mzGifWrite64(chain+32,3);
 mzGifWrite64(ref,1u|(6ULL<<28));
 mzGifWrite64(ref+16,1ULL|(1ULL<<15)|(1ULL<<60));mzGifWrite64(ref+24,14);
 assert(mzGsChainFlatten(0x100000,ranges,2,scratch,sizeof(scratch),&n)==0&&n==48);
 g.chain_tag_transfer=1;
 assert(mzGsChainStage(0x100000,ranges,2,scratch,sizeof(scratch),&g,&n)==MZ_GS_DMA_SKIPPED);
 assert(mzGsChainFlatten(0x100000,ranges,2,chain,sizeof(chain),&n)<0);
 return 0;
}
