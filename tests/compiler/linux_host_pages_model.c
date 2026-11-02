/* Execute the Linux VMA parser with deterministic proc responses on any host. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#undef _WIN32
#undef __APPLE__
#ifndef __linux__
#define __linux__ 1
#endif
static const char *mapping_text;
static FILE *proc_maps(const char *path,const char *mode) {
    assert(!strcmp(path,"/proc/self/maps") && !strcmp(mode,"r"));
    if(!mapping_text)return NULL;
    FILE *file=tmpfile();assert(file);
    assert(fputs(mapping_text,file)>=0);rewind(file);return file;
}
#define fopen proc_maps
#include "host_pages.c"
#undef fopen
long sysconf(int name) { (void)name;return 4096; }
void *mmap(void *a,size_t b,int c,int d,int e,int64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return MAP_FAILED;
}
int munmap(void *a,size_t b) { (void)a;(void)b;return -1; }
int mprotect(void *a,size_t b,int c) { (void)a;(void)b;(void)c;return -1; }
int main(void) {
    const uint64_t base=UINT64_C(0x123400000);
    tsfp_host_page_region result,before;
    char text[256];
    const char *modes[]={"---p","r--p","rw-s","--xp","r-xp","rwxp"};
    const uint32_t expected[]={1,2,4,16,32,64};
    for(unsigned i=0;i<sizeof(expected)/sizeof(*expected);++i) {
        snprintf(text,sizeof text,"malformed\n123400000-123402000 %s 00000000 00:00 0\n",modes[i]);
        mapping_text=text;
        assert(tsfp_host_pages_query((void *)(uintptr_t)(base+19),&result));
        assert(result.base==base && result.bytes==8192 && result.protection==expected[i]);
        assert(tsfp_host_pages_query((void *)(uintptr_t)(base+8191),&result));
    }
    memset(&before,0x35,sizeof before);
    const char *failures[]={NULL,"", "malformed\n",
        "123402000-123404000 rw-p 00000000 00:00 0\n",
        "1233ff000-123400000 rw-p 00000000 00:00 0\n123402000-123404000 rw-p 00000000 00:00 0\n"};
    for(unsigned i=0;i<sizeof(failures)/sizeof(*failures);++i) {
        mapping_text=failures[i];result=before;
        assert(!tsfp_host_pages_query((void *)(uintptr_t)(base+19),&result));
        assert(!memcmp(&result,&before,sizeof result));
    }
    assert(!tsfp_host_pages_query(NULL,&result));
    assert(!tsfp_host_pages_query((void *)(uintptr_t)base,NULL));
    puts("Linux containing-VMA, permissions, holes and failure preservation passed");
    return 0;
}
