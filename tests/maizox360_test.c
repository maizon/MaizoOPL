#include <assert.h>
#include <stdio.h>
#include "maizox360.h"

int main(void)
{
    struct ds2report d;
    uint8_t r[20] = {0, 0x14};
    const uint8_t cfg[] = {
        9, 2, 33, 0, 1, 1, 0, 0x80, 0xfa,
        9, 4, 0, 0, 2, 0xff, 0x5d, 1, 0,
        8, 0x21, 0, 0, 0, 0, 0, 0,
        7, 5, 0x81, 3, 32, 0, 4};
    assert(maizox360_input_offset(cfg, sizeof(cfg)) == 26);
    assert(maizox360_input_offset(cfg, sizeof(cfg) - 1) == -1);
    assert(maizox360_decode(r, sizeof(r), &d));
    assert(d.nButtonState == 0xffff && d.LeftStickX == 128 && d.LeftStickY == 127);
    r[3] = 0x10;
    assert(maizox360_decode(r, sizeof(r), &d));
    assert(d.nButtonState == (uint16_t)~DS2ButtonCross);
    r[2] = 0x0a;
    r[3] = 0x50;
    r[4] = 255;
    r[6] = 0;
    r[7] = 0x80;
    r[10] = 0xff;
    r[11] = 0x7f;
    assert(maizox360_decode(r, sizeof(r), &d));
    assert(d.nButtonState == (uint16_t)~(DS2ButtonDown | DS2ButtonRight | DS2ButtonCross | DS2ButtonSquare | DS2ButtonL2));
    assert(d.LeftStickX == 0 && d.RightStickX == 255 && d.PressureL2 == 255);
    assert(!maizox360_decode(r, 19, &d));
    r[0] = 1;
    assert(!maizox360_decode(r, 20, &d));
    puts("Xbox 360 capture decoder and endpoint tests passed");
    return 0;
}
