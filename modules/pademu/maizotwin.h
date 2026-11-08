#ifndef _MAIZOTWIN_H_
#define _MAIZOTWIN_H_

#include <ds34common.h>

/* User's 0810:0001 Twin USB Joystick: one HID interface and two report IDs.
 * Captured packets: [01/02, RY, RX, LX, LY, hat/buttons, buttons, reserved].
 * These reports have a fixed 8-byte format, unlike generic HID descriptors. */
#define MAIZOTWIN_VID    0x0810
#define MAIZOTWIN_PID    0x0001
#define MAIZOTWIN_PACKET 8
#define TWINHID          5

static inline int maizotwin_decode(const uint8_t *r, unsigned int len, struct ds2report *out)
{
    uint16_t pressed = 0;
    uint8_t hat;
    uint8_t face;
    uint8_t shoulder;

    if (!r || !out || len < MAIZOTWIN_PACKET || (r[0] != 1 && r[0] != 2))
        return 0;

    face = r[5];
    shoulder = r[6];
    hat = face & 0x0F;

    switch (hat) {
        case 0:
            pressed |= DS2ButtonUp;
            break;
        case 1:
            pressed |= DS2ButtonUp | DS2ButtonRight;
            break;
        case 2:
            pressed |= DS2ButtonRight;
            break;
        case 3:
            pressed |= DS2ButtonRight | DS2ButtonDown;
            break;
        case 4:
            pressed |= DS2ButtonDown;
            break;
        case 5:
            pressed |= DS2ButtonDown | DS2ButtonLeft;
            break;
        case 6:
            pressed |= DS2ButtonLeft;
            break;
        case 7:
            pressed |= DS2ButtonLeft | DS2ButtonUp;
            break;
        default:
            break; /* 8 / 15: neutral */
    }

    if (face & 0x10)
        pressed |= DS2ButtonSquare;
    if (face & 0x20)
        pressed |= DS2ButtonCross;
    if (face & 0x40)
        pressed |= DS2ButtonCircle;
    if (face & 0x80)
        pressed |= DS2ButtonTriangle;
    if (shoulder & 0x01)
        pressed |= DS2ButtonL1;
    if (shoulder & 0x02)
        pressed |= DS2ButtonR1;
    if (shoulder & 0x04)
        pressed |= DS2ButtonL2;
    if (shoulder & 0x08)
        pressed |= DS2ButtonR2;
    if (shoulder & 0x10)
        pressed |= DS2ButtonSelect;
    if (shoulder & 0x20)
        pressed |= DS2ButtonStart;

    out->nButtonState = (uint16_t)~pressed;
    out->LeftStickX = r[3];
    out->LeftStickY = r[4];
    out->RightStickX = r[2];
    out->RightStickY = r[1];
    out->PressureUp = (pressed & DS2ButtonUp) ? 255 : 0;
    out->PressureRight = (pressed & DS2ButtonRight) ? 255 : 0;
    out->PressureDown = (pressed & DS2ButtonDown) ? 255 : 0;
    out->PressureLeft = (pressed & DS2ButtonLeft) ? 255 : 0;
    out->PressureTriangle = (pressed & DS2ButtonTriangle) ? 255 : 0;
    out->PressureCircle = (pressed & DS2ButtonCircle) ? 255 : 0;
    out->PressureCross = (pressed & DS2ButtonCross) ? 255 : 0;
    out->PressureSquare = (pressed & DS2ButtonSquare) ? 255 : 0;
    out->PressureL1 = (pressed & DS2ButtonL1) ? 255 : 0;
    out->PressureR1 = (pressed & DS2ButtonR1) ? 255 : 0;
    out->PressureL2 = (pressed & DS2ButtonL2) ? 255 : 0;
    out->PressureR2 = (pressed & DS2ButtonR2) ? 255 : 0;
    return r[0]; /* logical gamepad number: 1 or 2 */
}

#endif
