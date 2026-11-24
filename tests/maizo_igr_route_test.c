#include <assert.h>
#include <stdio.h>
#include "maizo_igr_route.h"
int main(void)
{
    assert(maizoIgrSafeChoice(0, MAIZO_IGR_UNAVAILABLE) == MAIZO_IGR_RESUME);
    assert(maizoIgrSafeChoice(0, MAIZO_IGR_EXIT) == MAIZO_IGR_RESUME);
    assert(maizoIgrSafeChoice(1, MAIZO_IGR_UNAVAILABLE) == MAIZO_IGR_RESUME);
    assert(maizoIgrSafeChoice(1, MAIZO_IGR_RESUME) == MAIZO_IGR_RESUME);
    assert(maizoIgrSafeChoice(1, MAIZO_IGR_EXIT) == MAIZO_IGR_EXIT);
    assert(maizoIgrSafeChoice(1, MAIZO_IGR_POWEROFF) == MAIZO_IGR_POWEROFF);
    assert(maizoIgrSafeChoice(1, -1) == MAIZO_IGR_RESUME);
    assert(maizoIgrSafeChoice(1, 0x1234) == MAIZO_IGR_RESUME);
    puts("MAIZO IGR never exits unless visible menu selected EXIT: PASS");
    return 0;
}
