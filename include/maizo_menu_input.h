#ifndef MAIZO_MENU_INPUT_H
#define MAIZO_MENU_INPUT_H

/* 16-byte menu snapshot: 4 logical pad slots x [buttons LO, HI, LX, LY].
 * Digital buttons are PS2 active-low. A missing device publishes FFFF
 * and centered sticks; no false input edges can be generated. */
#define MAIZO_MENU_SLOTS         4
#define MAIZO_MENU_SNAPSHOT_SIZE 16

static unsigned int maizoMenuPadMask(const unsigned char snapshot[16],
                                     int sensitivityX, int sensitivityY)
{
    unsigned int mask = 0;
    int slot, deadX, deadY;
    if (!snapshot)
        return 0;
    deadX = 80;
    deadY = 80;
    if (sensitivityX == 0)
        deadX = 100;
    else if (sensitivityX == 2)
        deadX = 60;
    if (sensitivityY == 0)
        deadY = 100;
    else if (sensitivityY == 2)
        deadY = 60;
    for (slot = 0; slot < 4; slot++) {
        unsigned int offset = slot * 4;
        unsigned int buttons = snapshot[offset] | ((unsigned int)snapshot[offset + 1] << 8);
        mask |= 0xffffu ^ buttons;
        if (snapshot[offset + 2] < 127 - deadX)
            mask |= 0x0080u; /* PS2 Left */
        else if (snapshot[offset + 2] > 127 + deadX)
            mask |= 0x0020u; /* PS2 Right */
        if (snapshot[offset + 3] < 127 - deadY)
            mask |= 0x0010u; /* PS2 Up */
        else if (snapshot[offset + 3] > 127 + deadY)
            mask |= 0x0040u; /* PS2 Down */
    }
    return mask;
}
#endif
