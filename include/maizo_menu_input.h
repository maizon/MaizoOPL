#ifndef MAIZO_MENU_INPUT_H
#define MAIZO_MENU_INPUT_H

/* Explicit, versioned RPC snapshot. IOP USB driver only supports two
 * physical pad slots: 0-3 = P1, 4-7 = P2, [8,9] = magic,
 * [10] = version, [11] = valid bitmask, [12-14] diagnostic,
 * [15] = two 4-bit controller type codes (0 absent, type+1 otherwise).
 * A missing/incompatible response must NEVER become a pressed key. */
#define MAIZO_MENU_SLOTS         2
#define MAIZO_MENU_SNAPSHOT_SIZE 16
#define MAIZO_MENU_MAGIC0        0x4d
#define MAIZO_MENU_MAGIC1        0x5a
#define MAIZO_MENU_VERSION       2

static inline unsigned int maizoMenuPadType(const unsigned char snapshot[16], unsigned int slot)
{
    return slot < MAIZO_MENU_SLOTS ? (snapshot[15] >> (slot * 4)) & 15u : 0;
}

static unsigned int maizoMenuPadValidMask(const unsigned char snapshot[16])
{
    unsigned int mask = 0, slot;
    if (!snapshot || snapshot[8] != MAIZO_MENU_MAGIC0 ||
        snapshot[9] != MAIZO_MENU_MAGIC1 ||
        snapshot[10] != MAIZO_MENU_VERSION)
        return 0;
    for (slot = 0; slot < MAIZO_MENU_SLOTS; ++slot) {
        unsigned int offset = slot * 4;
        unsigned int buttons = snapshot[offset] |
                               ((unsigned int)snapshot[offset + 1] << 8);
        /* All sixteen PS2 buttons held simultaneously indicates an
         * uninitialised or corrupted report, not a human action. */
        if ((snapshot[11] & (1u << slot)) && buttons != 0)
            mask |= 1u << slot;
    }
    return mask;
}

static unsigned int maizoMenuPadNeutralMask(const unsigned char snapshot[16])
{
    unsigned int slot, mask = 0;
    unsigned int valid = maizoMenuPadValidMask(snapshot);
    for (slot = 0; slot < MAIZO_MENU_SLOTS; ++slot) {
        unsigned int offset = slot * 4;
        /* Digital-neutral is sufficient even if raw sticks idle at zero. */
        if ((valid & (1u << slot)) &&
            snapshot[offset] == 0xff && snapshot[offset + 1] == 0xff)
            mask |= 1u << slot;
    }
    return mask;
}

static unsigned int maizoMenuPadMask(const unsigned char snapshot[16],
                                     int sensitivityX, int sensitivityY,
                                     unsigned int armedMask,
                                     const unsigned char centers[4])
{
    unsigned int slot, mask = 0;
    unsigned int valid = maizoMenuPadValidMask(snapshot) & armedMask;
    int deadX = 80, deadY = 80;
    if (sensitivityX == 0)
        deadX = 100;
    else if (sensitivityX == 2)
        deadX = 60;
    if (sensitivityY == 0)
        deadY = 100;
    else if (sensitivityY == 2)
        deadY = 60;

    for (slot = 0; slot < MAIZO_MENU_SLOTS; ++slot) {
        unsigned int offset = slot * 4;
        unsigned int buttons;
        int x, y;
        if (!(valid & (1u << slot)))
            continue;
        buttons = snapshot[offset] |
                  ((unsigned int)snapshot[offset + 1] << 8);
        x = snapshot[offset + 2];
        y = snapshot[offset + 3];
        mask |= 0xffffu ^ buttons;
        /* Per-device stick origin is captured from a neutral report. */
        x -= centers ? centers[slot * 2] : 127;
        y -= centers ? centers[slot * 2 + 1] : 127;
        if (x < -deadX)
            mask |= 0x0080u;
        else if (x > deadX)
            mask |= 0x0020u;
        if (y < -deadY)
            mask |= 0x0010u;
        else if (y > deadY)
            mask |= 0x0040u;
    }
    return mask;
}

#endif
