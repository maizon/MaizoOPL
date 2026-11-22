#ifndef MAIZO_INPUT_DECODE_H
#define MAIZO_INPUT_DECODE_H
#include "../../include/maizo_input_profiles.h"
#include "maizohid_latch.h"
static inline void maizoInputSetButtons(struct ds2report *out, unsigned int mask)
{
    out->nButtonState = 0xffff;
    out->PressureRight = out->PressureLeft = out->PressureUp = out->PressureDown = 0;
    out->PressureTriangle = out->PressureCircle = out->PressureCross = out->PressureSquare = 0;
    out->PressureL1 = out->PressureR1 = out->PressureL2 = out->PressureR2 = 0;
    maizohid_apply_press(out, (uint16_t)mask);
}
static inline void maizoRemapHid(struct ds2report *out, const unsigned char *profiles)
{
    unsigned int mask = maizoMapHid((uint16_t)~out->nButtonState, profiles);
    maizoInputSetButtons(out, mask);
}
static inline int maizoDecodeKeyboard(struct ds2report *out, const unsigned char *data,
                                      unsigned int bytes, const unsigned char *profiles)
{
    unsigned int mask;
    if (!maizoKeyboardMask(data, bytes, profiles, &mask))
        return 0;
    out->RightStickX = out->RightStickY = out->LeftStickX = out->LeftStickY = 127;
    maizoInputSetButtons(out, mask);
    return 1;
}
#endif
