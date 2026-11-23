/*
 * MAIZO Store - Ethernet-only browser of vetted, pinned GitHub PS2 homebrews.
 * Uses the existing PS2IP stack; menu + downloads never touch in-game PadEmu.
 * Public relay speaks HTTP/1.1 identity with fixed Content-Length. A cleartext
 * relay is untrusted: install only if the ELF's SHA-256 equals our local pin.
 */
#include "include/opl.h"
#include "include/maizo_store.h"
#include "include/maizo_store_tls.h"
#include "include/maizo_store_sha256.h"
#include "include/appsupport.h"
#include "include/ethsupport.h"
#include "include/maizo_library.h"
#include "include/menusys.h"
#include "include/maizo_oled.h"
#include "include/maizo_ui.h"
#include "include/renderman.h"
#include "include/fntsys.h"
#include "include/themes.h"
#include "include/gui.h"
#include "include/pad.h"
#include "include/ioman.h"
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>

#define STORE_RX 16384
#define STORE_MAX_HEADER 4096
#define STORE_MAX_APP (12 * 1024 * 1024)
typedef struct {
    const char *id, *title, *category, *summary, *hash;
    unsigned int bytes;
    const char *credits, *license;
} store_app_t;
#include "include/maizo_store_catalog.h"

static char storeHost[64];
static int storePort;
static unsigned char storeOnline[STORE_ITEMS];
static unsigned char storeInstalledMask[STORE_ITEMS];
static char storeCatalogText[STORE_RX];
static char storeStatus[128] = "Configure um host HTTP/TCP para acessar a loja.";
static unsigned int storeProgressBytes, storeProgressTotal;

static int storeHostValid(const char *host)
{
    int len = 0;
    if (!host || !*host)
        return 0;
    for (; host[len] && len < (int)sizeof(storeHost); len++) {
        unsigned char c = (unsigned char)host[len];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-'))
            return 0;
    }
    return len && len < (int)sizeof(storeHost);
}

static void storeReadConfig(void)
{
    const char *host = NULL;
    config_set_t *cfg = configGetByType(CONFIG_OPL);
    snprintf(storeHost, sizeof(storeHost), "raw.githubusercontent.com");
    storePort = 443;
    if (configGetStr(cfg, "maizo_store_host", &host) && storeHostValid(host))
        snprintf(storeHost, sizeof(storeHost), "%s", host);
    configGetInt(cfg, "maizo_store_port", &storePort);
    if (storePort < 1 || storePort > 65535)
        storePort = 443;
}

static void storeSetServer(void)
{
    char host[64], port[8];
    int newPort;
    snprintf(host, sizeof(host), "%s", storeHost);
    if (!guiShowKeyboard(host, sizeof(host)))
        return;
    if (!storeHostValid(host)) {
        guiMsgBox("Host invalido. Use apenas letras, numeros, ponto ou hifen.", 0, NULL);
        return;
    }
    snprintf(port, sizeof(port), "%d", storePort);
    if (!guiShowKeyboard(port, sizeof(port)))
        return;
    for (int i = 0; port[i]; i++)
        if (port[i] < '0' || port[i] > '9') {
            guiMsgBox("Porta TCP invalida.", 0, NULL);
            return;
        }
    newPort = atoi(port);
    if (newPort < 1 || newPort > 65535) {
        guiMsgBox("Porta deve estar entre 1 e 65535.", 0, NULL);
        return;
    }
    snprintf(storeHost, sizeof(storeHost), "%s", host);
    storePort = newPort;
    configSetStr(configGetByType(CONFIG_OPL), "maizo_store_host", storeHost);
    configSetInt(configGetByType(CONFIG_OPL), "maizo_store_port", storePort);
    saveConfig(CONFIG_OPL, 0);
    memset(storeOnline, 0, sizeof(storeOnline));
}

/* HTTPS 1.2 from the PS2; external host is public GitHub raw/CDN.
 * The final ELF must match the SHA-256 shipped inside this binary. */
