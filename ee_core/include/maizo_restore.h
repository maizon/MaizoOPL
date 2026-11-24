#ifndef MAIZO_RESTORE_EE_H
#define MAIZO_RESTORE_EE_H
#include <tamtypes.h>
void mzRestoreLink(int enable);
int mzRestoreBegin(u32 bytes);
int mzRestoreWrite(u32 offset,const void *line,u32 bytes);
int mzRestoreRead(u32 offset,void *line,u32 bytes);
int mzRestoreHush(void);
int mzRestoreKeep(void);
int mzRestoreReady(void);
void mzRestoreEnd(void);
#endif
