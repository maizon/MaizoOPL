#include <assert.h>
#include <stdio.h>
#include "maizo_pademu_bits.h"
#include "maizo_sio2_guard.h"
#include "maizohid_latch.h"

int main(void)
{
    unsigned int settings = 0;
    unsigned char data[40] = {0};

    /* Global P1 and P2 must be independently writable. */
    settings = maizoPademuSetSlot(settings, 0, 1, 0);
    settings = maizoPademuSetSlot(settings, 1, 1, 1);
    assert((settings & 0x300u) == 0x300u);
    assert((settings & 0x30000u) == 0x20000u);
    settings = maizoPademuSetSlot(settings, 0, 0, 0);
    assert((settings & 0x300u) == 0x200u);
    settings = maizoPademuSetSlot(settings, 0, 1, 1);
    assert((settings & 0x300u) == 0x300u);
    assert(maizoPademuSetSlot(settings, 4, 0, 0) == settings);

    /* A multitap on port 2 may emulate slots 0..3; preserve each slot's
     * mode and vibration while toggling unrelated UI selections. */
    settings = maizoPademuSetSlot(settings, 2, 1, 0);
    settings = maizoPademuSetSlot(settings, 3, 1, 1);
    settings = maizoPademuSetOptions(settings, 0, 1, 2, 0);
    assert(((settings >> 24) & 3u) == 3u);
    assert((settings & 0xF00u) == 0xF00u);
    assert((settings & 0xF0000u) == 0xB0000u);
    settings = maizoPademuSetOptions(settings, 1, 1, 1, 1);
    assert((settings & 0xFFu) == 1u);
    assert(((settings >> 24) & 3u) == 1u);
    assert(((settings >> 26) & 1u) == 1u);
    settings = maizoPademuSetOptions(settings, 0, 0, 1, 0);
    assert(((settings >> 24) & 1u) == 0u);
    assert((settings & 0xC00u) == 0);
    assert((settings & 0xC0000u) == 0);
    assert((settings & 0x300u) == 0x300u);

    /* Invalid or truncated SIO2 second packets must never overrun
     * or leave the first gamepad command without a response. */
    assert(maizoSio2SecondCommand(NULL, 32, 32) == -1);
    assert(maizoSio2SecondCommand(data, 2, 2) == -1);
    data[9] = 0x01;
    data[10] = 0x42;
    data[11] = 0x00;
    assert(maizoSio2SecondCommand(data, sizeof(data), sizeof(data)) == 9);
    assert(maizoSio2SecondCommand(data, sizeof(data), 10) == -1);
    data[9] = 0x00;
    data[21] = 0x01;
    data[22] = 0x40;
    data[23] = 0x00;
    assert(maizoSio2SecondCommand(data, sizeof(data), sizeof(data)) == 21);

    {
        struct ds2report report = {0};
        uint16_t edges = 0;
        report.nButtonState = 0xFFFF;
        edges |= maizohid_press_edges(0xFFFF, (uint16_t)~DS2ButtonCross);
        edges |= maizohid_press_edges((uint16_t)~DS2ButtonCross, 0xFFFF);
        maizohid_apply_press(&report, edges);
        assert((report.nButtonState & DS2ButtonCross) == 0);
        assert(report.PressureCross == 255);
        report.nButtonState = 0xFFFF;
        edges = 0;
        maizohid_apply_press(&report, edges);
        assert(report.nButtonState == 0xFFFF);
    }
    puts("MAIZO: PadEmu P2, multitap, SIO2 and short USB press tests passed");
    return 0;
}
