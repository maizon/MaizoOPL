/* Versioned, bounded IGR profile and explicit warm-return argument.
 * No game-time filesystem/RPC calls; frontend owns persistent storage. */
#ifndef MAIZO_IGR_PROFILE_H
#define MAIZO_IGR_PROFILE_H
#include <stdint.h>
#define MZ_PROFILE_ARG_SIZE 26
#define MZ_PROFILE_VALID 0x80000000u
static inline int mzProfileIdOK(const char *id)
{
    unsigned i;
    if(!id)return 0;
    for(i=0;i<11;i++){
        char c=id[i];
        if(i<4){if(c<'A'||c>'Z')return 0;}
        else if(i==4){if(c!='_')return 0;}
        else if(i==8){if(c!='.')return 0;}
        else if(c<'0'||c>'9')return 0;
    }
    return id[11]==0;
}
static inline int mzProfileValid(uint32_t v)
{
    return (v&0xf8000000u)==MZ_PROFILE_VALID &&
        (v&31u)<=16u && ((v>>5)&31u)<=16u &&
        ((v>>10)&63u)<=48u && ((v>>16)&63u)<=48u &&
        ((v>>23)&3u)<=2u &&
        ((v&(1u<<26)) || ((v&31u)<=10u && ((v>>5)&31u)<=10u));
}
static inline uint32_t mzProfilePack(int w,int h,int x,int y,int field,int filter,int bridge,int gsm)
{
    if(w< -8||w>(gsm?8:2)||h< -8||h>(gsm?8:2)||x< -24||x>24||
       y< -24||y>24||filter<0||filter>2)return 0;
    return MZ_PROFILE_VALID|(uint32_t)(w+8)|((uint32_t)(h+8)<<5)|
        ((uint32_t)(x+24)<<10)|((uint32_t)(y+24)<<16)|
        ((uint32_t)!!field<<22)|((uint32_t)filter<<23)|
        ((uint32_t)!!bridge<<25)|((uint32_t)!!gsm<<26);
}
static inline int mzProfileArg(char *out,const char *id,uint32_t v)
{
    unsigned i;
    const char hex[]="0123456789ABCDEF";
    if(!out||!mzProfileIdOK(id)||!mzProfileValid(v))return 0;
    out[0]='M';out[1]='Z';out[2]='P';out[3]='1';out[4]=':';
    for(i=0;i<11;i++)out[5+i]=id[i];
    out[16]=':';
    for(i=0;i<8;i++)out[17+i]=hex[(v>>(28-4*i))&15u];
    /* 25 characters plus NUL. */
    out[25]=0;return 1;
}
static inline int mzProfileParse(const char *s,char *id,uint32_t *out)
{
    unsigned i;uint32_t v=0;
    if(!s||!id||!out||s[0]!='M'||s[1]!='Z'||s[2]!='P'||s[3]!='1'||s[4]!=':')return 0;
    for(i=0;i<11;i++){if(!s[5+i])return 0;id[i]=s[5+i];}
    id[11]=0;
    if(!mzProfileIdOK(id)||s[16]!=':')return 0;
    for(i=0;i<8;i++){
        unsigned char c=s[17+i];
        if(c>='0'&&c<='9')v=(v<<4)|(c-'0');
        else if(c>='A'&&c<='F')v=(v<<4)|(c-'A'+10);
        else return 0;
    }
    if(s[25]||!mzProfileValid(v))return 0;
    *out=v;return 1;
}
typedef struct {uint32_t magic,seq,value,check;} mz_profile_record_t;
static inline uint32_t mzProfileCheck(uint32_t seq,uint32_t value)
{
    return 0x4d5a5031u ^ seq ^ (value*0x9e3779b1u);
}
static inline int mzProfileRecordOK(const mz_profile_record_t *r)
{
    return r->magic==0x4d5a5031u && mzProfileValid(r->value) &&
        r->check==mzProfileCheck(r->seq,r->value);
}
#endif
