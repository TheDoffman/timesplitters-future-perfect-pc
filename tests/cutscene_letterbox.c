/* Run the production rectangle wrapper with disposable guest memory. Optional
 * local coverage executes the original checksum-verified US cinematic code. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#include "pc_display.h"
#include "pc_cutscene.h"
#include "d3d8_movie.h"
#define TSFP_PC_CONTROLS
static PcDisplayMode mode;
static float fov_scale=1.f;
float pc_display_fov_scale(void) { return fov_scale; }
PcDisplayMode pc_display_effective(void) { return mode; }
float pc_display_camera_factor(void)
{ return mode.height ? (float)mode.width*3.f/(mode.height*4.f) : 1.f; }
static int guest_writable(uint32_t address,size_t size)
{ return address && address < 0x800000 && size <= 0x800000-address; }
#define WORD_AT(a) MEM32(a)

ptrdiff_t g_xbox_mem_offset;
#include "display_memory_mock.h"
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_ebp, g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top;
#define FP_MEM32(va) MEM32(va)
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %s\n",#x); exit(1); } } while (0)

static unsigned draws, subtitles, states, baseline;
static uint32_t last_rectangle[6], rectangles[16][6];
void sub_0001EFB0_gen(void)
{
    memcpy(last_rectangle, (void *)(g_xbox_mem_offset + g_esp), sizeof last_rectangle);
    CHECK(draws < 16);
    memcpy(rectangles[draws++], last_rectangle, sizeof last_rectangle);
    g_eax = 0x12345678;
    g_esp += 4;
}

int pc_display_transition_overlay(void (*original)(void))
{ (void)original; return 0; }

int pc_display_scope_mask(void (*original)(void))
{ (void)original; return 0; }

/* Footer dispatch must never intercept a cinematic rectangle. */
void pc_display_footer_background(void (*original)(void))
{ (void)original; CHECK(0); }

#define sub_0001EFB0 filtered_rectangle
/* PRODUCTION_WRAPPER */
#undef sub_0001EFB0
void sub_0001EFB0(void)
{
    if (baseline) sub_0001EFB0_gen(); else filtered_rectangle();
}

/* PRODUCTION_CAMERA */

static void original_camera(void)
{
    MEM32(0x7B0CC0)=0x4000; MEM32(0x4204)=0x5000;
    MEMF(0x5000)=2; MEMF(0x5014)=3; MEMF(0x437C)=.25f; MEMF(0x4380)=1.f/3.f;
    g_esp+=4;
}

static void camera_check(float expected_zoom)
{
    float factor=pc_display_camera_factor();
    g_esp=0x10000;
    pc_display_camera(original_camera);
    CHECK(g_esp==0x10004);
    CHECK(fabsf(MEMF(0x5000)-2*expected_zoom/factor)<.00001f);
    CHECK(fabsf(MEMF(0x5014)-3*expected_zoom)<.00001f);
    CHECK(fabsf(MEMF(0x437C)-.25f/expected_zoom)<.00001f);
    CHECK(fabsf(MEMF(0x4380)-factor/(3*expected_zoom))<.00001f);
}

