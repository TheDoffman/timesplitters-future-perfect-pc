#define main one_contract_main
#include "guest_callback_call_contract.c"
#undef main
static unsigned nargs;static uint32_t expected_sp;static unsigned calls;
static void driver_callback(void) {
    unsigned i;uint32_t sp=g_esp;
    CHECK(sp==expected_sp && MEM32(sp)==0 && g_seh_ebp==sp);++calls;
    for(i=0;i<nargs;++i)CHECK(MEM32(sp+4+4*i)==0x100+i);
    seed(99);
    if(jumping) {(void)recomp_guest_longjmp(0x100,7);CHECK(0);}
    g_eax=0x87654321;g_esp=sp+4+4*nargs-(malformed?4:0);
}
static tsfp_guest_entry_t driver_lookup(void *ctx,uint32_t routine) {
    CHECK(ctx==ram);return routine==0x5555?driver_callback:NULL;
}
static void *driver_output(void *ctx,uint32_t address,uint32_t bytes) {
    CHECK(ctx==ram && bytes==4+4*nargs);return resolve_fail || address>sizeof(ram)-bytes?NULL:ram+address;
}
static unsigned admissions,allow_admission;
static int admit(void *context) {
    CHECK(context==&admissions && g_esp==0x1007 && MEM32(0xffb)==0xa7a7a7a7);
    ++admissions;return (int)allow_admission;
}
int main(void) {
    tsfp_guest_callback_ops ops={ram,driver_lookup,driver_output};
    uint32_t args[4]={0x100,0x101,0x102,0x103},result;unsigned i,old_calls;
    CHECK(one_contract_main()==0);jumping=0;
    for(nargs=0;nargs<=4;++nargs) {
        memset(ram,0xa7,sizeof(ram));seed(3);g_esp=0x1007;result=0xfeedface;
        expected_sp=g_esp-4-4*nargs;old_calls=calls;
        resolve_fail=1;CHECK(!tsfp_guest_callback_stdcall(&ops,0x5555,args,nargs,&result));resolve_fail=0;
        CHECK(calls==old_calls && result==0xfeedface && g_esp==0x1007);check_seed();
        for(i=0;i<sizeof(ram);++i)CHECK(ram[i]==0xa7);
        CHECK(tsfp_guest_callback_stdcall(&ops,0x5555,nargs?args:NULL,nargs,&result));
        CHECK(calls==old_calls+1 && result==0x87654321 && g_esp==0x1007);check_seed();
        CHECK(ram[expected_sp-1]==0xa7 && ram[0x1007]==0xa7);
        result=0xfeedface;malformed=1;
        CHECK(!tsfp_guest_callback_stdcall(&ops,0x5555,args,nargs,&result));malformed=0;
        CHECK(result==0xfeedface && g_esp==0x1007);check_seed();
        g_esp=3;CHECK(!tsfp_guest_callback_stdcall(&ops,0x5555,args,nargs,&result));CHECK(g_esp==3);
    }
    nargs=2;seed(3);g_esp=0x1007;expected_sp=g_esp-12;old_calls=calls;result=0xfeedface;
    CHECK(!tsfp_guest_callback_stdcall(NULL,0x5555,args,2,&result));
    CHECK(!tsfp_guest_callback_stdcall(&ops,0x5555,args,5,&result));
    CHECK(!tsfp_guest_callback_stdcall(&ops,0x5555,NULL,2,&result));
    CHECK(!tsfp_guest_callback_stdcall(&ops,0x7777,args,2,&result));
    CHECK(!tsfp_guest_callback_stdcall(&ops,0,args,2,&result));
    CHECK(calls==old_calls && result==0xfeedface);check_seed();
    CHECK(tsfp_guest_callback_stdcall(&ops,0x5555,args,2,NULL));
    expected_sp=0x1007-12;jumping=1;result=0xfeedface;
    MEM32(0x100)=0x1122;MEM32(0x104)=0x3344;MEM32(0x108)=0x5566;MEM32(0x10c)=0x7788;MEM32(0x110)=0x9900;
    switch(setjmp(*recomp_setjmp_slot(0x100))) {
    case 0:(void)tsfp_guest_callback_stdcall(&ops,0x5555,args,2,&result);CHECK(0);break;
    case 7:break;
    default:CHECK(0);
    }
    CHECK(result==0xfeedface && g_ebp==0x1122 && g_ebx==0x3344 && g_edi==0x5566 && g_esi==0x7788 && g_esp==0x9904);
    {
        tsfp_guest_callback_plan plan,before_plan;unsigned char before_ram[sizeof(ram)];
        jumping=0;malformed=0;nargs=2;seed(3);g_esp=0x1007;expected_sp=g_esp-12;
        memset(ram,0xa7,sizeof(ram));memcpy(before_ram,ram,sizeof(ram));result=0xfeedface;old_calls=calls;
        memset(&plan,0xdd,sizeof(plan));before_plan=plan;
        CHECK(!tsfp_guest_callback_prepare(&ops,0x5555,args,5,&plan));CHECK(!memcmp(&plan,&before_plan,sizeof(plan)));
        CHECK(tsfp_guest_callback_prepare(&ops,0x5555,args,2,&plan));
        CHECK(!memcmp(before_ram,ram,sizeof(ram)) && calls==old_calls && result==0xfeedface);check_seed();
        CHECK(!tsfp_guest_callback_invoke(&plan,admit,&admissions,&result));
        CHECK(admissions==1 && !plan.entry && calls==old_calls && !memcmp(before_ram,ram,sizeof(ram)) && result==0xfeedface);check_seed();
        CHECK(!tsfp_guest_callback_invoke(&plan,admit,&admissions,&result) && admissions==1);
        CHECK(tsfp_guest_callback_prepare(&ops,0x5555,args,2,&plan));allow_admission=1;
        CHECK(tsfp_guest_callback_invoke(&plan,admit,&admissions,&result));
        CHECK(admissions==2 && !plan.entry && calls==old_calls+1 && result==0x87654321 && g_esp==0x1007);check_seed();
        CHECK(!tsfp_guest_callback_invoke(&plan,NULL,NULL,&result));
        CHECK(tsfp_guest_callback_prepare(&ops,0x5555,args,2,&plan));g_esp=0x1003;
        CHECK(!tsfp_guest_callback_invoke(&plan,admit,&admissions,&result));CHECK(!plan.entry && admissions==2 && g_esp==0x1003);
    }
    puts("PASS: bounded 0..4 argument stdcall, guest EAX result, invalid spans, malformed cleanup, native jump and full state restoration");return 0;
}
