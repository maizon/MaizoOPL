#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "maizo_igr_gsm_layout.h"
struct gsm_flags {
    uint32_t dx_offset, dy_offset;
    uint8_t adaptation, pmode, smode1, smode2, srfsh, synch, syncv,
            dispfb, display, field, gs576;
} __attribute__((packed));
int main(void)
{
    assert(offsetof(struct gsm_flags, smode2) == MAIZO_GSM_SMODE2_FIX_OFFSET);
    assert(offsetof(struct gsm_flags, field) == MAIZO_GSM_FIELD_FIX_OFFSET);
    assert(sizeof(struct gsm_flags) == 19);
    puts("GSM C/ASM SMODE2 and FIELD offsets: PASS");
    return 0;
}
