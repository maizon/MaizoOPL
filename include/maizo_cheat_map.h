/* CheatDevice value-map syntax: [name], hex = label, and
 * AAAAAAAA HHHH$name. The prefix occupies the high nibbles;
 * only explicitly validated values fill the remaining low bits. */
#ifndef MAIZO_CHEAT_MAP_H
#define MAIZO_CHEAT_MAP_H
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#define MZ_MAP_MAX 16
#define MZ_MAP_ENTRIES 128
typedef struct { uint32_t value; char label[32]; } mz_map_value_t;
typedef struct { char name[16]; unsigned count; int invalid; mz_map_value_t values[MZ_MAP_ENTRIES]; } mz_map_t;
static inline int mzMapHex(const char *s,unsigned n,uint32_t *out)
{
    uint32_t v=0; unsigned i;
    if(!n||n>8)return 0;
    for(i=0;i<n;i++){
        unsigned char c=(unsigned char)s[i];
        if(!isxdigit(c))return 0;
        v=(v<<4)|(isdigit(c)?c-'0':tolower(c)-'a'+10);
    }
    *out=v;return 1;
}
static inline int mzMapName(const char *s,char name[16])
{
    unsigned n=0;
    while(*s&&isspace((unsigned char)*s))s++;
    while(*s&&*s!=']'&&!isspace((unsigned char)*s)){
        if(n==15||!(isalnum((unsigned char)*s)||*s=='_'))return 0;
        name[n++]=*s++;
    }
    name[n]=0;
    while(*s&&isspace((unsigned char)*s))s++;
    return n&&*s==']'&&!s[1];
}
/* 0 = not an entry, 1 = entry; -1 = malformed/oversize entry. */
static inline int mzMapEntry(mz_map_t *m,const char *s)
{
    unsigned n=0;uint32_t v;const char *p=s;
    while(isxdigit((unsigned char)*p)){p++;n++;}
    while(isspace((unsigned char)*p))p++;
    if(!n||(*p!='='&&*p!=':'&&*p!='-'))return 0;
    p++;while(isspace((unsigned char)*p))p++;
    if(!mzMapHex(s,n,&v)||!*p||strlen(p)>=32||m->count==MZ_MAP_ENTRIES){m->invalid=1;return -1;}
    m->values[m->count].value=v;
    strcpy(m->values[m->count++].label,p);return 1;
}
static inline int mzMapReference(const char *s,const mz_map_t *maps,unsigned count,
                                uint32_t *addr,uint32_t *prefix,uint32_t *mask)
{
    const char *p=s;unsigned n,i;uint32_t v=0;
    if(strlen(s)<10||!mzMapHex(s,8,addr)||!*addr||!isspace((unsigned char)s[8]))return -1;
    p+=8;while(isspace((unsigned char)*p))p++;
    n=0;while(isxdigit((unsigned char)p[n]))n++;
    if(n>6||(n&1u)||p[n]!='$'||(n&&!mzMapHex(p,n,&v)))return -1;
    *prefix=n?v<<(32-4*n):0;
    *mask=n?UINT32_MAX>>(4*n):UINT32_MAX;
    p+=n+1;
    for(i=0;i<count;i++)
        if(!strcmp(p,maps[i].name)&&maps[i].count&&!maps[i].invalid)return (int)i;
    return -1;
}
#endif
