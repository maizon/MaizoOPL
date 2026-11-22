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
#include "include/ioman.h"
#include "include/themes.h"
#include "include/fntsys.h"
#include "include/renderman.h"
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <libmc.h>
#include <malloc.h>
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
    unsigned char folder, marked;
} fm_item_t;

typedef struct
{
    char root[FM_PATH], path[FM_PATH];
    fm_item_t items[FM_ITEMS];
    int count, selection, scroll, error, freeKB;
} fm_pane_t;

static struct
{
    fm_pane_t pane[2];
    int current, clipboardSet, clipMove;
    char clipboard[FM_PATH], status[164];
    unsigned char clipboardDir;
    int batchCount;
    struct { char path[FM_PATH]; unsigned char folder; } batch[FM_ITEMS];
} fm;

static int screenWidth;
static unsigned long long fmDone, fmTotal;
static int fmCancelled, fmTicks;
static void fm_draw(void);

static const char *const fm_devices[] = {
    "mass0:/", "mass1:/", "mass2:/", "mass3:/", "mass4:/", "mc0:/", "mc1:/"};

static void fm_status(const char *message)
{
    snprintf(fm.status, sizeof(fm.status), "%s", message);
}

static int fm_join(char *out, size_t len, const char *base,
                   const char *name)
{
    size_t n = strlen(base);
    char slash = !strncmp(base, "smb", 3) ? '\\' : '/';
    if (!name || !name[0] || !strcmp(name, ".") || !strcmp(name, "..") ||
        strchr(name, ':') || strchr(name, '/') || strchr(name, '\\'))
        return 0;
    if ((!strncmp(base, "mc", 2) && (strlen(name) > 31 || strchr(name, '*') || strchr(name, '?'))) ||
        n + strlen(name) + 2 > len)
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
    return !strncasecmp(parent, maybeChild, size) &&
           (maybeChild[size] == '/' || maybeChild[size] == '\\');
}

static int fm_compare(const void *a, const void *b)
{
    const fm_item_t *left = a, *right = b;
    if (left->folder != right->folder)
        return right->folder - left->folder;
    return strcasecmp(left->name, right->name);
}

/* Native memory-card RPC. MCMAN is not a FAT filesystem: extended
 * fileXio operations and Unix creation flags are not portable to it. */
static int fm_mc_port(const char *path)
{
    return !strncmp(path, "mc0:", 4) ? 0 : !strncmp(path, "mc1:", 4) ? 1 :
                                                                       -1;
}

static int fm_mc_result(int queued)
{
    int result;
    if (queued < 0)
        return queued;
    return mcSync(MC_WAIT, NULL, &result) == 1 ? result : -EIO;
}

/* Collect a directory before recursion: mcGetDir has a shared cursor. */
static int fm_list(const char *path, fm_item_t *items, int capacity, int strict)
{
    int port = fm_mc_port(path), count = 0;
    if (port >= 0) {
        char pattern[FM_PATH];
        sceMcTblGetDir entries[16];
        int ret, mode = 0;
        if (snprintf(pattern, sizeof(pattern), "%s%s*", path + 4,
                     path[strlen(path) - 1] == '/' ? "" : "/") >= sizeof(pattern))
            return -ENAMETOOLONG;
        do {
            ret = fm_mc_result(mcGetDir(port, 0, pattern, mode, 16, entries));
            if (ret < 0)
                return ret;
            for (int i = 0; i < ret; i++) {
                char name[33];
                memcpy(name, entries[i].EntryName, 32);
                name[32] = 0;
                if (!name[0] || !strcmp(name, ".") || !strcmp(name, ".."))
                    continue;
                if (count == capacity)
                    return strict ? -EOVERFLOW : count;
                snprintf(items[count].name, sizeof(items[count].name), "%s", name);
                items[count].marked = 0;
                items[count].size = entries[i].FileSizeByte;
                items[count].folder = !!(entries[i].AttrFile & MC_ATTR_SUBDIR);
                count++;
            }
            mode = 1;
        } while (ret == 16);
        return count;
    }
    iox_dirent_t entry;
    int dd = fileXioDopen(path), ret;
    if (dd < 0)
        return dd;
    while ((ret = fileXioDread(dd, &entry)) > 0) {
        if (!entry.name[0] || !strcmp(entry.name, ".") || !strcmp(entry.name, ".."))
            continue;
        if (count == capacity) {
            fileXioDclose(dd);
            return strict ? -EOVERFLOW : count;
        }
        snprintf(items[count].name, sizeof(items[count].name), "%s", entry.name);
        items[count].marked = 0;
        items[count].folder = FIO_S_ISDIR(entry.stat.mode) != 0;
        items[count].size = entry.stat.size;
        count++;
    }
    fileXioDclose(dd);
    return ret < 0 ? ret : count;
}

