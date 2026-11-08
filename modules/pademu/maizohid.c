#include "maizohid.h"
#ifdef __mips__
/* IOP's ds34usb embeds mips_memset (smsutils.S); PadEmu imports it. */
extern void mips_memset(void *, int, unsigned);
#else
#include <string.h>
#define mips_memset memset
#endif

/*
 * Small HID INPUT report-descriptor reader for the 0079:0006 family.
 * Read usages and bit offsets instead of guessing that every device with
 * this VID/PID has the same raw report (Microntek != DragonRise).
 * Only the first input report is supported; unsupported layouts fail closed.
 */
static uint32_t item_unsigned(const uint8_t *p, unsigned int n)
{
    uint32_t value = 0;
    unsigned int i;
    for (i = 0; i < n; i++)
        value |= (uint32_t)p[i] << (i * 8);
    return value;
}

static int32_t item_signed(const uint8_t *p, unsigned int n)
{
    uint32_t value = item_unsigned(p, n);
    if (n && n < 4 && (value & (1u << (n * 8 - 1))))
        value |= ~((1u << (n * 8)) - 1u);
    return (int32_t)value;
}

static void set_field(maizohid_field *f, unsigned int pos, unsigned int size, int32_t min, int32_t max)
{
    if (!f->valid && size > 0 && size <= 16 && pos + size <= 512 && max > min) {
        f->bit_offset = (uint16_t)pos;
        f->bits = (uint8_t)size;
        f->logical_min = min;
        f->logical_max = max;
        f->valid = 1;
    }
}

int maizohid_parse(maizohid_layout *out, const uint8_t *desc, unsigned int length)
{
    unsigned int p = 0, bitpos = 0;
    unsigned int page = 0, report_size = 0, report_count = 0;
    unsigned int usages[32], usage_count = 0, usage_min = 0, usage_max = 0;
    int have_usage_range = 0, seen_input = 0, have_other_id = 0;
    int32_t logical_min = 0, logical_max = 0;

    if (!out || !desc || !length || length > MAIZOHID_MAX_DESCRIPTOR)
        return 0;
    mips_memset(out, 0, sizeof(*out));
    while (p < length) {
        uint8_t prefix = desc[p++];
        unsigned int size, type, tag, value, i;

        if (prefix == 0xFE) {
            if (p + 2 > length || p + 2 + desc[p] > length)
                return 0;
            p += 2 + desc[p];
            continue;
        }
        size = prefix & 3;
        if (size == 3)
            size = 4;
        type = (prefix >> 2) & 3;
        tag = prefix >> 4;
        if (p + size > length)
            return 0;
        value = item_unsigned(desc + p, size);

        if (type == 1) {
            switch (tag) {
                case 0:
                    page = value;
                    break; /* Usage Page */
                case 1:
                    logical_min = item_signed(desc + p, size);
                    break;
                case 2:
                    logical_max = logical_min < 0 ? item_signed(desc + p, size) : (int32_t)value;
                    break;
                case 7:
                    report_size = value;
                    break;
                case 8: /* Report ID */
                    if (value == 0 || (out->has_report_id && value != out->report_id)) {
                        have_other_id = 1;
                    } else if (!out->has_report_id) {
                        out->has_report_id = 1;
                        out->report_id = (uint8_t)value;
                        bitpos = 8;
                    }
                    break;
                case 9:
                    report_count = value;
                    break;
            }
        } else if (type == 2) {
            switch (tag) {
                case 0:
                    if (usage_count < 32)
                        usages[usage_count++] = value & 0xFFFF;
                    break;
                case 1:
                    usage_min = value & 0xFFFF;
                    have_usage_range = 1;
                    break;
                case 2:
                    usage_max = value & 0xFFFF;
                    break;
            }
        } else if (type == 0) {
            if (tag == 8) { /* INPUT */
                if (!have_other_id && !(value & 1) && (value & 2) &&
                    report_size > 0 && report_count < 256 && bitpos + report_size * report_count <= 512) {
                    for (i = 0; i < report_count; i++) {
                        unsigned int usage = usage_count ? usages[i < usage_count ? i : usage_count - 1] :
                                                           (have_usage_range ? usage_min + (i < (usage_max - usage_min + 1) ? i : (usage_max - usage_min)) : 0);
                        maizohid_field *f = 0;
                        if (page == 1 && usage >= 0x30 && usage <= 0x35)
                            f = &out->axis[usage - 0x30];
                        else if (page == 1 && usage == 0x39)
                            f = &out->hat;
                        else if (page == 1 && usage >= 0x90 && usage <= 0x93)
                            f = &out->dpad[usage - 0x90];
                        else if (page == 9 && usage >= 1 && usage <= 12)
                            f = &out->button[usage - 1];
                        if (f)
                            set_field(f, bitpos + i * report_size, report_size, logical_min, logical_max);
                    }
                }
                seen_input = 1;
                if (report_size > 0 && report_count <= 512 && bitpos + report_size * report_count <= 512)
                    bitpos += report_size * report_count;
                else
                    return 0;
            }
            /* HID local items apply to one main item only. */
            usage_count = 0;
            usage_min = usage_max = 0;
            have_usage_range = 0;
        }
        p += size;
    }
    out->report_bits = (uint16_t)bitpos;
    if (!seen_input || have_other_id || !out->axis[0].valid || !out->axis[1].valid ||
        !out->button[0].valid || !out->button[7].valid ||
        (!out->hat.valid && !out->dpad[0].valid))
        return 0;
    out->ready = 1;
    return 1;
}

