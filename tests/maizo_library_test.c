#include "maizo_library.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void testLibrary(void)
{
    maizo_library_t library = {0};
    int usb, smb, ps1, apps;
    char firstKey[32], secondKey[32], platformKey[32];
    assert(maizoLibraryAdd(&library, &usb, &usb, MAIZO_PS2, "SLUS_123.45", "Alpha", 1));
    assert(maizoLibraryAdd(&library, &smb, &smb, MAIZO_PS2, "SLUS_123.45", "Alpha", 0));
    assert(maizoLibraryAdd(&library, &usb, &usb, MAIZO_PS2, "SLUS_123.45", "Alpha", 1));
    assert(maizoLibraryAdd(&library, &ps1, &ps1, MAIZO_PS1, "SLUS_123.45", "Beta PS1", 0));
    assert(maizoLibraryAdd(&library, &apps, &apps, MAIZO_APP, "mass0:/APPS/A/BOOT.ELF", "Gamma", 1));
    assert(maizoLibraryAdd(&library, &apps, &apps, MAIZO_APP, "mass0:/APPS/B/BOOT.ELF", "Delta", 0));
    maizoLibrarySort(&library);
    assert(library.count == 5); /* Distinct sources survive; repeated source is deduplicated. */
    assert(maizoLibraryFilter(&library, MAIZO_ALL));
    assert(library.view_count == 5);
    assert(maizoLibrarySlot(&library, -1) == NULL);
    assert(maizoLibrarySlot(&library, 5) == NULL);
    assert(strcmp(maizoLibrarySlot(&library, 0)->title, "Alpha") == 0);
    maizoLibraryMove(&library, 1);
    assert(strcmp(maizoLibrarySlot(&library, 0)->title, "Alpha") == 0);
    assert(maizoLibraryFilter(&library, MAIZO_APPS_TAB));
    assert(library.view_count == 2);
    maizoLibraryMove(&library, -1);
    assert(strcmp(maizoLibrarySlot(&library, 0)->title, "Gamma") == 0);
    assert(maizoLibraryFilter(&library, MAIZO_ALL));
    assert(strcmp(maizoLibrarySlot(&library, 0)->title, "Alpha") == 0);
    assert(maizoLibraryFilter(&library, MAIZO_APPS_TAB));
    assert(strcmp(maizoLibrarySlot(&library, 0)->title, "Gamma") == 0);
    assert(maizoLibraryFilter(&library, MAIZO_PS2_TAB));
    assert(library.view_count == 2);
    assert(maizoLibraryFilter(&library, MAIZO_PS1_TAB));
    assert(library.view_count == 1 && maizoLibrarySlot(&library, 0)->owner == &ps1);
    assert(maizoLibraryFilter(&library, MAIZO_FAVORITES));
    assert(library.view_count == 2);
    maizoLibrarySlot(&library, 0)->favorite = 0;
    assert(maizoLibraryFilter(&library, MAIZO_FAVORITES));
    assert(library.view_count == 1);
    maizoFavoriteKey(MAIZO_PS2, "SLUS_123.45", firstKey);
    maizoFavoriteKey(MAIZO_PS2, "SLUS_123.45", secondKey);
    maizoFavoriteKey(MAIZO_PS1, "SLUS_123.45", platformKey);
    assert(strlen(firstKey) < 32 && strcmp(firstKey, secondKey) == 0);
    assert(strcmp(firstKey, platformKey) != 0);
    maizoLibrarySortMode(&library, 2);
    assert(strcmp(library.entries[0].title, "Alpha") == 0);
    assert(strcmp(library.entries[1].title, "Alpha") == 0);
    assert(strcmp(library.entries[2].title, "Beta PS1") == 0);
    assert(strcmp(library.entries[3].title, "Gamma") == 0);
    maizoLibrarySortMode(&library, 1);
    assert(library.entries[0].platform == MAIZO_PS2);
    assert(library.entries[2].platform == MAIZO_PS1);
    maizoLibrarySortMode(&library, 0);
    assert(!maizoLibraryFilter(&library, -1));
    assert(!maizoLibraryFilter(&library, MAIZO_TAB_COUNT));
    assert(maizoLibraryFilter(&library, MAIZO_SETTINGS));
    assert(library.view_count == 0);
    maizoLibraryMove(&library, 1);
    maizoLibraryClear(&library);
    maizoLibraryClear(&library);
}

static void testLargeRefresh(void)
{
    maizo_library_t library = {0};
    char identity[40], title[40];
    int i;
    for (i = 0; i < 12000; i++) {
        snprintf(identity, sizeof(identity), "GAME_%05d", i);
        snprintf(title, sizeof(title), "Title %05d", 12000 - i);
        assert(maizoLibraryAdd(&library, NULL, NULL, MAIZO_PS2, identity, title, i % 3 == 0));
    }
    maizoLibrarySortMode(&library, 0);
    assert(maizoLibraryFilter(&library, MAIZO_ALL));
    assert(library.view_count == 12000);
    for (i = 0; i < 12001; i++)
        maizoLibraryMove(&library, 1);
    char selected[256];
    strcpy(selected, maizoLibrarySlot(&library, 0)->identity);
    assert(maizoLibraryFilter(&library, MAIZO_FAVORITES));
    assert(library.view_count == 4000);
    assert(maizoLibraryFilter(&library, MAIZO_ALL));
    assert(strcmp(maizoLibrarySlot(&library, 0)->identity, selected) == 0);
    maizoLibraryClear(&library);
    assert(maizoLibraryAdd(&library, NULL, NULL, MAIZO_PS2, selected, "Restored", 0));
    maizoLibrarySort(&library);
    assert(maizoLibraryFilter(&library, MAIZO_ALL));
    assert(library.position == 0 && library.view_count == 1);
    maizoLibraryClear(&library);
}

int main(void)
{
    testLibrary();
    testLargeRefresh();
    puts("MAIZO library: stable sources, sort modes, favorites, hotplug and 12000-item navigation passed");
    return 0;
}