static int fm_reload(fm_pane_t *pane)
{
    int port = fm_mc_port(pane->path);
    pane->freeKB = -1;
    if (port >= 0) {
        int type = 0, freeKB = 0, formatted = 0;
        int status = fm_mc_result(mcGetInfo(port, 0, &type, &freeKB, &formatted));
        if (status >= -1 && type == MC_TYPE_PS2)
            pane->freeKB = freeKB;
    }
    int count = fm_list(pane->path, pane->items, FM_ITEMS, 0);
    pane->count = count < 0 ? 0 : count;
    pane->error = count < 0 ? count : 0;
    if (count < 0)
        return count;
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
    const char *roots[8];
    char smb[FM_PATH];
    int idx = 0, size = sizeof(fm_devices) / sizeof(fm_devices[0]);
    item_list_t *eth = ethGetObject(1);
    for (int i = 0; i < size; i++)
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
        fm_status("Dispositivo indisponível. Troque a unidade.");
    else if (!strncmp(pane->path, "smb", 3))
        fm_status("SMB conectado: usando automaticamente a sessão OPL.");
    else
        fm_status("Unidade selecionada.");
}

static int fm_exists(const char *path)
{
    int port = fm_mc_port(path);
    if (port >= 0) {
        sceMcTblGetDir info;
        return fm_mc_result(mcGetDir(port, 0, path + 4, 0, 1, &info)) > 0;
    }
    iox_stat_t st;
    return fileXioGetStat(path, &st) >= 0;
}

static int fm_mkdir(const char *path)
{
    int port = fm_mc_port(path);
    return port >= 0 ? fm_mc_result(mcMkDir(port, 0, path + 4)) : fileXioMkdir(path, 0777);
}

static int fm_remove(const char *path, int folder)
{
    int port = fm_mc_port(path);
    if (port >= 0)
        return fm_mc_result(mcDelete(port, 0, path + 4));
    return folder ? fileXioRmdir(path) : fileXioRemove(path);
}

static int fm_rename(const char *source, const char *dest)
{
    int port = fm_mc_port(source);
    if (port >= 0) {
        const char *a = strrchr(source, '/'), *b = strrchr(dest, '/');
        if (!a || !b || a - source != b - dest || strncmp(source, dest, a - source + 1))
            return -EXDEV;
        return fm_mc_result(mcRename(port, 0, source + 4, b + 1));
    }
    return fileXioRename(source, dest);
}

static int fm_open(const char *path, int writing)
{
    int port = fm_mc_port(path);
    if (port >= 0)
        return fm_mc_result(mcOpen(port, 0, path + 4, writing ? O_WRONLY | O_CREAT : O_RDONLY));
    return fileXioOpen(path, writing ? O_WRONLY | O_CREAT | O_EXCL : O_RDONLY, 0666);
}

static int fm_close(int fd, int mc)
{
    return mc ? fm_mc_result(mcClose(fd)) : fileXioClose(fd);
}

static int fm_read(int fd, int mc, void *buffer, int size)
{
    return mc ? fm_mc_result(mcRead(fd, buffer, size)) : fileXioRead(fd, buffer, size);
}

static int fm_write(int fd, int mc, const void *buffer, int size)
{
    const unsigned char *data = buffer;
    int off = 0;
    while (off < size) {
        int n = mc ? fm_mc_result(mcWrite(fd, data + off, size - off)) : fileXioWrite(fd, (void *)(data + off), size - off);
        if (n <= 0)
            return n < 0 ? n : -EIO;
        off += n;
    }
    return 0;
}

static int fm_read_exact(int fd, int mc, unsigned char *data, int size)
{
    int off = 0;
    while (off < size) {
        int n = fm_read(fd, mc, data + off, size - off);
        if (n <= 0)
            return n < 0 ? n : -EIO;
        off += n;
    }
    return 0;
}

static int fm_progress(const char *phase, int force)
{
    if (!force && ++fmTicks % 8)
        return fmCancelled;
    readPads();
    if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE))
        fmCancelled = 1;
    snprintf(fm.status, sizeof(fm.status), "%s: %llu / %llu KB (voltar cancela)", phase, fmDone / 1024, fmTotal / 1024);
    rmStartFrame();
    fm_draw();
    maizoDrawPanel(90, gTheme->usedHeight / 2 - 50, 460, 99, 1);
    fntRenderString(gTheme->fonts[0], 320, gTheme->usedHeight / 2 - 32, ALIGN_CENTER, 430, 27, fm.status, MAIZO_TEXT);
    rmDrawRect(115, gTheme->usedHeight / 2 + 10, 410, 10, MAIZO_DIVIDER);
    if (fmTotal)
        rmDrawRect(115, gTheme->usedHeight / 2 + 10, (int)(410 * (fmDone > fmTotal ? fmTotal : fmDone) / fmTotal), 10, MAIZO_ACCENT);
    rmEndFrame();
    return fmCancelled;
}

