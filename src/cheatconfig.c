#include "cheatconfig.h"
#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define CHEAT_SELECTION_KEY_MAX CONFIG_KEY_NAME_LEN

static void normalizeCheatName(const char *source, char *destination, size_t destinationSize)
{
    size_t output = 0;
    int pendingSpace = 0;

    if (destinationSize == 0)
        return;

    while (*source != NUL && isspace((unsigned char)*source))
        source++;

    while (*source != NUL && output + 1 < destinationSize) {
        unsigned char character = (unsigned char)*source++;

        if (isspace(character)) {
            pendingSpace = output > 0;
            continue;
        }

        if (pendingSpace) {
            if (output + 2 >= destinationSize)
                break;
            destination[output++] = SPACE;
            pendingSpace = 0;
        }

        destination[output++] = tolower(character);
    }

    destination[output] = NUL;
}

int cheatConfigIsMasterCode(const char *name)
{
    char normalized[CHEAT_NAME_MAX + 1];

    normalizeCheatName(name, normalized, sizeof(normalized));

    return strcmp(normalized, "mastercode") == 0 ||
           strcmp(normalized, "master code") == 0 ||
           strcmp(normalized, "enable") == 0 ||
           strcmp(normalized, "enable code") == 0 ||
           strcmp(normalized, "(m)") == 0 ||
           strcmp(normalized, "enable code (must be on)") == 0;
}

static int cheatNamesMatch(const char *first, const char *second)
{
    char normalizedFirst[CHEAT_NAME_MAX + 1];
    char normalizedSecond[CHEAT_NAME_MAX + 1];

    normalizeCheatName(first, normalizedFirst, sizeof(normalizedFirst));
    normalizeCheatName(second, normalizedSecond, sizeof(normalizedSecond));

    return strcmp(normalizedFirst, normalizedSecond) == 0;
}

/* Persist the actual code group, not merely its display name. Two different
 * cheats can share a label, and databases can reorder their entries. */
static unsigned int cheatCodeKey(const cheat_entry_t *entry)
{
    unsigned int hash = 2166136261u;
    for (int i = 0; i < MAX_CHEATLIST && entry->codes[i].addr; i++) {
        unsigned int words[2] = {entry->codes[i].addr, entry->codes[i].val};
        for (int w = 0; w < 2; w++)
            for (int b = 0; b < 4; b++) {
                hash ^= (words[w] >> (b * 8)) & 255;
                hash *= 16777619u;
            }
    }
    return hash;
}

static void cheatSelectionKey(const cheat_entry_t *entry, char saved[CONFIG_KEY_VALUE_LEN])
{
    snprintf(saved, CONFIG_KEY_VALUE_LEN, "@%08X:%s", cheatCodeKey(entry), entry->name);
}

void cheatConfigLoadSelections(config_set_t *configSet)
{
    int cheatIndex;
    int selectionIndex;
    char key[CHEAT_SELECTION_KEY_MAX];
    char savedName[CONFIG_KEY_VALUE_LEN];

    /* First use is safe: required code on, every optional code off. */
    for (cheatIndex = 0; cheatIndex < MAX_CODES && gCheats[cheatIndex].name[0] != NUL; cheatIndex++)
        gCheats[cheatIndex].enabled = cheatConfigIsMasterCode(gCheats[cheatIndex].name);

    if (configSet == NULL)
        return;

    for (selectionIndex = 0; selectionIndex < MAX_CODES; selectionIndex++) {
        snprintf(key, sizeof(key), "%s%03d", CONFIG_ITEM_CHEAT_SELECTION_PREFIX, selectionIndex);

        if (!configGetStrCopy(configSet, key, savedName, sizeof(savedName)))
            break;

        int candidate = -1, matches = 0;
        for (cheatIndex = 0; cheatIndex < MAX_CODES && gCheats[cheatIndex].name[0] != NUL; cheatIndex++) {
            if (cheatConfigIsMasterCode(gCheats[cheatIndex].name))
                continue;
            char current[CONFIG_KEY_VALUE_LEN];
            cheatSelectionKey(&gCheats[cheatIndex], current);
            if ((savedName[0] == '@' && savedName[9] == ':' && !strncmp(savedName, current, 10) &&
                 cheatNamesMatch(savedName + 10, current + 10)) ||
                (savedName[0] != '@' && cheatNamesMatch(gCheats[cheatIndex].name, savedName))) {
                candidate = cheatIndex;
                matches++;
            }
        }
        /* Fail closed on ambiguous legacy labels instead of activating the
         * first unrelated group bearing the same name. */
        if (matches == 1)
            gCheats[candidate].enabled = 1;
    }

    /* Database changes must never leave the required code disabled. */
    for (cheatIndex = 0; cheatIndex < MAX_CODES && gCheats[cheatIndex].name[0] != NUL; cheatIndex++) {
        if (cheatConfigIsMasterCode(gCheats[cheatIndex].name))
            gCheats[cheatIndex].enabled = 1;
    }
}

void cheatConfigSaveSelections(config_set_t *configSet)
{
    int cheatIndex;
    int selectionIndex = 0;
    char key[CHEAT_SELECTION_KEY_MAX];

    if (configSet == NULL)
        return;

    /* Clear stale numbered values from earlier selections. */
    for (cheatIndex = 0; cheatIndex < MAX_CODES; cheatIndex++) {
        snprintf(key, sizeof(key), "%s%03d", CONFIG_ITEM_CHEAT_SELECTION_PREFIX, cheatIndex);
        configRemoveKey(configSet, key);
    }

    for (cheatIndex = 0; cheatIndex < MAX_CODES && gCheats[cheatIndex].name[0] != NUL; cheatIndex++) {
        if (gCheats[cheatIndex].enabled && !cheatConfigIsMasterCode(gCheats[cheatIndex].name)) {
            snprintf(key, sizeof(key), "%s%03d", CONFIG_ITEM_CHEAT_SELECTION_PREFIX, selectionIndex++);
            char value[CONFIG_KEY_VALUE_LEN];
            cheatSelectionKey(&gCheats[cheatIndex], value);
            configSetStr(configSet, key, value);
        }
    }
}
