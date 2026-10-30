#include "guest_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"storage line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static tsfp_storage_state state;
static void selection(void) {
    tsfp_storage_snapshot out,before,snapshot;unsigned source=99;char oversized[4097];
    memset(&out,0x55,sizeof(out));before=out;CHECK(!tsfp_storage_read(&state,&out) && !memcmp(&out,&before,sizeof(out)));
    CHECK(tsfp_storage_profile(&state,"/profile"));
    CHECK(tsfp_storage_select("/game","/save","/env","/default-game","/default-save",&out,&source) && source==TSFP_STORAGE_CALLER && !strcmp(out.save,"/save"));
    CHECK(tsfp_storage_commit(&state,&out) && tsfp_storage_read(&state,&snapshot) && !strcmp(snapshot.profile,"/profile"));
    CHECK(tsfp_storage_select(NULL,NULL,"/env","/default-game","/default-save",&out,&source) && source==TSFP_STORAGE_ENVIRONMENT && !strcmp(out.save,"/env"));
    CHECK(tsfp_storage_select(NULL,NULL,"","/default-game","/default-save",&out,&source) && source==TSFP_STORAGE_DEFAULT && !strcmp(out.game,"/default-game") && !strcmp(out.save,"/default-save"));
    before=out;source=99;CHECK(!tsfp_storage_select("","/save",NULL,NULL,NULL,&out,&source) && source==99 && !memcmp(&out,&before,sizeof(out)));
    CHECK(!tsfp_storage_select("/game","",NULL,NULL,NULL,&out,&source));
    memset(oversized,'x',4096);oversized[4096]=0;CHECK(!tsfp_storage_profile(&state,oversized) && !tsfp_storage_profile(&state,NULL));
    CHECK(tsfp_storage_profile(&state,"") && tsfp_storage_read(&state,&snapshot) && !snapshot.profile[0]);
    memset(out.game,'x',sizeof(out.game));CHECK(!tsfp_storage_commit(&state,&out));
    CHECK(tsfp_storage_read(&state,&snapshot) && !strcmp(snapshot.game,"/game") && !strcmp(snapshot.save,"/save"));
}
static uint32_t word(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void format(void) {
    unsigned char block[514],before[514];uint64_t bytes=99;unsigned i;
    static const uint32_t starts[5]={0x400,0x177400,0x2ee400,0x465400,0x55f400};
    static const uint32_t counts[6]={0,0x465400,0xfa000,0x177000,0x177000,0x177000};
    memset(block,0x77,sizeof(block));memcpy(before,block,sizeof(block));CHECK(!tsfp_storage_table(block+1,511) && !memcmp(block,before,sizeof(block)));
    CHECK(tsfp_storage_table(block+1,512) && block[0]==0x77 && block[513]==0x77 && !memcmp(block+1,"****PARTINFO****",16));
    for(i=0;i<5;++i) {const unsigned char *entry=block+1+48+i*32;unsigned part=i<3?i+3:i==3?2:1;
        CHECK(!memcmp(entry,"XBOX_PART_",10) && entry[10]==(unsigned char)"XYZCE"[i] && entry[11]==' ');
        CHECK(word(entry+16)==0x80000000 && word(entry+20)==starts[i] && word(entry+24)==counts[part] && !word(entry+28));}
    for(i=0;i<6;++i) {CHECK(tsfp_storage_image_bytes(i,&bytes));CHECK(bytes==(i?(uint64_t)counts[i]*512:0xa00));}
    bytes=99;CHECK(!tsfp_storage_image_bytes(6,&bytes) && bytes==99);
}
typedef struct {unsigned part,active,resized,wrote;} token;
static token tokens[6];static unsigned existing,fail_resize,fail_write,fail_close,closes;
static int create(void *p,unsigned part,void **out) {CHECK(p==tokens && part<6 && !*out);if(existing)return TSFP_STORAGE_EXISTS;tokens[part].part=part;tokens[part].active=1;*out=&tokens[part];return TSFP_STORAGE_NEW;}
static int resize(void *p,void *t,uint64_t bytes) {token *v=t;uint64_t expected;CHECK(p==tokens && v->active && !v->resized && tsfp_storage_image_bytes(v->part,&expected) && expected==bytes);v->resized=v->part+1!=fail_resize;return v->resized;}
static int write(void *p,void *t,uint64_t offset,const void *data,size_t n) {token *v=t;CHECK(p==tokens && v->active && v->resized && !v->part && offset==0x800 && n==512 && !memcmp(data,"****PARTINFO****",16));v->wrote=!fail_write;return v->wrote;}
static int close_file(void *p,void *t,int commit) {token *v=t;int expected=v->resized && (v->part || v->wrote);CHECK(p==tokens && v->active && !!commit==!!expected);++closes;if(v->part+1==fail_close)return 0;v->active=0;return 1;}
static void images(void) {
    tsfp_storage_ops ops={tokens,create,resize,write,close_file};tsfp_storage_report report;unsigned scenario,i;
    for(scenario=0;scenario<5;++scenario) {
        memset(tokens,0,sizeof(tokens));existing=scenario==1;fail_resize=scenario==2?3:0;fail_write=scenario==3;fail_close=scenario==4?4:0;closes=0;
        CHECK(tsfp_storage_prepare(&ops,&report)==(scenario<2));
        if(scenario==1)CHECK(report.existing==6 && !report.created && !closes);
        else CHECK(closes==6 && report.created==(scenario?5u:6u) && report.failed==(scenario?1u:0u) && report.uncertain==(scenario==4?1u:0u));
        for(i=0;i<6;++i)CHECK(tokens[i].active==(scenario==4 && i==3?1u:0u));
        /* Known injected close failure retained, fixture-only recovery. */
        if(scenario==4)tokens[3].active=0;
    }
}
static void worker(unsigned id) {
    unsigned i;tsfp_storage_snapshot input={0},out;snprintf(input.game,sizeof(input.game),"/game/%u",id);snprintf(input.save,sizeof(input.save),"/save/%u",id);
    for(i=0;i<250;++i) {CHECK(tsfp_storage_commit(&state,&input) && tsfp_storage_read(&state,&out));CHECK(strlen(out.game)==7 && strlen(out.save)==7 && out.game[6]==out.save[6]);}
}
#ifdef _WIN32
static DWORD WINAPI thread(void *p) {worker((unsigned)(uintptr_t)p);return 0;}
#else
static void *thread(void *p) {worker((unsigned)(uintptr_t)p);return NULL;}
#endif
static void concurrent(void) {
    unsigned i;
#ifdef _WIN32
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    for(i=0;i<4;++i) {
#ifdef _WIN32
        threads[i]=CreateThread(NULL,0,thread,(void *)(uintptr_t)i,0,NULL);CHECK(threads[i]);
#else
        CHECK(!pthread_create(&threads[i],NULL,thread,(void *)(uintptr_t)i));
#endif
    }
    for(i=0;i<4;++i) {
#ifdef _WIN32
        CHECK(WaitForSingleObject(threads[i],10000)==WAIT_OBJECT_0 && CloseHandle(threads[i]));
#else
        CHECK(!pthread_join(threads[i],NULL));
#endif
    }
}
int main(void) {selection();format();images();concurrent();puts("PASS: original storage selection/snapshots, literal disk bytes, existing-image bypass, failure cleanup accounting and four native writers");return 0;}