/* Byte-for-byte verification: read source and destination again before
 * reporting success or deleting a source selected for a move. */
static int fm_verify_file(const char *source, const char *dest)
{
    int a = -1, b = -1, n, result = -EIO;
    int am = fm_mc_port(source) >= 0, bm = fm_mc_port(dest) >= 0;
    unsigned char *left = memalign(64, FM_CHUNK), *right = memalign(64, FM_CHUNK);
    if (!left || !right) {
        result = -ENOMEM;
        goto finish;
    }
    a = fm_open(source, 0);
    b = fm_open(dest, 0);
    if (a < 0 || b < 0)
        goto finish;
    while ((n = fm_read(a, am, left, FM_CHUNK)) > 0) {
        if (fm_read_exact(b, bm, right, n) < 0 || memcmp(left, right, n))
            goto finish;
        fmDone += n;
        if (fm_progress("Verificando", 0))
            goto finish;
    }
    if (!n && fm_read(b, bm, right, 1) == 0)
        result = 0;
finish:
    if (a >= 0)
        fm_close(a, am);
    if (b >= 0)
        fm_close(b, bm);
    free(left);
    free(right);
    return result;
}

static int fm_copy_file(const char *source, const char *dest)
{
    int fd = -1, out = -1, got, result = -EIO;
    int srcMc = fm_mc_port(source) >= 0, dstMc = fm_mc_port(dest) >= 0;
    unsigned char *buffer;
    if (fm_exists(dest))
        return -EEXIST;
    if (fmCancelled)
        return -EINTR;
    buffer = memalign(64, FM_CHUNK);
    if (!buffer)
        return -ENOMEM;
    fd = fm_open(source, 0);
    if (fd < 0) {
        result = fd;
        goto done;
    }
    out = fm_open(dest, 1);
    if (out < 0) {
        result = out;
        goto done;
    }
    for (;;) {
        got = fm_read(fd, srcMc, buffer, FM_CHUNK);
        if (got < 0) {
            result = got;
            break;
        }
        if (!got) {
            result = 0;
            break;
        }
        result = fm_write(out, dstMc, buffer, got);
        if (result < 0)
            goto done;
        fmDone += got;
        if (fm_progress("Copiando", 0)) {
            result = -EINTR;
            goto done;
        }
    }
done:
    if (out >= 0) {
        int flush = dstMc && !result ? fm_mc_result(mcFlush(out)) : 0;
        int closed = fm_close(out, dstMc);
        if (!result && (flush < 0 || closed < 0))
            result = flush < 0 ? flush : closed;
    }
    if (fd >= 0)
        fm_close(fd, srcMc);
    if (!result)
        result = fm_verify_file(source, dest);
    if (result && out >= 0)
        fm_remove(dest, 0);
    free(buffer);
    return result;
}

static int fm_remove_tree(const char *path, int folder, int depth);

static int fm_copy_tree(const char *source, const char *dest,
                        int folder, int depth)
{
    int ret, count;
    fm_item_t *items;
    char srcChild[FM_PATH], dstChild[FM_PATH];
    if (depth > FM_DEPTH)
        return -ELOOP;
    if (!folder)
        return fm_copy_file(source, dest);
    if (fm_exists(dest))
        return -EEXIST;
    if (fm_is_child(source, dest))
        return -EINVAL;
    items = malloc(sizeof(*items) * FM_ITEMS);
    if (!items)
        return -ENOMEM;
    count = fm_list(source, items, FM_ITEMS, 1);
    if (count < 0) {
        free(items);
        return count;
    }
    ret = fm_mkdir(dest);
    int created = ret >= 0;
    for (int i = 0; ret >= 0 && i < count; i++) {
        if (!fm_join(srcChild, sizeof(srcChild), source, items[i].name) ||
            !fm_join(dstChild, sizeof(dstChild), dest, items[i].name)) {
            ret = -ENAMETOOLONG;
            break;
        }
        ret = fm_copy_tree(srcChild, dstChild, items[i].folder, depth + 1);
    }
    free(items);
    if (created && ret < 0)
        fm_remove_tree(dest, 1, 0);
    return ret;
}

