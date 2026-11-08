/*
 * Maizo OPL - PlayStation 1 VCD browser and POPStarter handoff
 * Follows the ELM/PS1 page behavior of Jay-Jay-OPL/OPL-Daily-Builds.
 * POPStarter and Sony POPS runtime files are not included.
 */
#include "include/opl.h"
#include "include/lang.h"
#include "include/gui.h"
#include "include/system.h"
#include "include/ps1support.h"
#include "include/bdmsupport.h"
#include "include/ethsupport.h"
#include "include/hddsupport.h"
#include "include/textures.h"
#include "include/ioman.h"

#define PS1_MAX_GAMES      256
#define PS1_MAX_NAME       128
#define PS1_REFRESH_FRAMES 360

typedef struct ps1_game
{
    char name[PS1_MAX_NAME];
    char id[16];
    char vcd[256];
    char loader[256];
    char prefix[20];
    unsigned sizeMB;
    struct ps1_game *next;
} ps1_game_t;

static item_list_t ps1List;
static ps1_game_t *ps1Games;
static ps1_game_t *ps1GamesTail;
static int ps1Count;
static int ps1ForceRefresh = 1;

static void ps1Clear(void)
{
    ps1_game_t *next;
    while (ps1Games) {
        next = ps1Games->next;
        free(ps1Games);
        ps1Games = next;
    }
    ps1GamesTail = NULL;
    ps1Count = 0;
}

static ps1_game_t *ps1ByIndex(int id)
{
    ps1_game_t *cur = ps1Games;
    if (id < 0 || id >= ps1Count)
        return NULL;
    while (id-- > 0 && cur)
        cur = cur->next;
    return cur;
}

static unsigned int ps1NameHash(const char *name)
{
    unsigned int hash = 2166136261u;
    while (*name) {
        hash ^= (unsigned char)*name++;
        hash *= 16777619u;
    }
    return hash;
}

static int ps1Add(const char *folder, const char *filename)
{
    int len = strlen(filename), titleOffset = 0, tlen;
    ps1_game_t *game;
    struct stat st;
    char path[256];

    if (ps1Count >= PS1_MAX_GAMES || len <= 4 ||
        len >= PS1_MAX_NAME || strcasecmp(filename + len - 4, ".VCD"))
        return 0;
    if (snprintf(path, sizeof(path), "%s%s", folder, filename) >= sizeof(path))
        return 0;
    if (stat(path, &st) < 0 || st.st_size <= 0)
        return 0;
    game = calloc(1, sizeof(*game));
    if (!game)
        return 0;
    snprintf(game->vcd, sizeof(game->vcd), "%s", path);
    snprintf(game->prefix, sizeof(game->prefix), "PS1");
    snprintf(game->loader, sizeof(game->loader), "%sPOPSTARTER.ELF", folder);
    game->sizeMB = (unsigned)st.st_size >> 20;
    /* ID-prefixed names get automatic PS1 artwork keys; plain Game.VCD
     * is also accepted, without any extra config file. */
    if (len > 16 && filename[4] == '_' && filename[8] == '.' &&
        filename[11] == '.') {
        memcpy(game->id, filename, 11);
        titleOffset = 12;
    } else {
        /* Use the basename as a stable, distinct artwork key instead of
         * assigning every non-serial game the same "PS1" identifier. */
        snprintf(game->id, sizeof(game->id), "%08X", (unsigned)ps1NameHash(filename));
    }
    tlen = len - titleOffset - 4;
    if (tlen >= sizeof(game->name))
        tlen = sizeof(game->name) - 1;
    memcpy(game->name, filename + titleOffset, tlen);
    game->name[tlen] = 0;
    if (!ps1Games)
        ps1Games = game;
    else
        ps1GamesTail->next = game;
    ps1GamesTail = game;
    ps1Count++;
    return 1;
}

static void ps1ScanFolder(const char *folder)
{
    DIR *dir = opendir(folder);
    struct dirent *entry;
    if (!dir)
        return;
    while ((entry = readdir(dir)) != NULL && ps1Count < PS1_MAX_GAMES)
        ps1Add(folder, entry->d_name);
    closedir(dir);
}

static void ps1Init(item_list_t *list)
{
    ps1ForceRefresh = 1;
    list->enabled = 1;
}

