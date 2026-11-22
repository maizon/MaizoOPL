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
    int fd, mc;
    unsigned char *scratch;
} db_file_t;

static int mcResult(int result)
{
    int value;
    return result < 0 ? result : mcSync(MC_WAIT, NULL, &value) == 1 ? value : -1;
}

static int readDb(void *ctx, uint32_t offset, void *out, size_t length)
{
    db_file_t *f = ctx;
    unsigned char *p = out;
    if ((f->mc ? mcResult(mcSeek(f->fd, offset, SEEK_SET)) : fileXioLseek(f->fd, offset, SEEK_SET)) != (int)offset)
        return 0;
    while (length) {
        int n = length > 8192 ? 8192 : length;
        int count = f->mc ? mcResult(mcRead(f->fd, f->scratch, n)) : fileXioRead(f->fd, f->scratch, n);
        if (count <= 0 || count > n)
            return 0;
        memcpy(p, f->scratch, count);
        p += count;
        length -= count;
    }
    return 1;
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
        return MZDB_INVALID;
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
        result = size > 0 ? maizoCheatZipFind(readDb, &f, size, serial, title, text, heading) : MZDB_INVALID;
        if (f.mc)
            mcResult(mcClose(f.fd));
        else
            fileXioClose(f.fd);
        break;
    }
    free(f.scratch);
    return result;
}
