#ifndef MAIZOX360_H
#define MAIZOX360_H
#include <ds34common.h>

#define MAIZOX360_VID    0x045E
#define MAIZOX360_PID    0x028E
#define MAIZOX360_PACKET 20
#define MAIZOX360        6

/* Captured on 2026-11-20: 19,899 identical-length reports, 00 14 prefix.
 * The wireless clone exposes the wired Xbox 360 USB protocol.
 * Input is active-high; PS2 buttons are active-low. */
static inline uint8_t maizox360_axis(const uint8_t *r, int invert)
{
    unsigned int raw = r[0] | ((unsigned int)r[1] << 8);
    unsigned int biased = raw ^ 0x8000u;
    return (uint8_t)((invert ? 65535u - biased : biased) >> 8);
}

static inline int maizox360_decode(const uint8_t *r, unsigned int len, struct ds2report *out)
{
    uint16_t p = 0;
    if (!r || !out || len != MAIZOX360_PACKET || r[0] != 0 || r[1] != MAIZOX360_PACKET)
        return 0;

    if (r[2] & 0x01)
        p |= DS2ButtonUp;
    if (r[2] & 0x02)
        p |= DS2ButtonDown;
    if (r[2] & 0x04)
        p |= DS2ButtonLeft;
    if (r[2] & 0x08)
        p |= DS2ButtonRight;
    if (r[2] & 0x10)
        p |= DS2ButtonStart;
    if (r[2] & 0x20)
        p |= DS2ButtonSelect;
    if (r[2] & 0x40)
        p |= DS2ButtonL3;
    if (r[2] & 0x80)
        p |= DS2ButtonR3;
    if (r[3] & 0x01)
        p |= DS2ButtonL1;
    if (r[3] & 0x02)
        p |= DS2ButtonR1;
    if (r[3] & 0x10)
        p |= DS2ButtonCross;
    if (r[3] & 0x20)
        p |= DS2ButtonCircle;
    if (r[3] & 0x40)
        p |= DS2ButtonSquare;
    if (r[3] & 0x80)
        p |= DS2ButtonTriangle;
    /* Guide has no equivalent DualShock 2 button. Do not synthesize START. */
    if (r[4] >= 30)
        p |= DS2ButtonL2;
    if (r[5] >= 30)
        p |= DS2ButtonR2;

    out->nButtonState = (uint16_t)~p;
    out->LeftStickX = maizox360_axis(r + 6, 0);
    out->LeftStickY = maizox360_axis(r + 8, 1);
    out->RightStickX = maizox360_axis(r + 10, 0);
    out->RightStickY = maizox360_axis(r + 12, 1);
    out->PressureRight = (p & DS2ButtonRight) ? 255 : 0;
    out->PressureLeft = (p & DS2ButtonLeft) ? 255 : 0;
    out->PressureUp = (p & DS2ButtonUp) ? 255 : 0;
    out->PressureDown = (p & DS2ButtonDown) ? 255 : 0;
    out->PressureTriangle = (p & DS2ButtonTriangle) ? 255 : 0;
    out->PressureCircle = (p & DS2ButtonCircle) ? 255 : 0;
    out->PressureCross = (p & DS2ButtonCross) ? 255 : 0;
    out->PressureSquare = (p & DS2ButtonSquare) ? 255 : 0;
    out->PressureL1 = (p & DS2ButtonL1) ? 255 : 0;
    out->PressureR1 = (p & DS2ButtonR1) ? 255 : 0;
    out->PressureL2 = r[4];
    out->PressureR2 = r[5];
    return 1;
}

/* Parse only interface 0, alternate 0, FF/5D/01 and its 81 IN endpoint.
 * The audio/accessory interfaces (82,83,86) are deliberately ignored. */
static inline int maizox360_input_offset(const uint8_t *cfg, unsigned int len)
{
    unsigned int off;
    int selected = 0;
    if (!cfg || len < 9 || cfg[0] < 9 || cfg[0] > len || cfg[1] != 2)
        return -1;
    for (off = cfg[0]; off + 2 <= len;) {
        unsigned int n = cfg[off];
        const uint8_t *d = cfg + off;
        if (n < 2 || n > len - off)
            return -1;
        if (d[1] == 4)
            selected = n >= 9 && d[2] == 0 && d[3] == 0 &&
                       d[5] == 0xff && d[6] == 0x5d && d[7] == 1;
        else if (selected && d[1] == 5 && n >= 7 &&
                 d[2] == 0x81 && (d[3] & 3) == 3 &&
                 ((unsigned int)d[4] | ((unsigned int)d[5] << 8)) == 32)
            return (int)off;
        off += n;
    }
    return -1;
}
#endif