static int ps1NeedsUpdate(item_list_t *list)
{
    /* UI refresh requests may be manual or timed, so always rescan. */
    (void)list;
    ps1ForceRefresh = 0;
    return 1;
}

static int ps1Update(item_list_t *list)
{
    int i;
    char folder[32];
    (void)list;
    ps1Clear();
    /* Standard mass0..4 USB FAT paths. POPS files must be colocated
     * with the selected VCD so game launches never depend on APPS configs. */
    for (i = 0; i < 5; i++) {
        snprintf(folder, sizeof(folder), "mass%d:POPS/", i);
        ps1ScanFolder(folder);
    }
    LOG("MAIZO PS1: %d VCD images discovered\n", ps1Count);
    return ps1Count;
}

static int ps1GetCount(item_list_t *list)
{
    (void)list;
    return ps1Count;
}

static char *ps1GetName(item_list_t *list, int id)
{
    ps1_game_t *game = ps1ByIndex(id);
    (void)list;
    return game ? game->name : "";
}

static int ps1GetNameLength(item_list_t *list, int id)
{
    (void)list;
    (void)id;
    return PS1_MAX_NAME;
}

static char *ps1GetStartup(item_list_t *list, int id)
{
    ps1_game_t *game = ps1ByIndex(id);
    (void)list;
    return game ? game->id : "";
}

static void ps1NoDelete(item_list_t *list, int id)
{
    (void)list;
    (void)id;
}

static void ps1NoRename(item_list_t *list, int id, char *name)
{
    (void)list;
    (void)id;
    (void)name;
}

static void ps1Launch(item_list_t *list, int id, config_set_t *configSet)
{
    ps1_game_t *game = ps1ByIndex(id);
    char vcd[256], elfPath[256], pakPath[256], fakePath[256], memPath[32];
    char *dot;
    unsigned char *image;
    int fd, pak, size, total, n, mode;
    (void)list;
    (void)configSet;

    if (!game)
        return;
    snprintf(vcd, sizeof(vcd), "%s", game->vcd);
    snprintf(elfPath, sizeof(elfPath), "%s", game->loader);
    snprintf(pakPath, sizeof(pakPath), "%s", game->loader);
    dot = strrchr(pakPath, '/');
    if (!dot)
        return;
    strcpy(dot + 1, "POPS_IOX.PAK");

    fd = open(elfPath, O_RDONLY);
    if (fd < 0) {
        guiMsgBox("PS1: coloque POPSTARTER.ELF na pasta POPS do USB.", 0, NULL);
        return;
    }
    pak = open(pakPath, O_RDONLY);
    if (pak < 0) {
        close(fd);
        guiMsgBox("PS1: falta POPS_IOX.PAK na pasta POPS (runtime do usuario).", 0, NULL);
        return;
    }
    close(pak);

    /* OPL Daily Builds handoff passes a synthetic XX.<game>.ELF path,
     * so the POPStarter binary locates the VCD without a per-game launcher. */
    snprintf(fakePath, sizeof(fakePath), "%s", vcd);
    dot = strrchr(fakePath, '/');
    if (!dot) {
        close(fd);
        return;
    }
    if (strlen(fakePath) + 3 >= sizeof(fakePath)) {
        close(fd);
        return;
    }
    memmove(dot + 4, dot + 1, strlen(dot + 1) + 1);
    memcpy(dot + 1, "XX.", 3);
    dot = strrchr(fakePath, '.');
    if (!dot || strcasecmp(dot, ".VCD")) {
        close(fd);
        return;
    }
    memcpy(dot, ".ELF", 5);

    size = lseek(fd, 0, SEEK_END);
    if (size < 52 || size > 8 * 1024 * 1024) {
        close(fd);
        guiMsgBox("PS1: POPSTARTER.ELF invalido ou muito grande.", 0, NULL);
        return;
    }
    lseek(fd, 0, SEEK_SET);
    image = malloc(size);
    if (!image) {
        close(fd);
        guiMsgBox("PS1: memoria insuficiente para POPSTARTER.", 0, NULL);
        return;
    }
    total = 0;
    while (total < size) {
        n = read(fd, image + total, size - total);
        if (n <= 0)
            break;
        total += n;
    }
    close(fd);
    if (total != size || image[0] != 0x7F || image[1] != 'E' ||
        image[2] != 'L' || image[3] != 'F') {
        free(image);
        guiMsgBox("PS1: POPSTARTER.ELF nao e um ELF valido.", 0, NULL);
        return;
    }

    /* PS1 can be enabled even when the PS2 BDM games page is disabled.
     * Derive the mounted mass unit directly, without depending on that page. */
    mode = -1;
    if (!strncmp(vcd, "mass", 4) && vcd[4] >= '0' && vcd[4] <= '4' &&
        vcd[5] == ':')
        mode = BDM_MODE + vcd[4] - '0';
    if (mode < BDM_MODE || mode > BDM_MODE4) {
        free(image);
        guiMsgBox("PS1: unidade de armazenamento nao suportada.", 0, NULL);
        return;
    }

    snprintf(memPath, sizeof(memPath), "mem:%u", (unsigned int)image);
    LOG("PS1 launch %s via %s\n", vcd, fakePath);
    deinit(UNMOUNT_EXCEPTION, mode);
    sysExecElfWithParam(memPath, fakePath);
    /* Exec normally does not return. */
}

