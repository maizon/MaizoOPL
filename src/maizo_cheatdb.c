/* Streaming ZIP/Deflate reader for CheatDevice TXT and serial-named CHT.
 * Original implementation. No filesystem extraction, no game engine changes. */
#include "maizo_cheatdb.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <zlib.h>

#define DB_TEXT_LIMIT    (256u * 1024u)
/* These are I/O/work budgets, not heap allocations. Only matching
 * sections and two small streaming chunks are retained in memory. */
#define DB_ARCHIVE_LIMIT (512u * 1024u * 1024u)
#define DB_SCAN_LIMIT    (256u * 1024u * 1024u)
#define DB_CHUNK         8192

static uint16_t u16le(const unsigned char *p)
{
    return p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t u32le(const unsigned char *p)
{
    return u16le(p) | ((uint32_t)u16le(p + 2) << 16);
}

/* Accept SLUS_123.45, SLUS-12345, SLUS12345, etc, but not partial IDs. */
static int serialAt(const char *s, char out[10])
{
    unsigned int n = 0;
    const char *p = s;
    while (n < 9) {
        if (n == 4 || n == 7) {
            if (*p == '_' || *p == '-' || *p == '.' || *p == ' ')
                p++;
        }
        if (!*p || (n < 4 ? !isalpha((unsigned char)*p) : !isdigit((unsigned char)*p)))
            return 0;
        out[n++] = toupper((unsigned char)*p++);
    }
    out[9] = 0;
    return !isalnum((unsigned char)*p);
}

static int containsSerial(const char *s, const char *wanted)
{
    char token[10];
    for (const char *p = s; *p; p++)
        if ((p == s || !isalnum((unsigned char)p[-1])) && serialAt(p, token) && !strcmp(token, wanted))
            return 1;
    return 0;
}

static int hasOtherSerial(const char *s, const char *wanted)
{
    char token[10];
    for (const char *p = s; *p; p++)
        if ((p == s || !isalnum((unsigned char)p[-1])) && serialAt(p, token) &&
            ((token[0] == 'S' && (token[1] == 'L' || token[1] == 'C')) || !strncmp(token, "SCAJ", 4)) && strcmp(token, wanted))
            return 1;
    return 0;
}

static void normalized(const char *s, char out[256])
{
    unsigned int n = 0;
    for (; *s && n < 255; s++)
        if (isalnum((unsigned char)*s))
            out[n++] = tolower((unsigned char)*s);
    out[n] = 0;
}

/* A name-only match is never authorized silently at launch. A region suffix
 * must agree with the serial, and an explicitly different serial is rejected. */
static int matchTitle(const char *s, const char *title, const char *serial)
{
    char a[256], b[256], name[256];
    size_t len = strlen(s);
    if (!title[0] || len >= sizeof(name))
        return 0;
    memcpy(name, s, len + 1);
    if (len && (name[len - 1] == ')' || name[len - 1] == ']')) {
        char *tag = strrchr(name, name[len - 1] == ')' ? '(' : '[');
        if (tag) {
            char region[256];
            normalized(tag, region);
            int expected = 0, given = 0;
            if (!strncmp(serial, "SLUS", 4) || !strncmp(serial, "SCUS", 4))
                expected = 1;
            else if (!strncmp(serial, "SLES", 4) || !strncmp(serial, "SCES", 4))
                expected = 2;
            else if (!strncmp(serial, "SLPS", 4) || !strncmp(serial, "SLPM", 4) || !strncmp(serial, "SCPS", 4))
                expected = 3;
            if (!strcmp(region, "usa") || !strcmp(region, "us") || !strcmp(region, "u") || !strcmp(region, "ntscu") || !strcmp(region, "ntscuc"))
                given = 1;
            else if (!strcmp(region, "pal") || !strcmp(region, "e") || !strcmp(region, "europe"))
                given = 2;
            else if (!strcmp(region, "japan") || !strcmp(region, "j") || !strcmp(region, "ntscj"))
                given = 3;
            if (given) {
                if (given != expected)
                    return 0;
                *tag = 0;
            }
        }
    }
    normalized(name, a);
    normalized(title, b);
    if(a[0]&&!strcmp(a,b))return 1;
    normalized(s,a);
    return a[0]&&!strcmp(a,b);
}

typedef struct
{
    char serial[10];
    const char *title;
    char *candidate;
    char *best;
    char heading[256], bestHeading[256];
    char line[4096];
    size_t used, lineUsed;
    int currentRank, bestRank, ambiguous, overflow, longLine, quoted, fileRank, failed;
} scanner_t;

static void finishSection(scanner_t *s)
{
    if (s->currentRank && s->overflow)
        s->failed = MZDB_LIMIT;
    if (s->currentRank && s->used && !s->overflow) {
        if (s->currentRank > s->bestRank) {
            if (!s->best)
                s->best = malloc(DB_TEXT_LIMIT + 1);
            if (!s->best) {
                s->failed = MZDB_MEMORY;
                return;
            }
            memcpy(s->best, s->candidate, s->used);
            s->best[s->used] = 0;
            strcpy(s->bestHeading, s->heading);
            s->bestRank = s->currentRank;
            s->ambiguous = 0;
        } else if (s->currentRank == s->bestRank &&
                   (strcmp(s->bestHeading, s->heading) || strlen(s->best) != s->used || memcmp(s->best, s->candidate, s->used)))
            s->ambiguous = 1;
    }
    s->used = 0;
    s->overflow = 0;
    s->currentRank = 0;
}

static void scanLine(scanner_t *s)
{
    char *p = s->line;
    size_t len = s->lineUsed;
    if (s->longLine) {
        if (s->currentRank)
            s->overflow = 1;
        if (!s->quoted && s->fileRank)
            s->failed = MZDB_LIMIT;
        s->longLine = 0;
        s->lineUsed = 0;
        return;
    }
    while (len && isspace((unsigned char)p[len - 1]))
        len--;
    p[len] = 0;
    if ((unsigned char)p[0] == 0xef && len >= 3 && (unsigned char)p[1] == 0xbb && (unsigned char)p[2] == 0xbf)
        p += 3;
    while (*p && isspace((unsigned char)*p))
        p++;
    len = strlen(p);
    if (len >= 2 && p[0] == '"' && p[len - 1] == '"') {
        /* Serial-named loose files may start with an unquoted preamble.
         * A quoted game boundary must never leak codes from adjacent games. */
        if (!s->quoted)
            s->used = 0;
        finishSection(s);
        s->quoted = 1;
        p[len - 1] = 0;
        p++;
        if (strlen(p) < sizeof(s->heading)) {
            strcpy(s->heading, p);
            if (!hasOtherSerial(p, s->serial)) {
                if (containsSerial(p, s->serial) || s->fileRank)
                    s->currentRank = MZDB_SERIAL;
                else if (matchTitle(p, s->title, s->serial))
                    s->currentRank = MZDB_TITLE;
            }
        }
    } else if (s->currentRank && !s->overflow) {
        if (len + 1 > DB_TEXT_LIMIT - s->used)
            s->overflow = 1;
        else {
            memcpy(s->candidate + s->used, p, len);
            s->used += len;
            s->candidate[s->used++] = '\n';
        }
    }
    s->lineUsed = 0;
}

static void scanBytes(scanner_t *s, const unsigned char *p, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        if (p[i] == '\n')
            scanLine(s);
        else if (s->lineUsed + 1 < sizeof(s->line) && !s->longLine && p[i])
            s->line[s->lineUsed++] = p[i];
        else
            s->longLine = 1;
    }
}

