#ifndef MAIZO_LIBRARY_H
#define MAIZO_LIBRARY_H

#include <stddef.h>
#include <stdint.h>

enum maizo_platform { MAIZO_PS2,
                      MAIZO_PS1,
                      MAIZO_APP };
enum maizo_tab { MAIZO_ALL,
                 MAIZO_PS2_TAB,
                 MAIZO_PS1_TAB,
                 MAIZO_APPS_TAB,
                 MAIZO_FAVORITES,
                 MAIZO_SETTINGS,
                 MAIZO_TAB_COUNT };

typedef struct
{
    void *owner;
    void *item;
    char *identity;
    char *title;
    int platform;
    int favorite;
    size_t ordinal;
} maizo_entry_t;

typedef struct
{
    maizo_entry_t *entries;
    size_t count, capacity;
    size_t *view;
    size_t view_count, position;
    int tab;
    char remembered[MAIZO_SETTINGS][256];
    int remembered_platform[MAIZO_SETTINGS];
} maizo_library_t;

void maizoLibraryClear(maizo_library_t *library);
int maizoLibraryAdd(maizo_library_t *library, void *owner, void *item, int platform, const char *identity, const char *title, int favorite);
void maizoLibrarySort(maizo_library_t *library);
/* 0: title, 1: platform+title, 2: original source order. */
void maizoLibrarySortMode(maizo_library_t *library, int mode);
int maizoLibraryFilter(maizo_library_t *library, int tab);
void maizoLibraryMove(maizo_library_t *library, int direction);
maizo_entry_t *maizoLibrarySlot(maizo_library_t *library, int offset);
void maizoFavoriteKey(int platform, const char *identity, char key[32]);

#endif
