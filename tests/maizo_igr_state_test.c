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
    for(int i=0;i<30;i++)maizoIgrStep(&s,MZ_KEY_RIGHT);
    assert(s.width==8);
    for(int i=0;i<40;i++)maizoIgrStep(&s,MZ_KEY_LEFT);
    assert(s.width==-8);
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
    maizoIgrStep(&s,MZ_KEY_DOWN);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(((s.draw_modes>>4)&3u)==1);
    maizoIgrStep(&s,MZ_KEY_DOWN);
    assert(s.selected==6);
    maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.page==MZ_PAGE_GS_STATUS && s.selected==0);
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
    maizoIgrStep(&s,MZ_KEY_RIGHT);assert(s.width==1&&s.changed);
    maizoIgrStep(&s,MZ_KEY_LEFT);maizoIgrStep(&s,MZ_KEY_LEFT);
    assert(s.width==-1&&s.changed);
    s.selected=5;s.changed=0;
    maizoIgrStep(&s,MZ_KEY_OK);assert(!s.field_fix&&!s.changed);
    s.selected=4;
    maizoIgrStep(&s,MZ_KEY_OK);assert(!s.width&&!s.height);
    maizoIgrStep(&s,MZ_KEY_OK);assert(s.width==-6&&s.height==-6);
    s.selected=7;
    maizoIgrStep(&s,MZ_KEY_OK);assert(!s.width&&!s.height&&!s.x&&!s.y);
    puts("MAIZO IGR navigation + verified-hook draw toggle gating OK");return 0;
}