static int scanMember(maizo_zip_read read_at, void *ctx, uint32_t offset, uint32_t compressed, uint32_t uncompressed,
                      uint16_t method, uint32_t crc, scanner_t *s)
{
    unsigned char *in = malloc(DB_CHUNK), *out = malloc(DB_CHUNK);
    z_stream z;
    uint32_t actual = 0, checksum = crc32(0, NULL, 0);
    int result = MZDB_CORRUPT, initialized = 0, ended = 0;
    memset(&z, 0, sizeof(z));
    if (!in || !out) {
        result = MZDB_MEMORY;
        goto done;
    }
    if (method == 8) {
        int code = inflateInit2(&z, -MAX_WBITS);
        if (code != Z_OK) {
            result = code == Z_MEM_ERROR ? MZDB_MEMORY : MZDB_METHOD;
            goto done;
        }
        initialized = 1;
    }
    while (compressed && !ended) {
        uint32_t size = compressed < DB_CHUNK ? compressed : DB_CHUNK;
        if (!read_at(ctx, offset, in, size)) {
            result = MZDB_IO;
            goto done;
        }
        offset += size;
        compressed -= size;
        if (method == 0) {
            if (size > uncompressed - actual)
                goto done;
            actual += size;
            checksum = crc32(checksum, in, size);
            scanBytes(s, in, size);
            continue;
        }
        z.next_in = in;
        z.avail_in = size;
        do {
            z.next_out = out;
            z.avail_out = DB_CHUNK;
            unsigned int before = z.avail_in;
            int code = inflate(&z, Z_NO_FLUSH);
            size_t have = DB_CHUNK - z.avail_out;
            /* A full output chunk can require one more inflate call
             * even when the current input chunk is exhausted. Z_BUF_ERROR
             * here means "need more input", not a corrupt member. */
            if (code == Z_BUF_ERROR && !z.avail_in && !have)
                break;
            if (code == Z_MEM_ERROR) {
                result = MZDB_MEMORY;
                goto done;
            }
            if (have > uncompressed - actual || (code != Z_OK && code != Z_STREAM_END))
                goto done;
            if (code == Z_OK && before == z.avail_in && !have)
                goto done;
            actual += have;
            checksum = crc32(checksum, out, have);
            scanBytes(s, out, have);
            if (code == Z_STREAM_END) {
                if (compressed || z.avail_in)
                    goto done;
                ended = 1;
                break;
            }
        } while (z.avail_in || !z.avail_out);
    }
    if (actual != uncompressed || checksum != crc || (method == 8 && !ended))
        goto done;
    if (s->lineUsed || s->longLine)
        scanLine(s);
    finishSection(s);
    result = s->failed ? s->failed : 1;
done:
    if (initialized)
        inflateEnd(&z);
    free(in);
    free(out);
    return result;
}

