#ifndef MAIZO_PROFILE_CONFIG_H
#define MAIZO_PROFILE_CONFIG_H
#include <stdio.h>
#include "config.h"
#include "maizo_input_profiles.h"

static inline void maizoLoadInputProfiles(unsigned char profile[MAIZO_PROFILE_SIZE])
{
    config_set_t *config = configGetByType(CONFIG_OPL);
    char key[24];
    int i, value;
    maizoDefaultProfiles(profile);
    for (i = 0; i < MAIZO_PROFILE_SIZE; i++) {
        snprintf(key, sizeof(key), "maizo_input_%02d", i);
        if (config && configGetInt(config, key, &value) && value >= 0 && value <= 255)
            profile[i] = (unsigned char)value;
    }
    maizoSanitizeProfiles(profile);
}
#endif