static void camera_tests(void)
{
    mode.width=1920;mode.height=1080;
    MEM32(0x7DE458)=0; MEMF(0x7490EC)=0;
    camera_check(1); /* Normal gameplay retains its wide camera. */
    /* The setting changes both frustum axes, never the original zoom state.
       Rebuilding each frame prevents scaling from accumulating. */
    MEM32(0x7B0C7C)=0x6000; MEM32(0x6008)=0x4000; MEM32(0x6114)=2;
    fov_scale=2.f;
    const unsigned states[]={2,0x800,0x800,4,2}; /* Ground, air lift, ascent, fall, land. */
    const unsigned callers[]={0xCCBED,0xCDD35};
    /* The frontend also owns a state-2 player and uses these camera callers.
       Change scenes without clearing that actor, as on return to the menu. */
    const unsigned scenes[]={1,52,100,101,102,103,104,105,106,107,1};
    const unsigned movement_states[]={2,4,0x800};
    const float settings[]={1.f,tanf(90.f*.00872664626f)/tanf(55.f*.00872664626f)};
    for(unsigned c=0;c<2;c++)for(unsigned s=0;s<2;s++)
    for(unsigned a=0;a<3;a++)for(unsigned i=0;i<sizeof scenes/sizeof *scenes;i++) {
        MEM32(0x10000)=callers[c]; MEM32(0x6114)=movement_states[a];
        MEM32(0x79094C)=scenes[i]; fov_scale=settings[s];
        MEMF(0x4384)=55.f;
        camera_check(scenes[i]>100 ? 1.f : 1.f/fov_scale);
        CHECK(MEM32(0x6114)==movement_states[a] && MEMF(0x4384)==55.f);
    }
    MEM32(0x79094C)=1; fov_scale=2.f;
    for(unsigned c=0;c<2;c++)for(unsigned i=0;i<sizeof states/sizeof *states;i++) {
        MEM32(0x6114)=states[i];
        MEMF(0x4384)=55.f; /* Original gameplay/weapon lens must not be rewritten. */
        g_esp=0x10000; MEM32(g_esp)=callers[c];
        pc_display_camera(original_camera);
        CHECK(fabsf(MEMF(0x5000)-.75f)<.00001f);
        CHECK(fabsf(MEMF(0x5014)-1.5f)<.00001f);
        CHECK(fabsf(MEMF(0x437C)-.5f)<.00001f);
        CHECK(fabsf(MEMF(0x4380)-8.f/9.f)<.00001f);
        CHECK(MEM32(0x6114)==states[i] && MEMF(0x4384)==55.f);
    }
    MEM32(0x6114)=4;
    fov_scale=1.f; camera_check(1); /* Default FOV unchanged while falling. */
    fov_scale=tanf(90.f*.00872664626f)/tanf(55.f*.00872664626f);
    camera_check(1.f/fov_scale); /* Reported 90-degree setting. */
    fov_scale=2.f;
    const unsigned excluded[]={0,1,0x10,0x100,0x2000,0x2004};
    for(unsigned i=0;i<sizeof excluded/sizeof *excluded;i++) {
        MEM32(0x6114)=excluded[i]; camera_check(1); /* Non-gameplay/terminal unchanged. */
    }
    MEM32(0x6114)=0x800; /* Air lifts still respect camera/cinematic ownership. */
    MEM32(0x6008)=0; camera_check(1); /* Other cameras unchanged. */
    MEM32(0x6008)=0x4000;
    MEM32(0x10000)=0xC999E; camera_check(1); /* Story caller unchanged. */
    MEM32(0x10000)=0xCDD35; /* Non-default FOV through cinematic tests. */
    MEM32(0x7DE458)=0x20000000;
    camera_check(4.f/3.f); /* Story composition fits the thinner matte. */
    mode.width=3440;mode.height=1440;
    camera_check(43.f/24.f); /* Ultrawide fills horizontally without stretching. */
    mode.width=1280;mode.height=960;
    camera_check(1);
    mode.width=1920;mode.height=1080;
    MEM32(0x4C045C)=9; MEM32(0x4C0458)=16;
    camera_check(1); /* Original no-matte story mode. */
    MEM32(0x4C045C)=0;MEM32(0x4C0458)=0;
    MEM32(0x7DE458)=0;MEMF(0x7490EC)=.5f;
    camera_check(466.f/408.f);
    MEMF(0x7490EC)=1;
    camera_check(450.f/338.f);
    MEM32(0x7490E8)=1; MEM32(0x10000)=0;camera_check(1);
    MEM32(0x7490E8)=0;MEMF(0x7490EC)=0;camera_check(1);
    fov_scale=1.f;
}

