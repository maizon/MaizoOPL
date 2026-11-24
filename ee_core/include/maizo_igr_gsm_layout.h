/* GSM flags: packed C struct AND MIPS assembly source of truth.
 * gsm_defines.h: SMODE1_fix=10, SMODE2_fix=11, FIELD_fix=17.
 * Using index 10 for SMODE2 corrupts video mode forcing.
 */
#ifndef MAIZO_IGR_GSM_LAYOUT_H
#define MAIZO_IGR_GSM_LAYOUT_H
enum {
    MAIZO_GSM_SMODE2_FIX_OFFSET = 11,
    MAIZO_GSM_FIELD_FIX_OFFSET = 17
};
#endif
