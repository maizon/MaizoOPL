/*
 * MAIZO Store - Ethernet-only browser of vetted, pinned GitHub PS2 homebrews.
 * Uses the existing PS2IP stack; menu + downloads never touch in-game PadEmu.
 * Public relay speaks HTTP/1.1 identity with fixed Content-Length. A cleartext
 * relay is untrusted: install only if the ELF's SHA-256 equals our local pin.
 */
#include "include/opl.h"
#include "include/maizo_store.h"
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

#define STORE_ITEMS 4
#define STORE_RX 4096
#define STORE_MAX_HEADER 1024
#define STORE_MAX_APP (8 * 1024 * 1024)

typedef struct {
    const char *id, *title, *category, *summary, *hash;
    unsigned int bytes;
} store_app_t;

/* Verified against GitHub Releases metadata on 2026-11-23. Never use a
 * digest received in an unencrypted catalogue as the install authority. */
static const store_app_t storeApps[STORE_ITEMS] = {
    {"wlaunchelf", "wLaunchELF", "Ferramentas", "Gerenciador de arquivos", "beb688160ab29ba441f5b4f9a20fb151a4abb44ee4a285a7b1a702b6542c2c66", 497908},
    {"wlaunchelf-ds4", "wLaunchELF DS34", "Ferramentas", "Suporte DualShock 3 e 4", "4ed8eb48b735411356012946082d2d61d1f72a1aad613cb6d1f5e65953187df2", 366868},
    {"wlaunchelf-exfat", "wLaunchELF exFAT", "Ferramentas", "Gerenciador com exFAT", "2254d5684904d42088421fc6ef6cf46299faa8c85e5764a3789de7108180a6d7", 356052},
    {"fceumm", "FCEUmm PS2", "Emuladores", "Emulador NES para PS2", "aeebec25228ed2fe2261f22e1ab692d4d518f2dbca84e81d980a4a1bc53f3a3e", 1384060}
};

static char storeHost[64];
static int storePort;
static unsigned int storeOnline;
static unsigned int storeInstalledMask;
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
    storeHost[0] = 0;
    storePort = 8080;
    if (configGetStr(cfg, "maizo_store_host", &host) && storeHostValid(host))
        snprintf(storeHost, sizeof(storeHost), "%s", host);
    configGetInt(cfg, "maizo_store_port", &storePort);
    if (storePort < 1 || storePort > 65535)
        storePort = 8080;
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
    storeOnline = 0;
}

/* This method needs no SMB session and no external phone/PC. */
static int storeConnect(void)
{
    struct hostent *entry;
    struct sockaddr_in addr;
    struct timeval tv;
    int s;
    if (!storeHostValid(storeHost))
        return -1;
    entry = gethostbyname(storeHost);
    if (!entry || !entry->h_addr_list || !entry->h_addr_list[0])
        return -1;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;
    tv.tv_sec = 12;
    tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(storePort);
    memcpy(&addr.sin_addr, entry->h_addr_list[0], sizeof(addr.sin_addr));
    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        closesocket(s);
        return -1;
    }
    return s;
}

static int storeSendAll(int s, const char *buf, int len)
{
    while (len > 0) {
        int count = send(s, buf, len, 0);
        if (count <= 0)
            return 0;
        buf += count;
        len -= count;
    }
    return 1;
}

/* Return a socket positioned at the first body byte, or -1. Refuse
 * redirects, chunked encoding, oversized headers and truncated lengths. */
static int storeHttpOpen(const char *uri, int *contentLength)
{
    char req[256], headers[STORE_MAX_HEADER + 1];
    int s = storeConnect(), used = 0, count;
    if (s < 0)
        return -1;
    count = snprintf(req, sizeof(req),
                     "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: MAIZO-PS2/1\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n",
                     uri, storeHost);
    if (count <= 0 || count >= (int)sizeof(req) || !storeSendAll(s, req, count))
        goto fail;
    while (used < STORE_MAX_HEADER) {
        char c;
        if (recv(s, &c, 1, 0) != 1)
            goto fail;
        headers[used++] = c;
        headers[used] = '\0';
        if (used >= 4 && !memcmp(headers + used - 4, "\r\n\r\n", 4))
            break;
    }
    if (used >= STORE_MAX_HEADER ||
        strncmp(headers, "HTTP/1.1 200 ", 13) != 0)
        goto fail;
    {
        char *size = strstr(headers, "\r\nContent-Length: ");
        char *end;
        long n;
        if (!size || strstr(headers, "\r\nTransfer-Encoding: "))
            goto fail;
        n = strtol(size + 18, &end, 10);
        if (end == size + 18 || strncmp(end, "\r\n", 2) ||
            n < 1 || n > STORE_MAX_APP)
            goto fail;
        *contentLength = n;
    }
    return s;
fail:
    closesocket(s);
    return -1;
}