#ifdef ORIGINAL_CUTSCENES
void sub_00019650(void) { ++states; g_esp += 4; }
void sub_00019D70(void) { ++states; g_esp += 4; }
void sub_0001ED60(void) { ++states; g_esp += 4; }
void sub_00098DA0(void) { g_eax = 0x2000; g_esp += 4; }
void sub_000607A0(void)
{
    unsigned i;
    for (i = 3; i <= 7; ++i) MEM32(MEM32(g_esp + i * 4)) = 0;
    g_esp += 4;
}
void sub_0003C420(void) { g_eax = 0; g_esp += 4; }
void sub_0006EEA0(void) { ++subtitles; g_esp += 4; }
void sub_000F8BB0(void) { abort(); } /* Unused depth-of-field branch. */
void sub_000F8ED0(void) { abort(); }
void sub_003C848C(void) { abort(); }
/* ORIGINAL_ROUTINES */

/* Run the original US region selector and state setter against all four
 * original Zeppelin lift cylinders, then project through the production hook.
 * Only positions are supplied here; the guest routine chooses 0x800 and 4. */
static void original_air_lifts(void)
{
    /* ORIGINAL_CONSTANTS */
    MEM32(0x7B0C48)=MEM32(0x7B0C7C)=0x6000; MEM32(0x790950)=1;
    MEM32(0x6014)=0x8000; MEM32(0x807C)=0x9000; MEM32(0x6008)=0x4000;
    MEM32(0x7B0CC0)=0x4000; MEM32(0x79094C)=52;
    MEM32(0x7DE458)=MEM32(0x7490E8)=0; MEMF(0x7490EC)=0;
    const PcDisplayMode shapes[]={{1280,960},{1920,1080},{3440,1440}};
    const float settings[]={1.f,tanf(90.f*.00872664626f)/tanf(55.f*.00872664626f)};
    const uint32_t callers[]={0xCCBED,0xCDD35};
    for(unsigned shape=0;shape<3;shape++)for(unsigned setting=0;setting<2;setting++)
    for(unsigned caller=0;caller<2;caller++)for(unsigned lift=0;lift<4;lift++) {
        uint32_t region=0x50D408+20*lift;
        mode=shapes[shape];fov_scale=settings[setting];
        MEM32(0x6114)=2;MEMF(0x4384)=55.f;
        MEM32(0x10000)=callers[caller];camera_check(1.f/fov_scale);
        for(unsigned step=0;step<3;step++) {
            MEMF(0x60A8)=MEMF(region)+(step==2 ? 3.f : 0.f);
            MEMF(0x60AC)=MEMF(region+4)+(step==0 ? 1.f : 10.f);
            MEMF(0x60B0)=MEMF(region+8);
            g_esp=0x10000;MEM32(g_esp)=0xDEADBEEF;
            g_ebx=0x123;g_esi=0x456;
            sub_00225C70();
            CHECK(g_esp==0x10004 && g_ebx==0x123 && g_esi==0x456);
            uint32_t state=step==2 ? 4 : 0x800;
            CHECK(MEM32(0x6114)==state);
            CHECK(step==2 ? MEMF(0x9B20)==0 : MEMF(0x9B20)>0);
            MEM32(0x10000)=callers[caller];camera_check(1.f/fov_scale);
            CHECK(MEM32(0x6114)==state && MEMF(0x4384)==55.f);
        }
        /* Landing is covered by the separate original player-force fixture;
         * here verify the camera again after the state returns to grounded. */
        MEM32(0x6114)=2;camera_check(1.f/fov_scale);
    }
    puts("PASS: all four original US air lifts preserve default/custom FOV");
}

static void run_original(void (*routine)(void), unsigned expected)
{
    draws = subtitles = states = 0;
    g_esp = 0x10000; MEM32(g_esp) = 0xDEADBEEF;
    g_esi = 0xABC; g_ebx = 0xDEF; g_edi = 0x123; g_ebp = 0x456;
    routine();
    CHECK(draws == expected);
    CHECK(g_esp == 0x10004);
    CHECK(g_esi == 0xABC && g_ebx == 0xDEF && g_edi == 0x123 && g_ebp == 0x456);
}

