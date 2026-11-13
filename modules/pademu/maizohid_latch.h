#ifndef MAIZOHID_LATCH_H
#define MAIZOHID_LATCH_H
#include <ds34common.h>

/* Preserve observed USB press edges until the next SIO2/menu poll. This
 * catches quick presses captured by USB even if released before a game's
 * next poll. A latched press is delivered once, never held indefinitely. */
static uint16_t maizohid_press_edges(uint16_t previous, uint16_t next)
{
    return previous & (uint16_t)~next;
}

static void maizohid_apply_press(struct ds2report *report, uint16_t edges)
{
    report->nButtonState &= (uint16_t)~edges;
    if (edges & DS2ButtonRight)
        report->PressureRight = 255;
    if (edges & DS2ButtonLeft)
        report->PressureLeft = 255;
    if (edges & DS2ButtonUp)
        report->PressureUp = 255;
    if (edges & DS2ButtonDown)
        report->PressureDown = 255;
    if (edges & DS2ButtonTriangle)
        report->PressureTriangle = 255;
    if (edges & DS2ButtonCircle)
        report->PressureCircle = 255;
    if (edges & DS2ButtonCross)
        report->PressureCross = 255;
    if (edges & DS2ButtonSquare)
        report->PressureSquare = 255;
    if (edges & DS2ButtonL1)
        report->PressureL1 = 255;
    if (edges & DS2ButtonR1)
        report->PressureR1 = 255;
    if (edges & DS2ButtonL2)
        report->PressureL2 = 255;
    if (edges & DS2ButtonR2)
        report->PressureR2 = 255;
}
#endif
