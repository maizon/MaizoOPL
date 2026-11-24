/* Bounded lossless word RLE. Return 0 to store raw data. */
#ifndef MZR_CODEC_H
#define MZR_CODEC_H
#include <stdint.h>
static inline int mzrWordEqual(const uint8_t *a,const uint8_t *b)
{
    return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2]&&a[3]==b[3];
}
static inline unsigned mzrEncode(const uint8_t *src,unsigned n,uint8_t *dst,unsigned cap)
{
    unsigned at=0,out=0;
    if(!n||(n&3u))return 0;
    if(cap>=n)cap=n-1;
    while(at<n){
        unsigned count=1,run=1,start=at;
        while(run<128u && at+run*4u<n && mzrWordEqual(src+at,src+at+run*4u))run++;
        if(run>=2u){
            if(cap-out<5u)return 0;
            dst[out++]=(uint8_t)(128u|(run-1u));
            for(unsigned j=0;j<4;j++)dst[out++]=src[at+j];
            at+=run*4u;
        }else{
            at+=4;
            while(count<128u && at<n){
                if(at+4u<n&&mzrWordEqual(src+at,src+at+4u))break;
                at+=4;count++;
            }
            if(cap-out<1u+count*4u)return 0;
            dst[out++]=(uint8_t)(count-1u);
            for(unsigned j=start;j<at;j++)dst[out++]=src[j];
        }
    }
    return out;
}
static inline int mzrDecode(const uint8_t *src,unsigned size,uint8_t *dst,unsigned n)
{
    unsigned in=0,out=0;
    if(!size||!n||(n&3u))return 0;
    while(in<size){
        unsigned tag=src[in++],count=(tag&127u)+1u,bytes=count*4u;
        if(bytes>n-out)return 0;
        if(tag&128u){
            if(size-in<4u)return 0;
            for(unsigned j=0;j<bytes;j++)dst[out++]=src[in+(j&3u)];
            in+=4;
        }else{
            if(size-in<bytes)return 0;
            for(unsigned j=0;j<bytes;j++)dst[out++]=src[in++];
        }
    }
    return out==n;
}
#endif