int maizoCheatZipFind(maizo_zip_read read_at, void *ctx, uint32_t size, const char *serial, const char *title, char **text, char heading[256])
{
    unsigned char h[46], local[30];
    unsigned char *tail = NULL;
    scanner_t *s = NULL;
    int result = MZDB_INVALID;
    uint32_t tailSize, central = 0, centralStart = 0, centralEnd = 0, scanned = 0;
    unsigned int members = 0;
    int foundEnd = 0;
    *text = NULL;
    heading[0] = 0;
    if (size < 22)
        return result;
    if (size > DB_ARCHIVE_LIMIT)
        return MZDB_LIMIT;
    tailSize = size < 65557 ? size : 65557;
    tail = malloc(tailSize);
    s = calloc(1, sizeof(*s));
    if (!tail || !s) {
        result = MZDB_MEMORY;
        goto done;
    }
    if (!serial || !serialAt(serial, s->serial)) {
        result = MZDB_SERIAL_INVALID;
        goto done;
    }
    s->candidate = malloc(DB_TEXT_LIMIT + 1);
    s->title = title ? title : "";
    if (!s->candidate) {
        result = MZDB_MEMORY;
        goto done;
    }
    if (!read_at(ctx, size - tailSize, tail, tailSize)) {
        result = MZDB_IO;
        goto done;
    }
    for (int i = (int)tailSize - 22; i >= 0; i--) {
        unsigned char *p = tail + i;
        if (u32le(p) != 0x06054b50 || (uint32_t)i + 22 + u16le(p + 20) != tailSize)
            continue;
        if (u16le(p + 4) || u16le(p + 6) || u16le(p + 8) != u16le(p + 10))
            goto done;
        members = u16le(p + 10);
        central = u32le(p + 16);
        uint32_t length = u32le(p + 12), eocd = size - tailSize + i;
        if (central > eocd || length > eocd - central || members == 65535)
            goto done;
        centralStart = central;
        centralEnd = central + length;
        foundEnd = 1;
        break;
    }
    if (!foundEnd || (!centralEnd && members))
        goto done;
    if (!centralEnd) {
        result = MZDB_NONE;
        goto done;
    }
    free(tail);
    tail = NULL;
    for (unsigned int i = 0; i < members; i++) {
        char name[512];
        if (central > centralEnd || centralEnd - central < sizeof(h) || !read_at(ctx, central, h, sizeof(h)) || u32le(h) != 0x02014b50)
            goto done;
        uint16_t flags = u16le(h + 8), method = u16le(h + 10);
        uint32_t compressed = u32le(h + 20), uncompressed = u32le(h + 24), offset = u32le(h + 42);
        unsigned int n = u16le(h + 28), advance = n + u16le(h + 30) + u16le(h + 32);
        central += sizeof(h);
        if (advance > centralEnd - central || !n)
            goto done;
        if (n >= sizeof(name)) {
            central += advance;
            continue;
        }
        if (!read_at(ctx, central, name, n))
            goto done;
        name[n] = 0;
        central += advance;
        const char *ext = strrchr(name, '.');
        if (!ext || (strcasecmp(ext, ".txt") && strcasecmp(ext, ".cht")))
            continue;
        if (hasOtherSerial(name, s->serial) && !containsSerial(name, s->serial))
            continue;
        if ((flags & 1) || (method != 0 && method != 8) || u16le(h + 34)) {
            result = MZDB_METHOD;
            goto done;
        }
        if (uncompressed > DB_SCAN_LIMIT - scanned) {
            result = MZDB_LIMIT;
            goto done;
        }
        if (centralStart < sizeof(local) || offset > centralStart - sizeof(local) ||
            !read_at(ctx, offset, local, sizeof(local)) || u32le(local) != 0x04034b50 ||
            u16le(local + 8) != method || (u16le(local + 6) & 1))
            goto done;
        uint32_t dataOffset = offset + sizeof(local);
        uint32_t extra = u16le(local + 26) + u16le(local + 28);
        if (extra > centralStart - dataOffset)
            goto done;
        dataOffset += extra;
        if (compressed > centralStart - dataOffset || (method == 0 && compressed != uncompressed))
            goto done;
        if(!uncompressed)continue;
        scanned += uncompressed;
        s->fileRank = containsSerial(name, s->serial) ? MZDB_SERIAL : 0;
        s->currentRank = s->fileRank;
        s->quoted = s->overflow = s->longLine = 0;
        s->used = s->lineUsed = 0;
        strncpy(s->heading, name, sizeof(s->heading) - 1);
        s->heading[sizeof(s->heading) - 1] = 0;
        int memberResult = scanMember(read_at, ctx, dataOffset, compressed, uncompressed, method, u32le(h + 16), s);
        if (memberResult < 0) {
            result = memberResult;
            goto done;
        }
    }
    if (central != centralEnd)
        goto done;
    if (s->ambiguous)
        result = MZDB_AMBIGUOUS;
    else {
        result = s->bestRank;
        if (result) {
            *text = s->best;
            s->best = NULL;
            strcpy(heading, s->bestHeading);
        }
    }
done:
    free(tail);
    if (s) {
        free(s->candidate);
        free(s->best);
        free(s);
    }
    return result;
}
