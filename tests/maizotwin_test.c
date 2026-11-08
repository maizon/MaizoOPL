#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "maizotwin.h"

int main(void)
{
    struct ds2report players[2];
    uint8_t first[8] = {1, 128, 128, 128, 128, 8, 0, 0};
    uint8_t second[8] = {2, 64, 192, 48, 208, 8, 0, 0};
    uint8_t broken[7] = {1, 128, 128, 128, 128, 8, 0};

    memset(players, 0, sizeof(players));
    assert(maizotwin_decode(first, sizeof(first), &players[0]) == 1);
    assert(maizotwin_decode(second, sizeof(second), &players[1]) == 2);
    assert(players[0].nButtonState == 0xFFFF);
    assert(players[1].nButtonState == 0xFFFF);
    assert(players[1].LeftStickX == 48 && players[1].RightStickY == 64);
    assert(players[0].LeftStickX == 128);
    first[5] = 0x20 | 0x01;  /* P1 Cross + Up/Right */
    second[5] = 0x40 | 0x06; /* P2 Circle + Left */
    second[6] = 0x21;        /* P2 L1 + Start */
    assert(maizotwin_decode(first, sizeof(first), &players[0]) == 1);
    assert(maizotwin_decode(second, sizeof(second), &players[1]) == 2);
    assert(!players[0].nCross && !players[0].nUp && !players[0].nRight);
    assert(players[0].nCircle && players[0].nStart);
    assert(!players[1].nCircle && !players[1].nLeft && !players[1].nStart);
    assert(!players[1].nL1 && players[1].nCross);
    assert(players[0].PressureCross == 255 && players[0].PressureCircle == 0);
    assert(players[1].PressureCircle == 255 && players[1].PressureCross == 0);
    assert(!maizotwin_decode(broken, sizeof(broken), &players[0]));
    first[0] = 3;
    assert(!maizotwin_decode(first, sizeof(first), &players[0]));
    puts("Maizo Twin USB: 0810:0001 dual-report tests passed");
    return 0;
}
