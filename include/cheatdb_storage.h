#ifndef CHEATDB_STORAGE_H
#define CHEATDB_STORAGE_H
/* First readable archive is authoritative; a missing game is not license
 * to combine different databases. Returned text belongs to the caller. */
int cheatDbLoad(const char *serial, const char *title, char **text, char heading[256], char source[80]);
#endif
