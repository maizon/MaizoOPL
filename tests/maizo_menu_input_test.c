#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "maizo_menu_input.h"

static void make_snapshot(unsigned char s[16])
{
    unsigned int p;
    memset(s, 0, 16);
    s[8] = MAIZO_MENU_MAGIC0;
    s[9] = MAIZO_MENU_MAGIC1;
    s[10] = MAIZO_MENU_VERSION;
    for (p = 0; p < MAIZO_MENU_SLOTS; p++) {
        s[p * 4] = s[p * 4 + 1] = 0xff;
        s[p * 4 + 2] = s[p * 4 + 3] = 0x7f;
    }
}

int main(void)
{
    unsigned char s[16], junk[16] = {0};
    unsigned int armed = 0;

    /* Exact reported regression: an empty USB RPC response formerly
     * produced "all buttons pressed", trapping the config category list. */
    assert(maizoMenuPadValidMask(junk) == 0);
    assert(maizoMenuPadMask(junk, 1, 1, 3) == 0);
    make_snapshot(s);
    assert(maizoMenuPadValidMask(s) == 0);
    assert(maizoMenuPadMask(s, 1, 1, 3) == 0);

    s[11] = 3;
    armed |= maizoMenuPadNeutralMask(s);
    assert(armed == 3);
    assert(maizoMenuPadMask(s, 1, 1, armed) == 0);

    s[4] = 0xf7; /* START P2 */
    assert((maizoMenuPadMask(s, 1, 1, armed) & 8) != 0);
    s[4] = 0xff;
    s[1] = 0xbf; /* CROSS P1 */
    assert((maizoMenuPadMask(s, 1, 1, armed) & 0x4000) != 0);
    s[1] = 0xff;
    s[7] = 255; /* P2 stick down */
    assert((maizoMenuPadMask(s, 1, 1, armed) & 0x40) != 0);
    s[7] = 0x7f;

    /* Two valid player slots, not four (driver MAX_PADS==2). */
    s[11] = 0;
    assert(maizoMenuPadMask(s, 1, 1, armed) == 0);
    s[11] = 2;
    s[4] = 0;
    s[5] = 0; /* Impossible all-16 report must be rejected */
    assert(maizoMenuPadValidMask(s) == 0);
    assert(maizoMenuPadMask(s, 1, 1, armed) == 0);
    s[4] = s[5] = 0xff;
    s[10] = 0xff; /* Wrong RPC version cannot issue input */
    assert(maizoMenuPadValidMask(s) == 0);

    puts("MAIZO: fail-closed HID, P1/P2, neutral arming and ghost DOWN tests OK");
    return 0;
}
