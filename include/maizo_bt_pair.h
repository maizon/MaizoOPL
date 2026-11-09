#ifndef MAIZO_BT_PAIR_H
#define MAIZO_BT_PAIR_H

#include <string.h>

#define MAIZO_BT_PAIR_CONFIG_KEY "maizo_bt_pair"
#define MAIZO_BT_PAIR_SIZE 23
#define MAIZO_BT_PAIR_HEX_SIZE 47

static inline int maizoBtNibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static inline int maizoBtPairValid(const unsigned char *pair)
{
    int i, nonzero = 0;
    if (!pair || (pair[0] & 2) == 0)
        return 0;
    for (i = 1; i < 7; i++)
        nonzero |= pair[i];
    return nonzero != 0;
}

static inline void maizoBtPairEncode(const unsigned char *pair, char hex[MAIZO_BT_PAIR_HEX_SIZE])
{
    static const char digits[] = "0123456789ABCDEF";
    int i;
    for (i = 0; i < MAIZO_BT_PAIR_SIZE; i++) {
        hex[i * 2] = digits[pair[i] >> 4];
        hex[i * 2 + 1] = digits[pair[i] & 15];
    }
    hex[MAIZO_BT_PAIR_HEX_SIZE - 1] = '\0';
}

static inline int maizoBtPairDecode(const char *hex, unsigned char pair[MAIZO_BT_PAIR_SIZE])
{
    unsigned char decoded[MAIZO_BT_PAIR_SIZE];
    int i, hi, lo;
    if (!hex || strlen(hex) != MAIZO_BT_PAIR_SIZE * 2)
        return 0;
    for (i = 0; i < MAIZO_BT_PAIR_SIZE; i++) {
        hi = maizoBtNibble(hex[i * 2]);
        lo = maizoBtNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return 0;
        decoded[i] = (hi << 4) | lo;
    }
    if (!maizoBtPairValid(decoded))
        return 0;
    memcpy(pair, decoded, MAIZO_BT_PAIR_SIZE);
    return 1;
}

#endif
