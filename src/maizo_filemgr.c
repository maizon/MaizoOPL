/*
 * MAIZO Supreme native file browser, inspired by wLaunchELF's device /
 * file / clipboard workflow. Original implementation; does not bundle or
 * copy wLaunchELF code. Optional advanced handoff launches a separately
 * installed wLaunchELF ELF with OPL's existing ELF loader.
 */
#include "include/opl.h"
#include "include/maizo_filemgr.h"
#include "include/maizo_ui.h"
#include "include/ethsupport.h"
#include "include/system.h"
#include "include/gui.h"
#include "include/dia.h"
#include "include/pad.h"
#include "include/themes.h"
#include "include/fntsys.h"
#include "include/renderman.h"
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <elf-loader.h>
#include <strings.h>

#define FM_PATH  384
#define FM_NAME  256
#define FM_ITEMS 128
#define FM_DEPTH 9
#define FM_CHUNK 16384

typedef struct
{
    char name[FM_NAME];
    unsigned int size;
    unsigned char folder;
} fm_item_t;

typedef struct
{
    char root[FM_PATH], path[FM_PATH];
    fm_item_t items[FM_ITEMS];
    int count, selection, scroll, error;
} fm_pane_t;

static struct
{
    fm_pane_t pane[2];
    int current, clipboardSet, clipMove;
    char clipboard[FM_PATH], status[164];
    unsigned char clipboardDir;
} fm;

static int screenWidth;

static const char *const fm_devices[] = {
    "mass0:/", "mass1:/", "mc0:/", "mc1:/"};

static void fm_status(const char *message)
{
    snprintf(fm.status, sizeof(fm.status), "%s", message);
}

static int fm_join(char *out, size_t len, const char *base,
                   const char *name)
{
    size_t n = strlen(base);
    char slash = !strncmp(base, "smb", 3) ? '\\' : '/';
    if (!name || !name[0] || strstr(name, "..") ||
        strchr(name, ':') || strchr(name, '/') || strchr(name, '\\'))
        return 0;
    if (n + strlen(name) + 2 > len)
        return 0;
    snprintf(out, len, "%s%s%s", base,
             n && base[n - 1] != slash && base[n - 1] != ':' ?
                 (slash == '/' ? "/" : "\\") :
                 (n && base[n - 1] == ':' ?
                      (slash == '/' ? "/" : "\\") :
                      ""),
             name);
    return 1;
}

static int fm_same_device(const char *a, const char *b)
{
    const char *ca = strchr(a, ':'), *cb = strchr(b, ':');
    return ca && cb && ca - a == cb - b &&
           !strncmp(a, b, ca - a + 1);
}

static int fm_is_child(const char *parent, const char *maybeChild)
{
    size_t size = strlen(parent);
    return !strncmp(parent, maybeChild, size) &&
           (maybeChild[size] == '/' || maybeChild[size] == '\\');
}

static int fm_compare(const void *a, const void *b)
{
    const fm_item_t *left = a, *right = b;
    if (left->folder != right->folder)
        return right->folder - left->folder;
    return strcasecmp(left->name, right->name);
}

static int fm_reload(fm_pane_t *pane)
{
    iox_dirent_t entry;
    int dd, count = 0, ret;
    dd = fileXioDopen(pane->path);
    pane->count = 0;
    pane->error = dd < 0 ? dd : 0;
    if (dd < 0)
        return -1;
    /* Bounded to protect EE RAM from folders with thousands of files. */
    while (count < FM_ITEMS && (ret = fileXioDread(dd, &entry)) > 0) {
        fm_item_t *dst = &pane->items[count];
        if (!strcmp(entry.name, ".") || !strcmp(entry.name, "..") ||
            !entry.name[0])
            continue;
        snprintf(dst->name, sizeof(dst->name), "%s", entry.name);
        dst->folder = FIO_S_ISDIR(entry.stat.mode) != 0;
        dst->size = entry.stat.size;
        count++;
    }
    fileXioDclose(dd);
    pane->count = count;
    qsort(pane->items, count, sizeof(fm_item_t), fm_compare);
    if (pane->selection >= count)
        pane->selection = count ? count - 1 : 0;
    if (pane->selection < 0)
        pane->selection = 0;
    if (pane->scroll > pane->selection)
        pane->scroll = pane->selection;
    return count;
}

static void fm_set_root(fm_pane_t *pane, const char *root)
{
    snprintf(pane->root, sizeof(pane->root), "%s", root);
    snprintf(pane->path, sizeof(pane->path), "%s", root);
    pane->selection = pane->scroll = 0;
    fm_reload(pane);
}

