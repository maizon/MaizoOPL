#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "maizohid.h"

/* Synthetic USB HID report: X/Y/Z/Rx, 4-bit hat, 12 buttons. */
static const uint8_t descriptor[] = {
    0x05, 0x01, 0x09, 0x04, 0xA1, 0x01, /* Joystick */
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x04,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x33, 0x81, 0x02,
    0x25, 0x07, 0x75, 0x04, 0x95, 0x01, 0x09, 0x39, 0x81, 0x42,
    0x75, 0x04, 0x95, 0x01, 0x81, 0x01, /* nibble padding */
    0x05, 0x09, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0C,
    0x19, 0x01, 0x29, 0x0C, 0x81, 0x02,
    0x75, 0x04, 0x95, 0x01, 0x81, 0x03, 0xC0};

int main(void)
{
    maizohid_layout layout;
    struct ds2report pad;
    uint8_t r[] = {128, 128, 128, 128, 8, 0, 0};

    assert(maizohid_parse(&layout, descriptor, sizeof(descriptor)));
    assert(layout.report_bits == sizeof(r) * 8);
    assert(maizohid_decode(&layout, r, sizeof(r), &pad));
    assert(pad.nButtonState == 0xFFFF);
    assert(pad.LeftStickX == 128 && pad.RightStickX == 128);
    assert(pad.PressureCross == 0 && pad.PressureUp == 0);

    r[4] = 0;    /* up */
    r[5] = 0x04; /* button #3 = cross */
    assert(maizohid_decode(&layout, r, sizeof(r), &pad));
    assert(!pad.nUp && !pad.nCross && pad.nDown && pad.nRight);
    assert(pad.PressureUp == 255 && pad.PressureCross == 255);

    r[4] = 0x0F; /* other common neutral hat */
    r[5] = 0;
    assert(maizohid_decode(&layout, r, sizeof(r), &pad));
    assert(pad.nButtonState == 0xFFFF);
    assert(!maizohid_decode(&layout, r, sizeof(r) - 1, &pad));
    assert(!maizohid_parse(&layout, descriptor, 1));

    /* Gamepad uses report ID 1, while an independent FEATURE uses report ID 2.
     * A second report ID must not make a valid INPUT layout fail. */
    {
        uint8_t multi[sizeof(descriptor) + 10];
        uint8_t numbered[sizeof(r) + 1] = {1, 128, 128, 128, 128, 8, 0, 0};
        memcpy(multi, descriptor, 6);
        multi[6] = 0x85;
        multi[7] = 0x01;
        memcpy(multi + 8, descriptor + 6, sizeof(descriptor) - 7);
        multi[sizeof(descriptor) + 1] = 0x85;
        multi[sizeof(descriptor) + 2] = 0x02;
        multi[sizeof(descriptor) + 3] = 0x75;
        multi[sizeof(descriptor) + 4] = 0x08;
        multi[sizeof(descriptor) + 5] = 0x95;
        multi[sizeof(descriptor) + 6] = 0x01;
        multi[sizeof(descriptor) + 7] = 0xB1;
        multi[sizeof(descriptor) + 8] = 0x02;
        multi[sizeof(descriptor) + 9] = 0xC0;
        assert(maizohid_parse(&layout, multi, sizeof(multi)));
        assert(layout.has_report_id && layout.report_id == 1);
        assert(layout.report_bits == sizeof(numbered) * 8);
        assert(maizohid_decode(&layout, numbered, sizeof(numbered), &pad));
        numbered[0] = 2;
        assert(!maizohid_decode(&layout, numbered, sizeof(numbered), &pad));
    }

    puts("MaizoHID: HID descriptor + DS2 translator tests passed");
    return 0;
}
