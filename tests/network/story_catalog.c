/* Exercise the production catalog reader and guest adapter with bounded files. */
#include <assert.h>
#include <wchar.h>
/* CATALOG_READER */
typedef int BOOL;
typedef unsigned long DWORD;
typedef wchar_t WCHAR;
#define MAX_PATH 260
static unsigned char memory[0x800000],archive[84];
static uintptr_t g_xbox_mem_offset;
static uint32_t g_esp,g_eax,g_ecx;
#define WORD_AT(a) (*(uint32_t *)(memory+(a)))
typedef struct {uint32_t sp,ax,cx;} GuestRegisters;
static void save_registers(GuestRegisters *s){*s=(GuestRegisters){g_esp,g_eax,g_ecx};}
static void restore_registers(const GuestRegisters *s){g_esp=s->sp;g_eax=s->ax;g_ecx=s->cx;}
static int guest_writable(uint32_t p,size_t n){return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p;}
static unsigned active=1,file_ok=1,alloc_ok=1,opens,allocations;
static int story_campaign_scene_active(void){return active;}
static BOOL xbox_translate_path(const char *path,WCHAR *out,DWORD size)
{assert(!strcmp(path,"D:\\pak\\arcade\\l_100.pak") && size==MAX_PATH);out[0]=0;return file_ok;}
static FILE *catalog_open(const WCHAR *path,const WCHAR *mode)
{(void)path;assert(!wcscmp(mode,L"rb"));++opens;FILE *f=tmpfile();assert(f);assert(fwrite(archive,1,sizeof archive,f)==sizeof archive);rewind(f);return f;}
#define _wfopen catalog_open
void sub_0003DFA0(void){}
static uint32_t guest_call(void (*fn)(void),unsigned count,uint32_t bytes,uint32_t tag)
{assert(fn==sub_0003DFA0 && count==2 && bytes==56 && tag==1);++allocations;g_esp-=99;g_eax=55;g_ecx=66;return alloc_ok?0x10000:0;}
/* CATALOG_ADAPTER */
static void original(void){g_eax=0x3000;g_ecx=42;WORD_AT(0x4000)=123;g_esp+=4;}
static void run(void)
{g_esp=0x2000;WORD_AT(g_esp+4)=0x49eff4;WORD_AT(g_esp+12)=0x4000;pc_story_network_catalog(original);assert(g_esp==0x2004 && g_ecx==42);}
int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;memory[0x7de400]=4;
    memcpy(archive,"P5CK",4);uint32_t index[2]={12,16},entry[4]={0xd9e2667a,28,56,0};
    memcpy(archive+4,index,8);memcpy(archive+12,entry,16);
    uint32_t head[2]={0x3f214482,8},record[10]={1595,0,0,2,0xffffffff,0,0,0,0x205,0};
    memcpy(archive+28,head,8);memcpy(archive+36,"K\0F\0",4);memcpy(archive+44,record,40);
    run();assert(g_eax==0x10000 && WORD_AT(0x4000)==56 && WORD_AT(0x10000)==head[0] && allocations==1);
    active=0;run();assert(g_eax==0x3000 && opens==1);active=1;
    memory[0x7de400]=1;run();assert(g_eax==0x3000 && opens==1);memory[0x7de400]=4;
    file_ok=0;run();assert(g_eax==0x3000 && WORD_AT(0x4000)==123);file_ok=1;
    alloc_ok=0;run();assert(g_eax==0x3000 && WORD_AT(0x4000)==123);alloc_ok=1;
    archive[24]=1;run();assert(g_eax==0x3000 && allocations==2);archive[24]=0;
    archive[60]=8;run();assert(g_eax==0x3000 && allocations==2);archive[60]=0xff;
    archive[4]=0xff;run();assert(g_eax==0x3000 && allocations==2);
    puts("PASS: complete catalog, native arena allocation, register/argument preservation and malformed-file fallback");return 0;
}