static int fm_parent(fm_pane_t *pane)
{
    char *end, *sep;
    size_t min = strlen(pane->root);
    if (strlen(pane->path) <= min)
        return 0;
    end = pane->path + strlen(pane->path);
    while (end > pane->path + min && (end[-1] == '/' || end[-1] == '\\'))
        end--;
    sep = end;
    while (sep > pane->path + min && sep[-1] != '/' && sep[-1] != '\\')
        sep--;
    if (sep <= pane->path + min) {
        snprintf(pane->path, sizeof(pane->path), "%s", pane->root);
    } else {
        *sep = 0;
    }
    pane->selection = pane->scroll = 0;
    fm_reload(pane);
    return 1;
}

static int fm_selected(fm_pane_t *pane, char *full, size_t len)
{
    if (pane->selection >= pane->count || pane->count < 1)
        return 0;
    return fm_join(full, len, pane->path,
                   pane->items[pane->selection].name);
}

static void fm_pick_device(fm_pane_t *pane, int forward)
{
    const char *roots[6];
    char smb[FM_PATH];
    int idx = 0, size = 4;
    item_list_t *eth = ethGetObject(1);
    for (int i = 0; i < 4; i++)
        roots[i] = fm_devices[i];
    if (gNetworkStartup == 0 && gPCShareName[0] && eth &&
        eth->itemGetPrefix) {
        const char *prefix = eth->itemGetPrefix(eth);
        if (prefix && !strncmp(prefix, "smb0:", 5)) {
            snprintf(smb, sizeof(smb), "%s", prefix);
            roots[size++] = smb;
        }
    }
    for (int i = 0; i < size; i++)
        if (!strcmp(pane->root, roots[i]))
            idx = i;
    idx = (idx + size + (forward ? 1 : -1)) % size;
    fm_set_root(pane, roots[idx]);
    if (pane->error)
        fm_status("Dispositivo indisponivel. Troque a unidade.");
    else if (!strncmp(pane->path, "smb", 3))
        fm_status("SMB conectado: usando automaticamente a sessao OPL.");
    else
        fm_status("Unidade selecionada.");
}

static int fm_exists(const char *path)
{
    iox_stat_t st;
    return fileXioGetStat(path, &st) >= 0;
}