static void original_scenes(void)
{
    unsigned scene, shape, phase;
    const PcDisplayMode shapes[] = {{0,0},{1280,960},{1920,1080},{2560,1440},
                                    {3840,2160},{1920,1200},{3440,1440},{5120,1440}};
    /* ORIGINAL_CONSTANTS */
    MEM32(0x7E71C8) = 0x3000;
    MEM32(0x2000 + 0x20) = 1;
    MEM32(0x4E8CF0) = 640; MEM32(0x4E8CF4) = 480;
    MEM32(0x3004) = 639; MEM32(0x3018) = 479;
    for (scene = 0; scene < 2; ++scene) {
        void (*routine)(void) = scene ? sub_001396B0 : sub_0006F150;
        for (phase = 0; phase < 3; ++phase) {
            unsigned original_count, original_subtitles, original_states, i;
            uint32_t original_rectangles[16][6];
            MEMF(0x7490EC) = phase * .5f;
            baseline = 1;
            run_original(routine, scene ? (phase ? 2 : 0) : 4);
            original_count = draws; original_subtitles = subtitles; original_states = states;
            memcpy(original_rectangles, rectangles, sizeof rectangles);
            for (shape = 0; shape < sizeof shapes / sizeof *shapes; ++shape) {
                unsigned expected = original_count;
                int full = scene ? 71 : 69;
                int animated = scene ? (int)(70 * phase * .5f) + 1 : 69;
                int resized, side;
                baseline = 0; mode = shapes[shape];
                resized = pc_cutscene_bar_height(mode.width,mode.height,480,full,animated);
                side = pc_cutscene_bar_width(mode.width,mode.height,640,480,full,animated);
                if (!resized && !side && (!scene || phase)) expected -= 2;
                run_original(routine, expected);
                CHECK(subtitles == original_subtitles && states == original_states);
                for (i = 0; i < original_count; ++i) {
                    uint32_t *r = original_rectangles[i];
                    unsigned j, found = 0;
                    int bar = r[0] == 0x6F322 || r[0] == 0x6F34B ||
                              r[0] == 0x13970E || r[0] == 0x13972E;
                    for (j = 0; j < draws; ++j) if (rectangles[j][0] == r[0]) {
                        ++found;
                        if (!bar) CHECK(memcmp(r,rectangles[j],sizeof last_rectangle) == 0);
                        else {
                            if (side) {
                                CHECK(rectangles[j][3]-rectangles[j][1] == (unsigned)side);
                                CHECK(rectangles[j][2] == 0 && rectangles[j][4] == 480);
                            } else {
                                CHECK(rectangles[j][4] - rectangles[j][2] == (unsigned)resized);
                                CHECK(rectangles[j][1] == r[1] && rectangles[j][3] == r[3]);
                            }
                            CHECK(rectangles[j][5] == r[5]);
                        }
                    }
                    CHECK(found == (unsigned)(!bar || resized > 0 || side > 0));
                }
            }
        }
    }
    baseline = 0;
}

#endif