static int fm_remove_tree(const char *path, int folder, int depth)
{
    int ret = 0, count;
    fm_item_t *items;
    char full[FM_PATH];
    if (depth > FM_DEPTH)
        return -ELOOP;
    if (!folder)
        return fm_remove(path, 0);
    items = malloc(sizeof(*items) * FM_ITEMS);
    if (!items)
        return -ENOMEM;
    count = fm_list(path, items, FM_ITEMS, 1);
    if (count < 0) {
        free(items);
        return count;
    }
    for (int i = 0; i < count; i++) {
        if (!fm_join(full, sizeof(full), path, items[i].name)) {
            ret = -ENAMETOOLONG;
            break;
        }
        ret = fm_remove_tree(full, items[i].folder, depth + 1);
        if (ret < 0)
            break;
    }
    free(items);
    return ret < 0 ? ret : fm_remove(path, 1);
}

static int fm_visible_rows(void)
{
    int rows = (gTheme->usedHeight - 296) / 28;
    return rows > 1 ? rows : 1;
}

static void fm_draw_pane(fm_pane_t *pane, int x, int focused)
{
    int top = 117, h = gTheme->usedHeight - 211;
    char title[60];
    maizoDrawPanel(x, top, 287, h, focused);
    if (pane->freeKB >= 0)
        snprintf(title, sizeof(title), "%s  Livre: %d KB", pane->root, pane->freeKB);
    else
        snprintf(title, sizeof(title), "%s  %d itens", pane->root, pane->count);
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
                        "Não foi possível listar esta unidade.",
                        MAIZO_WARNING);
    for (int i = 0; i < fm_visible_rows(); ++i) {
        int index = pane->scroll + i, y = top + 85 + i * 28;
        const fm_item_t *entry;
        if (index >= pane->count)
            break;
        entry = &pane->items[index];
        if (index == pane->selection && focused) {
            rmDrawRect(x + 9, y, 269, 27, MAIZO_CARD);
            rmDrawRect(x + 9, y, 3, 27, MAIZO_ACCENT);
        }
        maizoDrawIcon(entry->marked ? MAIZO_ICON_CHECK : entry->folder ? MAIZO_ICON_FOLDER : MAIZO_ICON_FILE,
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
                 "SMB desconectado: use a configuração de rede existente.");
    }
    fntRenderString(gTheme->fonts[0], 31, gTheme->usedHeight - 89,
                    ALIGN_NONE, 573, 25, server, MAIZO_MUTED);
    rmDrawRect(0, gTheme->usedHeight - 63,
               screenWidth, 63, MAIZO_SURFACE);
    fntRenderString(gTheme->fonts[0], 20, gTheme->usedHeight - 57,
                    ALIGN_NONE, screenWidth - 38, 25, fm.status, MAIZO_TEXT);
    fntRenderString(gTheme->fonts[0], 20, gTheme->usedHeight - 32,
                    ALIGN_NONE, screenWidth - 38, 24,
                    "X/O ABRIR  L/R PAINEL  L2/R2 UNIDADE  [] COPIAR  /\\ COLAR  START OPÇÕES",
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
                ioBlockOps(0);
                deinit(UNMOUNT_EXCEPTION, APP_MODE);
                LoadELFFromFileWithPartition(filename, "", 0, NULL);
            }
            return;
        }
    }
    guiMsgBox("wLaunchELF não instalado. Copie seu BOOT.ELF para mass0:/APPS/wLaunchELF/", 0, NULL);
}

static int fm_tree_bytes(const char *path, int folder, int depth, unsigned long long *size)
{
    if (depth > FM_DEPTH)
        return -ELOOP;
    if (!folder) {
        if (fm_mc_port(path) >= 0) {
            sceMcTblGetDir info;
            if (fm_mc_result(mcGetDir(fm_mc_port(path), 0, path + 4, 0, 1, &info)) != 1)
                return -EIO;
            *size += info.FileSizeByte;
        } else {
            iox_stat_t stat;
            if (fileXioGetStat(path, &stat) < 0)
                return -EIO;
            *size += ((unsigned long long)stat.hisize << 32) | stat.size;
        }
        return 0;
    }
    fm_item_t *items = malloc(sizeof(*items) * FM_ITEMS);
    if (!items)
        return -ENOMEM;
    int count = fm_list(path, items, FM_ITEMS, 1), result = count < 0 ? count : 0;
    for (int i = 0; i < count && !result; i++) {
        char child[FM_PATH];
        result = fm_join(child, sizeof(child), path, items[i].name) ? fm_tree_bytes(child, items[i].folder, depth + 1, size) : -ENAMETOOLONG;
    }
    free(items);
    return result;
}

