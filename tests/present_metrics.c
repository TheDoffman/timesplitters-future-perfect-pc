/* Deterministic clock samples through the production recorder. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static const char *delay_value;
static int suppress_profile;
static unsigned delay_reads;
static char *metrics_getenv(const char *name) {
    if(!strcmp(name,"TSFP_FRAME_PROFILE_DELAY_MS")) {
        delay_reads++;return (char *)delay_value;
    }
    if(suppress_profile && (!strcmp(name,"TSFP_FRAME_PROFILE") ||
                            !strcmp(name,"TSFP_NATIVE_FRAME_PROFILE")))return NULL;
    return getenv(name);
}
#define getenv metrics_getenv
#include "graphics/present_metrics.h"
#undef getenv
#include <assert.h>
static void delayed_profile(void) {
    const char *values[]={NULL,"","0","1","600000","600001","-1","+1","1x",
                          " 5","9999999999999999999999999999"};
    const uint64_t expected[]={0,0,0,1000000,600000000000,0,0,0,0,0,0};
    for(unsigned i=0;i<sizeof values/sizeof *values;i++) {
        TsfpPresentMetrics m={0};delay_value=values[i];
        tsfp_present_record(&m,"test",1,0,2000000,3000000);
        assert(m.output && m.profile_delay_ns==expected[i]);
        fclose(m.output);
    }
    TsfpPresentMetrics off={0};suppress_profile=1;delay_reads=0;
    tsfp_present_record(&off,"test",1,0,2000000,3000000);
    assert(!off.output && !delay_reads);suppress_profile=0;

    TsfpPresentMetrics m={0};delay_value="5";
    tsfp_present_record(&m,"test",1,0,2000000,3000000);
    tsfp_present_record(&m,"test",1,0,4000000,5000000);
    assert(m.rows==0 && m.total==2 && m.last_ns==5000000);
    tsfp_present_record(&m,"test",1,1,5000000,6000000);
    tsfp_present_record(&m,"test",1,-1,6000000,7000000);
    tsfp_present_record(&m,"test",1,0,7000000,8000000);
    assert(m.rows==1 && m.total==3 && m.guest==3 && m.skipped==1 && m.failed==1);
    fflush(m.output);
    FILE *input=fopen(getenv("TSFP_FRAME_PROFILE"),"r");assert(input);
    char line[256];assert(fgets(line,sizeof line,input));
    assert(fgets(line,sizeof line,input));
    /* Boundary row retains only the last successful interval, including failed
     * and skipped attempts. The delayed period is not one artificial hitch. */
    assert(!strcmp(line,"3,5.000,3.000,1.000,guest,3,8000000\n"));
    assert(!fgets(line,sizeof line,input));fclose(input);
    m.rows=TSFP_PRESENT_PROFILE_LIMIT-1;
    tsfp_present_record(&m,"test",1,0,9000000,10000000);
    assert(!m.output && m.rows==TSFP_PRESENT_PROFILE_LIMIT);
    tsfp_present_record(&m,"test",1,0,11000000,12000000);
    assert(!m.output && m.rows==TSFP_PRESENT_PROFILE_LIMIT && m.guest==5);
    delay_value=NULL;
}
int main(void) {
    delayed_profile();
    TsfpPresentMetrics m={0};
    tsfp_present_record(&m,"test",1,1,0,1000000); /* skipped */
    tsfp_present_record(&m,"test",2,-1,1000000,2000000); /* failed */
    assert(!m.total && m.skipped==1 && m.failed==1 && !m.checked);
    tsfp_present_record(&m,"test",1,0,2000000,3000000);
    tsfp_present_record(&m,"test",2,1,4000000,5000000);
    tsfp_present_record(&m,"test",2,-1,6000000,7000000);
    tsfp_present_record(&m,"test",2,0,8000000,10000000);
    assert(m.total==2 && m.guest==1 && m.interpolated==1 && !m.other);
    assert(m.skipped==2 && m.failed==2 && m.last_ns==10000000 && m.rows==1);
    assert(m.output);fflush(m.output);
    /* Exercise cap/close without producing 20,000 log lines. */
    m.rows=TSFP_PRESENT_PROFILE_LIMIT-1;
    tsfp_present_record(&m,"test",0,0,11000000,12000000);
    assert(!m.output && m.rows==TSFP_PRESENT_PROFILE_LIMIT && m.other==1);
    tsfp_present_record(&m,"test",1,0,13000000,14000000);
    assert(!m.output && m.rows==TSFP_PRESENT_PROFILE_LIMIT && m.total==4);
    puts("PASS: successful presentation kinds, skipped/failed gaps and bounded profile");
    return 0;
}
