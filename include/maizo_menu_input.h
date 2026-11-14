#ifndef MAIZO_MENU_INPUT_H
#define MAIZO_MENU_INPUT_H

/* Explicit, versioned RPC snapshot. IOP USB driver only supports two
 * physical pad slots: 0-3 = P1, 4-7 = P2, [8,9] = magic,
 * [10] = version, [11] = connected/valid bitmask, [12-15] reserved.
 * A missing/incompatible response must NEVER become a pressed key. */
#define MAIZO_MENU_SLOTS         2
#define MAIZO_MENU_SNAPSHOT_SIZE 16
#define MAIZO_MENU_MAGIC0        0x4d
#define MAIZO_MENU_MAGIC1        0x5a
#define MAIZO_MENU_VERSION       1

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
        if ((valid & (1u << slot)) &&
            snapshot[offset] == 0xff && snapshot[offset + 1] == 0xff &&
            snapshot[offset + 2] >= 105 && snapshot[offset + 2] <= 150 &&
            snapshot[offset + 3] >= 105 && snapshot[offset + 3] <= 150)
            mask |= 1u << slot;
    }
    return mask;
}

static unsigned int maizoMenuPadMask(const unsigned char snapshot[16],
                                     int sensitivityX, int sensitivityY,
                                     unsigned int armedMask)
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
        unsigned int buttons, x, y;
        if (!(valid & (1u << slot)))
            continue;
        buttons = snapshot[offset] |
                  ((unsigned int)snapshot[offset + 1] << 8);
        x = snapshot[offset + 2];
        y = snapshot[offset + 3];
        mask |= 0xffffu ^ buttons;
        if (x < 127 - deadX)
            mask |= 0x0080u;
        else if (x > 127 + deadX)
            mask |= 0x0020u;
        if (y < 127 - deadY)
            mask |= 0x0010u;
        else if (y > 127 + deadY)
            mask |= 0x0040u;
    }
    return mask;
}

#endif