static int fm_copy_replace(const char *source, const char *dest, int folder, const char *directory)
{
    char temp[FM_PATH], backup[FM_PATH], name[32];
    if (!fm_exists(dest))
        return fm_copy_tree(source, dest, folder, 0);
    if (folder)
        return -EEXIST;
    if (!guiMsgBox("O arquivo já existe. Substituir após verificar a nova cópia?", 1, NULL))
        return -EEXIST;
    for (int i = 0; i < 100; i++) {
        snprintf(name, sizeof(name), "maizo-%02d.tmp", i);
        if (!fm_join(temp, sizeof(temp), directory, name) || !strcmp(temp, dest) || fm_exists(temp))
            continue;
        snprintf(name, sizeof(name), "maizo-%02d.bak", i);
        if (!fm_join(backup, sizeof(backup), directory, name) || !strcmp(backup, dest) || fm_exists(backup))
            continue;
        int ret = fm_copy_file(source, temp);
        if (ret < 0)
            return ret;
        if (fm_rename(dest, backup) < 0) {
            fm_remove(temp, 0);
            return -EACCES;
        }
        if (fm_rename(temp, dest) < 0) {
            if (fm_rename(backup, dest) >= 0)
                fm_remove(temp, 0);
            else
                guiMsgBox("Falha ao restaurar. Preserve os arquivos maizo-XX.bak e .tmp.", 0, NULL);
            return -EIO;
        }
        fm_remove(backup, 0);
        return 0;
    }
    return -EEXIST;
}

static int fm_paste_one(fm_pane_t *pane)
{
    char dst[FM_PATH];
    if (!fm.clipboardSet || pane->error)
        return -EIO;
    const char *filename = strrchr(fm.clipboard, '/');
    const char *slash = strrchr(fm.clipboard, '\\');
    if (!filename || (slash && slash > filename))
        filename = slash;
    filename = filename ? filename + 1 : fm.clipboard;
    if (!fm_join(dst, sizeof(dst), pane->path, filename) ||
        !strcasecmp(fm.clipboard, dst) ||
        (fm.clipboardDir && fm_is_child(fm.clipboard, dst))) {
        fm_status("Destino inválido ou dentro da origem.");
        return -EINVAL;
    }
    if (!fm.clipboardDir && fm_exists(dst)) {
        /* Never replace an existing directory with a file. */
        fm_item_t *items = malloc(sizeof(*items) * FM_ITEMS);
        if (!items)
            return -ENOMEM;
        int n = fm_list(pane->path, items, FM_ITEMS, 1);
        int regular = 0;
        for (int i = 0; i < n; i++)
            if (!strcasecmp(items[i].name, filename))
                regular = !items[i].folder;
        free(items);
        if (!regular)
            return -EEXIST;
    }
    fmDone = fmTotal = 0;
    int result = fm_tree_bytes(fm.clipboard, fm.clipboardDir, 0, &fmTotal);
    if (result < 0)
        return result;
    fmTotal *= 2;
    if (fm_progress("Preparando", 1))
        return -EINTR;
    if (fm.clipMove && fm_same_device(fm.clipboard, dst) && !fm_exists(dst) && fm_rename(fm.clipboard, dst) >= 0)
        return 0;
    result = fm_copy_replace(fm.clipboard, dst, fm.clipboardDir, pane->path);
    if (!result && fm.clipMove && fm_remove_tree(fm.clipboard, fm.clipboardDir, 0) < 0)
        return -EIO; /* verified destination remains if deleting source fails */
    return result;
}

static void fm_paste(fm_pane_t *pane)
{
    int result = 0, completed = 0;
    fmCancelled = fmTicks = 0;
    if (fm.batchCount) {
        for (int i = 0; i < fm.batchCount; i++) {
            snprintf(fm.clipboard, sizeof(fm.clipboard), "%s", fm.batch[i].path);
            fm.clipboardDir = fm.batch[i].folder;
            fm.clipboardSet = 1;
            result = fm_paste_one(pane);
            if (result < 0)
                break;
            completed++;
        }
        if (completed) {
            memmove(fm.batch, fm.batch + completed, (fm.batchCount - completed) * sizeof(fm.batch[0]));
            fm.batchCount -= completed;
        }
    } else {
        result = fm_paste_one(pane);
    }
    if (!result) {
        fm.clipboardSet = 0;
        fm_status(fm.clipMove ? "Movido com sucesso." : "Cópia verificada com sucesso.");
    } else if (fmCancelled)
        fm_status("Cancelado. Arquivos concluídos foram preservados.");
    else
        snprintf(fm.status, sizeof(fm.status), "Operação incompleta (%d). Confira origem e destino.", result);
    fm_reload(&fm.pane[0]);
    fm_reload(&fm.pane[1]);
}

