#ifndef _MAIZOHID_H_
#define _MAIZOHID_H_

#include <stdint.h>
#include <ds34common.h>

/* DragonRise / Microntek PC Twin Shock, not a universal VID/PID mapping. */
#define MAIZOHID_VID            0x0079
#define MAIZOHID_PID            0x0006
#define MAIZOHID_MAX_DESCRIPTOR 512

typedef struct
{
    uint16_t bit_offset;
    uint8_t bits;
    int32_t logical_min;
    int32_t logical_max;
    uint8_t valid;
} maizohid_field;

typedef struct
{
    maizohid_field axis[6]; /* HID X, Y, Z, Rx, Ry, Rz */
    maizohid_field hat;
    maizohid_field dpad[4]; /* Up, Down, Right, Left */
    maizohid_field button[12];
    uint16_t report_bits;
    uint8_t report_id;
    uint8_t has_report_id;
    uint8_t ready;
} maizohid_layout;

int maizohid_parse(maizohid_layout *out, const uint8_t *descriptor, unsigned int length);
int maizohid_decode(const maizohid_layout *layout, const uint8_t *report, unsigned int length, struct ds2report *out);

#endif
