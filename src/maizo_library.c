/* MAIZO library model: no GS, I/O or launch ownership. */
#include "maizo_library.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

static char *copyString(const char *value)
{
    size_t size = strlen(value) + 1;
    char *copy = malloc(size);
    if (copy)
        memcpy(copy, value, size);
    return copy;
}

void maizoFavoriteKey(int platform, const char *identity, char key[32])
{
    uint64_t hash = UINT64_C(14695981039346656037);
    const unsigned char *p = (const unsigned char *)identity;
    while (*p) {
        hash ^= *p++;
        hash *= UINT64_C(1099511628211);
    }
    snprintf(key, 32, "maizo_f%d_%08lx%08lx", platform,
             (unsigned long)(uint32_t)(hash >> 32), (unsigned long)(uint32_t)hash);
}

void maizoLibraryClear(maizo_library_t *library)
{
    size_t i;
    for (i = 0; i < library->count; i++) {
        free(library->entries[i].identity);
        free(library->entries[i].title);
    }
    free(library->entries);
    free(library->view);
    library->entries = NULL;
    library->view = NULL;
    library->count = library->capacity = library->view_count = 0;
    library->position = 0;
}

int maizoLibraryAdd(maizo_library_t *library, void *owner, void *item, int platform, const char *identity, const char *title, int favorite)
{
    maizo_entry_t entry;
    if (!identity || !title || strlen(identity) >= 256)
        return 0;
    if (library->count == library->capacity) {
        size_t capacity = library->capacity ? library->capacity * 2 : 64;
        maizo_entry_t *entries = realloc(library->entries, capacity * sizeof(*entries));
        if (!entries)
            return 0;
        library->entries = entries;
        library->capacity = capacity;
    }
    entry.identity = copyString(identity);
    entry.title = copyString(title);
    if (!entry.identity || !entry.title) {
        free(entry.identity);
        free(entry.title);
        return 0;
    }
    entry.owner = owner;
    entry.item = item;
    entry.platform = platform;
    entry.favorite = favorite;
    entry.ordinal = library->count;
    library->entries[library->count++] = entry;
    return 1;
}

static int compareIdentity(const void *a, const void *b)
{
    const maizo_entry_t *left = a, *right = b;
    int result = left->platform - right->platform;
    return result ? result : strcmp(left->identity, right->identity);
}

static int compareSourceOrder(const void *a, const void *b)
{
    const maizo_entry_t *left = a, *right = b;
    int identity = compareIdentity(a, b);
    if (identity)
        return identity;
    return left->ordinal < right->ordinal ? -1 : left->ordinal > right->ordinal ? 1 :
                                                                                  0;
}

static int compareTitle(const void *a, const void *b)
{
    const maizo_entry_t *left = a, *right = b;
    int result = strcasecmp(left->title, right->title);
    return result ? result : compareIdentity(a, b);
}

static int comparePlatformTitle(const void *a, const void *b)
{
    const maizo_entry_t *left = a, *right = b;
    int platform = left->platform - right->platform;
    return platform ? platform : compareTitle(a, b);
}

static int compareOrdinal(const void *a, const void *b)
{
    const maizo_entry_t *left = a, *right = b;
    return left->ordinal < right->ordinal ? -1 : left->ordinal > right->ordinal ? 1 :
                                                                                  0;
}

void maizoLibrarySortMode(maizo_library_t *library, int mode)
{
    size_t read, write = 0;
    if (!library->count)
        return;
    qsort(library->entries, library->count, sizeof(*library->entries), compareSourceOrder);
    for (read = 0; read < library->count; read++) {
        maizo_entry_t *entry = &library->entries[read];
        size_t scan;
        int duplicate = 0;
        /* Separate devices with the same disc ID are distinct installations.
         * Deduplicate only repeated items from the same real source. */
        for (scan = write; scan > 0; scan--) {
            maizo_entry_t *previous = &library->entries[scan - 1];
            if (compareIdentity(previous, entry) != 0)
                break;
            if (previous->owner == entry->owner) {
                previous->favorite |= entry->favorite;
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            free(entry->identity);
            free(entry->title);
        } else {
            library->entries[write++] = *entry;
        }
    }
    library->count = write;
    qsort(library->entries, library->count, sizeof(*library->entries),
          mode == 1 ? comparePlatformTitle : mode == 2 ? compareOrdinal :
                                                        compareTitle);
}

void maizoLibrarySort(maizo_library_t *library)
{
    maizoLibrarySortMode(library, 0);
}

static void remember(maizo_library_t *library)
{
    maizo_entry_t *entry = maizoLibrarySlot(library, 0);
    if (entry && library->tab < MAIZO_SETTINGS) {
        strcpy(library->remembered[library->tab], entry->identity);
        library->remembered_platform[library->tab] = entry->platform;
    }
}

int maizoLibraryFilter(maizo_library_t *library, int tab)
{
    size_t i;
    size_t *view;
    if (tab < 0 || tab >= MAIZO_TAB_COUNT)
        return 0;
    remember(library);
    view = library->count ? malloc(library->count * sizeof(*view)) : NULL;
    if (library->count && !view)
        return 0;
    free(library->view);
    library->view = view;
    library->view_count = library->position = 0;
    library->tab = tab;
    for (i = 0; i < library->count; i++) {
        maizo_entry_t *entry = &library->entries[i];
        int match = tab == MAIZO_ALL || (tab == MAIZO_PS2_TAB && entry->platform == MAIZO_PS2) ||
                    (tab == MAIZO_PS1_TAB && entry->platform == MAIZO_PS1) ||
                    (tab == MAIZO_APPS_TAB && entry->platform == MAIZO_APP) ||
                    (tab == MAIZO_FAVORITES && entry->favorite);
        if (!match)
            continue;
        if (entry->platform == library->remembered_platform[tab] &&
            strcmp(entry->identity, library->remembered[tab]) == 0)
            library->position = library->view_count;
        library->view[library->view_count++] = i;
    }
    return 1;
}

void maizoLibraryMove(maizo_library_t *library, int direction)
{
    if (!library->view_count)
        return;
    if (direction < 0)
        library->position = library->position ? library->position - 1 : library->view_count - 1;
    else
        library->position = (library->position + 1) % library->view_count;
    remember(library);
}

maizo_entry_t *maizoLibrarySlot(maizo_library_t *library, int offset)
{
    long position = (long)library->position + offset;
    if (position < 0 || (size_t)position >= library->view_count)
        return NULL;
    return &library->entries[library->view[position]];
}
