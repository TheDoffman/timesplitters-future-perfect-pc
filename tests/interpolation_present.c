/* Exercise the real HLE redraw/gate with renderer outcomes, no game assets. */
#include "platform/xbox_winnt.h"
static BOOL test_counter(LARGE_INTEGER *value);
static BOOL test_frequency(LARGE_INTEGER *value);
#define QueryPerformanceCounter test_counter
#define QueryPerformanceFrequency test_frequency
#include "hle/hle_d3d8_interp.c"
#include <assert.h>
#include <stdlib.h>
/* Keep the full production translation unit linkable on MSVC as well as Clang.
 * These hooks are outside the redraw/gate cases tested here. Fail loudly if a
 * change makes a case depend on recording, configuration or shader discovery. */
static void unexpected_hook(const char *name){fprintf(stderr,"Unexpected interpolation test hook: %s\n",name);abort();}
void d3d8_vsh_interp_registers(int *projection,int *first,int *count){
    (void)projection;(void)first;(void)count;unexpected_hook("register discovery");
}
uint32_t d3d8_vsh_bound_reads(int first,int count){
    (void)first;(void)count;unexpected_hook("shader reads");return 0;
}
int hle_d3d8_defer_on(void){unexpected_hook("deferred recording");return 0;}
void hle_d3d8_interp_snapshot(void){unexpected_hook("snapshot");}
int recomp_config_int(const char *env_name,const char *key,int fallback){
    (void)env_name;(void)key;(void)fallback;unexpected_hook("configuration");return 0;
}
const char *recomp_config_lookup(const char *env_name,const char *key){
    (void)env_name;(void)key;unexpected_hook("configuration lookup");return NULL;
}
void xbox_Nv2aFlipGateSetIdle(xbox_FlipGateIdleFn fn){(void)fn;unexpected_hook("gate installation");}
#ifndef TSFP_TEST_TITLE_REGISTERS
void xbox_D3D8SetInterpRegisters(int p,int a,int n){(void)p;(void)a;(void)n;unexpected_hook("title registers");}
#endif
void xbox_SetRecompIdle(void (*fn)(void)){(void)fn;unexpected_hook("CPU prediction installation");}
const char *xbox_Nv2aFlipGateModeName(void){unexpected_hook("gate mode");return "60";}
BOOL xbox_D3D8InterpCanPredict(void){unexpected_hook("CPU prediction target");return FALSE;}
void xbox_D3D8InterpPredict(BOOL on){(void)on;unexpected_hook("CPU prediction state");}
static int begin_ok=1,present_ok=1,begins,presents,ends,overlays;
static char operations[256];static unsigned operation_count;
static void operation(char value){assert(operation_count+1<sizeof operations);operations[operation_count++]=value;operations[operation_count]=0;}
static int dump_result=-1;
int hle_d3d8_interp_rec;
BOOL xbox_D3D8InterpBegin(void){operation('B');begins++;return begin_ok;}
BOOL xbox_D3D8InterpPresent(void){operation('P');presents++;return present_ok;}
void xbox_D3D8InterpEnd(void){operation('E');ends++;}
const float *hle_d3d8_interp_constants(void){static float values[256*4];return values;}
static float uploaded_constants[256*4];
void d3d8_vsh_set_constant(int start,const float *values,int count){
    assert(start>=0 && count>=0 && start+count<=256);
    memcpy(uploaded_constants+start*4,values,(size_t)count*4*sizeof(float));
}
void hle_d3d8_overlay_redraw(void){operation('O');overlays++;}
int hle_d3d8_dump_back_buffer(const char *path){operation(strstr(path,"-guest.bmp")?'G':'M');return dump_result;}
int hle_d3d8_capture_active(void){return 0;}
int hle_d3d8_movie_layer_shown(void){return 0;}
static LONGLONG clock_ticks=100;
static BOOL test_counter(LARGE_INTEGER *value){value->QuadPart=++clock_ticks;return TRUE;}
static BOOL test_frequency(LARGE_INTEGER *value){(void)value;unexpected_hook("clock initialization");return FALSE;}
int main(void) {
    g_on=1;hle_d3d8_interp_rec=1;
    assert(redraw(.5));
    assert(begins==1 && presents==1 && ends==1 && overlays==1);
    assert(g_presents_total==1 && g_shown==1 && hle_d3d8_interp_rec==1);
    present_ok=0;
    assert(!redraw(.5));
    assert(begins==2 && presents==2 && ends==2 && overlays==2);
    assert(g_presents_total==1 && g_shown==1 && hle_d3d8_interp_rec==1);
    begin_ok=0;assert(!redraw(.5));
    assert(begins==3 && presents==2 && ends==2 && !g_on && hle_d3d8_interp_rec==1);
    /* A failed begin must not feed a previous present's timestamp into the
     * scheduling estimate. A failed present consumes the slot without retry. */
    for(int available=0;available<2;available++) {
        begin_ok=available;g_on=1;g_prepared=g_frame_ok=1;
        g_last_present=100000;g_qpf=1000000;g_multiple=2;g_slot=1;
        g_redraw_est=100;g_presented_at=-99999;
        assert(gate_idle(108000,116000)==0);
        assert(g_slot==2 && g_redraw_est==100 && g_redraws==0);
        assert(g_presents_total==1 && hle_d3d8_interp_rec==1);
    }
    /* The PC target clock must keep the same failure/counting contract,
     * while retrying unavailable targets at most once per second. */
    g_target=144;g_replayed=0;g_on=1;g_frame_ok=1;g_retry_after=0;
    begin_ok=0;present_ok=1;
    int before=begins;
    assert(!redraw(.5) && g_on && begins==before+1 && !g_frame_ok);
    assert(!hle_d3d8_interp_skip_guest() && g_replayed==0);
    assert(!redraw(.5) && begins==before+1);
    clock_ticks+=g_qpf;begin_ok=1;present_ok=0;g_frame_ok=1;
    assert(!redraw(.5) && begins==before+2 && g_replayed==0);
    assert(!hle_d3d8_interp_skip_guest() && g_presents_total==1);
    present_ok=1;assert(redraw(.5) && g_replayed==1 && g_presents_total==2);
    assert(hle_d3d8_interp_skip_guest());
    g_target=0;
    puts("PASS: HLE present outcome counts, bounded retry, restore on failure and stale-timestamp rejection");
    return 0;
}
