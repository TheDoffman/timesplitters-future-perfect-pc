/* Real HLE matrix preparation and per-draw upload, without game assets. */
#define main present_contract_main
#define TSFP_TEST_TITLE_REGISTERS 1
#include "interpolation_present.c"
#undef main
#include "graphics/tsfp_interpolation.h"
static int configured;
void xbox_D3D8SetInterpRegisters(int p,int f,int n){assert(p==60 && f==64 && n==12);configured++;}

static void projection(float *p,float x)
{
    memset(p,0,16*sizeof(float));p[0]=x;p[5]=1.5f;p[10]=-1.0001f;p[11]=-.15f;p[14]=-1;
}
/* Analytic P * yaw/translation, independent of the production multiplier. */
static void combined(float *m,float angle,float x,float scale)
{
    float c=cosf(angle),s=sinf(angle);
    float values[16]={scale*c,0,scale*s,scale*x, 0,1.5f,0,0,
                     1.0001f*s,0,-1.0001f*c,20.002f-.15f, s,0,-c,20};
    memcpy(m,values,sizeof values);
}
static void near_matrix(const float *a,const float *b)
{
    for(int k=0;k<16;k++)assert(isfinite(a[k]) && fabsf(a[k]-b[k])<2e-4f);
}

int main(void)
{
    tsfp_configure_interpolation();assert(configured==1);
    float previous[64]={0},current[64]={0},saved[64],expected[16];
    Prepared prepared[3]={0};InterpDraw draw={0};
    projection(previous,1);projection(current,2);
    /* c64 is deliberately stale and unrelated. Decompose c68 itself. */
    previous[16]=current[16]=previous[21]=current[21]=previous[26]=current[26]=previous[31]=current[31]=1;
    previous[19]=current[19]=1000;
    combined(previous+32,.2f,2,1);combined(current+32,.4f,4,2);
    memcpy(saved,current,sizeof saved);
    g_proj=60;g_aff_first=64;g_aff_count=12;g_nregs=16;g_groups=3;g_cur=1;g_prep=prepared;
    g_list[0].regs=previous;g_list[1].regs=current;g_list[1].draws=&draw;
    draw.key.uses_proj=1;draw.match=0;draw.groups=2;
    prepare_transform_group(&prepared[1],previous+32,current+32,previous,current,&draw.cut);
    assert(prepared[1].ok && prepared[1].projective && !draw.cut);
    set_draw_registers(0,.5);
    combined(expected,.3f,3,1.5f);
    near_matrix(uploaded_constants+68*4,expected);
    assert(memcmp(current,saved,sizeof saved)==0);
    assert(uploaded_constants[64*4+3]==1000); /* no stale affine substitution */
    set_draw_registers(0,0);near_matrix(uploaded_constants+68*4,previous+32);
    set_draw_registers(0,1);near_matrix(uploaded_constants+68*4,current+32);
    /* Unmatched geometry, HUD and camera cuts keep current-frame constants. */
    draw.match=-1;set_draw_registers(0,.5);near_matrix(uploaded_constants+68*4,current+32);
    draw.match=0;draw.key.uses_proj=0;set_draw_registers(0,.5);near_matrix(uploaded_constants+68*4,current+32);
    draw.key.uses_proj=1;draw.cut=1;set_draw_registers(0,.5);near_matrix(uploaded_constants+68*4,current+32);
    /* A static model with changing FOV still needs a blended projection. */
    combined(current+32,.2f,2,2);draw.cut=0;
    prepare_transform_group(&prepared[1],previous+32,current+32,previous,current,&draw.cut);
    assert(prepared[1].ok && !draw.cut);
    set_draw_registers(0,.5);combined(expected,.2f,2,1.5f);near_matrix(uploaded_constants+68*4,expected);
    float invalid[16]={0},out[16];
    assert(!unproject_group(invalid,current+32,out));
    invalid[0]=NAN;assert(!unproject_group(invalid,current+32,out));
    combined(current+32,2,2,2);draw.cut=0;
    prepare_transform_group(&prepared[1],previous+32,current+32,previous,current,&draw.cut);
    assert(draw.cut && !prepared[1].ok);
    /* Preparation uses real time too. A ready frame must not redraw using
     * the stale time supplied before matching and decomposition. */
    combined(current+32,.4f,4,2);draw.cut=0;draw.key.prims=1;
    InterpDraw previous_draw=draw;
    g_list[0].draws=&previous_draw;g_list[0].ndraws=g_list[1].ndraws=1;
    g_list[0].whole=g_list[1].whole=1;g_prep_cap=3;
    g_on=1;g_last_present=100000;g_qpf=1000000;g_multiple=2;g_slot=1;
    g_redraw_est=100;g_prepare_est=0;g_prepared=g_frame_ok=0;
    clock_ticks=116100;
    assert(gate_idle(108000,116000)==0);
    assert(g_frame_ok && g_slot==2 && begins==0 && g_late==1);
    g_prepared=0;g_prepare_est=10000;g_slot=1;
    assert(gate_idle(108000,116000)==0);
    assert(!g_frame_ok && begins==0 && g_prepare_est<10000);
    puts("PASS: shared composed matrix motion, FOV, endpoint preservation, cuts and invalid projection rejection");
    return 0;
}