static maizo_store_tls_t *storeHttpOpen(const char *uri, int *contentLength)
{
    char req[384], headers[STORE_MAX_HEADER + 1], path[208];
    int used = 0, count;
    int foundSize = 0;
    maizo_store_tls_t *s;
    if (!storeHostValid(storeHost))
        return NULL;

    if (!strcmp(storeHost, "raw.githubusercontent.com"))
        snprintf(path, sizeof(path), "/maizon/MaizoOPLstore/main/site%s", uri);
    else if (!strcmp(storeHost, "cdn.jsdelivr.net"))
        snprintf(path, sizeof(path), "/gh/maizon/MaizoOPLstore@main/site%s", uri);
    else
        snprintf(path, sizeof(path), "%s", uri);

    s = maizoStoreTlsConnect(storeHost, (unsigned short)storePort);
    if (!s)
        return NULL;
    count = snprintf(req, sizeof(req),
                     "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: MAIZO-PS2/2\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n",
                     path, storeHost);
    if (count <= 0 || count >= (int)sizeof(req) ||
        maizoStoreTlsWrite(s, req, count) != count)
        goto fail;
    while (used < STORE_MAX_HEADER) {
        char byte;
        if (maizoStoreTlsRead(s, &byte, 1) != 1)
            goto fail;
        headers[used++] = byte;
        headers[used] = '\0';
        if (used >= 4 && !memcmp(headers + used - 4, "\r\n\r\n", 4))
            break;
    }
    if (used >= STORE_MAX_HEADER ||
        (strncmp(headers, "HTTP/1.1 200 ", 13) &&
         strncmp(headers, "HTTP/1.0 200 ", 13)))
        goto fail;

    /* Parse exact HTTP headers; don't assume capitalization from CDNs. */
    {
        char *line = strstr(headers, "\r\n");
        if (!line)
            goto fail;
        line += 2;
        while (*line != '\r' && *line != '\0') {
            char *end = strstr(line, "\r\n");
            if (!end)
                goto fail;
            if (!strncasecmp(line, "Transfer-Encoding:", 18))
                goto fail; /* No chunked transfer support. */
            if (!strncasecmp(line, "Content-Length:", 15)) {
                char *p = line + 15;
                char *after;
                long n;
                if (foundSize)
                    goto fail;
                while (*p == ' ' || *p == '\t')
                    p++;
                n = strtol(p, &after, 10);
                if (p == after || after != end || n < 1 || n > STORE_MAX_APP)
                    goto fail;
                *contentLength = n;
                foundSize = 1;
            }
            line = end + 2;
        }
    }
    if (!foundSize)
        goto fail;
    return s;
fail:
    maizoStoreTlsClose(s);
    return NULL;
}

static int storeHttpText(const char *uri, char output[STORE_RX])
{
    int length, offset = 0;
    maizo_store_tls_t *s = storeHttpOpen(uri, &length);
    if (!s)
        return -1;
    if (length >= STORE_RX) {
        maizoStoreTlsClose(s);
        return -1;
    }
    while (offset < length) {
        int got = maizoStoreTlsRead(s, output + offset, length - offset);
        if (got <= 0) {
            maizoStoreTlsClose(s);
            return -1;
        }
        offset += got;
    }
    maizoStoreTlsClose(s);
    output[length] = '\0';
    return length;
}

/* Remote metadata can mark an ID available, but ONLY if it matches the
 * release size and hash pinned locally. No remote path is ever trusted. */