static void fm_capture(fm_pane_t *pane, int move)
{
    fm.batchCount = 0;
    fm.clipboardSet = 0;
    for (int i = 0; i < pane->count; i++) {
        if (!pane->items[i].marked)
            continue;
        if (fm_join(fm.batch[fm.batchCount].path, FM_PATH, pane->path, pane->items[i].name)) {
            fm.batch[fm.batchCount].folder = pane->items[i].folder;
            fm.batchCount++;
        }
    }
    if (fm.batchCount) {
        snprintf(fm.clipboard, sizeof(fm.clipboard), "%s", fm.batch[0].path);
        fm.clipboardDir = fm.batch[0].folder;
        fm.clipboardSet = 1;
    } else if (fm_selected(pane, fm.clipboard, sizeof(fm.clipboard))) {
        fm.clipboardDir = pane->items[pane->selection].folder;
        fm.clipboardSet = 1;
    }
    fm.clipMove = move;
    if (fm.clipboardSet)
        snprintf(fm.status, sizeof(fm.status), "%d item(ns) %s. Triângulo para colar.", fm.batchCount ? fm.batchCount : 1, move ? "recortados" : "copiados");
}

/* EMS/PSU flat-save format: 512-byte directory entries, name at 64,
 * little-endian count/size at 4, payload rounded to 1024 bytes.
 * Reject traversal, subfolders and existing destinations. */
static unsigned int fm_psu_u32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static int fm_psu_name(const unsigned char *header, char out[33])
{
    if (!memchr(header + 64, 0, 32))
        return 0;
    memcpy(out, header + 64, 32);
    out[32] = 0;
    return out[0] != 0;
}

static void fm_psu_header(unsigned char *header, unsigned int mode, unsigned int count,
                          const char *name, const sceMcTblGetDir *info)
{
    memset(header, 0, 512);
    header[0] = mode & 255;
    header[1] = mode >> 8;
    for (int i = 0; i < 4; i++)
        header[4 + i] = count >> (i * 8);
    if (info) {
        memcpy(header + 8, &info->_Create, 8);
        memcpy(header + 24, &info->_Modify, 8);
    } else {
        header[12] = header[28] = 1;
        header[13] = header[29] = 1;
        header[14] = header[30] = 0xd0;
        header[15] = header[31] = 7;
    }
    memcpy(header + 64, name, strlen(name));
}

static int fm_psu_transfer(int in, int im, int out, int om, unsigned int size, unsigned char *buffer)
{
    while (size) {
        int n = size > FM_CHUNK ? FM_CHUNK : size;
        if (fm_read_exact(in, im, buffer, n) < 0 || fm_write(out, om, buffer, n) < 0)
            return -EIO;
        size -= n;
        if (fm_progress("Transferindo PSU", 0))
            return -EINTR;
    }
    return 0;
}

static int fm_psu_export(const char *source, const char *target)
{
    int port = fm_mc_port(source), out = -1, in = -1, result = -EIO;
    fm_item_t *items = malloc(sizeof(*items) * FM_ITEMS);
    unsigned char *buffer = memalign(64, FM_CHUNK), header[512] __attribute__((aligned(64)));
    char child[FM_PATH], name[FM_NAME];
    sceMcTblGetDir info;
    if (!items || !buffer || port < 0 || fm_mc_port(target) >= 0 || fm_exists(target))
        goto finish;
    int count = fm_list(source, items, FM_ITEMS, 1);
    const char *base = strrchr(source, '/');
    if (count < 0 || !base || strlen(base + 1) > 31)
        goto finish;
    for (int i = 0; i < count; i++)
        if (items[i].folder || strlen(items[i].name) > 31)
            goto finish;
    snprintf(name, sizeof(name), "%s", base + 1);
    if (fm_mc_result(mcGetDir(port, 0, source + 4, 0, 1, &info)) != 1)
        goto finish;
    out = fm_open(target, 1);
    if (out < 0)
        goto finish;
    fm_psu_header(header, info.AttrFile | 0x8020, count + 2, name, &info);
    if (fm_write(out, 0, header, 512) < 0)
        goto finish;
    fm_psu_header(header, 0x8427, 0, ".", &info);
    if (fm_write(out, 0, header, 512) < 0)
        goto finish;
    fm_psu_header(header, 0x8427, 0, "..", &info);
    if (fm_write(out, 0, header, 512) < 0)
        goto finish;
    for (int i = 0; i < count; i++) {
        if (!fm_join(child, sizeof(child), source, items[i].name) ||
            fm_mc_result(mcGetDir(port, 0, child + 4, 0, 1, &info)) != 1)
            goto finish;
        fm_psu_header(header, info.AttrFile | 0x8010, items[i].size, items[i].name, &info);
        if (fm_write(out, 0, header, 512) < 0)
            goto finish;
        in = fm_open(child, 0);
        if (in < 0 || fm_psu_transfer(in, 1, out, 0, items[i].size, buffer) < 0)
            goto finish;
        fm_close(in, 1);
        in = -1;
        int pad = (1024 - (items[i].size & 1023)) & 1023;
        memset(buffer, 0, pad);
        if (pad && fm_write(out, 0, buffer, pad) < 0)
            goto finish;
    }
    result = 0;
finish:
    if (in >= 0)
        fm_close(in, 1);
    if (out >= 0) {
        if (fm_close(out, 0) < 0)
            result = -EIO;
        if (result < 0)
            fm_remove(target, 0);
    }
    free(items);
    free(buffer);
    return result;
}

