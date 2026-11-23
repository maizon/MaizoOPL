#include <assert.h>
#include <stdio.h>
#include "maizo_igr_state.h"
int main(void) {
    maizo_igr_state_t s;maizoIgrStateInit(&s,1);
    assert(s.field_fix&&s.page==MZ_PAGE_MAIN);
    maizoIgrStep(&s,MZ_KEY_DOWN);maizoIgrStep(&s,MZ_KEY_OK);
    assert(s.page==MZ_PAGE_VIDEO);
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
    maizoIgrStep(&s,MZ_KEY_BACK);assert(s.page==MZ_PAGE_MAIN);
    puts("MAIZO IGR navigation + filters ON/OFF safety OK");return 0;
}
