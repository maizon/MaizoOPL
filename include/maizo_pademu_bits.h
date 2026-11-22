#ifndef MAIZO_PADEMU_BITS_H
#define MAIZO_PADEMU_BITS_H

/* The 32-bit OPL PadEmuSettings layout:
 * 0..7  transport (0=USB,1=BT); 8..11 emulated ports 1..4;
 * 16..19 vibration ports; 24 multitap; 25 physical multitap port;
 * 26 workaround. Other bits must survive UI edits. */
#define MAIZO_PADEMU_PORT_BITS 0x00000F00u
#define MAIZO_PADEMU_VIB_BITS  0x000F0000u
#define MAIZO_PADEMU_META_BITS 0x070000FFu

static unsigned int maizoPademuSetSlot(unsigned int original, unsigned int slot,
                                       int enabled, int vibration)
{
    unsigned int en, vib;
    if (slot >= 4)
        return original;
    en = 1u << (8 + slot);
    vib = 1u << (16 + slot);
    original &= ~(en | vib);
    if (enabled)
        original |= en;
    if (enabled && vibration)
        original |= vib;
    return original;
}

static unsigned int maizoPademuSetOptions(unsigned int original, int mode, int multitap,
                                          int multitapPort, int workaround)
{
    unsigned int meta = (mode ? 1u : 0u) |
                        (multitap ? 1u << 24 : 0u) |
                        (multitapPort == 2 ? 1u << 25 : 0u) |
                        (workaround ? 1u << 26 : 0u);
    original = (original & ~MAIZO_PADEMU_META_BITS) | meta;
    if (!multitap)
        original &= ~(0x00000C00u | 0x000C0000u);
    return original;
}

static inline unsigned int maizoPademuPortChoice(unsigned int settings)
{
    static const unsigned char choice[4] = {3, 0, 1, 2};
    return choice[(settings >> 8) & 3u];
}
static inline unsigned int maizoPademuAssignPorts(unsigned int settings, unsigned int choice, int vibration)
{
    static const unsigned char masks[4] = {1, 2, 3, 0};
    unsigned int mask = choice < 4 ? masks[choice] : 0;
    settings &= ~(MAIZO_PADEMU_PORT_BITS | MAIZO_PADEMU_VIB_BITS);
    return settings | (mask << 8) | (vibration ? mask << 16 : 0);
}
#endif