static int fm_psu_import(const char *source, const char *directory)
{
    int port = fm_mc_port(directory), in = -1, out = -1, result = -EIO, created = 0;
    unsigned char *buffer = memalign(64, FM_CHUNK), header[512] __attribute__((aligned(64)));
    char target[FM_PATH], child[FM_PATH], name[33];
    unsigned int count, size;
    sceMcTblGetDir dirInfo;
    if (!buffer || port < 0 || fm_mc_port(source) >= 0)
        goto finish;
    in = fm_open(source, 0);
    if (in < 0 || fm_read_exact(in, 0, header, 512) < 0 ||
        (header[0] & 0x30) != 0x20 || !fm_psu_name(header, name) ||
        !fm_join(target, sizeof(target), directory, name) || fm_exists(target))
        goto finish;
    count = fm_psu_u32(header + 4);
    memset(&dirInfo, 0, sizeof(dirInfo));
    memcpy(&dirInfo._Create, header + 8, 8);
    memcpy(&dirInfo._Modify, header + 24, 8);
    if (count < 2 || count > FM_ITEMS + 2)
        goto finish;
    for (int i = 0; i < 2; i++)
        if (fm_read_exact(in, 0, header, 512) < 0 || (header[0] & 0x30) != 0x20 ||
            !fm_psu_name(header, name) || strcmp(name, i ? ".." : "."))
            goto finish;
    if (fm_mkdir(target) < 0)
        goto finish;
    created = 1;
    for (unsigned int i = 2; i < count; i++) {
        sceMcTblGetDir info;
        if (fm_read_exact(in, 0, header, 512) < 0 || (header[0] & 0x30) != 0x10 ||
            !fm_psu_name(header, name) || !fm_join(child, sizeof(child), target, name) || fm_exists(child))
            goto finish;
        size = fm_psu_u32(header + 4);
        if (size > 64u * 1024 * 1024)
            goto finish;
        memset(&info, 0, sizeof(info));
        memcpy(&info._Create, header + 8, 8);
        memcpy(&info._Modify, header + 24, 8);
        out = fm_open(child, 1);
        if (out < 0 || fm_psu_transfer(in, 0, out, 1, size, buffer) < 0 ||
            fm_mc_result(mcFlush(out)) < 0)
            goto finish;
        int closed = fm_close(out, 1);
        out = -1;
        if (closed < 0)
            goto finish;
        fm_mc_result(mcSetFileInfo(port, 0, child + 4, &info, sceMcFileInfoCreate | sceMcFileInfoModify));
        int pad = (1024 - (size & 1023)) & 1023;
        if (pad && fm_read_exact(in, 0, buffer, pad) < 0)
            goto finish;
    }
    if (fm_read(in, 0, buffer, 1) != 0)
        goto finish;
    fm_mc_result(mcSetFileInfo(port, 0, target + 4, &dirInfo, sceMcFileInfoCreate | sceMcFileInfoModify));
    result = 0;
finish:
    if (out >= 0)
        fm_close(out, 1);
    if (in >= 0)
        fm_close(in, 0);
    if (result < 0 && created)
        fm_remove_tree(target, 1, 0);
    free(buffer);
    return result;
}

