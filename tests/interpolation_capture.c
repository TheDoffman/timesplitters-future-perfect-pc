/* Drive the production capture/redraw ordering and failure metadata. */
#define main present_contract_main
#include "interpolation_present.c"
#undef main
static void environment(const char *name,const char *value){
#ifdef _WIN32
    _putenv_s(name,value);
#else
    if(*value)setenv(name,value,1);else unsetenv(name);
#endif
}
static void request(const char *text){FILE *f=fopen("request.txt","w");assert(f);fputs(text,f);assert(!fclose(f));operation_count=0;operations[0]=0;}
static void metadata(const char *expected){char data[1024]={0};FILE *f=fopen("pair-pair.json","r");assert(f);assert(fread(data,1,sizeof data-1,f)>0);fclose(f);assert(strstr(data,expected));}
int main(void){
    environment("RECOMP_INTERP_CAPTURE_REQUEST","request.txt");
    environment("RECOMP_INTERP_CAPTURE_PREFIX","pair");
    g_on=1;hle_d3d8_interp_rec=1;g_swaps=41;dump_result=0;
    request("0");assert(redraw(.5));assert(!strcmp(operations,"BOPE"));
    /* CPU prediction starts from a partial guest image: defer the request
     * until a complete guest/intermediate pair is available. */
    hle_d3d8_interp_predicting=1;
    request("1");assert(redraw(1.1));assert(!strcmp(operations,"BOPE"));
    FILE *pending=fopen("request.txt","r");assert(pending && fgetc(pending)=='1');fclose(pending);
    hle_d3d8_interp_predicting=0;
    request("1");assert(redraw(.5));assert(!strcmp(operations,"GBMOPE"));
    metadata("\"complete\":true");metadata("\"guest_frame\":42");metadata("\"blend\":0.500000000");
    FILE *f=fopen("request.txt","r");assert(f && fgetc(f)=='0');fclose(f);
    operation_count=0;operations[0]=0;assert(redraw(.5));assert(!strcmp(operations,"BOPE"));
    /* A failed begin must invalidate old evidence and never claim a midpoint. */
    f=fopen("pair-midpoint.bmp","w");assert(f);fputs("stale",f);fclose(f);
    begin_ok=0;request("1");assert(!redraw(.5));assert(!strcmp(operations,"GB"));
    metadata("\"complete\":false");metadata("\"midpoint_written\":false");
    f=fopen("pair-midpoint.bmp","r");assert(!f);
    begin_ok=1;present_ok=0;request("1");assert(!redraw(.5));assert(!strcmp(operations,"GBMOPE"));
    metadata("\"presented\":false");metadata("\"complete\":false");
    present_ok=1;dump_result=-1;request("1");assert(redraw(.5));metadata("\"complete\":false");
    metadata("\"guest_written\":false");
    /* Legacy environment names route through the same implementation. */
    environment("RECOMP_INTERP_CAPTURE_REQUEST","");environment("RECOMP_INTERP_CAPTURE_PREFIX","");
    environment("TSFP_NATIVE_INTERP_CAPTURE_REQUEST","request.txt");environment("TSFP_NATIVE_INTERP_CAPTURE_PREFIX","pair");
    dump_result=0;request("1");assert(redraw(.25));metadata("\"complete\":true");metadata("\"blend\":0.250000000");
    assert(hle_d3d8_interp_rec==1);
    puts("PASS: shared one-shot capture ordering, stale evidence, aliases and failed begin/readback/present");
    return 0;
}
