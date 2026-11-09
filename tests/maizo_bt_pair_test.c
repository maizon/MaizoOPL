#include "include/maizo_bt_pair.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned char original[MAIZO_BT_PAIR_SIZE] = {3, 0x13, 0x24, 0xA5, 0xB6, 0xC7, 0xD8};
    unsigned char decoded[MAIZO_BT_PAIR_SIZE] = {0};
    char hex[MAIZO_BT_PAIR_HEX_SIZE];
    int i;
    for (i = 7; i < MAIZO_BT_PAIR_SIZE; ++i)
        original[i] = (unsigned char)(i * 13);
    assert(maizoBtPairValid(original));
    maizoBtPairEncode(original, hex);
    assert(strlen(hex) == 46);
    assert(maizoBtPairDecode(hex, decoded));
    assert(memcmp(original, decoded, MAIZO_BT_PAIR_SIZE) == 0);
    hex[4] = 'G';
    assert(!maizoBtPairDecode(hex, decoded));
    maizoBtPairEncode(original, hex);
    hex[45] = '\0';
    assert(!maizoBtPairDecode(hex, decoded));
    memset(original, 0, sizeof(original));
    assert(!maizoBtPairValid(original));
    puts("MAIZO BT bond persistence tests passed");
    return 0;
}
