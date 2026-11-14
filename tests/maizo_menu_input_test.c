#include <assert.h>
#include <stdio.h>
#include "maizo_menu_input.h"
int main(void)
{
    unsigned char s[16];
    unsigned int i;
    for (i = 0; i < 16; i += 4) {
        s[i] = s[i + 1] = 0xFF;
        s[i + 2] = s[i + 3] = 0x7F;
    }
    assert(maizoMenuPadMask(s, 1, 1) == 0);
    s[4] = 0xF7; /* START on P2 */
    assert((maizoMenuPadMask(s, 1, 1) & 0x08) != 0);
    s[4] = 0xFF;
    s[8] = 0xBF; /* multitap slot 3 Down */
    assert((maizoMenuPadMask(s, 1, 1) & 0x40) != 0);
    s[8] = 0xFF;
    s[14] = 5; /* X axis on fourth slot */
    assert((maizoMenuPadMask(s, 1, 1) & 0x80) != 0);
    s[14] = 0x7F;
    s[15] = 254;
    assert((maizoMenuPadMask(s, 2, 2) & 0x40) != 0);
    puts("MAIZO: controller P1/P2/multitap menu snapshot tests OK");
    return 0;
}