static void storeRefresh(void)
{
    char *line;
    int available = 0;
    int ret;
    memset(storeOnline, 0, sizeof(storeOnline));
    if (!storeHostValid(storeHost)) {
        snprintf(storeStatus, sizeof(storeStatus), "Configure servidor: TRIANGULO (host e porta).");
        return;
    }
    guiRenderTextScreen("MAIZO Store: conectando pela Ethernet...");
    ret = ethLoadInitModules();
    if (ret != 0) {
        snprintf(storeStatus, sizeof(storeStatus), "Ethernet indisponivel (erro %d).", ret);
        return;
    }
    int received = storeHttpText("/v1/catalog", storeCatalogText);
    if ((received < 0 || strncmp(storeCatalogText, "MAIZOSTORE1\n", 12)) &&
        !strcmp(storeHost, "raw.githubusercontent.com") && storePort == 443) {
        snprintf(storeHost, sizeof(storeHost), "cdn.jsdelivr.net");
        received = storeHttpText("/v1/catalog", storeCatalogText);
    }
    if (received < 0 || strncmp(storeCatalogText, "MAIZOSTORE1\n", 12)) {
        snprintf(storeStatus, sizeof(storeStatus), "HTTPS indisponivel ou catalogo invalido.");
        return;
    }
    line = strchr(storeCatalogText, '\n');
    if (!line)
        return;
    line++;
    while (*line) {
        char *end = strchr(line, '\n');
        char *fields[7];
        int field = 0;
        if (!end)
            break;
        *end = '\0';
        fields[field++] = line;
        for (char *p = line; *p && field < 7; p++)
            if (*p == '|') {
                *p = '\0';
                fields[field++] = p + 1;
            }
        if (field == 7) {
            for (int i = 0; i < STORE_ITEMS; i++)
                if (!strcmp(fields[0], storeApps[i].id) &&
                    strtoul(fields[5], NULL, 10) == storeApps[i].bytes &&
                    !strcmp(fields[6], storeApps[i].hash))
                    storeOnline[i] = 1;
        }
        line = end + 1;
    }
    for (int i = 0; i < STORE_ITEMS; i++)
        available += !!storeOnline[i];
    snprintf(storeStatus, sizeof(storeStatus), "%d de %d ELFs online / HTTPS validado", available, STORE_ITEMS);
}

static int storeInstalled(int index)
{
    char path[128];
    iox_stat_t st;
    snprintf(path, sizeof(path), "mass0:/APPS/%s/BOOT.ELF", storeApps[index].id);
    return fileXioGetStat(path, &st) >= 0;
}

static void storeScanInstalled(void)
{
    memset(storeInstalledMask, 0, sizeof(storeInstalledMask));
    for (int i = 0; i < STORE_ITEMS; i++)
        if (storeInstalled(i))
            storeInstalledMask[i] = 1;
}

static void storeDraw(int selected, int downloading)
{
    int height = gTheme->usedHeight;
    rmStartFrame();
    rmDrawRect(0, 0, 640, height, MAIZO_BLACK);
    maizoDrawBrand();
    maizoDrawPanel(26, 78, 588, height - 133, 0);
    fntRenderString(gTheme->fonts[0], 42, 89, ALIGN_NONE, 560, 29,
                    "MAIZO STORE  /  HOMEBREWS", MAIZO_BRIGHT);
    fntRenderString(gTheme->fonts[0], 42, 119, ALIGN_NONE, 552, 22,
                    storeStatus, MAIZO_MUTED);
    if (downloading) {
        char progress[100];
        snprintf(progress, sizeof(progress), "Baixando %u / %u KB - VOLTAR cancela",
                 storeProgressBytes / 1024, storeProgressTotal / 1024);
        maizoDrawPanel(42, 174, 556, 110, 1);
        fntRenderString(gTheme->fonts[0], 52, 196, ALIGN_NONE, 520, 30,
                        progress, MAIZO_TEXT);
        if (storeProgressTotal)
            rmDrawRect(53, 245, (u64)storeProgressBytes * 520 / storeProgressTotal, 10, MAIZO_ACCENT);
    } else {
        int visible = (height - 205) / 49;
        if (visible < 2) visible = 2;
        if (visible > 7) visible = 7;
        int first = (selected / visible) * visible;
        int last = first + visible;
        if (last > STORE_ITEMS) last = STORE_ITEMS;
        for (int i = first; i < last; i++) {
            const store_app_t *app = &storeApps[i];
            char status[48];
            int y = 151 + (i - first) * 49;
            if (i == selected)
                rmDrawRect(36, y, 566, 53, MAIZO_CARD);
            snprintf(status, sizeof(status), "%s", storeInstalledMask[i] ? "INSTALADO" :
                     storeOnline[i] ? "DISPONIVEL" : "OFFLINE");
            fntRenderString(gTheme->fonts[0], 46, y + 4, ALIGN_NONE, 380, 24,
                            app->title, i == selected ? MAIZO_BRIGHT : MAIZO_TEXT);
            fntRenderString(gTheme->fonts[0], 46, y + 27, ALIGN_NONE, 410, 19,
                            app->summary, MAIZO_MUTED);
            fntRenderString(gTheme->fonts[0], 473, y + 12, ALIGN_NONE, 113, 20,
                            status, storeOnline[i] ? MAIZO_SUCCESS : MAIZO_MUTED);
        }
        {
            char credit[180];
            const store_app_t *app = &storeApps[selected];
            snprintf(credit, sizeof(credit), "%d/%d  Autor: %.75s  |  %.55s",
                     selected + 1, STORE_ITEMS, app->credits, app->license);
            fntRenderString(gTheme->fonts[0], 42, height - 70, ALIGN_NONE, 548, 19,
                            credit, MAIZO_MUTED);
        }
    }
    fntRenderString(gTheme->fonts[0], 320, height - 37, ALIGN_CENTER, 618, 26,
                    downloading ? "VOLTAR: cancelar download" :
                    "A: instalar/atualizar  |  TRIANGULO: servidor  |  QUADRADO: atualizar  |  B: voltar",
                    MAIZO_MUTED);
    guiDrawControllerNotification();
    rmEndFrame();
}

