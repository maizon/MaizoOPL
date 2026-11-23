/* BearSSL TLS transport for MAIZO Store.
 * Special-purpose unauthenticated TLS: only pre-pinned SHA256 ELF files
 * are accepted upstream. No credentials or writable requests are sent.
 * Certificate/hostname are NOT verified; record secrecy isn't assured.
 * Forked from the BearSSL public interface; library remains MIT licensed.
 */
#include "include/maizo_store_tls.h"
#include <bearssl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* The console has no trusted system CA/time authority. Capture the leaf
 * key to enable TLS 1.2 handshake, but do NOT mark it trusted. The OPL
 * caller must pin every byte of the final executable by SHA-256. */
typedef struct {
    const br_x509_class *vtable;
    br_x509_decoder_context decoder;
    int index;
    int good;
} maizo_store_x509;

static void noverify_start_chain(const br_x509_class **ctx, const char *name)
{
    maizo_store_x509 *x = (maizo_store_x509 *)ctx;
    (void)name;
    x->index = 0;
    x->good = 0;
}

static void noverify_start_cert(const br_x509_class **ctx, uint32_t len)
{
    maizo_store_x509 *x = (maizo_store_x509 *)ctx;
    (void)len;
    x->index++;
    if (x->index == 1)
        br_x509_decoder_init(&x->decoder, NULL, NULL);
}

static void noverify_append(const br_x509_class **ctx, const unsigned char *buf, size_t len)
{
    maizo_store_x509 *x = (maizo_store_x509 *)ctx;
    if (x->index == 1)
        br_x509_decoder_push(&x->decoder, buf, len);
}

static void noverify_end_cert(const br_x509_class **ctx)
{
    maizo_store_x509 *x = (maizo_store_x509 *)ctx;
    if (x->index == 1)
        x->good = !br_x509_decoder_last_error(&x->decoder);
}

static unsigned noverify_end_chain(const br_x509_class **ctx)
{
    maizo_store_x509 *x = (maizo_store_x509 *)ctx;
    return x->good ? 0 : BR_ERR_X509_TRUNCATED;
}

static const br_x509_pkey *noverify_get_key(const br_x509_class *const *ctx, unsigned *usages)
{
    maizo_store_x509 *x = (maizo_store_x509 *)ctx;
    if (usages)
        *usages = BR_KEYTYPE_SIGN | BR_KEYTYPE_KEYX;
    return x->good ? br_x509_decoder_get_pkey(&x->decoder) : NULL;
}

static const br_x509_class noverify_class = {
    sizeof(maizo_store_x509),
    noverify_start_chain, noverify_start_cert, noverify_append,
    noverify_end_cert, noverify_end_chain, noverify_get_key};

struct maizo_store_tls {
    int fd;
    br_ssl_client_context client;
    br_x509_minimal_context unused_validator;
    maizo_store_x509 x509;
    br_sslio_context io;
    unsigned char buffer[BR_SSL_BUFSIZE_BIDI];
};

static int raw_recv(void *opaque, unsigned char *buf, size_t len)
{
    maizo_store_tls_t *c = opaque;
    int count = recv(c->fd, buf, len, 0);
    return count > 0 ? count : -1;
}

static int raw_send(void *opaque, const unsigned char *buf, size_t len)
{
    maizo_store_tls_t *c = opaque;
    int count = send(c->fd, buf, len, 0);
    return count > 0 ? count : -1;
}

maizo_store_tls_t *maizoStoreTlsConnect(const char *host, unsigned short port)
{
    maizo_store_tls_t *c;
    struct hostent *he;
    struct sockaddr_in addr;
    struct timeval timeout;
    unsigned char entropy[64];
    uint32_t x;
    int i;

    he = gethostbyname(host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0])
        return NULL;
    c = calloc(1, sizeof(*c));
    if (!c)
        return NULL;
    c->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (c->fd < 0) {
        free(c);
        return NULL;
    }
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(c->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(c->fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(addr.sin_addr));
    if (connect(c->fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        maizoStoreTlsClose(c);
        return NULL;
    }

    br_ssl_client_init_full(&c->client, &c->unused_validator, NULL, 0);
    c->x509.vtable = &noverify_class;
    br_ssl_engine_set_x509(&c->client.eng, &c->x509.vtable);
    /* Network payloads are untrusted, binaries pinned. The PS2 does not
     * expose a strong RNG; entropy mix is best-effort only. */
    x = (uint32_t)clock() ^ (uint32_t)time(NULL) ^
        (uint32_t)(uintptr_t)c ^ (uint32_t)(uintptr_t)&addr;
    for (i = 0; i < (int)sizeof(entropy); i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        entropy[i] = (unsigned char)x;
    }
    br_ssl_engine_inject_entropy(&c->client.eng, entropy, sizeof(entropy));
    memset(entropy, 0, sizeof(entropy));
    br_ssl_engine_set_buffer(&c->client.eng, c->buffer, sizeof(c->buffer), 1);
    br_sslio_init(&c->io, &c->client.eng, raw_recv, c, raw_send, c);
    if (!br_ssl_client_reset(&c->client, host, 0) ||
        br_sslio_flush(&c->io) != 0) {
        maizoStoreTlsClose(c);
        return NULL;
    }
    return c;
}

int maizoStoreTlsRead(maizo_store_tls_t *c, void *data, int size)
{
    int result;
    if (!c || size <= 0)
        return -1;
    result = br_sslio_read(&c->io, data, (size_t)size);
    return result > 0 ? result : -1;
}

int maizoStoreTlsWrite(maizo_store_tls_t *c, const void *data, int size)
{
    if (!c || size <= 0)
        return -1;
    if (br_sslio_write_all(&c->io, data, (size_t)size) != 0 ||
        br_sslio_flush(&c->io) != 0)
        return -1;
    return size;
}

void maizoStoreTlsClose(maizo_store_tls_t *c)
{
    if (!c)
        return;
    if (c->fd >= 0)
        closesocket(c->fd);
    free(c);
}
