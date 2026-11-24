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
    /* Choice is independent of capability; actual mode is not modified
     * by the UI state machine until the verified renderer applies it. */
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(mzIntentGet(s.draw_modes,6)==MZ_INTENT_AUTO && s.filter_changed && !s.filter_mode);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(mzIntentGet(s.draw_modes,6)==MZ_INTENT_FORCE);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(!mzIntentGet(s.draw_modes,6));
    maizoIgrStep(&s,MZ_KEY_OK);
    maizoIgrStep(&s,MZ_KEY_DOWN);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(mzIntentGet(s.draw_modes,5)==MZ_INTENT_AUTO && !mzIntentGet(s.draw_modes,6));
    maizoIgrStep(&s,MZ_KEY_DOWN);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(mzIntentGet(s.draw_modes,6)==MZ_INTENT_AUTO && !mzIntentGet(s.draw_modes,5));
    maizoIgrStep(&s,MZ_KEY_DOWN);
    assert(s.selected==3 && !s.draw_caps);
    for(unsigned i=1;i<=3;i++){
        maizoIgrStep(&s,MZ_KEY_OK);
        assert(mzIntentGet(s.draw_modes,0)==i%3u && (s.draw_changed&1u));
    }
    maizoIgrStep(&s,MZ_KEY_LEFT);
    assert(mzIntentGet(s.draw_modes,0)==MZ_INTENT_ALT);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(mzIntentGet(s.draw_modes,0)==MZ_INTENT_OFF);
    maizoIgrStep(&s,MZ_KEY_DOWN);
    for(unsigned i=1;i<=3;i++){
        maizoIgrStep(&s,MZ_KEY_OK);
        assert(mzIntentGet(s.draw_modes,1)==i%3u);
    }
    maizoIgrStep(&s,MZ_KEY_DOWN);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(mzIntentGet(s.draw_modes,2)==MZ_INTENT_AUTO);
    maizoIgrStep(&s,MZ_KEY_DOWN);
    assert(s.selected==6);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.page==MZ_PAGE_GS_STATUS && s.selected==0);
    for(unsigned j=3;j<=4;j++){
        maizoIgrStep(&s,MZ_KEY_DOWN);
        for(unsigned i=1;i<=3;i++){
            maizoIgrStep(&s,MZ_KEY_OK);
            assert(mzIntentGet(s.draw_modes,j)==i%3u && (s.draw_changed&(1u<<j)));
        }
        maizoIgrStep(&s,MZ_KEY_LEFT);
        assert(mzIntentGet(s.draw_modes,j)==MZ_INTENT_ALT);
        maizoIgrStep(&s,MZ_KEY_OK);
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
    for(unsigned i=1;i<=3;i++){
        maizoIgrStep(&s,MZ_KEY_OK);
        assert(s.effect_choice==1 && mzIntentGet(s.effect_modes,0)==i%3u);
    }
    s.selected=6;
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.effect_choice==7 && mzIntentGet(s.effect_modes,6)==MZ_INTENT_AUTO);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(mzIntentGet(s.effect_modes,6)==MZ_INTENT_FORCE);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(!mzIntentGet(s.effect_modes,6));
    s.selected=7;maizoIgrStep(&s,MZ_KEY_OK);assert(s.page==MZ_PAGE_EFFECTS_2);
    s.selected=6;maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.effect_choice==14 && mzIntentGet(s.effect_modes,13)==MZ_INTENT_AUTO);
    maizoIgrStep(&s,MZ_KEY_BACK);assert(s.page==MZ_PAGE_GS_STATUS&&s.selected==3);
    puts("MAIZO IGR navigation + independent pending filter choices OK");return 0;
}
