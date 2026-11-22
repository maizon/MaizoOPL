#ifndef MAIZO_INPUT_PROFILES_H
#define MAIZO_INPUT_PROFILES_H
#define MAIZO_PROFILE_SIZE 32
#define MAIZO_KEYBOARD     7
static const unsigned char maizoDefaultKeys[16] = {42, 8, 21, 40, 82, 79, 81, 80, 20, 26, 4, 22, 6, 27, 29, 25};
static inline void maizoDefaultProfiles(unsigned char *profile)
{
    unsigned int i;
    for (i = 0; i < 16; i++) {
        profile[i] = (unsigned char)i;
        profile[16 + i] = maizoDefaultKeys[i];
    }
}
static inline void maizoSanitizeProfiles(unsigned char *profile)
{
    unsigned int i;
    for (i = 0; i < 16; i++)
        if (profile[i] > 16)
            profile[i] = (unsigned char)i;
}
static inline unsigned int maizoMapHid(unsigned int active, const unsigned char *profile)
{
    unsigned int i, mapped = 0;
    for (i = 0; i < 16; i++)
        if (profile[i] < 16 && (active & (1u << profile[i])))
            mapped |= 1u << i;
    return mapped;
}
static inline int maizoKeyboardMask(const unsigned char *report, unsigned int bytes,
                                    const unsigned char *profile, unsigned int *mask)
{
    unsigned int i, j;
    if (bytes < 8 || report[1] != 0)
        return 0;
    for (j = 2; j < 8; j++)
        if (report[j] >= 1 && report[j] <= 3)
            return 0;
    *mask = 0;
    for (i = 0; i < 16; i++) {
        unsigned int key = profile[16 + i];
        if (key >= 224 && key <= 231) {
            if (report[0] & (1u << (key - 224)))
                *mask |= 1u << i;
        } else if (key != 0) {
            for (j = 2; j < 8; j++)
                if (report[j] == key)
                    *mask |= 1u << i;
        }
    }
    return 1;
}
#endif