int main(void)
{
    static const uint32_t bars[] = {0x6F322, 0x6F34B, 0x13970E, 0x13972E};
    static const uint32_t other[] = {
        0x6F227, 0x6F264, 0x6F2F0, 0x6F381, 0x6F3AB, /* fades/tints */
        0x138FB9, 0x138FD3, 0x138FE6, 0x138FFD, /* scope borders */
        0x430C1, 0, 0xFFFFFFFF
    };
    unsigned i, j;
    unsigned char *memory = calloc(1, 0x800000);
    CHECK(memory != NULL);
    g_xbox_mem_offset = (ptrdiff_t)(uintptr_t)memory;
    MEM32(0x4E8CF4) = 480;
    MEM32(0x7E71C8) = 0x3000; MEM32(0x3018) = 479;
    for (i = 0; i < sizeof bars / sizeof *bars; ++i) {
        const PcDisplayMode shapes[] = {{0,0},{1280,960},{1920,1080},{3840,2160},
                                        {1920,1200},{3440,1440},{5120,1440}};
        unsigned shape;
        int top = i == 0 || i == 3, full = i < 2 ? 69 : 71;
        for (shape = 0; shape < sizeof shapes / sizeof *shapes; ++shape) {
            uint32_t saved[6];
            int adjusted, side;
            mode = shapes[shape];
            g_esp = 0x10000; MEM32(g_esp) = bars[i];
            MEM32(g_esp+4) = 0; MEM32(g_esp+12) = 640; MEM32(g_esp+20) = 0x7F;
            MEM32(g_esp+8) = top ? 0 : 480-full;
            MEM32(g_esp+16) = top ? full : 480;
            memcpy(saved, memory + g_esp, sizeof saved);
            draws = 0; g_eax = 42; g_esi = 123;
            sub_0001EFB0();
            adjusted = pc_cutscene_bar_height(mode.width,mode.height,480,full,full);
            side = pc_cutscene_bar_width(mode.width,mode.height,640,480,full,full);
            CHECK(draws == (unsigned)(adjusted > 0 || side > 0) && g_esp == 0x10004 && g_esi == 123);
            CHECK(memcmp(saved, memory + 0x10000, sizeof saved) == 0);
            if (side) {
                CHECK(last_rectangle[3]-last_rectangle[1] == (unsigned)side);
                CHECK(top ? last_rectangle[1] == 0 : last_rectangle[3] == 640);
                CHECK(last_rectangle[2] == 0 && last_rectangle[4] == 480);
            }
            else if (adjusted) {
                CHECK(last_rectangle[4]-last_rectangle[2] == (unsigned)adjusted);
                CHECK(top ? last_rectangle[2] == 0 : last_rectangle[4] == 480);
                CHECK(last_rectangle[1] == 0 && last_rectangle[3] == 640 && last_rectangle[5] == 0x7F);
            }
        }
        for (j = 0; j < 2; ++j) {
            g_esp = 0x10000; MEM32(g_esp) = bars[i] + (j ? 1 : -1);
            draws = 0; sub_0001EFB0(); CHECK(draws == 1);
        }
    }
    /* Independently calculated reference heights and animation checks. */
    CHECK(pc_cutscene_bar_height(1280,960,480,69,69) == 69);
    CHECK(pc_cutscene_bar_height(1920,1080,480,69,69) == 12);
    CHECK(pc_cutscene_bar_height(3840,2160,480,69,69) == 12);
    CHECK(pc_cutscene_bar_height(1920,1200,480,69,69) == 35);
    CHECK(pc_cutscene_bar_height(3440,1440,480,69,69) == 0);
    CHECK(pc_cutscene_bar_height(1920,1080,480,71,36) == 7);
    CHECK(pc_cutscene_bar_height(0,0,480,71,36) == 36);
    CHECK(pc_cutscene_bar_height(1920,1080,0,71,36) == 36);
    for (i = 0; i < sizeof other / sizeof *other; ++i) {
        g_esp = 0x10000; MEM32(g_esp) = other[i];
        for (j = 1; j < 6; ++j) MEM32(g_esp + j * 4) = 200 + j;
        draws = 0; sub_0001EFB0();
        CHECK(draws == 1 && g_esp == 0x10004 && g_eax == 0x12345678);
        CHECK(last_rectangle[0] == other[i]);
        for (j = 1; j < 6; ++j) CHECK(last_rectangle[j] == 200 + j);
    }
#ifdef ORIGINAL_CUTSCENES
    original_scenes();
#endif
    camera_tests();
#ifdef ORIGINAL_CUTSCENES
    original_air_lifts();
#endif
    {
        const unsigned displays[][2] = {{640,480},{1920,1080},{1920,1200},
            {3440,1440},{5120,1440},{3840,2160},{1080,1920},{0,0}};
        CHECK(pc_cutscene_movie_fill_width("D:\\xmv\\attract.xmv"));
        CHECK(pc_cutscene_movie_fill_width("/xmv/ATTRACT.XMV"));
        CHECK(!pc_cutscene_movie_fill_width(NULL));
        CHECK(!pc_cutscene_movie_fill_width("attract.xmv.bak"));
        CHECK(!pc_cutscene_movie_fill_width("notattract.xmv"));
        CHECK(pc_cutscene_movie_fill_width("D:\\xmv\\eag_e.xmv"));
        CHECK(pc_cutscene_movie_fill_width("D:\\xmv\\frd.xmv"));
        CHECK(pc_cutscene_movie_fill_width("/xmv/EAG_F.XMV"));
        CHECK(pc_cutscene_movie_fill_width("eag_i.xmv"));
        CHECK(pc_cutscene_movie_fill_width("eag_s.xmv"));
        CHECK(!pc_cutscene_movie_fill_width("eag.xmv"));
        CHECK(!pc_cutscene_movie_fill_width("unknown.xmv"));
        CHECK(!pc_cutscene_movie_fill_width(""));
        for (unsigned d=0;d<sizeof displays/sizeof *displays;d++) {
            unsigned width=displays[d][0],height=displays[d][1];
            float w,h,aspect=height?(float)width/height:4.f/3.f;
            d3d8_movie_rectangle(width,height,1,&w,&h);
            CHECK(w == 2.f); /* Neither side can be left uncovered. */
            CHECK(fabsf(w*aspect/h-4.f/3.f)<.00001f); /* Square content stays square. */
            d3d8_movie_rectangle(width,height,0,&w,&h);
            CHECK(w<=2.f && h<=2.f); /* Default fit mode retains the complete source picture. */
            CHECK(fabsf(w*aspect/h-4.f/3.f)<.00001f);
            {
                unsigned left, right;
                float x, y;
                pc_cutscene_movie_crop(720,480,&left,&right);
                CHECK(left==38 && right==42);
                d3d8_movie_bounds(width,height,1,720,left,right,&x,&y,&w,&h);
                CHECK(fabsf(w*aspect/h-4.f/3.f)<.00001f);
                CHECK(y==h/2.f);
                if(height && aspect>4.f/3.f) {
                    /* The displayed edges sample actual picture columns,
                     * not the encoded padding; retain square proportions. */
                    CHECK(fabsf((-1.f-x)/w*720.f-38.f)<.0001f);
                    CHECK(fabsf((1.f-x)/w*720.f-678.f)<.0001f);
                } else CHECK(x==-1.f && w==2.f);
                pc_cutscene_movie_crop(640,480,&left,&right);
                CHECK(!left && !right);
                d3d8_movie_bounds(width,height,1,640,left,right,&x,&y,&w,&h);
                CHECK(x==-1.f && w==2.f);
                d3d8_movie_bounds(width,height,0,720,38,42,&x,&y,&w,&h);
                CHECK(w<=2.f && h<=2.f && x==-w/2.f);
                d3d8_movie_bounds(width,height,1,720,720,1,&x,&y,&w,&h);
                CHECK(x==-1.f && w==2.f);
            }
            float previous=1.f;
            for(int phase=0;phase<=71;phase++) {
                float zoom=pc_cutscene_camera_zoom(width,height,480,71,phase);
                CHECK(pc_cutscene_bar_width(width,height,640,480,71,phase)==0);
                CHECK(isfinite(zoom) && zoom>0.f);
                if(height && aspect>480.f/338.f*4.f/3.f)
                    CHECK(zoom>=previous-.00001f);
                previous=zoom;
            }
            if(height && aspect>480.f/338.f*4.f/3.f) {
                /* Full ultrawide aperture keeps the original horizontal lens:
                 * no view past the edges of the opening's starfield plane. */
                CHECK(fabsf(previous/(aspect*.75f)-1.f)<.00001f);
            }
        }
    }
    free(memory);
    puts("PASS: adaptive cutscene bars, animation, other overlays and guest ABI");
    return 0;
}