static int storeDownload(int index)
{
    const store_app_t *app = &storeApps[index];
    maizo_sha256_t sha;
    unsigned char buf[4096];
    char directory[128], path[148], tmp[148], backup[148], configPath[148], uri[96], hex[65];
    int expected = 0, fd = -1, total = 0, ok = 0, wasInstalled, movedOld = 0;
    maizo_store_tls_t *s;
    int firstBytes = 0;
    unsigned char signature[4];
    iox_stat_t st;
    if (!storeOnline[index] || !gEnableWrite) {
        guiMsgBox("Download indisponivel ou gravacao desativada.", 0, NULL);
        return 0;
    }
    snprintf(uri, sizeof(uri), "/v1/apps/%s.elf", app->id);
    guiRenderTextScreen("MAIZO Store: preparando download verificado...");
    s = storeHttpOpen(uri, &expected);
    if (!s || expected != app->bytes) {
        if (s)
            maizoStoreTlsClose(s);
        guiMsgBox("Arquivo remoto indisponivel ou tamanho inesperado.", 0, NULL);
        return 0;
    }

    snprintf(directory, sizeof(directory), "mass0:/APPS/%s", app->id);
    snprintf(path, sizeof(path), "%s/BOOT.ELF", directory);
    snprintf(tmp, sizeof(tmp), "%s/DOWNLOAD.PART", directory);
    snprintf(backup, sizeof(backup), "%s/BOOT.BAK", directory);
    snprintf(configPath, sizeof(configPath), "%s/title.cfg", directory);
    fileXioMkdir("mass0:/APPS", 0777);
    fileXioMkdir(directory, 0777);
    fileXioRemove(tmp);
    fd = fileXioOpen(tmp, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0) {
        maizoStoreTlsClose(s);
        guiMsgBox("USB mass0 indisponivel ou sem espaco/gravadacao.", 0, NULL);
        return 0;
    }
    storeProgressTotal = expected;
    storeProgressBytes = 0;
    maizo_sha256_init(&sha);
    while (total < expected) {
        int left = expected - total;
        int got, off = 0;
        if (left > (int)sizeof(buf))
            left = sizeof(buf);
        got = maizoStoreTlsRead(s, buf, left);
        if (got <= 0)
            break;
        /* TCP is a stream: the ELF signature may span multiple packets. */
        for (int i = 0; i < got && firstBytes < 4; i++)
            signature[firstBytes++] = buf[i];
        while (off < got) {
            int written = fileXioWrite(fd, buf + off, got - off);
            if (written <= 0)
                break;
            off += written;
        }
        if (off != got)
            break;
        maizo_sha256_update(&sha, buf, got);
        total += got;
        storeProgressBytes = total;
        if (!(total % (32 * 1024)) || total == expected) {
            storeDraw(index, 1);
            readPads();
            if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE))
                break;
        }
    }
    fileXioClose(fd);
    maizoStoreTlsClose(s);
    maizo_sha256_final(&sha, hex);
    if (firstBytes != 4 || memcmp(signature, "\177ELF", 4) ||
        total != expected || strcmp(hex, app->hash)) {
        fileXioRemove(tmp);
        guiMsgBox("Cancelado, incompleto ou SHA-256 divergente. Nada foi instalado.", 0, NULL);
        return 0;
    }
    wasInstalled = storeInstalled(index);
    /* If interrupted during an earlier update, never destroy the previous
     * backup automatically. The file manager can restore it explicitly. */
    if (fileXioGetStat(backup, &st) >= 0) {
        fileXioRemove(tmp);
        guiMsgBox("BOOT.BAK encontrado. Restaure-o no gerenciador antes de atualizar.", 0, NULL);
        return 0;
    }
    if (wasInstalled) {
        if (fileXioRename(path, backup) < 0) {
            fileXioRemove(tmp);
            guiMsgBox("Falha ao preservar a versao existente.", 0, NULL);
            return 0;
        }
        movedOld = 1;
    }
    if (fileXioRename(tmp, path) < 0) {
        if (movedOld)
            fileXioRename(backup, path);
        fileXioRemove(tmp);
        guiMsgBox("Falha ao concluir a instalacao no USB.", 0, NULL);
        return 0;
    }
    /* Preserve user-modified metadata when updating. A new app needs one
     * valid title.cfg so the existing OPL APPS scanner discovers its ELF. */
    if (wasInstalled && fileXioGetStat(configPath, &st) >= 0)
        ok = 1;
    else {
        char metadata[192];
        int len = snprintf(metadata, sizeof(metadata), "title=%s\nboot=BOOT.ELF\n",
                           app->title);
        int out = fileXioOpen(configPath, O_WRONLY | O_CREAT | O_EXCL, 0666);
        if (out >= 0) {
            ok = fileXioWrite(out, metadata, len) == len;
            fileXioClose(out);
            if (!ok)
                fileXioRemove(configPath);
        }
    }
    if (!ok) {
        /* Restore an existing app if we fail to create the OPL metadata. */
        fileXioRemove(path);
        if (movedOld)
            fileXioRename(backup, path);
        guiMsgBox("Nao foi possivel gravar title.cfg; instalacao revertida.", 0, NULL);
        return 0;
    }
    if (movedOld)
        fileXioRemove(backup);
    storeScanInstalled();
    menuDashboardInvalidate();
    guiMsgBox("Homebrew instalado em mass0:/APPS. Atualize a biblioteca.", 0, NULL);
    return 1;
}

