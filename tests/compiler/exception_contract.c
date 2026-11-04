/* Original guest-memory contracts; no host SEH structures or private game data. */
#include "guest_exception.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char ram[8192],before[8192];
static unsigned mode,calls,phase;
static uint32_t seen[4][5],denied;
static int recurse(void);
static uint32_t get(uint32_t at) {return (uint32_t)ram[at]|((uint32_t)ram[at+1]<<8)|((uint32_t)ram[at+2]<<16)|((uint32_t)ram[at+3]<<24);}
static void put(uint32_t at,uint32_t value){unsigned i;for(i=0;i<4;++i)ram[at+i]=(unsigned char)(value>>(8*i));}
static void *resolve(void *opaque,uint32_t at,uint32_t size,int writable) {
    (void)opaque;(void)writable;return at && at!=denied && (uint64_t)at+size<=sizeof ram?ram+at:NULL;
}
static int admitted(void *opaque,uint32_t pc){(void)opaque;return pc==0x500000 || (pc>=0x400000 && pc<=0x400002);}
static int invoke(void *opaque,uint32_t routine,uint32_t record,uint32_t frame,uint32_t context,uint32_t dispatcher,uint32_t *result) {
    uint32_t guard=get(0x100);unsigned index=calls++;(void)opaque;
    CHECK(index<4);
    if(record==0x300){
        CHECK(guard==0x780 && context==0x900 && dispatcher==0x740);
        if(routine==0x500000){
            CHECK(frame==0x800 && get(frame+8)==0x1000);
            put(dispatcher,get(frame+8));*result=TSFP_EXCEPTION_NESTED;
        }else{
            CHECK(routine==0x400000 && frame==0x1000 && get(record+4)==16);
            *result=TSFP_EXCEPTION_CONTINUE;
        }
        return 1;
    }
    CHECK(guard==0x800 && get(guard+4)==0x500000);
    CHECK(record==(phase==2?0x280u:0x200u) && context==0x400 && dispatcher==0x700);
    CHECK(get(guard+8)==frame && get(guard+12)==(uint32_t)(phase!=0));
    CHECK(routine==0x400000+(frame-0x1000)/0x100);
    CHECK(get(guard)==(phase?frame:0x1000));
    seen[index][0]=frame;seen[index][1]=get(record+4);seen[index][2]=get(context+176);
    seen[index][3]=get(context+184);seen[index][4]=get(record+12);
    if(mode&2){put(context+176,0x87654321);put(context+184,0x9009);}
    *result=phase?TSFP_EXCEPTION_SEARCH:((mode&1) && !index?TSFP_EXCEPTION_SEARCH:TSFP_EXCEPTION_CONTINUE);
    if(mode>=14 && mode<=16){
        put(context,mode==14?0x10001u:mode==15?0x10002u:0x10000u);
        put(context+176,42);put(context+184,0x9009);*result=TSFP_EXCEPTION_CONTINUE;
    }
    if(mode==13 && !index){CHECK(recurse());*result=TSFP_EXCEPTION_CONTINUE;}
    if(mode==4)*result=99;
    if(mode==5)return 0;
    if(mode==6){put(dispatcher,0x1200);*result=TSFP_EXCEPTION_NESTED;}
    if(mode==7 && !index){put(dispatcher,0x1100);*result=TSFP_EXCEPTION_COLLIDED;}
    if(mode==8)put(0x100,0x1200);
    if(mode==9){put(context,0);*result=TSFP_EXCEPTION_CONTINUE;}
    if(mode==11){
        if(!index){put(dispatcher,0x1200);*result=TSFP_EXCEPTION_NESTED;}
        else *result=index==2?TSFP_EXCEPTION_CONTINUE:TSFP_EXCEPTION_SEARCH;
    }
    if(mode==12)*result=TSFP_EXCEPTION_SEARCH;
    if(mode==10){put(frame,frame);*result=TSFP_EXCEPTION_SEARCH;}
    return 1;
}
static tsfp_exception_ops ops={NULL,resolve,admitted,invoke};
static tsfp_exception_request request;
static tsfp_x86_context_state initial,output,old_output;
static void reset(unsigned selected,unsigned unwind) {
    unsigned i;memset(ram,0xa5,sizeof ram);memset(&initial,0,sizeof initial);
    for(i=0;i<sizeof initial;++i)((unsigned char *)&initial)[i]=(unsigned char)(i*19+7);
    initial.r_eax=0xdeadbeef;initial.r_eip=0x9000;initial.r_esp=0xf00;initial.r_ebp=0xf10;
    initial.r_eflags=0x246;CHECK(tsfp_x86_context_encode(ram+0x400,716,&initial,0x1003f));
    memset(ram+0x200,0,80);put(0x200,0xe0424771);put(0x20c,0x1234);
    put(0x210,2);put(0x214,0x11223344);put(0x218,0x55667788);
    put(0x100,0x1000);
    for(i=0;i<3;++i){put(0x1000+0x100*i,i<2?0x1100+0x100*i:UINT32_MAX);put(0x1004+0x100*i,0x400000+i);}
    memset(&request,0,sizeof request);request.head=0x100;request.record=0x200;
    request.context=0x400;request.guard=0x800;request.dispatcher=0x700;request.guard_handler=0x500000;
    request.fallback_record=0x280;request.stack_low=0x780;request.stack_high=sizeof ram;
    request.baseline=&initial;
    denied=0;mode=selected;phase=unwind;calls=0;memset(seen,0,sizeof seen);
    memset(&output,0xce,sizeof output);old_output=output;
}
static int recurse(void) {
    tsfp_exception_request child=request;tsfp_x86_context_state resumed;
    child.record=0x300;child.context=0x900;child.guard=0x780;child.dispatcher=0x740;
    memset(ram+0x300,0,80);put(0x300,0xe0420002);
    CHECK(tsfp_x86_context_encode(ram+0x900,716,&initial,0x10007));
    CHECK(tsfp_exception_raise(&ops,&child,&resumed)==TSFP_EXCEPTION_CONTINUED);
    CHECK(get(0x100)==0x800 && get(0x304)==0 && resumed.r_eip==initial.r_eip);
    CHECK(!memcmp(resumed.x87,initial.x87,112) && !memcmp(resumed.extended,initial.extended,512));
    return 1;
}
static void ordinary_raise(void) {
    unsigned attempt;
    for(attempt=0;attempt<4;++attempt) {
        reset(attempt,0);
        CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_CONTINUED);
        CHECK(calls==((attempt&1)?2u:1u) && get(0x100)==0x1000);
        CHECK(output.r_eax==((attempt&2)?0x87654321u:0xdeadbeefu));
        CHECK(output.r_eip==((attempt&2)?0x9009u:0x9000u));
        CHECK(output.r_esp==initial.r_esp && output.r_ebp==initial.r_ebp);
        CHECK(!memcmp(output.x87,initial.x87,112) && !memcmp(output.extended,initial.extended,512));
        CHECK(get(0x20c)==0x9000 && get(0x214)==0x11223344 && get(0x218)==0x55667788);
        CHECK(seen[0][2]==0xdeadbeef && seen[0][3]==0x9000 && seen[0][4]==0x9000);
        if(attempt&1)CHECK(seen[1][2]==((attempt&2)?0x87654321u:0xdeadbeefu));
    }
}
static void ordinary_unwind(void) {
    unsigned attempt,i;
    for(attempt=0;attempt<16;++attempt) {
        uint32_t target=(attempt&1)?0x1200:0x1100;
        reset((attempt&8)?2:0,(attempt&4)?1:2);
        put(0x204,1);if(!(attempt&4))request.record=0;
        CHECK(tsfp_exception_unwind(&ops,&request,target,0x12345678,&output)==TSFP_EXCEPTION_UNWOUND);
        CHECK(get(0x100)==target && calls==((attempt&1)?2u:1u));
        CHECK(output.r_eax==((attempt&8)?0x87654321u:0x12345678u));
        CHECK(output.r_eip==((attempt&8)?0x9009u:0x9000u));
        CHECK(get(request.record?0x200:0x280)==((attempt&4)?0xe0424771u:0xc0000027u));
        CHECK(get((request.record?0x200:0x280)+4)==((attempt&4)?3u:2u));
        for(i=0;i<calls;++i)CHECK(seen[i][0]==0x1000+i*0x100 && seen[i][1]==((attempt&4)?3u:2u));
        CHECK(seen[0][2]==0x12345678);
        if((attempt&1) && (attempt&8))CHECK(seen[1][2]==0x87654321);
    }
    reset(0,1);CHECK(tsfp_exception_unwind(&ops,&request,0,7,&output)==TSFP_EXCEPTION_UNWOUND);
    CHECK(calls==3 && get(0x100)==UINT32_MAX && get(0x204)==6 && output.r_eax==7);
    reset(0,1);CHECK(tsfp_exception_unwind(&ops,&request,0x1000,8,&output)==TSFP_EXCEPTION_UNWOUND);
    CHECK(!calls && get(0x100)==0x1000 && output.r_eax==8);
}
static void invalid_inputs(void) {
    unsigned attempt;
    for(attempt=0;attempt<5;++attempt){
        static const uint32_t denied_addresses[]={0x100,0x200,0x400,0x800,0x700};
        reset(0,0);denied=denied_addresses[attempt];memcpy(before,ram,sizeof ram);
        CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_BAD_ARGUMENT);
        CHECK(!memcmp(before,ram,sizeof ram) && !memcmp(&output,&old_output,sizeof output));
    }
    reset(0,0);CHECK(tsfp_exception_raise(NULL,&request,&output)==0);
    CHECK(tsfp_exception_raise(&ops,NULL,&output)==0);
    CHECK(tsfp_exception_raise(&ops,&request,NULL)==0);
    for(attempt=0;attempt<12;++attempt) {
        reset(0,0);
        switch(attempt){
        case 0:request.context=0;break;
        case 1:request.guard=request.context;break;
        case 2:request.head=request.record+1;break;
        case 3:request.stack_high=UINT64_C(0x100000001);break;
        case 4:request.guard=0x801;break;
        case 5:request.guard_handler=0x600000;break;
        case 6:request.record=0;break;
        case 7:put(0x400,0x10002);break;
        case 8:put(0x400+196,UINT32_MAX);break;
        case 9:put(0x210,16);break;
        case 10:request.context=0xffffff00;break;
        case 11:request.stack_low=request.stack_high;break;
        }
        memcpy(before,ram,sizeof ram);
        CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_BAD_ARGUMENT);
        CHECK(!memcmp(before,ram,sizeof ram) && !memcmp(&output,&old_output,sizeof output) && !calls);
    }
}
static void failure_and_nested_paths(void) {
    reset(0,0);put(0x204,1);
    CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_CANNOT_CONTINUE);
    CHECK(get(0x100)==0x1000 && !memcmp(&output,&old_output,sizeof output));
    reset(4,0);CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_BAD_DISPOSITION);
    reset(5,0);CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_CALLBACK_FAILED);CHECK(get(0x100)==0x1000);
    reset(8,0);CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_BAD_CHAIN);CHECK(get(0x100)==0x1200);
    reset(9,0);CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_BAD_CONTEXT);CHECK(!memcmp(&output,&old_output,sizeof output));
    reset(10,0);CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_BAD_CHAIN);CHECK(calls==1 && (get(0x204)&8));
    reset(0,0);put(0x100,0x1001);CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_BAD_CHAIN);CHECK(get(0x204)&8);
    reset(0,0);put(0x1004,0x600000);CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_BAD_HANDLER);CHECK(!calls);
    reset(1,0);put(0x1100,UINT32_MAX);
    /* Both handlers search; the third mode value selects search unconditionally. */
    mode=6;CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_UNHANDLED);
    CHECK(get(0x204)&TSFP_EXCEPTION_NESTED_CALL);
    reset(11,0);CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_CONTINUED);
    CHECK(calls==3 && seen[0][1]==0 && seen[1][1]==16 && seen[2][1]==16 && get(0x204)==0);
    reset(12,0);CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_UNHANDLED);
    CHECK(calls==3 && get(0x100)==0x1000 && !memcmp(&output,&old_output,sizeof output));
    reset(14,0);CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_CONTINUED);
    CHECK(output.r_eax==initial.r_eax && output.r_eip==0x9009);
    CHECK(!memcmp(output.x87,initial.x87,112));
    reset(15,0);CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_CONTINUED);
    CHECK(output.r_eax==42 && output.r_eip==initial.r_eip);
    reset(16,0);CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_CONTINUED);
    CHECK(!memcmp(&output,&initial,sizeof initial));
    reset(13,0);CHECK(tsfp_exception_raise(&ops,&request,&output)==TSFP_EXCEPTION_CONTINUED);
    CHECK(calls==3 && get(0x100)==0x1000);
    reset(7,1);CHECK(tsfp_exception_unwind(&ops,&request,0x1200,42,&output)==TSFP_EXCEPTION_UNWOUND);
    CHECK(calls==1 && get(0x100)==0x1200 && get(0x204)==2);
    reset(0,1);CHECK(tsfp_exception_unwind(&ops,&request,0x1001,42,&output)==TSFP_EXCEPTION_BAD_TARGET);CHECK(!calls);
}
int main(void) {
    ordinary_raise();ordinary_unwind();invalid_inputs();failure_and_nested_paths();
    puts("Original raise/unwind chain, guard, context, target and rejection contracts passed");return 0;
}
