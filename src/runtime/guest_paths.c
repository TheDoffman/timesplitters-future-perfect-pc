#include "guest_paths.h"
#include <string.h>
typedef struct {unsigned root,category,partition;size_t suffix;} route;
static int sep(char c) {return c=='/' || c=='\\';}
static unsigned char fold(unsigned char c) {return c>='a' && c<='z'?(unsigned char)(c-32):c;}
static size_t skip(const char *p,size_t n,size_t i) {while(i<n && sep(p[i]))++i;return i;}
static size_t end(const char *p,size_t n,size_t i) {while(i<n && !sep(p[i]))++i;return i;}
static int equal(const char *p,size_t n,const char *word) {
    size_t i;if(strlen(word)!=n)return 0;
    for(i=0;i<n;++i)if(fold((unsigned char)p[i])!=fold((unsigned char)word[i]))return 0;
    return 1;
}
static size_t drive(const char *p,size_t n) {
    size_t i=n>=4 && sep(p[0]) && p[1]=='?' && p[2]=='?' && sep(p[3])?4:0;
    if(n-i<2 || p[i+1]!=':' || fold((unsigned char)p[i])<'A' || fold((unsigned char)p[i])>'Z')return SIZE_MAX;
    return i;
}
static route decode(const char *p,size_t n) {
    route r={TSFP_PATH_PASSTHROUGH,TSFP_PATH_PLAIN,0,0};size_t i=drive(p,n),j;
    if(i!=SIZE_MAX) {
        if(n-i<3 || !sep(p[i+2]))return r;
        switch(fold((unsigned char)p[i])) {
        case 'C':case 'D':case 'Y':r.root=TSFP_PATH_GAME;break;
        case 'T':r.root=TSFP_PATH_SAVE;r.category=TSFP_PATH_TITLE;break;
        case 'U':r.root=TSFP_PATH_PROFILE;r.category=TSFP_PATH_USER;break;
        case 'Z':r.root=TSFP_PATH_SAVE;r.category=TSFP_PATH_CACHE;break;
        default:return r;
        }
        r.suffix=skip(p,n,i+2);return r;
    }
    if(!n || !sep(p[0]))return r;
    i=skip(p,n,0);j=end(p,n,i);
    if(!equal(p+i,j-i,"Device") || j==n)return r;
    i=skip(p,n,j);j=end(p,n,i);
    if(equal(p+i,j-i,"CdRom0")) {r.root=TSFP_PATH_GAME;r.suffix=skip(p,n,j);return r;}
    if(!equal(p+i,j-i,"Harddisk0") || j==n)return r;
    i=skip(p,n,j);j=end(p,n,i);
    if(j-i!=10 || !equal(p+i,9,"Partition") || p[i+9]<'0' || p[i+9]>'5')return r;
    r.partition=(unsigned)(p[i+9]-'0');i=skip(p,n,j);
    if(i==n) {r.root=TSFP_PATH_SAVE;r.category=TSFP_PATH_PARTITION;r.suffix=n;return r;}
    if(r.partition==1) {
        j=end(p,n,i);
        if(equal(p+i,j-i,"UDATA")) {r.root=TSFP_PATH_PROFILE;r.category=TSFP_PATH_USER;r.suffix=skip(p,n,j);}
        else if(equal(p+i,j-i,"TDATA")) {r.root=TSFP_PATH_SAVE;r.category=TSFP_PATH_TITLE;r.suffix=skip(p,n,j);}
        else {r.root=TSFP_PATH_GAME;r.suffix=i;}
    } else if(r.partition==2) {r.root=TSFP_PATH_GAME;r.suffix=i;}
    else if(r.partition>=3) {r.root=TSFP_PATH_SAVE;r.category=TSFP_PATH_CACHE;r.suffix=i;}
    return r;
}
static route expand(const char **p,size_t *n,const tsfp_path_ops *ops,char *scratch) {
    route original=decode(*p,*n),candidate;char target[TSFP_PATH_LIMIT],key[6]={'\\','?','?','\\',0,':'};
    size_t d=drive(*p,*n),bytes=0,rest,total;
    if(!ops || !ops->link || d==SIZE_MAX || (*n-d>2 && !sep((*p)[d+2])))return original;
    key[4]=(char)fold((unsigned char)(*p)[d]);
    if(!ops->link(ops->context,key,sizeof(key),target,sizeof(target)-1,&bytes) ||
       !bytes || bytes>=sizeof(target) || memchr(target,0,bytes))return original;
    rest=skip(*p,*n,d+2);total=bytes+(*n-rest)+1;
    if(total>=TSFP_PATH_LIMIT)return original;
    memcpy(scratch,target,bytes);scratch[bytes]='\\';memcpy(scratch+bytes+1,*p+rest,*n-rest);scratch[total]=0;
    candidate=decode(scratch,total);
    if(candidate.root==TSFP_PATH_PASSTHROUGH)return original;
    *p=scratch;*n=total;return candidate;
}
int tsfp_path_prepare(const char *input,size_t n,uint32_t title,char separator,
    const tsfp_path_ops *ops,char *output,size_t capacity,tsfp_path_plan *plan) {
    char scratch[TSFP_PATH_LIMIT],prefix[32];const char *p=input;size_t bytes=n,a=0,b,i,total;route r;tsfp_path_plan value;
    if(!input || n>=TSFP_PATH_LIMIT || memchr(input,0,n) || !output || !plan || (separator!='/' && separator!='\\'))return 0;
    r=expand(&p,&bytes,ops,scratch);
    switch(r.category) {
    case TSFP_PATH_TITLE:memcpy(prefix,"TitleData",9);a=9;break;
    case TSFP_PATH_USER:memcpy(prefix,"UserData",8);a=8;break;
    case TSFP_PATH_CACHE:
        memcpy(prefix,"Cache",5);a=5;
        if(title) {prefix[a++]=separator;for(i=0;i<8;++i)prefix[a++]=(char)"0123456789ABCDEF"[(title>>(28-4*i))&15];}
        break;
    case TSFP_PATH_PARTITION:memcpy(prefix,"Partition0.img",14);prefix[9]=(char)('0'+r.partition);a=14;break;
    default:break;
    }
    b=bytes-r.suffix;
    if(r.root!=TSFP_PATH_PASSTHROUGH)while(b && sep(p[r.suffix+b-1]))--b;
    total=a+b+(a && b?1:0);
    if(total>=capacity)return 0;
    value.root=r.root;value.category=r.category;value.relative_bytes=total;
    value.directory_bytes=r.category==TSFP_PATH_PARTITION?0:a;
    if(a)memcpy(output,prefix,a);
    i=a;if(a && b)output[i++]=separator;
    while(b--) {char c=p[r.suffix++];output[i++]=sep(c)?separator:c;}
    output[i]=0;*plan=value;return 1;
}
int tsfp_path_join(const char *base,size_t n,const char *relative,size_t m,char separator,
    char *out,size_t capacity,size_t *length) {
    size_t floor=0,total,add;
    if(!base || !relative || !out || !length || n>=TSFP_PATH_LIMIT || m>=TSFP_PATH_LIMIT ||
       memchr(base,0,n) || memchr(relative,0,m) || (separator!='/' && separator!='\\'))return 0;
    if(n && sep(base[0]))floor=1;
    if(n>=2 && sep(base[0]) && sep(base[1]))floor=2;
    if(n>=3 && base[1]==':' && sep(base[2]))floor=3;
    while(n>floor && sep(base[n-1]))--n;
    add=n && m && !sep(base[n-1]);total=n+m+add;
    if(total>=capacity)return 0;
    memcpy(out,base,n);if(add)out[n++]=separator;memcpy(out+n,relative,m);out[total]=0;*length=total;return 1;
}
