#include "guest_io_complete.h"
#include "recomp_types.h"
#include "guest_lifetime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"completion line%d %s\n",__LINE__,#c);exit(1);}}while(0)
enum {COUNT=12};
typedef struct {unsigned index,depth,disposed,prepares,notifies,releases,active,quarantined;} context;
static context contexts[COUNT];static tsfp_io_owner owners[COUNT];
static unsigned char packets[COUNT][256],outputs[COUNT][10],jump_memory[4096];
static tsfp_guest_objects objects;static tsfp_guest_handle_record handles[1];static tsfp_guest_object_record records[1];
static tsfp_guest_object_scopes scopes;static tsfp_guest_object_scope outer,extra;
static unsigned current,child_unwinds,escaped;
ptrdiff_t g_xbox_mem_offset;RECOMP_TLS uint32_t g_ebx,g_edi,g_esi,g_esp,g_ebp,g_seh_ebp;
static void put(unsigned char *p,uint32_t v) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(8*i));}
static uint32_t iosb_address(unsigned i) {return i==9?0:0x80000+16*i;}
static void lock(void *data) {context *c=data;CHECK(!c->depth);++c->depth;}
static void unlock(void *data) {context *c=data;CHECK(c->depth==1);--c->depth;}
static void dispose(void *data,uint32_t address,void *memory,size_t bytes) {
    context *c=data;CHECK(!c->depth && !c->disposed && address==0x2000+c->index*0x1000 && memory==packets[c->index] && bytes==256);++c->disposed;
}
static int close_host(void *data,uintptr_t host) {(void)data;(void)host;CHECK(0);return -1;}
static uint64_t capture(void) {return tsfp_guest_scope_mark(&scopes);}
static int restore(uint64_t mark) {return tsfp_guest_scope_unwind(&scopes,mark);}
static void mark_pending(void *data,tsfp_io_owner *owner) {CHECK(data==owner && tsfp_io_owner_pending(owner));}
static void init(unsigned i) {
    tsfp_io_owner_ops ops={&contexts[i],lock,unlock,dispose};context *c=&contexts[i];
    c->index=i;CHECK(tsfp_io_owner_init(&owners[i],&ops,packets[i],256,0x2000+0x1000*i,152,2,6));
    put(packets[i]+4,0x14);put(packets[i]+0x10,0xc0000022);put(packets[i]+0x14,0x11223344);
    put(packets[i]+0x1c,iosb_address(i));put(packets[i]+0x20,0x90000+16*i);
    put(packets[i]+0x28,0xaabbccdd);put(packets[i]+0x2c,0x12345678);
    put(packets[i]+0x50,0xa0000+16*i);put(packets[i]+0x60,0xb0000+16*i);
    put(packets[i]+0x30,0xc0000+16*i);put(packets[i]+0x34,0xd0000+16*i);put(packets[i]+0x38,4096);
    packets[i][0x1a]=1;packets[i][0x1b]=0x80;
    memset(outputs[i],0xa7,10);CHECK(tsfp_io_owner_dispatch(&owners[i],mark_pending,&owners[i]));
    CHECK(owners[i].clients==1 && owners[i].internal==1 && owners[i].phase==TSFP_IO_PENDING);
}
static void written(unsigned i) {
    static const unsigned char expected[8]={0x22,0,0,0xc0,0x44,0x33,0x22,0x11};
    CHECK(outputs[i][0]==0xa7 && outputs[i][9]==0xa7);
    if(i==9) {unsigned n;for(n=1;n<9;++n)CHECK(outputs[i][n]==0xa7);}
    else CHECK(!memcmp(outputs[i]+1,expected,8));
}
static void unchanged(unsigned i) {unsigned n;for(n=0;n<10;++n)CHECK(outputs[i][n]==0xa7);}
static int prepare(void *data,const tsfp_io_result *result,tsfp_io_delivery *delivery) {
    context *c=data;unsigned i=c->index;
    CHECK(!c->depth && !c->prepares && owners[i].phase==TSFP_IO_PENDING);++c->prepares;
    CHECK(result->status==0xc0000022 && result->information==0x11223344 && result->flags==0x14);
    CHECK(result->address==0x2000+0x1000*i && result->user_buffer==0xc0000+16*i && result->segment_array==0xd0000+16*i);
    CHECK(result->locked_buffer_length==4096 && result->pending_returned==1 && result->cancel==0x80);
    CHECK(result->user_iosb==iosb_address(i) && result->user_event==0x90000+16*i);
    CHECK(result->thread==0xa0000+16*i && result->original_file==0xb0000+16*i);
    CHECK(result->overlay_low==0xaabbccdd && result->overlay_high==0x12345678);
    unchanged(i);if(i==1)return 0;
    ++c->active;delivery->cookie=c;delivery->iosb=i==9?NULL:outputs[i]+1;delivery->iosb_bytes=i==2?7:(i==9?0:8);
    if(i==3)CHECK(tsfp_io_owner_complete(&owners[i],0x55,0x66,NULL,NULL));
    return 1;
}
static int child_cleanup(void *data,int abandoned) {CHECK(data==&extra && abandoned && !child_unwinds);++child_unwinds;return 1;}
static int notify(void *data,void *cookie,const tsfp_io_result *result) {
    context *c=data;unsigned i=c->index;CHECK(cookie==c && !c->depth && c->active==1 && !c->notifies);++c->notifies;
    CHECK(owners[i].phase==TSFP_IO_COMPLETING && owners[i].clients==1 && owners[i].internal==1);
    CHECK(result->status==0xc0000022 && result->information==0x11223344);written(i);
    if(i==5)return 0;
    if(i==6) {CHECK(tsfp_guest_scope_enter(&scopes,&extra,&objects));CHECK(tsfp_guest_scope_on_leave(&scopes,&extra,child_cleanup,&extra));}
    if(i==7) {(void)recomp_guest_longjmp(0x100,9);CHECK(0);}
    return 1;
}
static int release(void *data,void *cookie,int disposition) {
    context *c=data;unsigned i=c->index;CHECK(cookie==c && !c->depth && c->active==1 && !c->releases && !c->disposed);
    ++c->releases;--c->active;
    if(i==2 || i==3)CHECK(disposition==TSFP_IO_DELIVERY_PRIVATE && !c->notifies);
    else if(i==5 || i==6 || i==7)CHECK(disposition==TSFP_IO_DELIVERY_UNCERTAIN);
    else CHECK(disposition==TSFP_IO_DELIVERY_DONE);
    if(disposition==TSFP_IO_DELIVERY_UNCERTAIN || i==8)++c->quarantined;
    return i!=8;
}
int main(void) {
    tsfp_io_result value,before;unsigned char raw[104]={0};
    CHECK(tsfp_guest_objects_init(&objects,handles,records,1,close_host,NULL,NULL));
    CHECK(tsfp_guest_scope_enter(&scopes,&outer,&objects));g_xbox_mem_offset=(ptrdiff_t)jump_memory;
    CHECK(tsfp_guest_lifetime_hooks(capture,restore));
    raw[0]=6;raw[2]=104;memset(&value,0xdd,sizeof(value));memcpy(&before,&value,sizeof(value));
    CHECK(!tsfp_io_result_decode(raw,103,&value) && !memcmp(&value,&before,sizeof(value)));
    CHECK(!tsfp_io_result_decode(NULL,104,&value) && !tsfp_io_result_decode(raw,104,NULL));
    CHECK(tsfp_io_result_decode(raw,104,&value) && !value.status && !value.thread);
    memcpy(&before,&value,sizeof(value));raw[0]=7;CHECK(!tsfp_io_result_decode(raw,104,&value) && !memcmp(&value,&before,sizeof(value)));
    for(current=0;current<COUNT;++current) {
        tsfp_io_complete_ops ops={&contexts[current],prepare,notify,release};int result;
        context *c=&contexts[current];init(current);
        if(current==4)CHECK(tsfp_io_owner_complete(&owners[current],0,0,NULL,NULL));
        if(current==10)packets[current][0]=7;
        if(current==11) {packets[current][2]=1;packets[current][3]=1;}
        if(current==7) {
            MEM32(0x100)=0x1111;MEM32(0x104)=0x2222;MEM32(0x108)=0x3333;MEM32(0x10c)=0x4444;MEM32(0x110)=0x1400;
            switch(setjmp(*recomp_setjmp_slot(0x100))) {
            case 0:(void)tsfp_io_complete_request(&scopes,&objects,&owners[current],&ops);++escaped;CHECK(0);break;
            case 9:break;
            default:CHECK(0);
            }
            CHECK(!escaped && g_ebx==0x2222 && g_esp==0x1404);result=TSFP_IO_COMPLETE_UNCERTAIN;
        } else result=tsfp_io_complete_request(&scopes,&objects,&owners[current],&ops);
        CHECK(scopes.current==&outer && !owners[current].clients && !c->active);
        if(current==0 || current==9) {CHECK(result==TSFP_IO_COMPLETE_DONE && c->disposed==1 && c->notifies==1 && c->releases==1);written(current);}
        else if(current==5 || current==6 || current==7) {
            CHECK(result==TSFP_IO_COMPLETE_UNCERTAIN && !c->disposed && c->notifies==1 && c->releases==1 && c->quarantined==1);
            CHECK(owners[current].phase==TSFP_IO_UNCERTAIN && owners[current].internal==1);written(current);
        } else if(current==8) {CHECK(result==TSFP_IO_COMPLETE_UNCERTAIN && c->disposed==1 && c->notifies==1 && c->quarantined==1);written(current);}
        else {
            CHECK(result==TSFP_IO_COMPLETE_REJECTED && !c->notifies);unchanged(current);
            if(current==3 || current==4)CHECK(c->disposed==1);
            else {
                CHECK(!c->disposed && owners[current].phase==TSFP_IO_PENDING && owners[current].internal==1);
                packets[current][0]=6;packets[current][2]=152;packets[current][3]=0;
                CHECK(tsfp_io_owner_complete(&owners[current],0,0,NULL,NULL) && c->disposed==1);
            }
        }
    }
    CHECK(child_unwinds==1 && !escaped);CHECK(tsfp_guest_scope_leave(&scopes,&outer));CHECK(tsfp_guest_lifetime_hooks(NULL,NULL));
    puts("PASS: original completion publication, exact IOSB ordering, private failures, reentrant completion, native unwind and persistent uncertainty accounting");return 0;
}
