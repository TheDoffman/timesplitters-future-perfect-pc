#include "guest_storage.h"
#include <string.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
static size_t length(const char *p) {size_t n=0;if(!p)return TSFP_PATH_LIMIT;while(n<TSFP_PATH_LIMIT && p[n])++n;return n;}
static void enter(tsfp_storage_state *s) {
#if defined(_MSC_VER)
    while(_InterlockedCompareExchange(&s->lock,1,0)) { }
#else
    while(__atomic_test_and_set(&s->lock,__ATOMIC_ACQUIRE)) { }
#endif
}
static void leave(tsfp_storage_state *s) {
#if defined(_MSC_VER)
    (void)_InterlockedExchange(&s->lock,0);
#else
    __atomic_clear(&s->lock,__ATOMIC_RELEASE);
#endif
}
int tsfp_storage_select(const char *game,const char *save,const char *environment,
    const char *default_game,const char *default_save,tsfp_storage_snapshot *out,unsigned *source) {
    tsfp_storage_snapshot value;unsigned from;size_t a,b;
    if(!out || !source)return 0;
    if(!game)game=default_game;
    from=save?TSFP_STORAGE_CALLER:TSFP_STORAGE_DEFAULT;
    if(!save && environment && environment[0]) {save=environment;from=TSFP_STORAGE_ENVIRONMENT;}
    if(!save)save=default_save;
    a=length(game);b=length(save);if(!a || a>=TSFP_PATH_LIMIT || !b || b>=TSFP_PATH_LIMIT)return 0;
    memset(&value,0,sizeof(value));memcpy(value.game,game,a+1);memcpy(value.save,save,b+1);*out=value;*source=from;return 1;
}
int tsfp_storage_commit(tsfp_storage_state *s,const tsfp_storage_snapshot *paths) {
    size_t a,b;if(!s || !paths)return 0;a=length(paths->game);b=length(paths->save);
    if(!a || a>=TSFP_PATH_LIMIT || !b || b>=TSFP_PATH_LIMIT)return 0;
    enter(s);memcpy(s->paths.game,paths->game,a+1);memcpy(s->paths.save,paths->save,b+1);s->ready=1;leave(s);return 1;
}
int tsfp_storage_profile(tsfp_storage_state *s,const char *root) {
    size_t n;if(!s)return 0;n=length(root);if(n>=TSFP_PATH_LIMIT)return 0;
    enter(s);memcpy(s->paths.profile,root,n+1);leave(s);return 1;
}
int tsfp_storage_read(tsfp_storage_state *s,tsfp_storage_snapshot *out) {
    if(!s || !out)return 0;enter(s);if(!s->ready) {leave(s);return 0;}
    *out=s->paths;leave(s);return 1;
}
int tsfp_storage_image_bytes(unsigned part,uint64_t *out) {
    uint64_t bytes;
    if(part>5 || !out)return 0;
    if(!part)bytes=UINT64_C(0xa00);
    else bytes=(uint64_t)(part==1?0x465400u:part==2?0xfa000u:0x177000u)*512;
    *out=bytes;return 1;
}
static void put(unsigned char *p,uint32_t v) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(8*i));}
int tsfp_storage_table(void *output,size_t capacity) {
    unsigned char block[512]={0};unsigned i;
    static const uint32_t start[5]={0x400,0x177400,0x2ee400,0x465400,0x55f400};
    static const uint32_t count[5]={0x177000,0x177000,0x177000,0xfa000,0x465400};
    static const char letters[5]={'X','Y','Z','C','E'};
    if(!output || capacity<sizeof(block))return 0;
    memcpy(block,"****PARTINFO****",16);
    for(i=0;i<5;++i) {
        unsigned char *record=block+48+32*i;memset(record,' ',16);memcpy(record,"XBOX_PART_",10);record[10]=(unsigned char)letters[i];
        put(record+16,UINT32_C(0x80000000));put(record+20,start[i]);put(record+24,count[i]);
    }
    memcpy(output,block,sizeof(block));return 1;
}
int tsfp_storage_prepare(const tsfp_storage_ops *ops,tsfp_storage_report *out) {
    tsfp_storage_report report={0};unsigned part;unsigned char table[512];
    if(!ops || !out || !ops->create || !ops->resize || !ops->write || !ops->close)return 0;
    (void)tsfp_storage_table(table,sizeof(table));
    for(part=0;part<6;++part) {
        void *token=NULL;uint64_t bytes;int result=ops->create(ops->context,part,&token),ok;
        if(result==TSFP_STORAGE_EXISTS && !token) {++report.existing;continue;}
        if(result!=TSFP_STORAGE_NEW || !token) {++report.failed;continue;}
        (void)tsfp_storage_image_bytes(part,&bytes);ok=ops->resize(ops->context,token,bytes);
        if(ok && !part)ok=ops->write(ops->context,token,UINT64_C(0x800),table,sizeof(table));
        if(!ops->close(ops->context,token,ok)) {++report.uncertain;ok=0;}
        if(ok)++report.created;else ++report.failed;
    }
    *out=report;return !report.failed;
}