static int get_field(const maizohid_field *f, const uint8_t *data, unsigned int bytes, int32_t *value)
{
    unsigned int i;
    uint32_t raw = 0;
    if (!f->valid || !f->bits || (f->bit_offset + f->bits) > bytes * 8)
        return 0;
    for (i = 0; i < f->bits; i++)
        raw |= (uint32_t)((data[(f->bit_offset + i) >> 3] >> ((f->bit_offset + i) & 7)) & 1) << i;
    if (f->logical_min < 0 && (raw & (1u << (f->bits - 1))))
        raw |= ~((1u << f->bits) - 1u);
    *value = (int32_t)raw;
    return 1;
}

static uint8_t axis_value(const maizohid_field *f, const uint8_t *r, unsigned int bytes)
{
    int32_t value, scaled;
    if (!get_field(f, r, bytes, &value))
        return 0x7F;
    if (value < f->logical_min)
        value = f->logical_min;
    if (value > f->logical_max)
        value = f->logical_max;
    scaled = (value - f->logical_min) * 255 / (f->logical_max - f->logical_min);
    return (uint8_t)scaled;
}

static int pressed(const maizohid_field *f, const uint8_t *r, unsigned int bytes)
{
    int32_t value;
    return get_field(f, r, bytes, &value) && value != 0;
}

int maizohid_decode(const maizohid_layout *layout, const uint8_t *r, unsigned int bytes, struct ds2report *out)
{
    uint16_t b = 0;
    unsigned int i;
    int32_t hat;
    int up = 0, down = 0, right = 0, left = 0;
    static const uint16_t map[12] = {
        DS2ButtonTriangle, DS2ButtonCircle, DS2ButtonCross, DS2ButtonSquare,
        DS2ButtonL1, DS2ButtonR1, DS2ButtonL2, DS2ButtonR2,
        DS2ButtonSelect, DS2ButtonStart, DS2ButtonL3, DS2ButtonR3};
    const maizohid_field *rx, *ry;

    if (!layout || !layout->ready || !r || !out || bytes * 8 < layout->report_bits)
        return 0;
    if (layout->has_report_id && r[0] != layout->report_id)
        return 0;

    for (i = 0; i < 12; i++)
        if (pressed(&layout->button[i], r, bytes))
            b |= map[i];

    if (get_field(&layout->hat, r, bytes, &hat)) {
        /* Hat = 0..7; 8 and 15 are both neutral on common USB variants. */
        if (hat >= 0 && hat < 8) {
            up = (hat == 0 || hat == 1 || hat == 7);
            right = (hat >= 1 && hat <= 3);
            down = (hat >= 3 && hat <= 5);
            left = (hat >= 5 && hat <= 7);
        }
    } else {
        up = pressed(&layout->dpad[0], r, bytes);
        down = pressed(&layout->dpad[1], r, bytes);
        right = pressed(&layout->dpad[2], r, bytes);
        left = pressed(&layout->dpad[3], r, bytes);
    }
    if (up)
        b |= DS2ButtonUp;
    if (down)
        b |= DS2ButtonDown;
    if (right)
        b |= DS2ButtonRight;
    if (left)
        b |= DS2ButtonLeft;

    mips_memset(out, 0, sizeof(*out));
    out->nButtonState = (uint16_t)~b;

    /* Microntek: X/Y and Z/Rx. Other 0079:0006 units may use Rx/Ry. */
    rx = layout->axis[2].valid ? &layout->axis[2] : &layout->axis[3];
    ry = layout->axis[2].valid ? &layout->axis[3] : &layout->axis[4];
    out->LeftStickX = axis_value(&layout->axis[0], r, bytes);
    out->LeftStickY = axis_value(&layout->axis[1], r, bytes);
    out->RightStickX = axis_value(rx, r, bytes);
    out->RightStickY = axis_value(ry, r, bytes);

    out->PressureUp = up * 255;
    out->PressureDown = down * 255;
    out->PressureLeft = left * 255;
    out->PressureRight = right * 255;
    out->PressureTriangle = !!(b & DS2ButtonTriangle) * 255;
    out->PressureCircle = !!(b & DS2ButtonCircle) * 255;
    out->PressureCross = !!(b & DS2ButtonCross) * 255;
    out->PressureSquare = !!(b & DS2ButtonSquare) * 255;
    out->PressureL1 = !!(b & DS2ButtonL1) * 255;
    out->PressureR1 = !!(b & DS2ButtonR1) * 255;
    out->PressureL2 = !!(b & DS2ButtonL2) * 255;
    out->PressureR2 = !!(b & DS2ButtonR2) * 255;
    return 1;
}
