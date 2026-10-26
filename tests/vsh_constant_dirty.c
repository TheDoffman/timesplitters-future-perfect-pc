#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
#include <stdlib.h>
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define NV2A_VS_MAX_CONSTANTS 192
#define RHI_MAP_WRITE_DISCARD 1
typedef struct {float c[192][4];} NV2AVSConstants;
static NV2AVSConstants banks[2],gpu[2];
static BOOL dirty[2],g_hor_plus_claimed;
static unsigned active,maps[2],attempts[2],fail_map[2],hp_calls;
static float test_hp=1,test_zoom=1;
static int projection=60,screen=1;
static void *g_vsh_cb=&gpu;
#define g_vsh_constants banks[active]
#define g_vsh_constants_dirty dirty[active]
static float hor_plus_factor(void) {hp_calls++;return test_hp;}
static int d3d8_vsh_hor_plus_reg(void) {return projection;}
static float camera_zoom(void) {return test_zoom;}
static int d3d8_target_is_screen(void) {return screen;}
static void *rhi_buffer_map(void *buffer,int mode) {
    assert(buffer==g_vsh_cb&&mode==1);attempts[active]++;
    if(fail_map[active])return NULL;
    maps[active]++;return &gpu[active];
}
static void rhi_buffer_unmap(void *buffer) {assert(buffer==g_vsh_cb);}
/* REFERENCE_SETTER */
/* PRODUCTION_SETTER */
static void reference_upload(void) {void *mapped;
/* REFERENCE_UPLOAD */
}
static void production_upload(void) {void *mapped;
/* PRODUCTION_UPLOAD */
}
static void reset(void) {
    memset(banks,0,sizeof banks);memset(gpu,0xa5,sizeof gpu);
    dirty[0]=dirty[1]=TRUE;fail_map[0]=fail_map[1]=0;
    maps[0]=maps[1]=attempts[0]=attempts[1]=0;
    test_hp=test_zoom=1;projection=60;screen=1;g_hor_plus_claimed=FALSE;
}
static void upload_pair(void) {
    active=0;reference_upload();active=1;production_upload();
    assert(!memcmp(&gpu[0],&gpu[1],sizeof gpu[0]));
}
static void setter_pair(int first,const float *data,int count,int alias) {
    int flags[2];unsigned queried[2];
    for(active=0;active<2;active++) {
        const float *source=data;
        if(alias)source=(const float *)&banks[active]+alias-1;
        feclearexcept(FE_ALL_EXCEPT);unsigned before=hp_calls;
        if(active==0)reference_set_constant(first,source,count);
        else d3d8_vsh_set_constant(first,source,count);
        flags[active]=fetestexcept(FE_ALL_EXCEPT);queried[active]=hp_calls-before;
    }
    assert(flags[0]==flags[1]&&queried[0]==queried[1]);
    assert(!memcmp(&banks[0],&banks[1],sizeof banks[0]));
}
static void corpus(void) {
    static const int rounds[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    static const int starts[]={-1,0,1,7,31,191,192,193};
    static const int counts[]={-3,0,1,2,4,8,192,200};
    static const uint32_t edges[]={0,0x80000000,0x3f800000,0x7f800000,0xff800000,0x7fc12345,0x7f812345,1,0x80000001};
    float data[256*4];uint32_t random=0x337523ab;
    for(unsigned mode=0;mode<4;mode++) {
        assert(fesetround(rounds[mode])==0);reset();upload_pair();
        for(unsigned trial=0;trial<2048;trial++) {
            for(unsigned i=0;i<sizeof data/sizeof *data;i++) {
                random=random*1664525u+1013904223u;uint32_t bits=(i%11==0)?edges[(trial+i)%9]:random;
                memcpy(data+i,&bits,4);
            }
            test_hp=(trial%7==0)?1.25f:1;g_hor_plus_claimed=(trial&1)!=0;
            projection=(trial%3==0)?191:(trial%5==0)?0:60;
            test_zoom=(trial%4==0)?1.125f:1;screen=(trial%3)!=0;
            setter_pair(starts[trial%8],data,counts[(trial/8)%8],0);upload_pair();
            if(trial%13==0) {
                setter_pair(1,data,6,4);upload_pair(); /* forward overlapping source */
                setter_pair(0,data,6,6);upload_pair(); /* backward overlapping source */
                setter_pair(1,data,6,5);upload_pair(); /* exact alias */
            }
        }
    }
    assert(fesetround(FE_TONEAREST)==0);
}
static void unchanged_and_fresh(void) {
    reset();float data[4]={0,0,0,0};
    setter_pair(7,data,1,0);upload_pair();assert(maps[1]==1); /* first zero upload is required */
    for(unsigned i=0;i<1000;i++){setter_pair(7,data,1,0);upload_pair();}
    assert(maps[0]==1001&&maps[1]==1);
    unsigned before=maps[1];
    for(unsigned i=0;i<1000;i++) {
        data[0]=(float)i*.001f+1;setter_pair(7,data,1,0);upload_pair();
        assert(maps[1]==before+i+1); /* every fresh pose reaches the GPU mirror */
    }
    data[0]=0;setter_pair(7,data,1,0);upload_pair();before=maps[1];
    uint32_t negative_zero=0x80000000;memcpy(data,&negative_zero,4);
    setter_pair(7,data,1,0);upload_pair();assert(maps[1]==before+1);
    /* A pre-existing pending write must not be cleared by an identical set. */
    active=1;dirty[1]=TRUE;d3d8_vsh_set_constant(7,data,1);assert(dirty[1]);production_upload();
    /* Empty/clipped non-null ranges retain original invalidation. */
    active=1;d3d8_vsh_set_constant(192,data,0);assert(dirty[1]);production_upload();
    /* Null/negative-start calls remain no-ops and preserve either dirty state. */
    for(unsigned pending=0;pending<2;pending++) {
        active=1;dirty[1]=(BOOL)pending;unsigned before_queries=hp_calls;
        NV2AVSConstants saved=banks[1];
        d3d8_vsh_set_constant(7,NULL,1);d3d8_vsh_set_constant(-1,data,1);
        assert(dirty[1]==(BOOL)pending&&hp_calls==before_queries);
        assert(!memcmp(&saved,&banks[1],sizeof saved));
    }
}
static void failed_upload(void) {
    reset();active=1;float data[4]={1,2,3,4};
    d3d8_vsh_set_constant(3,data,1);fail_map[1]=1;production_upload();
    assert(dirty[1]&&attempts[1]==1&&maps[1]==0);
    d3d8_vsh_set_constant(3,data,1);assert(dirty[1]);
    fail_map[1]=0;production_upload();
    assert(!dirty[1]&&maps[1]==1&&!memcmp(&gpu[1],&banks[1],sizeof banks[1]));
    /* Retry also retains the pending screen zoom until it actually uploads. */
    test_zoom=1.125f;screen=1;fail_map[1]=1;production_upload();assert(dirty[1]);
    fail_map[1]=0;production_upload();assert(!dirty[1]);
    for(unsigned i=0;i<4;i++)assert(gpu[1].c[60][i]==banks[1].c[60][i]*test_zoom);
    screen=0;production_upload();assert(!memcmp(&gpu[1],&banks[1],sizeof banks[1]));
}
int main(void) {
    corpus();unchanged_and_fresh();failed_upload();
    puts("PASS: shader constant bytes/aliases/FP modes, fresh poses, unchanged uploads, zoom and failed-map retry");return 0;
}
