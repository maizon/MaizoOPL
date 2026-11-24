#include <assert.h>
#include "maizo_effects_client.h"
mz_gs_shared_t *mzGsShared;
int main(void)
{
    mz_gs_shared_t shared={0};
    unsigned caps=MZ_EFFECT_ALL,reduced=caps&~(1u<<MZ_EFFECT_FOG);
    assert(mzGsClientEffectStrength(MZ_EFFECT_MOTION_BLUR,100,1)==100);
    assert(!mzGsClientEffectsAttach(caps,reduced));
    mzGsShared=&shared;
    assert(!mzGsClientEffectsAttach(caps,reduced));
    shared.enabled=1;
    assert(!mzGsClientEffectsAttach(caps,1u<<15));
    assert(mzGsClientEffectsAttach(caps,reduced));
    for(unsigned i=0;i<MZ_EFFECT_COUNT;i++){
        assert(mzGsClientEffectShouldRun(i,1));
        assert(mzGsClientEffectStrength(i,101,1)==101);
        if(reduced&(1u<<i)){
            assert(mzGsEffectSet(i,MZ_EFFECT_REDUCED));
            assert(mzGsClientEffectStrength(i,101,1)==51);
            assert(mzGsClientEffectStrength(i,UINT32_MAX,1)==2147483648u);
        }else assert(!mzGsEffectSet(i,MZ_EFFECT_REDUCED));
        assert(mzGsEffectSet(i,MZ_EFFECT_OFF));
        assert(!mzGsClientEffectShouldRun(i,1));
        assert(mzGsClientEffectShouldRun(i,0));
        assert(mzGsClientEffectStrength(i,101,1)==0);
        assert(mzGsClientEffectStrength(i,101,0)==101);
        assert(mzGsEffectSet(i,MZ_EFFECT_ORIGINAL));
        assert(mzGsClientEffectStrength(i,101,1)==101);
    }
    assert(!mzGsEffectSet(MZ_EFFECT_COUNT,MZ_EFFECT_OFF));
    assert(!mzGsEffectSet(MZ_EFFECT_BLOOM,3));
    assert(mzGsEffectSet(MZ_EFFECT_GLOW,MZ_EFFECT_OFF));
    assert(mzGsRuntimeAttach(MZ_DRAW_AA1));
    assert(mzGsEffectGet(MZ_EFFECT_GLOW)==MZ_EFFECT_OFF);
    for(unsigned i=0;i<3;i++)mzGsRuntimeRecord(MZ_GS_DMA_BAD_PACKET);
    assert(mzGsEffectGet(MZ_EFFECT_GLOW)==MZ_EFFECT_ORIGINAL);
    assert(!mzGsClientEffectsAttach(caps,reduced));
    mzGsRuntimeReset();
    assert(!shared.effect_caps&&!shared.effect_modes);
    assert(mzGsClientEffectsAttach(1u<<MZ_EFFECT_FOG,0));
    assert(!mzGsEffectSet(MZ_EFFECT_GLOW,MZ_EFFECT_OFF));
    assert(mzGsEffectSet(MZ_EFFECT_FOG,MZ_EFFECT_OFF));
    mzGsRuntimeReset();
    mzGsEffectRequest(MZ_EFFECT_BLOOM,MZ_INTENT_AUTO);
    assert(mzGsClientEffectStrength(MZ_EFFECT_BLOOM,100,1)==100);
    assert(mzGsClientEffectsAttach(1u<<MZ_EFFECT_BLOOM,1u<<MZ_EFFECT_BLOOM));
    assert(mzGsClientEffectStrength(MZ_EFFECT_BLOOM,101,1)==51);
    mzGsEffectRequest(MZ_EFFECT_BLOOM,MZ_INTENT_FORCE);
    assert(!mzGsClientEffectShouldRun(MZ_EFFECT_BLOOM,1));
    assert(mzGsClientEffectShouldRun(MZ_EFFECT_BLOOM,0));
    mzGsEffectRequest(MZ_EFFECT_BLOOM,MZ_INTENT_OFF);
    assert(mzGsClientEffectStrength(MZ_EFFECT_BLOOM,100,1)==100);
    shared.enabled=0;
    assert(mzGsClientEffectShouldRun(MZ_EFFECT_FOG,1));
    return 0;
}
