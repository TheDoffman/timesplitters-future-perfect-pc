/* Exercise temporary render-memory lifetime through the production clock.
 * The arena and cache are controlled boundaries; no game assets are needed. */
static void observe_render_counter(void);
#define TIMING_COUNTER_OBSERVER() observe_render_counter()
#define main native_timing_baseline_main
#include "native_timing_guest.c"
#undef main
#include <math.h>
static void observe_render_counter(void)
{ WORD(0x563918)=100u+(uint32_t)(counter/2400000); }
static void original_render_clock(void)
{
    WORD(0x7497B8)++;
    WORD(0x7497AC)=WORD(0x563918);
    WORD(0x4E7940)=WORD(0x4E7944)=1;
    FLOAT(0x4E7948)=FLOAT(0x4E794C)=1.f;
    g_esp+=4;
}
typedef struct { uint32_t stamp;float *matrix; } Cache;
static float arena[4][16];
static Cache caches[4];
static unsigned recomputes,hits,checks;
static uint32_t previous_console;
static unsigned repeated_console;
static void render(void)
{
    uint32_t epoch=WORD(0x7497AC),console=WORD(0x563918);
    if(console==previous_console)repeated_console++;
    previous_console=console;
    /* A new render resets/reuses temporary memory even on zero whole ticks.
     * Camera/light data are intentionally unlike a texture UV transform. */
    for(unsigned n=0;n<4;n++)for(unsigned j=0;j<16;j++)arena[n][j]=-900.f-(float)j;
    for(unsigned pass=0;pass<2;pass++)for(unsigned n=0;n<4;n++) {
        Cache *c=&caches[n];
        if(c->stamp!=epoch) {
            c->matrix=arena[n];c->stamp=epoch;recomputes++;
            for(unsigned j=0;j<16;j++)c->matrix[j]=(float)(n*16+j+1);
        } else hits++;
        for(unsigned j=0;j<16;j++) {
            assert(c->matrix && c->matrix[j]==(float)(n*16+j+1));
            checks++;
        }
    }
}
static void step(const char *rate,int64_t elapsed)
{
    setting=rate;counter+=elapsed;observe_render_counter();
    uint32_t isr=WORD(0x563918);g_esp=0x1000;
    pc_native_timing_guest(original_render_clock);
    assert(g_esp==0x1004);
    assert(WORD(0x563918)>=isr); /* pacing may advance the controlled host clock */
    render();
}
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;WORD(0x4E8CEC)=60;
    for(unsigned i=0;i<4;i++)caches[i].stamp=UINT32_MAX;
    const char *rates[]={"0","120","144","180","240","0","240","120","0"};
    for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++) {
        int rate=atoi(rates[r]);if(!rate)rate=60;
        for(int i=0;i<rate;i++) {
            WORD(0x7497B4)=(i>=rate/3 && i<rate/2)?1:0;
            step(rates[r],144000000/rate);
        }
    }
    /* Wrap the existing render serial, then switch between both timing paths. */
    WORD(0x7497B8)=UINT32_MAX-2u;
    for(unsigned i=0;i<8;i++)step(i==4?"0":"240",600000);
    assert(repeated_console>100 && recomputes==hits && checks>100000);
    printf("PASS: %u temporary-matrix checks, %u recomputes/%u same-frame hits; %u repeated console ticks, pauses, live rates and wrap\n",checks,recomputes,hits,repeated_console);
    return 0;
}
