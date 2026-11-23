/*
 * MAIZO Store TLS 1.2 (BearSSL) for PS2 Ethernet / PS2IP.
 * Authenticates server hostname and certificate chain against a small
 * reviewed set of Mozilla root CAs. Fails closed if RTC and NTP unavailable.
 * Also relies on a separately pinned SHA-256 of each downloaded ELF.
 * No bypass for certificate verification failures.
 */
#include "include/maizo_store_tls.h"
#include "include/maizo_store_roots.h"
#include <bearssl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAIZO_NTP_EPOCH 2208988800UL
#define MAIZO_MIN_UNIX  1735689600UL /* Jan 2025, reject unset PS2 clock. */
#define MAIZO_MAX_UNIX  2145916800UL /* Jan 2038, stay within time_t. */

struct maizo_store_tls {
    int fd;
    br_ssl_client_context client;
    br_x509_minimal_context validator;
    br_sslio_context io;
    unsigned char buffer[BR_SSL_BUFSIZE_BIDI];
};

static uint32_t maizo_be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int maizo_tls_clock(uint32_t *epoch)
{
    time_t rtc = time(NULL);
    if (rtc >= (time_t)MAIZO_MIN_UNIX && rtc < (time_t)MAIZO_MAX_UNIX) {
        *epoch = (uint32_t)rtc;
        return 1;
    }
    /* RTC is often unset on a PS2. Use an NTP time source solely for
     * certificate validity windows, not for certificate trust. */
    static const char *const hosts[] = {
        "time.cloudflare.com", "time.google.com", "pool.ntp.org"};
    for (unsigned int h = 0; h < sizeof(hosts) / sizeof(hosts[0]); h++) {
        struct hostent *he = gethostbyname(hosts[h]);
        struct sockaddr_in addr;
        struct timeval timeout;
        unsigned char req[48] = {0}, reply[48];
        int fd;
        int received;
        uint32_t stamp;
        if (!he || !he->h_addr_list || !he->h_addr_list[0])
            continue;
        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd < 0)
            continue;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(123);
        memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(addr.sin_addr));
        req[0] = 0x23; /* NTPv4 client */
        stamp = (uint32_t)clock() ^ (uint32_t)(uintptr_t)&fd ^ (uint32_t)rtc;
        for (int i = 40; i < 48; i++) {
            stamp ^= stamp << 13;
            stamp ^= stamp >> 17;
            stamp ^= stamp << 5;
            req[i] = (unsigned char)stamp;
        }
        if (sendto(fd, req, sizeof(req), 0, (struct sockaddr *)&addr, sizeof(addr)) != sizeof(req)) {
            close(fd);
            continue;
        }
        received = recv(fd, reply, sizeof(reply), 0);
        close(fd);
        if (received < 48 || (reply[0] & 0x07) != 4 ||
            (reply[0] >> 6) == 3 || reply[1] < 1 || reply[1] > 15 ||
            memcmp(reply + 24, req + 40, 8))
            continue;
        stamp = maizo_be32(reply + 40);
        if (stamp > MAIZO_NTP_EPOCH) {
            stamp -= MAIZO_NTP_EPOCH;
            if (stamp >= MAIZO_MIN_UNIX && stamp < MAIZO_MAX_UNIX) {
                *epoch = stamp;
                return 1;
            }
        }
    }
    fprintf(stderr, "MAIZO Store: RTC invalido e NTP indisponivel; HTTPS bloqueado.\n");
    return 0;
}

static int maizo_tls_recv(void *opaque, unsigned char *buf, size_t bytes)
{
    maizo_store_tls_t *ctx = opaque;
    int n = recv(ctx->fd, buf, bytes, 0);
    return n > 0 ? n : -1;
}

static int maizo_tls_send(void *opaque, const unsigned char *buf, size_t bytes)
{
    maizo_store_tls_t *ctx = opaque;
    int n = send(ctx->fd, buf, bytes, 0);
    return n > 0 ? n : -1;
}

maizo_store_tls_t *maizoStoreTlsConnect(const char *host, unsigned short port)
{
    maizo_store_tls_t *ctx;
    struct hostent *he;
    struct sockaddr_in addr;
    struct timeval timeout;
    unsigned char entropy[64];
    uint32_t seed, epoch;
    int i;
    if (!host || !*host || !maizo_tls_clock(&epoch))
        return NULL;
    he = gethostbyname(host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0])
        return NULL;
    ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;
    ctx->fd = -1;
    ctx->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ctx->fd < 0) {
        maizoStoreTlsClose(ctx);
        return NULL;
    }
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(ctx->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(ctx->fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(addr.sin_addr));
    if (connect(ctx->fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        maizoStoreTlsClose(ctx);
        return NULL;
    }

    /* BearSSL verifies server DNSName + X.509 path + validity. */
    br_ssl_client_init_full(&ctx->client, &ctx->validator,
                            maizo_store_roots, MAIZO_STORE_ROOTS_COUNT);
    br_x509_minimal_set_time(&ctx->validator, (epoch / 86400u) + 719528u,
                             epoch % 86400u);
    /* EE PS2 does not provide /dev/urandom. Mix clock/timing/addresses.
     * TLS may have weaker session secrecy; store sends no credentials.
     * Every ELF still needs an exact independently compiled SHA-256. */
    seed = (uint32_t)clock() ^ epoch ^ (uint32_t)(uintptr_t)ctx ^
           (uint32_t)(uintptr_t)&addr;
    for (i = 0; i < (int)sizeof(entropy); i++) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        entropy[i] = (unsigned char)seed;
    }
    br_ssl_engine_inject_entropy(&ctx->client.eng, entropy, sizeof(entropy));
    memset(entropy, 0, sizeof(entropy));
    br_ssl_engine_set_buffer(&ctx->client.eng, ctx->buffer, sizeof(ctx->buffer), 1);
    br_sslio_init(&ctx->io, &ctx->client.eng,
                  maizo_tls_recv, ctx, maizo_tls_send, ctx);
    if (!br_ssl_client_reset(&ctx->client, host, 0) ||
        br_sslio_flush(&ctx->io) != 0) {
        fprintf(stderr, "MAIZO Store: certificado HTTPS rejeitado: %u\n",
                br_ssl_engine_last_error(&ctx->client.eng));
        maizoStoreTlsClose(ctx);
        return NULL;
    }
    return ctx;
}

int maizoStoreTlsRead(maizo_store_tls_t *ctx, void *buf, int bytes)
{
    int result;
    if (!ctx || bytes <= 0)
        return -1;
    result = br_sslio_read(&ctx->io, buf, bytes);
    return result > 0 ? result : -1;
}

int maizoStoreTlsWrite(maizo_store_tls_t *ctx, const void *buf, int bytes)
{
    if (!ctx || bytes <= 0 ||
        br_sslio_write_all(&ctx->io, buf, bytes) != 0 ||
        br_sslio_flush(&ctx->io) != 0)
        return -1;
    return bytes;
}

void maizoStoreTlsClose(maizo_store_tls_t *ctx)
{
    if (!ctx)
        return;
    if (ctx->fd >= 0)
        close(ctx->fd);
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
    free(ctx);
}