static config_set_t *ps1GetConfig(item_list_t *list, int id)
{
    ps1_game_t *game = ps1ByIndex(id);
    config_set_t *cfg = configAlloc(1, NULL, NULL);
    (void)list;
    if (game && cfg) {
        configSetStr(cfg, CONFIG_ITEM_NAME, game->name);
        configSetStr(cfg, CONFIG_ITEM_LONGNAME, game->name);
        configSetStr(cfg, CONFIG_ITEM_STARTUP, game->id);
        configSetInt(cfg, CONFIG_ITEM_SIZE, game->sizeMB);
        configSetStr(cfg, CONFIG_ITEM_FORMAT, "VCD");
        configSetStr(cfg, CONFIG_ITEM_MEDIA, "PS1");
    }
    return cfg;
}

static int ps1GetImage(item_list_t *list, char *folder, int relative, char *value, char *suffix, GSTEXTURE *tex, short psm)
{
    int i;
    char path[256];
    (void)list;

    /* Do not delegate to oplGetAppImage(): that dispatcher visits PS1
     * again, causing infinite recursion if an artwork file is absent.
     * Reuse the existing OPL ART/<key>_<type>.png naming convention. */
    if (!relative) {
        if (snprintf(path, sizeof(path), "%s%s_%s", folder, value, suffix) >= sizeof(path))
            return -1;
        return texDiscoverLoad(tex, path, -1);
    }
    for (i = 0; i < 5; i++) {
        if (snprintf(path, sizeof(path), "mass%d:%s/%s_%s", i, folder, value, suffix) >= sizeof(path))
            continue;
        if (texDiscoverLoad(tex, path, -1) >= 0)
            return 0;
    }
    return -1;
}

static int ps1TextId(item_list_t *list)
{
    (void)list;
    return _STR_PS1_GAMES;
}

static int ps1IconId(item_list_t *list)
{
    (void)list;
    return APP_ICON;
}

static void ps1Clean(item_list_t *list, int exception)
{
    (void)list;
    (void)exception;
    ps1Clear();
}

static void ps1Shutdown(item_list_t *list)
{
    (void)list;
    ps1Clear();
}

item_list_t *ps1GetObject(int initOnly)
{
    if (initOnly && !ps1List.enabled)
        return NULL;
    return &ps1List;
}

static item_list_t ps1List = {
    PS1_MODE, -1, 0, MODE_FLAG_NO_COMPAT | MODE_FLAG_NO_UPDATE,
    MENU_MIN_INACTIVE_FRAMES, PS1_REFRESH_FRAMES, NULL, NULL,
    &ps1TextId, NULL, &ps1Init, &ps1NeedsUpdate, &ps1Update,
    &ps1GetCount, NULL, &ps1GetName, &ps1GetNameLength, &ps1GetStartup,
    &ps1NoDelete, &ps1NoRename, &ps1Launch, &ps1GetConfig, &ps1GetImage,
    &ps1Clean, &ps1Shutdown, NULL, &ps1IconId};