static int storeHttpText(const char *uri, char output[STORE_RX])
{
    int length, offset = 0, s = storeHttpOpen(uri, &length);
    if (s < 0)
        return -1;
    if (length >= STORE_RX) {
        closesocket(s);
        return -1;
    }
    while (offset < length) {
        int got = recv(s, output + offset, length - offset, 0);
        if (got <= 0) {
            closesocket(s);
            return -1;
        }
        offset += got;
    }
    closesocket(s);
    output[length] = '\0';
    return length;
}

/* Remote metadata can mark an ID available, but ONLY if it matches the
 * release size and hash pinned locally. No remote path is ever trusted. */
static void storeRefresh(void)
{
    char text[STORE_RX], *line;
    unsigned int mask = 0;
    int ret;
    storeOnline = 0;
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
    if (storeHttpText("/v1/catalog", text) < 0 || strncmp(text, "MAIZOSTORE1\n", 12)) {
        snprintf(storeStatus, sizeof(storeStatus), "Servidor desconectado ou HTTP nao suportado.");
        return;
    }
    line = strchr(text, '\n');
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
                    mask |= 1u << i;
        }
        line = end + 1;
    }
    storeOnline = mask;
    snprintf(storeStatus, sizeof(storeStatus), "%d homebrews verificados - GitHub Releases",
             !!(mask & 1) + !!(mask & 2) + !!(mask & 4) + !!(mask & 8));
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
    storeInstalledMask = 0;
    for (int i = 0; i < STORE_ITEMS; i++)
        if (storeInstalled(i))
            storeInstalledMask |= 1u << i;
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
        for (int i = 0; i < STORE_ITEMS; i++) {
            const store_app_t *app = &storeApps[i];
            char status[48];
            int y = 150 + i * 57;
            if (i == selected)
                rmDrawRect(36, y, 566, 53, MAIZO_CARD);
            snprintf(status, sizeof(status), "%s", (storeInstalledMask & (1u << i)) ? "INSTALADO" :
                     storeOnline & (1u << i) ? "DISPONIVEL" : "OFFLINE");
            fntRenderString(gTheme->fonts[0], 46, y + 4, ALIGN_NONE, 380, 24,
                            app->title, i == selected ? MAIZO_BRIGHT : MAIZO_TEXT);
            fntRenderString(gTheme->fonts[0], 46, y + 27, ALIGN_NONE, 410, 19,
                            app->summary, MAIZO_MUTED);
            fntRenderString(gTheme->fonts[0], 473, y + 12, ALIGN_NONE, 113, 20,
                            status, (storeOnline & (1u << i)) ? MAIZO_SUCCESS : MAIZO_MUTED);
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
    int expected = 0, s, fd = -1, total = 0, ok = 0, wasInstalled, movedOld = 0;
    int firstBytes = 0;
    unsigned char signature[4];
    iox_stat_t st;
    if (!(storeOnline & (1u << index)) || !gEnableWrite) {
        guiMsgBox("Download indisponivel ou gravacao desativada.", 0, NULL);
        return 0;
    }
    snprintf(uri, sizeof(uri), "/v1/apps/%s.elf", app->id);
    guiRenderTextScreen("MAIZO Store: preparando download verificado...");
    s = storeHttpOpen(uri, &expected);
    if (s < 0 || expected != app->bytes) {
        if (s >= 0)
            closesocket(s);
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
        closesocket(s);
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
        got = recv(s, buf, left, 0);
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
    closesocket(s);
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
    storeOnline = 0;
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
        } else if (getKeyOn(gSelectButton) && (storeOnline & (1u << selected))) {
            if (guiMsgBox((storeInstalledMask & (1u << selected)) ?
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
