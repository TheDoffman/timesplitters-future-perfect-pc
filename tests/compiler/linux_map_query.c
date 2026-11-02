/* Kernel responses are controlled; the production guard must re-query every call. */
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#undef _WIN32
#undef __APPLE__
#define __linux__ 1
#define open test_open
#define close test_close
#define ioctl test_ioctl
static int opened,closed,queried,text_reads,query_error;
static uint64_t map_flags=3,start=0x1000,end=0x2000;
static FILE *test_maps(const char *name,const char *mode) {
    assert(!strcmp(name,"/proc/self/maps")&&!strcmp(mode,"r"));text_reads++;
    FILE *f=tmpfile();assert(f);fputs("1000-2000 r--p 00000000 00:00 0\n",f);rewind(f);return f;
}
#define fopen test_maps
#include "pc_guest_memory.h"
int test_open(const char *name,int flags,...) {
    assert(!strcmp(name,"/proc/self/maps")&&flags==O_RDONLY);opened++;return 42;
}
int test_close(int fd) {assert(fd==42);closed++;return 0;}
int test_ioctl(int fd,unsigned long command,...) {
    va_list args;va_start(args,command);PcLinuxMapQuery *q=va_arg(args,PcLinuxMapQuery *);va_end(args);
    assert(fd==42&&command==0xc0686611ul&&sizeof(*q)==104);
    assert(q->size==104&&q->address==0x1010&&!q->flags&&!q->name_size&&!q->build_id_size);
    queried++;if(query_error){errno=query_error;return -1;}
    q->start=start;q->end=end;q->permissions=map_flags;return 0;
}
int main(void) {
    for(unsigned i=0;i<8;i++) {
        map_flags=i;PcGuestRegion r=pc_guest_query_region(0x1010);
        assert(r.size==((i&1)?0x1000u:0u));assert(r.writable==((i&3)==3));
    }
    assert(!text_reads&&opened==8&&queried==8&&closed==8);
    /* A changed kernel permission must be observed without stale snapshots. */
    map_flags=3;assert(pc_guest_memory(0x1010,4,1));
    map_flags=1;assert(!pc_guest_memory(0x1010,4,1)&&pc_guest_memory(0x1010,4,0));
    map_flags=0;assert(!pc_guest_memory(0x1010,4,0));
    map_flags=3;start=0x1100;assert(!pc_guest_memory(0x1010,4,0));start=0x1000;
    end=0x1010;assert(!pc_guest_memory(0x1010,4,0));end=0x2000;
    query_error=ENOENT;assert(!pc_guest_memory(0x1010,4,0));
    query_error=EACCES;assert(!pc_guest_memory(0x1010,4,0));assert(!text_reads);
    query_error=ENOTTY;assert(pc_guest_memory(0x1010,4,0)&&!pc_guest_memory(0x1010,4,1));
    assert(text_reads==2&&opened==closed&&queried==opened);
    puts("PASS: live Linux binary VMA query, permission changes, holes and old-kernel fallback");return 0;
}
