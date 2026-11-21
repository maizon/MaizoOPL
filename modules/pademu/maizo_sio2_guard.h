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

/* PADMAN SIO2 register fields carry 9-bit TX and RX lengths. Use them
 * instead of guessing a second command from arbitrary button payload.
 * A legacy zero-length register can fall back to the known boundaries. */
static inline int maizoSio2PacketLengths(unsigned int reg0, unsigned int reg1,
                                         unsigned int inTotal, unsigned int outTotal,
                                         unsigned int *firstIn, unsigned int *firstOut)
{
    unsigned int tx = (reg0 >> 8) & 0x1ff;
    unsigned int rx = (reg0 >> 18) & 0x1ff;
    unsigned int tx2 = (reg1 >> 8) & 0x1ff;
    unsigned int rx2 = (reg1 >> 18) & 0x1ff;
    if (tx < 3 || rx < 3 || tx2 < 3 || rx2 < 3 ||
        tx + tx2 != inTotal || rx + rx2 != outTotal)
        return 0;
    *firstIn = tx;
    *firstOut = rx;
    return 1;
}

/* A masked virtual command may fail on the actual SIO2 bus. Keep the
 * genuine port's status while clearing only the virtual command's error.
 * PADMAN treats bit 13 as a global failure, and bits 16+n per command. */
static inline unsigned int maizoSio2MixedStatus(unsigned int status, unsigned int emulated)
{
    unsigned int virtualErrors = ((emulated & 3u) << 16);
    if (status & 0x2000u) {
        status |= ((~emulated & 3u) << 16);
        status &= ~0x2000u;
    }
    return status & ~virtualErrors;
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