static void fm_options(void)
{
    fm_pane_t *pane = &fm.pane[fm.current];
    char src[FM_PATH], dst[FM_PATH], name[FM_NAME];
    int have = fm_selected(pane, src, sizeof(src));
    int action;
    fm_pane_t *other = &fm.pane[fm.current ^ 1];
    int exportPsu = have && pane->items[pane->selection].folder && fm_mc_port(pane->path) >= 0 && fm_mc_port(other->path) < 0;
    const char *ext = have ? strrchr(pane->items[pane->selection].name, '.') : NULL;
    int importPsu = have && !pane->items[pane->selection].folder && ext && !strcasecmp(ext, ".psu") && fm_mc_port(pane->path) < 0 && fm_mc_port(other->path) >= 0;
    struct UIItem options[] = {
        {UI_LABEL, 0, 1, 1, -1, 0, 0, {.label = {"MAIZO / ARQUIVOS", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 201, have, have, -1, 0, 0, {.label = {"Copiar seleção", -1}}},
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
        {UI_BUTTON, 210, exportPsu, exportPsu, -1, 0, 0, {.label = {"Exportar save PSU para outro painel", -1}}},
        {UI_BREAK},
        {UI_BUTTON, 211, importPsu, importPsu, -1, 0, 0, {.label = {"Importar PSU para outro painel", -1}}},
        {UI_BREAK},
        {UI_TERMINATOR}};
    action = diaExecuteDialog(options, -1, 1, NULL);
    if (action == 201 || action == 203) {
        if (have)
            fm_capture(pane, action == 203);
    } else if (action == 202 && fm.clipboardSet) {
        fm_paste(pane);
    } else if (action == 204 && have) {
        snprintf(name, sizeof(name), "%s", pane->items[pane->selection].name);
        if (diaShowKeyb(name, sizeof(name), 0, "RENOMEAR") &&
            fm_join(dst, sizeof(dst), pane->path, name) &&
            !fm_exists(dst)) {
            if (fm_rename(src, dst) < 0)
                fm_status("Não foi possível renomear neste dispositivo.");
            else
                fm_status("Item renomeado.");
        }
    } else if (action == 205) {
        name[0] = 0;
        if (diaShowKeyb(name, sizeof(name), 0, "NOVA PASTA") &&
            fm_join(dst, sizeof(dst), pane->path, name) &&
            !fm_exists(dst)) {
            if (fm_mkdir(dst) < 0)
                fm_status("Não foi possível criar pasta aqui.");
            else
                fm_status("Nova pasta criada.");
        }
    } else if (action == 206 && have) {
        if (guiMsgBox("EXCLUIR este item e todos os arquivos internos? Sem desfazer.", 1, NULL)) {
            if (fm_remove_tree(src, pane->items[pane->selection].folder, 0) < 0)
                fm_status("Falha ao excluir. Verifique permissão e arquivos.");
            else
                fm_status("Exclusão concluída.");
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
    else if (action == 210 || action == 211) {
        fmDone = fmTotal = 0;
        fmCancelled = fmTicks = 0;
        int result = -EINVAL;
        if (!other->error && guiMsgBox("Transferir save para o outro painel? Não sobrescreve saves.", 1, NULL)) {
            if (action == 210) {
                snprintf(name, sizeof(name), "%s.psu", pane->items[pane->selection].name);
                if (fm_join(dst, sizeof(dst), other->path, name))
                    result = fm_psu_export(src, dst);
            } else
                result = fm_psu_import(src, other->path);
            fm_status(result < 0 ? "PSU não concluído; confira espaço, formato e destino." : "Transferência PSU concluída.");
        }
    }
    fm_reload(&fm.pane[0]);
    fm_reload(&fm.pane[1]);
}

void maizoFileManagerOpen(void)
{
    int screenHeight;
    rmGetScreenExtents(&screenWidth, &screenHeight);
    ioBlockOps(1); /* Native MC RPC has a single outstanding request. */
    memset(&fm, 0, sizeof(fm));
    fm_set_root(&fm.pane[0], "mass0:/");
    fm_set_root(&fm.pane[1], "mc0:/");
    fm_status("SELECT marca; quadrado copia; triângulo cola; L2/R2 unidade.");
    for (;;) {
        fm_pane_t *pane = &fm.pane[fm.current];
        int select, back;
        rmStartFrame();
        readPads();
        fm_draw();
        guiDrawControllerNotification();
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
        } else if (getKeyOn(KEY_START)) {
            fm_options();
        } else if (getKeyOn(KEY_SELECT) && pane->count) {
            pane->items[pane->selection].marked ^= 1;
        } else if (getKeyOn(KEY_SQUARE)) {
            fm_capture(pane, 0);
        } else if (getKeyOn(KEY_TRIANGLE) && fm.clipboardSet) {
            if (guiMsgBox("Colar nesta pasta? A substituição de arquivos exige confirmação.", 1, NULL))
                fm_paste(pane);
        } else if (getKeyOn(back)) {
            if (!fm_parent(pane))
                break;
        }
        if (pane->selection >= pane->scroll + fm_visible_rows())
            pane->scroll = pane->selection - fm_visible_rows() + 1;
        if (pane->selection < pane->scroll)
            pane->scroll = pane->selection;
    }
    ioBlockOps(0);
}