void maizoStoreOpen(void)
{
    int selected = 0, back = gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE;
    int armed = 0;
    storeReadConfig();
    storeScanInstalled();
    memset(storeOnline, 0, sizeof(storeOnline));
    if (storeHostValid(storeHost))
        storeRefresh();
    else
        snprintf(storeStatus, sizeof(storeStatus), "Pressione TRIANGULO para configurar host/porta.");
    ioBlockOps(1);
    for (;;) {
        storeDraw(selected, 0);
        readPads();
        if (!getKeyPressed(back))
            armed = 1;
        if (getKey(KEY_UP) && selected > 0)
            selected--;
        else if (getKey(KEY_DOWN) && selected < STORE_ITEMS - 1)
            selected++;
        else if (getKeyOn(KEY_TRIANGLE)) {
            ioBlockOps(0);
            storeSetServer();
            storeRefresh();
            ioBlockOps(1);
        } else if (getKeyOn(KEY_SQUARE)) {
            ioBlockOps(0);
            storeRefresh();
            ioBlockOps(1);
        } else if (getKeyOn(gSelectButton) && storeOnline[selected]) {
            if (guiMsgBox(storeInstalledMask[selected] ?
                              "Atualizar este homebrew? O arquivo antigo sera preservado durante a troca." :
                              "Instalar este homebrew gratuito em mass0:/APPS?",
                          1, NULL))
                storeDownload(selected);
        } else if (armed && getKeyOn(back)) {
            break;
        }
    }
    ioBlockOps(0);
}
