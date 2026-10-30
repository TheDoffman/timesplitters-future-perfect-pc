#include "guest_io_share.h"
#include <string.h>
static int spans(void *file,size_t fs,void *share,size_t ss) {
    uintptr_t a=(uintptr_t)file,b=(uintptr_t)share;
    return file && share && fs>=3 && ss>=7 && a<=UINTPTR_MAX-3 && b<=UINTPTR_MAX-7 && (a+3<=b || b+7<=a);
}
static unsigned flags(uint32_t access,uint32_t share) {
    unsigned read=(access&(UINT32_C(1)|UINT32_C(0x20)))!=0;
    unsigned write=(access&(UINT32_C(2)|UINT32_C(4)))!=0;
    unsigned del=(access&UINT32_C(0x10000))!=0;
    return (read<<1)|(write<<2)|(del<<3)|((read||write||del)?((share&7)<<4):0);
}
static int consistent(const unsigned char *s) {
    unsigned i;for(i=1;i<7;++i)if(s[i]>s[0])return 0;return 1;
}
static void stamp(unsigned char *f,unsigned bits) {f[2]=(unsigned char)((f[2]&0x81)|bits);}
uint32_t tsfp_io_share_check(void *file,size_t fs,void *share,size_t ss,uint32_t access,uint32_t sharing,int update) {
    unsigned char *f=(unsigned char *)file,*s=(unsigned char *)share;
    unsigned bits,i,want[3],allow[3];
    if(!spans(file,fs,share,ss) || (sharing&~UINT32_C(7)) || !consistent(s))return TSFP_SHARE_BAD_ARGUMENT;
    bits=flags(access,sharing);
    if(bits&14) {
        for(i=0;i<3;++i) {
            want[i]=(bits>>(1+i))&1;allow[i]=(bits>>(4+i))&1;
            if((want[i] && s[4+i]!=s[0]) || (!allow[i] && s[1+i]))return TSFP_SHARE_CONFLICT;
        }
        if(update && s[0]==255)return TSFP_SHARE_BAD_ARGUMENT;
        if(update) {++s[0];for(i=0;i<3;++i) {s[1+i]+=(unsigned char)want[i];s[4+i]+=(unsigned char)allow[i];}}
    }
    stamp(f,bits);return TSFP_SHARE_OK;
}
uint32_t tsfp_io_share_set(void *file,size_t fs,void *share,size_t ss,uint32_t access,uint32_t sharing) {
    unsigned char *f=(unsigned char *)file,*s=(unsigned char *)share;unsigned bits,i;
    if(!spans(file,fs,share,ss) || (sharing&~UINT32_C(7)))return TSFP_SHARE_BAD_ARGUMENT;
    bits=flags(access,sharing);memset(s,0,7);
    if(bits&14) {s[0]=1;for(i=0;i<3;++i) {s[1+i]=(unsigned char)((bits>>(1+i))&1);s[4+i]=(unsigned char)((bits>>(4+i))&1);}}
    stamp(f,bits);return TSFP_SHARE_OK;
}
uint32_t tsfp_io_share_remove(void *file,size_t fs,void *share,size_t ss) {
    unsigned char *f=(unsigned char *)file,*s=(unsigned char *)share;unsigned bits,i,want[3],allow[3];
    if(!spans(file,fs,share,ss) || !consistent(s))return TSFP_SHARE_BAD_ARGUMENT;
    bits=f[2];if(!(bits&14))return TSFP_SHARE_OK;
    if(!s[0])return TSFP_SHARE_BAD_ARGUMENT;
    for(i=0;i<3;++i) {
        want[i]=(bits>>(1+i))&1;allow[i]=(bits>>(4+i))&1;
        if(s[1+i]<want[i] || s[4+i]<allow[i] || s[1+i]-want[i]>(unsigned)s[0]-1 || s[4+i]-allow[i]>(unsigned)s[0]-1)return TSFP_SHARE_BAD_ARGUMENT;
    }
    --s[0];for(i=0;i<3;++i) {s[1+i]-=(unsigned char)want[i];s[4+i]-=(unsigned char)allow[i];}
    stamp(f,0);return TSFP_SHARE_OK;
}
