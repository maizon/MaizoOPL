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
#define MAIZO_MENU_PORT_SIZE     32
#define MAIZO_MENU_PORT_MAGIC1   0x50
#define MAIZO_MENU_PORT_VERSION  1

static inline unsigned int maizoMenuPadType(const unsigned char snapshot[16], unsigned int slot)
{
    return slot < MAIZO_MENU_SLOTS ? (snapshot[15] >> (slot * 4)) & 15u : 0;
}

static inline int maizoMenuSnapshotOK(const unsigned char snapshot[16])
{
    return snapshot && snapshot[8] == MAIZO_MENU_MAGIC0 &&
           snapshot[9] == MAIZO_MENU_MAGIC1 && snapshot[10] == MAIZO_MENU_VERSION;
}

static unsigned int maizoMenuPadValidMask(const unsigned char snapshot[16])
{
    unsigned int mask = 0, slot;
    if (!maizoMenuSnapshotOK(snapshot))
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

/* Neutral decoded reports are valid. Recover only missing connected slots,
 * or both slots after a transport failure. */
static inline unsigned int maizoMenuFallbackMask(const unsigned char snapshot[16])
{
    unsigned int slot, mask = 0;
    if (!maizoMenuSnapshotOK(snapshot))
        return (1u << MAIZO_MENU_SLOTS) - 1;
    for (slot = 0; slot < MAIZO_MENU_SLOTS; ++slot)
        if (maizoMenuPadType(snapshot, slot))
            mask |= 1u << slot;
    return mask & ~maizoMenuPadValidMask(snapshot);
}

static inline int maizoMenuMergeFallback(unsigned char snapshot[16], unsigned int slot,
                                         const unsigned char report[18])
{
    unsigned int pos = slot * 4;
    if (slot >= MAIZO_MENU_SLOTS || !report || !(report[0] || report[1]))
        return 0;
    snapshot[pos] = report[0];
    snapshot[pos + 1] = report[1];
    snapshot[pos + 2] = report[4];
    snapshot[pos + 3] = report[5];
    snapshot[11] |= 1u << slot;
    snapshot[12] |= 1u << slot;
    return 1;
}

/* The per-port RPC has its own header and slot identity. Reject transport
 * zeros, a reply from another port, and invalid/stale report data. */
static inline int maizoMenuMergePort(unsigned char snapshot[16], unsigned int slot,
                                     const unsigned char reply[MAIZO_MENU_PORT_SIZE])
{
    unsigned int bit;
    if (slot >= MAIZO_MENU_SLOTS || !reply ||
        reply[0] != MAIZO_MENU_MAGIC0 || reply[1] != MAIZO_MENU_PORT_MAGIC1 ||
        reply[2] != MAIZO_MENU_PORT_VERSION || reply[3] != slot ||
        reply[4] > 8)
        return 0;
    bit = 1u << slot;
    snapshot[15] &= ~(15u << (slot * 4));
    snapshot[15] |= reply[4] << (slot * 4);
    snapshot[11] &= ~bit;
    snapshot[12] &= ~bit;
    if (reply[4] && reply[5])
        maizoMenuMergeFallback(snapshot, slot, reply + 8);
    return 1;
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
