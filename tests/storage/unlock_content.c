#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#define RECOMP_TLS
static unsigned char memory[0x900000];
static uintptr_t g_xbox_mem_offset=(uintptr_t)memory;
static uint32_t g_esp=0x800000,g_eax,active_profile=0x7bedc0;
#define WORD_AT(a) (*(uint32_t *)(memory+(uint32_t)(a)))
typedef struct GuestRegisters {uint32_t sp,a;} GuestRegisters;
static void save_registers(GuestRegisters *r){r->sp=g_esp;r->a=g_eax;}
static void restore_registers(const GuestRegisters *r){g_esp=r->sp;g_eax=r->a;}
static int guest_writable(uint32_t a,size_t n){return a && a<sizeof memory && n<=sizeof memory-a;}
static uint32_t bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
static uint32_t heap=0x810000;
uint32_t xbox_HeapAlloc(uint32_t n,uint32_t alignment){uint32_t a=heap;(void)alignment;heap+=(n+3)&~3u;return a;}
#define STUB(n) void n(void){}
STUB(sub_00356870) STUB(sub_000696F0) STUB(sub_00069720)
STUB(sub_001CD120) STUB(sub_001CD040) STUB(sub_001922D0)
STUB(sub_0003D7B0) STUB(sub_00075E70) STUB(sub_00075B70)
STUB(sub_00071B10) STUB(sub_00071C50) STUB(sub_00079D60_gen)
STUB(sub_000771D0) STUB(sub_0007D480) STUB(sub_000717D0)
STUB(sub_00075430) STUB(sub_00079B70)
static uint32_t row_count,callbacks[32],row_ids[32],last_row,saves,transition;
static uint32_t expected_parent=0x2000,expected_previous=0x524578;
static uint32_t guest_call(void (*fn)(void),unsigned count,...);
#include "pc_unlock_menu.inc"
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    uint32_t a[8]={0};va_list ap;va_start(ap,count);
    assert(count<=8);for(unsigned i=0;i<count;i++)a[i]=va_arg(ap,uint32_t);va_end(ap);
    if(fn==sub_00356870)return active_profile?0:UINT32_MAX;
    if(fn==sub_000696F0)return active_profile;
    if(fn==sub_0003D7B0)return a[0];
    if(fn==sub_001CD120)return (uc_flags()&(1u<<PC_UNLOCK_CHEATS))?12:0;
    if(fn==sub_001CD040)return 4; /* Only this cheat was earned. */
    if(fn==sub_00079D60_gen) {
        assert(row_count<32);callbacks[row_count]=a[4];row_ids[row_count++]=a[1];
        last_row=xbox_HeapAlloc(0x110,4);WORD_AT(last_row+0x28)=a[2];return last_row;
    }
    if(fn==sub_00075430){WORD_AT(a[0]+0xc8)=a[1];return 0;}
    if(fn==sub_00079B70){assert(a[0]==expected_parent);transition=a[1];return 0;}
    if(fn==sub_001922D0) {
        assert(a[0]==expected_parent && a[1]==expected_previous && a[2]==UINT32_MAX);
        saves++;return 0;
    }
    return 0;
}
static unsigned originals;
static void locked(void){originals++;g_eax=0;g_esp+=4;}
static void earned(void){originals++;g_eax=1;g_esp+=4;}
static void initialized(void){g_eax=active_profile;g_esp+=4;}
static void call(void (*fn)(void),uint32_t menu)
{
    g_esp=0x800000;WORD_AT(g_esp)=0x777777;WORD_AT(g_esp+4)=menu;
    fn();assert(g_esp==0x800004);
}
static uint32_t check(uint32_t caller,uint32_t descriptor,void (*original)(void))
{
    g_esp=0x800000;WORD_AT(g_esp)=caller;WORD_AT(g_esp+4)=descriptor;
    pc_unlock_requirement(original);assert(g_esp==0x800004);return g_eax;
}
int main(void)
{
    unsigned char untouched[PC_UNLOCK_PROFILE_SIZE],saved[PC_UNLOCK_PROFILE_SIZE];
    uint32_t flags;
    WORD_AT(active_profile)=0x4000;WORD_AT(0x4024)=0;
    memset(untouched,0,sizeof untouched);memcpy(untouched,memory+active_profile,sizeof untouched);
    assert(pc_unlock_decode(memory+active_profile,&flags) && !flags);
    WORD_AT(0x3000)=2; /* A valid authored requirement. */
    const uint32_t callers[]={0x1cd8da,0x1cd9bb,0x1cda18,0x1cd916,0x1cd9e4,
                             0x1cda5b,0x1cda5b,0x1cd974,0};
    const uint32_t descs[]={0x3000,0x3000,0x3000,0x3000,0x3000,0x513b84,0x510804,0x3000};
    WORD_AT(0x513b84)=WORD_AT(0x510804)=2;
    for(unsigned category=0;category<PC_UNLOCK_MOVIES;category++) {
        assert(pc_unlock_set(memory+active_profile,category,1));
        for(unsigned query=0;query<PC_UNLOCK_MOVIES;query++)
            assert(check(callers[query],descs[query],locked)==(uint32_t)(query==category));
        assert(check(0xabcdef,0x3000,locked)==0);
        WORD_AT(0x3000)=0;assert(check(callers[category],0x3000,locked)==0);WORD_AT(0x3000)=2;
        assert(pc_unlock_set(memory+active_profile,category,0));
        assert(check(callers[category],descs[category],locked)==0);
        assert(check(callers[category],descs[category],earned)==1);
    }
    assert(pc_unlock_set(memory+active_profile,PC_UNLOCK_CHALLENGES,1));
    g_esp=0x800000;WORD_AT(g_esp+4)=20;WORD_AT(g_esp+8)=6;pc_unlock_event(locked);
    assert(g_eax==1 && g_esp==0x800004);
    g_esp=0x800000;WORD_AT(g_esp+4)=21;WORD_AT(g_esp+8)=7;pc_unlock_event(locked);
    assert(!g_eax); /* Challenge access never grants League access. */
    assert(pc_unlock_set(memory+active_profile,PC_UNLOCK_MOVIES,1));
    for(unsigned query=0;query<3;query++) {
        g_esp=0x800000;WORD_AT(g_esp)=query?0x2cc17c:0x123456;WORD_AT(g_esp+4)=query==2?20:5;
        pc_unlock_movie(locked);assert(g_eax==(uint32_t)(query==1));assert(g_esp==0x800004);
    }
    /* Progress made while an override is active survives disabling, saving,
     * restart and every other category. Other profiles are byte-identical. */
    WORD_AT(active_profile+0x1290+0xbf4)=0x21;
    for(unsigned i=0;i<PC_UNLOCK_COUNT;i++)assert(pc_unlock_set(memory+active_profile,i,1));
    memcpy(saved,memory+active_profile,sizeof saved);
    assert(pc_unlock_decode(saved,&flags) && flags==PC_UNLOCK_ALL);
    for(unsigned i=0;i<PC_UNLOCK_COUNT;i++)assert(pc_unlock_set(saved,i,0));
    assert(*(uint32_t *)(saved+0x1290+0xbf4)==0x21);
    memcpy(saved+PC_UNLOCK_OFFSET,untouched+PC_UNLOCK_OFFSET,16);
    memcpy(saved+0x1290+0xbf4,untouched+0x1290+0xbf4,4);
    assert(!memcmp(saved,untouched,sizeof saved));
    active_profile+=PC_UNLOCK_PROFILE_SIZE;WORD_AT(active_profile)=0x4100;
    assert(!uc_flags());assert(!check(0x1cd8da,0x3000,locked));
    active_profile-=PC_UNLOCK_PROFILE_SIZE;assert(uc_flags()==PC_UNLOCK_ALL);
    /* Actual Extras/category callbacks, including ABI, row ownership and save. */
    WORD_AT(0x2000+0x2c)=expected_parent;WORD_AT(0x2000+4)=expected_previous;
    row_count=0;call(uc_extras_build,0x2000);
    assert(row_count==4 && callbacks[0]==0x2d08a0 && callbacks[1]==0x2d0880);
    assert(callbacks[2]==UC_OPEN && callbacks[3]==0x2cbe30);
    call(uc_open,0x2000);assert(transition==uc_storage);
    row_count=0;call(uc_build,0x2000);assert(row_count==PC_UNLOCK_COUNT);
    for(unsigned i=0;i<PC_UNLOCK_COUNT;i++)assert(!callbacks[i] && row_ids[i]==i && WORD_AT(uc_rows[i]+0xc8)==1);
    WORD_AT(0x790954)=0x1fff;
    WORD_AT(uc_rows[PC_UNLOCK_CHEATS]+0xc8)=0;call(uc_tick,0x2000);
    assert(!(uc_flags()&(1u<<PC_UNLOCK_CHEATS)) && WORD_AT(0x790954)==4 && (WORD_AT(0x78ad24)&0x10000));
    uint32_t before=uc_flags();active_profile+=PC_UNLOCK_PROFILE_SIZE;
    WORD_AT(uc_rows[PC_UNLOCK_STORY]+0xc8)=0;call(uc_tick,0x2000);
    assert(!uc_flags());active_profile-=PC_UNLOCK_PROFILE_SIZE;assert(uc_flags()==before);
    call(uc_back,0x2000);assert(saves==1 && !uc_rows[0]);
    row_count=0;call(uc_extras_build,0x2000);assert(row_count==3);
    /* Known-new initialization clears stale metadata on slot reuse only. */
    g_esp=0x800000;pc_unlock_profile_init(initialized);assert(uc_flags());
    WORD_AT(0x4024)=UINT32_MAX;g_esp=0x800000;pc_unlock_profile_init(initialized);assert(!uc_flags());
    /* Unknown metadata is never overwritten or silently activated. */
    WORD_AT(active_profile+PC_UNLOCK_OFFSET)=0xdeadbeef;
    memcpy(saved,memory+active_profile,sizeof saved);
    assert(!pc_unlock_decode(saved,&flags) && !flags);
    assert(!pc_unlock_set(memory+active_profile,0,1));assert(!memcmp(saved,memory+active_profile,sizeof saved));
    active_profile=0;assert(!uc_flags());
    puts("PASS: category isolation, original access fallback, progress preservation, profile switching, menu ABI and save routing");
}
