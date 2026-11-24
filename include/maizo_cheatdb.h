#ifndef MAIZO_CHEATDB_H
#define MAIZO_CHEATDB_H
#include <stddef.h>
#include <stdint.h>

/* read_at must return 1 only for an exact read. No archive extraction to disk. */
typedef int (*maizo_zip_read)(void *ctx, uint32_t offset, void *out, size_t length);
enum { MZDB_NONE = 0,
       MZDB_SERIAL = 2,
       MZDB_TITLE = 1,
       MZDB_INVALID = -1,
       MZDB_AMBIGUOUS = -2,
       MZDB_IO = -3,
       MZDB_LIMIT = -4,
       MZDB_METHOD = -5,
       MZDB_CORRUPT = -6,
       MZDB_MEMORY = -7,
       MZDB_SERIAL_INVALID = -8 };
int maizoCheatZipFind(maizo_zip_read read_at, void *ctx, uint32_t size, const char *serial, const char *title, char **text, char heading[256]);
#endif
