#ifndef MAIZO_SIO2_GUARD_H
#define MAIZO_SIO2_GUARD_H

/* A 2P SIO2 packet can contain two pad transactions. Validate the
 * second command before editing its buffer; never interpret a partial
 * payload as a second emulated controller. The known packet boundaries
 * are 5, 9 and 21 bytes for compatible sio2man implementations. */
static int maizoSio2SecondCommand(const unsigned char *input,
                                  unsigned int inputSize,
                                  unsigned int outputSize)
{
    static const unsigned int boundaries[3] = {5, 9, 21};
    unsigned int index, offset;
    if (!input)
        return -1;
    for (index = 0; index < 3; index++) {
        offset = boundaries[index];
        if (offset + 3 <= inputSize && offset + 3 <= outputSize &&
            input[offset] == 0x01 &&
            (input[offset + 1] & 0xF0) == 0x40 &&
            input[offset + 2] == 0x00)
            return (int)offset;
    }
    return -1;
}

/* Map the real connector, not the ordinal position of the enabled port.
 * With a multitap, only the chosen physical connector is emulated. */
static inline int maizoSio2Slot(unsigned int physical, unsigned int enabled,
                                int tapActive, unsigned int tapPort, unsigned int tapSlot)
{
    unsigned int slot = physical;
    if (physical > 1)
        return -1;
    if (tapActive) {
        if (physical != tapPort)
            return -1;
        slot = tapSlot;
    }
    return slot < 4 && (enabled & (1u << slot)) ? (int)slot : -1;
}

static inline unsigned int maizoSio2MinCommand(unsigned char command)
{
    switch (command) {
        case 0x42:
            return 3;
        case 0x43:
            return 4;
        case 0x44:
            return 5;
        default:
            /* Other configuration responses read/write bytes through 8. */
            return 9;
    }
}

#endif
