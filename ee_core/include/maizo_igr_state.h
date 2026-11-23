/* MAIZO In-Game PRO state model, fixed footprint, host-testable. */
#ifndef MAIZO_IGR_STATE_H
#define MAIZO_IGR_STATE_H
enum { MZ_PAGE_MAIN, MZ_PAGE_VIDEO, MZ_PAGE_PADS, MZ_PAGE_GAME, MZ_PAGE_SYSTEM, MZ_PAGE_FILTERS };
enum { MZ_ACTION_NONE, MZ_ACTION_RESUME, MZ_ACTION_EXIT, MZ_ACTION_POWEROFF };
enum { MZ_KEY_UP=0x0010, MZ_KEY_DOWN=0x0040, MZ_KEY_LEFT=0x0080, MZ_KEY_RIGHT=0x0020,
       MZ_KEY_OK=0x4000, MZ_KEY_BACK=0x2000, MZ_KEY_START=0x0008 };
typedef struct { int page, selected, confirm, field_fix; int width, height, x, y, changed;
             int filter_mode, filter_caps, filter_changed;
             unsigned int draw_caps, draw_modes, draw_changed; } maizo_igr_state_t;
static void maizoIgrStateInit(maizo_igr_state_t *s,int field_fix)
{
    s->page=MZ_PAGE_MAIN;s->selected=0;s->confirm=0;s->field_fix=!!field_fix;
    s->width=s->height=s->x=s->y=0;s->changed=0;
    s->filter_mode=s->filter_caps=s->filter_changed=0;
    s->draw_caps=s->draw_modes=s->draw_changed=0;
}
static int maizoIgrPageCount(int page)
{
    switch(page) {
    case MZ_PAGE_MAIN: return 6;
    case MZ_PAGE_VIDEO: return 9;
    case MZ_PAGE_PADS: return 2;
    case MZ_PAGE_GAME: return 3;
    case MZ_PAGE_SYSTEM: return 4;
    case MZ_PAGE_FILTERS: return 7;
    default: return 1;
    }
}
static int maizoIgrBound(int v,int low,int high) {return v<low?low:v>high?high:v;}
static int maizoIgrStep(maizo_igr_state_t *s,unsigned int pressed)
{
    int *v=0,min=0,max=0,delta=0,n=maizoIgrPageCount(s->page);
    if(pressed&MZ_KEY_START)return MZ_ACTION_RESUME;
    if(pressed&MZ_KEY_BACK) {
        if(s->page==MZ_PAGE_MAIN)return MZ_ACTION_RESUME;
        s->page=MZ_PAGE_MAIN;s->selected=0;s->confirm=0;return MZ_ACTION_NONE;
    }
    if(pressed&(MZ_KEY_UP|MZ_KEY_DOWN)) {
        s->selected=(s->selected+(pressed&MZ_KEY_UP?n-1:1))%n;
        s->confirm=0;return MZ_ACTION_NONE;
    }
    if(s->page==MZ_PAGE_VIDEO&&(pressed&(MZ_KEY_LEFT|MZ_KEY_RIGHT))) {
        delta=pressed&MZ_KEY_RIGHT?1:-1;
        switch(s->selected) {
        case 0:v=&s->width;min=-8;max=8;break;
        case 1:v=&s->height;min=-8;max=8;break;
        case 2:v=&s->x;min=-24;max=24;break;
        case 3:v=&s->y;min=-24;max=24;break;
        case 5:s->field_fix^=1;s->changed=1;return MZ_ACTION_NONE;
        default:return MZ_ACTION_NONE;
        }
        if(v){int next=maizoIgrBound(*v+delta,min,max);if(next!=*v){*v=next;s->changed=1;}}
        return MZ_ACTION_NONE;
    }
    if(!(pressed&MZ_KEY_OK))return MZ_ACTION_NONE;
    if(s->page==MZ_PAGE_MAIN) {
        if(s->selected==0)return MZ_ACTION_RESUME;
        if(s->selected==5){s->page=MZ_PAGE_SYSTEM;s->selected=1;return MZ_ACTION_NONE;}
        s->page=s->selected;s->selected=0;return MZ_ACTION_NONE;
    }
    if(s->page==MZ_PAGE_VIDEO) {
        if(s->selected==4){s->width=s->width==0?6:0;s->changed=1;}
        else if(s->selected==5){s->field_fix^=1;s->changed=1;}
        else if(s->selected==6){s->page=MZ_PAGE_FILTERS;s->selected=0;}
        else if(s->selected==7){s->width=s->height=s->x=s->y=0;s->changed=1;}
        else if(s->selected==8){s->page=MZ_PAGE_MAIN;s->selected=1;}
    }else if(s->page==MZ_PAGE_FILTERS){
        int target=s->filter_mode;
        if(s->selected==0 || s->selected==2){
            /* Deflicker and Smooth are the SAME low-cost GS scanout filter. */
            target=(target==2)?0:2;
        }else if(s->selected==1){
            target=(target==1)?0:1;
        }else if(s->selected>=3 && s->selected<=5){
            unsigned int bit=1u<<(s->selected-3);
            unsigned int shift=2u*(s->selected-3);
            if(s->draw_caps&bit){
                unsigned int old=(s->draw_modes>>shift)&3u;
                unsigned int next=(old+1u)%3u; /* GAME -> ON -> OFF */
                s->draw_modes=(s->draw_modes&~(3u<<shift))|(next<<shift);
                s->draw_changed|=bit;
            }
            return MZ_ACTION_NONE;
        }else if(s->selected==6){
            s->page=MZ_PAGE_VIDEO;s->selected=6;
            return MZ_ACTION_NONE;
        }
        /* Never offer fake effects in unsupported video modes. */
        if(target!=s->filter_mode &&
           (target==0 || (s->filter_caps & target)==target)){
            s->filter_mode=target;
            s->filter_changed=1;
        }
    }else if(s->page==MZ_PAGE_PADS){
        if(s->selected==1){s->page=MZ_PAGE_MAIN;s->selected=2;}
    }else if(s->page==MZ_PAGE_GAME){
        if(s->selected==2){s->page=MZ_PAGE_MAIN;s->selected=3;}
    }else if(s->page==MZ_PAGE_SYSTEM){
        if(s->selected==0)return MZ_ACTION_RESUME;
        if(s->selected==1||s->selected==2){
            if(s->confirm==s->selected+1)
                return s->selected==1?MZ_ACTION_EXIT:MZ_ACTION_POWEROFF;
            s->confirm=s->selected+1;
        }else if(s->selected==3){s->page=MZ_PAGE_MAIN;s->selected=4;}
    }
    return MZ_ACTION_NONE;
}
#endif
