/* Compile the production filter with observable name scans. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: %s\n",#x);return 1;}}while(0)
static unsigned scans;
static size_t counted_strlen(const char *name){++scans;return strlen(name);}
#define strlen counted_strlen
/* PRODUCTION_TRACE_FILTER */
#undef strlen
int main(int argc,char **argv)
{
    CHECK(argc==2);
    if(!strcmp(argv[1],"disabled")) {
        for(unsigned i=0;i<10000;i++)CHECK(!trace_only_match("sub_00151E80"));
        CHECK(scans==0);
    } else if(!strcmp(argv[1],"limited")) {
        CHECK(!trace_only_match("alph"));CHECK(!trace_only_match("alphax"));
        CHECK(!trace_only_match("ALPHA"));CHECK(!trace_only_match("other"));
        CHECK(trace_only_match("alpha"));CHECK(trace_only_match("beta"));
        CHECK(trace_only_match("beta"));CHECK(!trace_only_match("beta"));
        CHECK(trace_only_match("alpha"));CHECK(!trace_only_match("alpha"));
        CHECK(scans>0);
    } else if(!strcmp(argv[1],"zero")) {
        CHECK(!trace_only_match("alpha"));CHECK(!trace_only_match("beta"));
    } else return 2;
    puts("PASS: disabled scan bypass; exact-name matching and per-filter hit limits");
    return 0;
}
