#ifndef MAIZO_STORE_SHA256_H
#define MAIZO_STORE_SHA256_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Portable streaming SHA-256. No dynamic allocation, independent of PS2SDK. */
typedef struct {
    uint32_t h[8];
    uint64_t bits;
    unsigned int used;
    unsigned char block[64];
} maizo_sha256_t;

static uint32_t maizo_sha_rotr(uint32_t a, unsigned int n)
{
    return (a >> n) | (a << (32 - n));
}

static void maizo_sha_transform(maizo_sha256_t *s, const unsigned char b[64])
{
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    uint32_t w[64], a, c, d, e, f, g, h, t1, t2, bb;
    unsigned int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)b[i * 4] << 24) | ((uint32_t)b[i * 4 + 1] << 16) |
               ((uint32_t)b[i * 4 + 2] << 8) | b[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        uint32_t x = w[i - 15], y = w[i - 2];
        uint32_t s0 = maizo_sha_rotr(x, 7) ^ maizo_sha_rotr(x, 18) ^ (x >> 3);
        uint32_t s1 = maizo_sha_rotr(y, 17) ^ maizo_sha_rotr(y, 19) ^ (y >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; bb = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = maizo_sha_rotr(e, 6) ^ maizo_sha_rotr(e, 11) ^ maizo_sha_rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t S0 = maizo_sha_rotr(a, 2) ^ maizo_sha_rotr(a, 13) ^ maizo_sha_rotr(a, 22);
        uint32_t maj = (a & bb) ^ (a & c) ^ (bb & c);
        t1 = h + S1 + ch + k[i] + w[i];
        t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = bb; bb = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += bb; s->h[2] += c; s->h[3] += d;
    s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void maizo_sha256_init(maizo_sha256_t *s)
{
    static const uint32_t init[] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    memcpy(s->h, init, sizeof(init));
    s->bits = 0;
    s->used = 0;
}

static void maizo_sha256_update(maizo_sha256_t *s, const void *buf, size_t bytes)
{
    const unsigned char *p = (const unsigned char *)buf;
    s->bits += (uint64_t)bytes * 8;
    while (bytes) {
        size_t take = sizeof(s->block) - s->used;
        if (take > bytes)
            take = bytes;
        memcpy(s->block + s->used, p, take);
        s->used += take;
        p += take;
        bytes -= take;
        if (s->used == 64) {
            maizo_sha_transform(s, s->block);
            s->used = 0;
        }
    }
}

static void maizo_sha256_final(maizo_sha256_t *s, char hex[65])
{
    static const char digits[] = "0123456789abcdef";
    uint64_t bits = s->bits;
    unsigned int i;
    s->block[s->used++] = 0x80;
    if (s->used > 56) {
        memset(s->block + s->used, 0, 64 - s->used);
        maizo_sha_transform(s, s->block);
        s->used = 0;
    }
    memset(s->block + s->used, 0, 56 - s->used);
    for (i = 0; i < 8; i++)
        s->block[56 + i] = (unsigned char)(bits >> (56 - 8 * i));
    maizo_sha_transform(s, s->block);
    for (i = 0; i < 8; i++) {
        hex[i * 8]     = digits[(s->h[i] >> 28) & 15];
        hex[i * 8 + 1] = digits[(s->h[i] >> 24) & 15];
        hex[i * 8 + 2] = digits[(s->h[i] >> 20) & 15];
        hex[i * 8 + 3] = digits[(s->h[i] >> 16) & 15];
        hex[i * 8 + 4] = digits[(s->h[i] >> 12) & 15];
        hex[i * 8 + 5] = digits[(s->h[i] >> 8) & 15];
        hex[i * 8 + 6] = digits[(s->h[i] >> 4) & 15];
        hex[i * 8 + 7] = digits[s->h[i] & 15];
    }
    hex[64] = '\0';
}
#endif
