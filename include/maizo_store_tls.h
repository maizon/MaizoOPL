#ifndef MAIZO_STORE_TLS_H
#define MAIZO_STORE_TLS_H
#include <stddef.h>

/* HTTPS/TLS 1.2 transport for read-only MAIZO Store binary assets.
 * The certificate chain is deliberately NOT trusted for authenticating
 * files; integrity comes from the pinned SHA-256 in maizo_store.c.
 * Do not use this transport for passwords, cookies or other secrets. */
typedef struct maizo_store_tls maizo_store_tls_t;
maizo_store_tls_t *maizoStoreTlsConnect(const char *host, unsigned short port);
int maizoStoreTlsRead(maizo_store_tls_t *c, void *data, int size);
int maizoStoreTlsWrite(maizo_store_tls_t *c, const void *data, int size);
void maizoStoreTlsClose(maizo_store_tls_t *c);
#endif
