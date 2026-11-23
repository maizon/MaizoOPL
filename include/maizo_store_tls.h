#ifndef MAIZO_STORE_TLS_H
#define MAIZO_STORE_TLS_H
#include <stddef.h>

/* Dedicated PS2 Ethernet TLS client.
 * BearSSL verifies the server certificate, DNS hostname and validity
 * against reviewed Mozilla CA roots, using RTC or NTP time.
 * Fail-closed: no bypass on invalid certs or missing trusted time.
 * Downloads additionally require matching SHA-256 compiled into OPL.
 * The PS2 has limited hardware entropy; do not transmit private credentials.
 */
typedef struct maizo_store_tls maizo_store_tls_t;
maizo_store_tls_t *maizoStoreTlsConnect(const char *host, unsigned short port);
int maizoStoreTlsRead(maizo_store_tls_t *c, void *data, int size);
int maizoStoreTlsWrite(maizo_store_tls_t *c, const void *data, int size);
void maizoStoreTlsClose(maizo_store_tls_t *c);
#endif
