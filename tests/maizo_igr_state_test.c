#include <assert.h>
#include <stdio.h>
#include "maizo_igr_state.h"
int main(void) {
    maizo_igr_state_t s;maizoIgrStateInit(&s,1);
    assert(s.field_fix&&s.page==MZ_PAGE_MAIN);
    maizoIgrStep(&s,MZ_KEY_DOWN);maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.page==MZ_PAGE_VIDEO);
    /* GSM OFF: menu works, GSM-dependent controls are read-only. */
    maizoIgrStep(&s,MZ_KEY_RIGHT);
    assert(s.width==0 && !s.changed);
    for(int i=0;i<4;i++)maizoIgrStep(&s,MZ_KEY_DOWN);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.width==0 && !s.changed);
    maizoIgrStep(&s,MZ_KEY_DOWN);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(!s.changed);
    s.selected=0;
    s.video_caps=1;
    for(int i=0;i<40;i++)maizoIgrStep(&s,MZ_KEY_RIGHT);
    assert(s.width==31);
    for(int i=0;i<80;i++)maizoIgrStep(&s,MZ_KEY_LEFT);
    assert(s.width==-32);
    maizoIgrStep(&s,MZ_KEY_BACK);
    assert(s.page==MZ_PAGE_MAIN);
    maizoIgrStep(&s,MZ_KEY_UP);maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.page==MZ_PAGE_SYSTEM&&s.selected==1);
    assert(maizoIgrStep(&s,MZ_KEY_OK)==MZ_ACTION_NONE);
    assert(maizoIgrStep(&s,MZ_KEY_OK)==MZ_ACTION_EXIT);
    maizoIgrStateInit(&s,0);
    maizoIgrStep(&s,MZ_KEY_DOWN);maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.page==MZ_PAGE_VIDEO);
    for(int i=0;i<6;i++)maizoIgrStep(&s,MZ_KEY_DOWN);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.page==MZ_PAGE_FILTERS);
    /* No fake on/off if hardware output is incompatible. */
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.filter_mode==0);
    s.filter_caps=2;
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.filter_mode==2 && s.filter_changed);
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.filter_mode==0);
    maizoIgrStep(&s,MZ_KEY_DOWN);s.filter_caps=1;
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.filter_mode==1);
    /* Drawing filters are unavailable unless an adapter was registered. */
    for(int i=0;i<2;i++)maizoIgrStep(&s,MZ_KEY_DOWN);
    assert(s.selected==3);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.draw_modes==0 && s.draw_changed==0);
    s.draw_caps=1|4; /* AA1 and dithering, NOT bilinear */
    maizoIgrStep(&s,MZ_KEY_OK);
    assert((s.draw_modes&3u)==1 && (s.draw_changed&1u));
    maizoIgrStep(&s,MZ_KEY_OK);
    assert((s.draw_modes&3u)==2);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert((s.draw_modes&3u)==0);
    maizoIgrStep(&s,MZ_KEY_DOWN);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(((s.draw_modes>>2)&3u)==0); /* No phantom bilinear toggle */
    s.draw_caps |= 2u;
    for (int i=1;i<=4;i++) {
        maizoIgrStep(&s,MZ_KEY_OK);
        assert(((s.draw_modes>>2)&3u)==(unsigned)(i%4));
    }
    maizoIgrStep(&s,MZ_KEY_DOWN);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(((s.draw_modes>>4)&3u)==1);
    maizoIgrStep(&s,MZ_KEY_DOWN);
    assert(s.selected==6);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.page==MZ_PAGE_GS_STATUS && s.selected==0);
    maizoIgrStep(&s,MZ_KEY_DOWN);
    maizoIgrStep(&s,MZ_KEY_OK);assert(!(s.draw_changed&8u));
    s.draw_caps|=8u;
    for(int i=1;i<=4;i++){
        maizoIgrStep(&s,MZ_KEY_OK);
        assert(((s.draw_modes>>6)&3u)==(unsigned)(i%4));
    }
    maizoIgrStep(&s,MZ_KEY_DOWN);
    assert(s.selected==2);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(!(s.draw_changed&16u));
    s.draw_caps|=16u;
    for(unsigned i=1;i<=3;i++){
        maizoIgrStep(&s,MZ_KEY_OK);
        assert(((s.draw_modes>>8)&3u)==i%3u);
    }
    maizoIgrStep(&s,MZ_KEY_BACK);
    assert(s.page==MZ_PAGE_FILTERS && s.selected==6);
    maizoIgrStep(&s,MZ_KEY_DOWN);
    assert(s.selected==7);
    /* Full-frame NTSC bridge is disabled unless the running game is
     * verifiably a native 480p renderer with compatible GS scanout. */
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(!s.frame480i_mode && !s.frame480i_changed);
    s.frame480i_caps=1;
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.frame480i_mode && s.frame480i_changed);
    s.frame480i_changed=0;
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(!s.frame480i_mode && s.frame480i_changed);
    maizoIgrStep(&s,MZ_KEY_DOWN);
    assert(s.selected==8);
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.page==MZ_PAGE_VIDEO);
    maizoIgrStep(&s,MZ_KEY_BACK);assert(s.page==MZ_PAGE_MAIN);
    maizoIgrStateInit(&s,0);
    s.page=MZ_PAGE_VIDEO;s.video_caps=MZ_VIDEO_NATIVE;
    maizoIgrStep(&s,MZ_KEY_RIGHT);assert(s.width==0&&!s.changed);
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.width==1&&s.changed);
    maizoIgrStep(&s,MZ_KEY_LEFT);assert(s.width==1);
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.width==0);
    maizoIgrStep(&s,MZ_KEY_LEFT);assert(s.width==-1&&s.changed);
    maizoIgrStep(&s,MZ_KEY_RIGHT|0x0800u);assert(s.width==0);
    maizoIgrStep(&s,MZ_KEY_LEFT);assert(s.width==-1);
    s.selected=5;s.changed=0;
    maizoIgrStep(&s,MZ_KEY_OK);assert(!s.field_fix&&!s.changed);
    s.native_field_caps=1;
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.field_fix&&s.field_changed&&!s.changed);
    s.field_changed=0;
    maizoIgrStep(&s,MZ_KEY_LEFT);
    assert(!s.field_fix&&s.field_changed&&!s.changed);
    s.selected=4;
    maizoIgrStep(&s,MZ_KEY_OK);assert(!s.width&&!s.height);
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.width==-12&&s.height==-12);
    s.selected=7;s.legacy_geometry=1;
    maizoIgrStep(&s,MZ_KEY_OK);assert(!s.width&&!s.height&&!s.x&&!s.y&&!s.legacy_geometry);
    maizoIgrStateInit(&s,0);s.page=MZ_PAGE_GAME;
    maizoIgrStep(&s,MZ_KEY_OK);assert(!s.cheat_changed&&!s.cheat_paused);
    s.cheat_caps=1;
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.cheat_changed&&s.cheat_paused);
    s.cheat_changed=0;
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.cheat_changed&&!s.cheat_paused);
    s.live_count=2;s.selected=1;
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.page==MZ_PAGE_CHEATS);
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.live_choice==1);
    maizoIgrStep(&s,MZ_KEY_UP);assert(s.selected==2);
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.page==MZ_PAGE_GAME);
    maizoIgrStateInit(&s,0);s.page=MZ_PAGE_GS_STATUS;s.selected=3;
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.page==MZ_PAGE_EFFECTS_1&&s.selected==0);
    maizoIgrStep(&s,MZ_KEY_OK);assert(!s.effect_choice&&!s.effect_modes);
    s.effect_caps=(1u<<MZ_EFFECT_MOTION_BLUR)|(1u<<MZ_EFFECT_FOG)|(1u<<MZ_EFFECT_REFLECTIONS);
    s.effect_reduced=1u<<MZ_EFFECT_MOTION_BLUR;
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.effect_choice==1&&mzEffectMode(s.effect_modes,0)==MZ_EFFECT_REDUCED);
    maizoIgrStep(&s,MZ_KEY_OK);assert(mzEffectMode(s.effect_modes,0)==MZ_EFFECT_OFF);
    maizoIgrStep(&s,MZ_KEY_OK);assert(mzEffectMode(s.effect_modes,0)==MZ_EFFECT_ORIGINAL);
    s.selected=6;maizoIgrStep(&s,MZ_KEY_OK);assert(mzEffectMode(s.effect_modes,MZ_EFFECT_FOG)==MZ_EFFECT_OFF);
    maizoIgrStep(&s,MZ_KEY_OK);assert(mzEffectMode(s.effect_modes,MZ_EFFECT_FOG)==0);
    s.selected=7;maizoIgrStep(&s,MZ_KEY_OK);assert(s.page==MZ_PAGE_EFFECTS_2);
    s.selected=6;maizoIgrStep(&s,MZ_KEY_OK);assert(s.effect_choice==14&&mzEffectMode(s.effect_modes,13)==MZ_EFFECT_OFF);
    maizoIgrStep(&s,MZ_KEY_BACK);assert(s.page==MZ_PAGE_GS_STATUS&&s.selected==3);
    puts("MAIZO IGR navigation + verified-hook draw toggle gating OK");return 0;
}