static int fm_copy_file(const char *source, const char *dest)
{
    int fd = -1, out = -1, got, result = -1;
    unsigned char *buffer;
    if (fm_exists(dest))
        return -EEXIST; /* Never silently overwrite save files. */
    buffer = malloc(FM_CHUNK);
    if (!buffer)
        return -ENOMEM;
    fd = fileXioOpen(source, O_RDONLY, 0);
    if (fd < 0)
        goto done;
    out = fileXioOpen(dest, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (out < 0)
        goto done;
    for (;;) {
        got = fileXioRead(fd, buffer, FM_CHUNK);
        if (got < 0)
            break;
        if (got == 0) {
            result = 0;
            break;
        }
        for (int off = 0; off < got;) {
            int sent = fileXioWrite(out, buffer + off, got - off);
            if (sent <= 0)
                goto done;
            off += sent;
        }
    }
done:
    if (out >= 0)
        fileXioClose(out);
    if (fd >= 0)
        fileXioClose(fd);
    if (result && out >= 0)
        fileXioRemove(dest); /* Drop a partial copy, keep source. */
    free(buffer);
    return result;
}

static int fm_copy_tree(const char *source, const char *dest,
                        int folder, int depth)
{
    iox_dirent_t item;
    int dd, ret = 0;
    char srcChild[FM_PATH], dstChild[FM_PATH];
    if (depth > FM_DEPTH)
        return -ELOOP;
    if (!folder)
        return fm_copy_file(source, dest);
    if (fm_exists(dest))
        return -EEXIST;
    if (fm_is_child(source, dest))
        return -EINVAL;
    if (fileXioMkdir(dest, 0777) < 0)
        return -EACCES;
    dd = fileXioDopen(source);
    if (dd < 0)
        return dd;
    while (fileXioDread(dd, &item) > 0) {
        if (!strcmp(item.name, ".") || !strcmp(item.name, ".."))
            continue;
        if (!fm_join(srcChild, sizeof(srcChild), source, item.name) ||
            !fm_join(dstChild, sizeof(dstChild), dest, item.name)) {
            ret = -ENAMETOOLONG;
            break;
        }
        ret = fm_copy_tree(srcChild, dstChild,
                           FIO_S_ISDIR(item.stat.mode), depth + 1);
        if (ret < 0)
            break;
    }
    fileXioDclose(dd);
    return ret;
}

static int fm_remove_tree(const char *path, int folder, int depth)
{
    iox_dirent_t item;
    int dd, ret = 0;
    char full[FM_PATH];
    if (depth > FM_DEPTH)
        return -ELOOP;
    if (!folder)
        return fileXioRemove(path);
    dd = fileXioDopen(path);
    if (dd < 0)
        return dd;
    while (fileXioDread(dd, &item) > 0) {
        if (!strcmp(item.name, ".") || !strcmp(item.name, ".."))
            continue;
        if (!fm_join(full, sizeof(full), path, item.name)) {
            ret = -ENAMETOOLONG;
            break;
        }
        ret = fm_remove_tree(full, FIO_S_ISDIR(item.stat.mode), depth + 1);
        if (ret < 0)
            break;
    }
    fileXioDclose(dd);
    return ret < 0 ? ret : fileXioRmdir(path);
}

static void fm_draw_pane(fm_pane_t *pane, int x, int focused)
{
    int top = 117, h = gTheme->usedHeight - 211;
    char title[60];
    maizoDrawPanel(x, top, 287, h, focused);
    snprintf(title, sizeof(title), "%s", pane->root);
    maizoDrawIcon(!strncmp(pane->root, "smb", 3) ?
                      MAIZO_ICON_NETWORK :
                      MAIZO_ICON_FOLDER,
                  x + 13, top + 12, 22,
                  focused ? MAIZO_ACCENT : MAIZO_MUTED);
    fntRenderString(gTheme->fonts[0], x + 45, top + 13,
                    ALIGN_NONE, 227, 26, title, MAIZO_TEXT);
    fntRenderString(gTheme->fonts[0], x + 12, top + 44,
                    ALIGN_NONE, 261, 31, pane->path, MAIZO_MUTED);
    maizoDrawSeparator(x + 10, top + 73, 265);

    if (pane->error)
        fntRenderString(gTheme->fonts[0], x + 18, top + 109,
                        ALIGN_NONE, 251, 53,
                        "Nao foi possivel listar esta unidade.",
                        MAIZO_WARNING);
    for (int i = 0; i < 7; ++i) {
        int index = pane->scroll + i, y = top + 85 + i * 28;
        const fm_item_t *entry;
        if (index >= pane->count)
            break;
        entry = &pane->items[index];
        if (index == pane->selection && focused) {
            rmDrawRect(x + 9, y, 269, 27, MAIZO_CARD);
            rmDrawRect(x + 9, y, 3, 27, MAIZO_ACCENT);
        }
        maizoDrawIcon(entry->folder ? MAIZO_ICON_FOLDER : MAIZO_ICON_FILE,
                      x + 14, y + 3, 20,
                      entry->folder ? MAIZO_ACCENT : MAIZO_MUTED);
        fntRenderString(gTheme->fonts[0], x + 43, y + 3,
                        ALIGN_NONE, 226, 25, entry->name,
                        index == pane->selection && focused ?
                            MAIZO_TEXT :
                            MAIZO_MUTED);
    }
}

static void fm_draw(void)
{
    char server[105];
    rmDrawRect(0, 0, screenWidth, gTheme->usedHeight, MAIZO_BLACK);
    maizoDrawBrand();
    maizoDrawIcon(MAIZO_ICON_FOLDER, 31, 78, 27, MAIZO_ACCENT);
    fntRenderString(gTheme->fonts[0], 69, 80, ALIGN_NONE,
                    528, 28, "ARQUIVOS / GERENCIADOR", MAIZO_TEXT);
    fm_draw_pane(&fm.pane[0], 19, fm.current == 0);
    fm_draw_pane(&fm.pane[1], 334, fm.current == 1);
    if (gNetworkStartup == 0 && gPCShareName[0]) {
        if (gPCShareAddressIsNetBIOS)
            snprintf(server, sizeof(server), "SMB %s:%d / %s",
                     gPCShareNBAddress, gPCPort, gPCShareName);
        else
            snprintf(server, sizeof(server),
                     "SMB %d.%d.%d.%d:%d / %s",
                     pc_ip[0], pc_ip[1], pc_ip[2], pc_ip[3],
                     gPCPort, gPCShareName);
    } else {
        snprintf(server, sizeof(server),
                 "SMB offline: use a configuracao de rede existente.");
    }
    fntRenderString(gTheme->fonts[0], 31, gTheme->usedHeight - 89,
                    ALIGN_NONE, 573, 25, server, MAIZO_MUTED);
    rmDrawRect(0, gTheme->usedHeight - 63,
               screenWidth, 63, MAIZO_SURFACE);
    fntRenderString(gTheme->fonts[0], 20, gTheme->usedHeight - 57,
                    ALIGN_NONE, screenWidth - 38, 25, fm.status, MAIZO_TEXT);
    fntRenderString(gTheme->fonts[0], 20, gTheme->usedHeight - 32,
                    ALIGN_NONE, screenWidth - 38, 24,
                    "X/O ABRIR  L/R PAINEL  L2/R2 UNIDADE  [] COPIAR  /\\ COLAR  START OPCOES",
                    MAIZO_MUTED);
}

static void fm_wlaunch(void)
{
    static const char *const paths[] = {
        "mass0:/APPS/wLaunchELF/BOOT.ELF",
        "mass0:/APPS/WLE/BOOT.ELF",
        "mass0:/wLE.ELF",
        "mass1:/APPS/wLaunchELF/BOOT.ELF",
        "mc0:/APPS/wLaunchELF/BOOT.ELF",
        "mc1:/APPS/wLaunchELF/BOOT.ELF"};
    for (unsigned int i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        if (fm_exists(paths[i])) {
            if (guiMsgBox("Iniciar wLaunchELF externo?", 1, NULL)) {
                char filename[FM_PATH];
                snprintf(filename, sizeof(filename), "%s", paths[i]);
                deinit(UNMOUNT_EXCEPTION, APP_MODE);
                LoadELFFromFileWithPartition(filename, "", 0, NULL);
            }
            return;
        }
    }
    guiMsgBox("wLaunchELF nao instalado. Copie seu BOOT.ELF para mass0:/APPS/wLaunchELF/", 0, NULL);
}

static void fm_options(void)
{
    fm_pane_t *pane = &fm.pane[fm.current];
    char src[FM_PATH], dst[FM_PATH], name[FM_NAME];
    int have = fm_selected(pane, src, sizeof(src));
    int action;
    struct UIItem options[] = {
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"MAIZO / ARQUIVOS", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 201, have, have, -1, 0, 0, {.label = {"Copiar selecao", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 202, fm.clipboardSet, fm.clipboardSet, -1, 0, 0, {.label = {"Colar nesta pasta", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 203, have, have, -1, 0, 0, {.label = {"Mover / recortar", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 204, have, have, -1, 0, 0, {.label = {"Renomear", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 205, 1, 1, -1, 0, 0, {.label = {"Nova pasta", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 206, have, have, -1, 0, 0, {.label = {"Excluir", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 207, have, have, -1, 0, 0, {.label = {"Detalhes", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 208, 1, 1, -1, 0, 0, {.label = {"Abrir wLaunchELF instalado", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 209, 1, 1, -1, 0, 0, {.label = {"Atualizar arquivos", -1}}},
        {UI_BREAK},
        {UI_TERMINATOR}};
    action = diaExecuteDialog(options, -1, 1, NULL);
    if (action == 201 || action == 203) {
        if (have) {
            snprintf(fm.clipboard, sizeof(fm.clipboard), "%s", src);
            fm.clipboardSet = 1;
            fm.clipMove = action == 203;
            fm.clipboardDir = pane->items[pane->selection].folder;
            fm_status(action == 203 ? "Recortado. Selecione destino e cole." :
                                      "Copiado. Selecione destino e cole.");
        }
    } else if (action == 202 && fm.clipboardSet) {
        const char *filename = strrchr(fm.clipboard, '/');
        const char *slash = strrchr(fm.clipboard, '\\');
        if (!filename || (slash && slash > filename))
            filename = slash;
        filename = filename ? filename + 1 : fm.clipboard;
        if (!fm_join(dst, sizeof(dst), pane->path, filename) ||
            !strcmp(fm.clipboard, dst) ||
            (fm.clipboardDir && fm_is_child(fm.clipboard, dst))) {
            fm_status("Destino invalido ou dentro da origem.");
        } else if (fm.clipMove && fm_same_device(fm.clipboard, dst)) {
            if (!fm_exists(dst) && fileXioRename(fm.clipboard, dst) >= 0) {
                fm.clipboardSet = 0;
                fm_status("Arquivo movido.");
            } else
                fm_status("Falha ao mover; origem preservada.");
        } else {
            fm_status("Copiando arquivos: nao desligue o console.");
            if (fm_copy_tree(fm.clipboard, dst, fm.clipboardDir, 0) >= 0) {
                fm.clipboardSet = 0;
                fm_status("Copia finalizada. Origem preservada.");
            } else {
                fm_status("Erro de copia / sem permissao. Origem preservada.");
            }
        }
    } else if (action == 204 && have) {
        snprintf(name, sizeof(name), "%s", pane->items[pane->selection].name);
        if (diaShowKeyb(name, sizeof(name), 0, "RENOMEAR") &&
            fm_join(dst, sizeof(dst), pane->path, name) &&
            !fm_exists(dst)) {
            if (fileXioRename(src, dst) < 0)
                fm_status("Nao foi possivel renomear neste dispositivo.");
            else
                fm_status("Item renomeado.");
        }
    } else if (action == 205) {
        name[0] = 0;
        if (diaShowKeyb(name, sizeof(name), 0, "NOVA PASTA") &&
            fm_join(dst, sizeof(dst), pane->path, name) &&
            !fm_exists(dst)) {
            if (fileXioMkdir(dst, 0777) < 0)
                fm_status("Nao foi possivel criar pasta aqui.");
            else
                fm_status("Nova pasta criada.");
        }
    } else if (action == 206 && have) {
        if (guiMsgBox("EXCLUIR este item e todos os arquivos internos? Sem desfazer.", 1, NULL)) {
            if (fm_remove_tree(src, pane->items[pane->selection].folder, 0) < 0)
                fm_status("Falha ao excluir. Verifique permissao e arquivos.");
            else
                fm_status("Exclusao concluida.");
        }
    } else if (action == 207 && have) {
        char info[210];
        snprintf(info, sizeof(info), "%s\n%s\n%u bytes",
                 pane->items[pane->selection].folder ? "Pasta" : "Arquivo",
                 pane->items[pane->selection].name,
                 pane->items[pane->selection].size);
        guiMsgBox(info, 0, NULL);
    } else if (action == 208)
        fm_wlaunch();
    else if (action == 209)
        fm_status("Lista atualizada.");
    fm_reload(&fm.pane[0]);
    fm_reload(&fm.pane[1]);
}

void maizoFileManagerOpen(void)
{
    int screenHeight;
    rmGetScreenExtents(&screenWidth, &screenHeight);
    memset(&fm, 0, sizeof(fm));
    fm_set_root(&fm.pane[0], "mass0:/");
    fm_set_root(&fm.pane[1], "mc0:/");
    fm_status("Gerenciador nativo MAIZO. wLaunchELF disponivel nas opcoes.");
    for (;;) {
        fm_pane_t *pane = &fm.pane[fm.current];
        int select, back;
        rmStartFrame();
        readPads();
        fm_draw();
        rmEndFrame();

        back = gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE;
        select = getKeyOn(gSelectButton);
        if (getKeyOn(KEY_L1) || getKeyOn(KEY_R1) ||
            getKeyOn(KEY_LEFT) || getKeyOn(KEY_RIGHT)) {
            fm.current ^= 1;
        } else if (getKeyOn(KEY_L2) || getKeyOn(KEY_R2)) {
            fm_pick_device(pane, getKeyOn(KEY_R2));
        } else if (getKey(KEY_UP) && pane->count) {
            pane->selection = (pane->selection + pane->count - 1) %
                              pane->count;
        } else if (getKey(KEY_DOWN) && pane->count) {
            pane->selection = (pane->selection + 1) % pane->count;
        } else if (select && pane->count) {
            char next[FM_PATH];
            if (fm_selected(pane, next, sizeof(next)) &&
                pane->items[pane->selection].folder) {
                snprintf(pane->path, sizeof(pane->path), "%s", next);
                pane->selection = pane->scroll = 0;
                fm_reload(pane);
            } else
                fm_options();
        } else if (getKeyOn(KEY_START) || getKeyOn(KEY_SELECT)) {
            fm_options();
        } else if (getKeyOn(KEY_SQUARE)) {
            char source[FM_PATH];
            if (fm_selected(pane, source, sizeof(source))) {
                snprintf(fm.clipboard, sizeof(fm.clipboard), "%s", source);
                fm.clipboardDir = pane->items[pane->selection].folder;
                fm.clipboardSet = 1;
                fm.clipMove = 0;
                fm_status("Copiado. Navegue e use triangulo para colar.");
            }
        } else if (getKeyOn(KEY_TRIANGLE) && fm.clipboardSet) {
            /* Paste through the options dialog to preserve confirmations
             * and prevent silent overwriting. */
            fm_options();
        } else if (getKeyOn(back)) {
            if (!fm_parent(pane))
                break;
        }
        if (pane->selection >= pane->scroll + 7)
            pane->scroll = pane->selection - 6;
        if (pane->selection < pane->scroll)
            pane->scroll = pane->selection;
    }
}
