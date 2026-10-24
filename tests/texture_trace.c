/* Exercise disabled, failed-open and bounded collector paths without assets. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif
static unsigned opens,clocks;
static FILE *counted_open(const char *p,const char *mode){opens++;return fopen(p,mode);}
#ifdef _WIN32
static BOOL counted_clock(LARGE_INTEGER *n){clocks++;return QueryPerformanceCounter(n);}
#define QueryPerformanceCounter counted_clock
#else
static int counted_clock(clockid_t id,struct timespec *n){clocks++;return clock_gettime(id,n);}
#define clock_gettime counted_clock
#endif
#define fopen counted_open
#include "texture_pack_trace.h"
#undef fopen
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv){
    CHECK(argc==2);
    int enabled=!strcmp(argv[1],"enabled"),failed=!strcmp(argv[1],"unwritable");
    for(unsigned i=0;i<TSFP_TEXTURE_TRACE_ROWS+10;i++){
        TsfpTextureTrace t=tsfp_texture_trace_begin(i,4,4,8,"fixture");
        TSFP_TEXTURE_STAMP(&t,hash_begin);TSFP_TEXTURE_STAMP(&t,hash_end);
        TSFP_TEXTURE_STAMP(&t,read_begin);TSFP_TEXTURE_STAMP(&t,open_end);
        TSFP_TEXTURE_STAMP(&t,read_end);TSFP_TEXTURE_STAMP(&t,validated);
        TSFP_TEXTURE_STAMP(&t,image_begin);TSFP_TEXTURE_STAMP(&t,image_end);
        TSFP_TEXTURE_STAMP(&t,view_begin);TSFP_TEXTURE_STAMP(&t,view_end);
        tsfp_texture_trace_finish(&t,"loaded",24);
        CHECK(t.active==(enabled && i<TSFP_TEXTURE_TRACE_ROWS));
    }
    CHECK(opens==(unsigned)(enabled||failed));
    CHECK(enabled?clocks>0:clocks==0);
    CHECK(tsfp_texture_trace_state.file==NULL);
    CHECK(tsfp_texture_trace_state.rows==(enabled?TSFP_TEXTURE_TRACE_ROWS:0));
    unsigned before=clocks;
    TsfpTextureTrace t=tsfp_texture_trace_begin(0,4,4,8,"fixture");
    tsfp_texture_trace_finish(&t,"loaded",24);
    CHECK(clocks==before);puts("PASS: disabled/no clock or file work, failed open, row/byte bounds and closed collector");return 0;
}
