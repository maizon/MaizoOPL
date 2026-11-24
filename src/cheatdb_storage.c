#include "include/opl.h"
#include "include/maizo_cheatdb.h"
#include "include/cheatdb_storage.h"
#include "include/ethsupport.h"
#include <libmc.h>
#include <malloc.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

typedef struct
{
    int fd, mc, ioError;
    uint32_t size, cacheStart;
    size_t cacheLen;
    unsigned char *scratch;
} db_file_t;

static int mcResult(int result)
{
    int value;
    return result < 0 ? result : mcSync(MC_WAIT, NULL, &value) == 1 ? value :
                                                                      -1;
}

static int readDb(void *ctx, uint32_t offset, void *out, size_t length)
{
    db_file_t *f = ctx;
    unsigned char *p = out;
    if (offset > f->size || length > f->size - offset)
        return 0;
    while (length) {
        if (!f->cacheLen || offset < f->cacheStart || offset - f->cacheStart >= f->cacheLen) {
            /* Common tiny ZIP central-directory reads reuse the same
             * 8 KiB cache, including on USB and SMB. */
            uint32_t start = offset;
            int want = f->size - start > 8192u ? 8192 : (int)(f->size - start);
            f->cacheLen = 0;
            if ((f->mc ? mcResult(mcSeek(f->fd, start, SEEK_SET)) : fileXioLseek(f->fd, start, SEEK_SET)) != (int)start)
                goto ioError;
            int count = f->mc ? mcResult(mcRead(f->fd, f->scratch, want)) : fileXioRead(f->fd, f->scratch, want);
            if (count <= 0 || count > want || offset - start >= (uint32_t)count)
                goto ioError;
            f->cacheStart = start;
            f->cacheLen = count;
        }
        size_t at = offset - f->cacheStart;
        size_t n = f->cacheLen - at;
        if (n > length) n = length;
        memcpy(p, f->scratch + at, n);
        p += n;
        offset += n;
        length -= n;
    }
    return 1;
ioError:
    f->ioError = 1;
    return 0;
}

int cheatDbLoad(const char *serial, const char *title, char **text, char heading[256], char source[80])
{
    static const char *const paths[] = {
        "mass0:/Cheats/CheatDatabase.zip", "mass1:/Cheats/CheatDatabase.zip",
        "mass2:/Cheats/CheatDatabase.zip", "mass3:/Cheats/CheatDatabase.zip",
        "mass4:/Cheats/CheatDatabase.zip", "mass5:/Cheats/CheatDatabase.zip",
        "mass6:/Cheats/CheatDatabase.zip", "mass7:/Cheats/CheatDatabase.zip",
        "mass8:/Cheats/CheatDatabase.zip", "mass9:/Cheats/CheatDatabase.zip",
        "mc0:/Cheats/CheatDatabase.zip", "mc1:/Cheats/CheatDatabase.zip",
        "smb0:\\Cheats\\CheatDatabase.zip"};
    db_file_t f;
    int result = MZDB_NONE;
    *text = NULL;
    source[0] = heading[0] = 0;
    f.scratch = memalign(64, 8192);
    if (!f.scratch)
        return MZDB_MEMORY;
    for (unsigned int i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        const char *path = paths[i];
        if (!strncmp(path, "smb", 3) && (!ethGetObject(1) || gNetworkStartup != 0 || !gPCShareName[0]))
            continue;
        f.mc = !strncmp(path, "mc", 2);
        f.fd = f.mc ? mcResult(mcOpen(path[2] - '0', 0, path + 4, O_RDONLY)) : fileXioOpen(path, O_RDONLY, 0);
        if (f.fd < 0)
            continue;
        snprintf(source, 80, "%s", path);
        int size = f.mc ? mcResult(mcSeek(f.fd, 0, SEEK_END)) : fileXioLseek(f.fd, 0, SEEK_END);
        f.size = size > 0 ? (uint32_t)size : 0;
        f.cacheLen = 0;
        f.ioError = 0;
        result = size > 0 ? maizoCheatZipFind(readDb, &f, size, serial, title, text, heading) :
                 size < 0 ? MZDB_IO : MZDB_INVALID;
        if (f.ioError && result < 0)
            result = MZDB_IO;
        if (f.mc)
            mcResult(mcClose(f.fd));
        else
            fileXioClose(f.fd);
        break;
    }
    free(f.scratch);
    return result;
}
