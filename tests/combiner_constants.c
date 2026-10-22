#include <assert.h>
#include <stdio.h>
#include "graphics/combiner_constants.h"
#include "pc_surface_bias.h"
#include "d3d8_capture.h"
static PcCombinerConstants constants;
static int deferred,hle_d3d8_interp_rec;
static float g_surface_bias_step=-1;
static float g_surface_coefficients[3];
static int g_surface_coefficients_valid;
static D3D8CapWriter *g_cap;
typedef struct { void (*op)(const void *); D3D8CapPsFloatConstant payload; } Queued;
static Queued queued[32],recorded[32];
static unsigned count,record_count;
static int hle_d3d8_defer_recording(void) { return deferred; }
static void hle_d3d8_defer_op(void (*op)(const void *),const void *p,size_t size)
{ assert(count<32 && size==sizeof queued[0].payload);queued[count].op=op;memcpy(&queued[count++].payload,p,size); }
static void hle_d3d8_interp_op(void (*op)(const void *),const void *p,size_t size)
{ assert(record_count<32 && size==sizeof recorded[0].payload);recorded[record_count].op=op;memcpy(&recorded[record_count++].payload,p,size); }
static void d3d8_combiners_set_float_constant(unsigned bank,unsigned stage,const float *value)
{ assert(pc_combiner_constant_set(&constants,bank,stage,value)); }
static void chunk(uint32_t type,const void *a,size_t an,const void *b,size_t bn,const void *c,size_t cn)
{ assert(d3d8cap_chunk(g_cap,type,a,an,b,bn,c,cn)==0); }
void host_combiners_set_float_constant(unsigned bank,unsigned stage,const float *value);
/* PRODUCTION */
typedef struct { unsigned malformed; } Replay;
static void replay(Replay *r,const D3D8CapChunk *c) {
    switch(c->type) {
/* REPLAY_CASE */
    default:break;
    }
}
static void expect(float value) {
    float got[4]={0};assert(pc_combiner_constant_get(&constants,1,6,got));
    assert(got[0]==0 && got[1]==value && got[2]==0 && got[3]==0);
}
int main(void) {
    float value[4]={0,pc_surface_bias(60.f/144),0,0};
    float saved=value[1],untouched[4]={1,2,3,4};
    assert(!pc_combiner_constant_get(&constants,1,6,untouched) && untouched[1]==2);
    assert(!pc_combiner_constant_set(&constants,2,0,value));
    assert(!pc_combiner_constant_set(&constants,0,8,value));
    deferred=1;
    host_combiners_set_float_constant(1,6,value);
    value[1]=pc_surface_bias(.5f);
    host_combiners_set_float_constant(1,6,value);
    host_combiners_set_float_constant(1,6,NULL);
    value[1]=NAN;host_combiners_set_float_constant(1,6,value);
    value[1]=INFINITY;host_combiners_set_float_constant(1,6,value);
    host_combiners_set_float_constant(2,6,NULL);
    assert(count==3 && constants.mask[1]==0);
    g_cap=d3d8cap_create("constants.d3dcap",0,1,1);assert(g_cap);
    deferred=0;hle_d3d8_interp_rec=1;
    queued[0].op(&queued[0].payload);expect(saved);
    queued[1].op(&queued[1].payload);expect(pc_surface_bias(.5f));
    queued[2].op(&queued[2].payload);assert(constants.mask[1]==0);
    assert(record_count==3 && d3d8cap_close(g_cap)==0);g_cap=NULL;
    hle_d3d8_interp_rec=0;
    for(unsigned i=0;i<3;i++)recorded[i].op(&recorded[i].payload);
    assert(constants.mask[1]==0);
    char err[256];D3D8CapReader *reader=d3d8cap_open("constants.d3dcap",err,sizeof err);assert(reader);
    assert(d3d8cap_header(reader)->version==9);
    Replay r={0};D3D8CapChunk c;
    for(unsigned i=0;i<3;i++) {
        assert(d3d8cap_next(reader,&c)>0 && c.type==D3D8CAP_PS_FLOAT_CONSTANT);
        replay(&r,&c);
        if(i<2)expect(i?pc_surface_bias(.5f):saved);else assert(constants.mask[1]==0);
    }
    d3d8cap_close_read(reader);assert(r.malformed==0);
    D3D8CapPsFloatConstant bad={1,6,1,{0,0,0,0}};
    c.type=D3D8CAP_PS_FLOAT_CONSTANT;c.data=&bad;c.bytes=sizeof bad-1;replay(&r,&c);
    c.bytes=sizeof bad;bad.bank=2;replay(&r,&c);bad.bank=1;
    bad.stage=8;replay(&r,&c);bad.stage=6;
    bad.enabled=2;replay(&r,&c);bad.enabled=1;
    bad.value[1]=NAN;replay(&r,&c);bad.value[1]=INFINITY;replay(&r,&c);
    assert(r.malformed==6 && constants.mask[1]==0);
    count=0;deferred=1;
    const float authored[3]={.105f,.65f,.98f};
    hle_d3d8_surface_coefficients(authored[0],authored[1],authored[2]);
    hle_d3d8_surface_bias_step(60.f/144);forward_surface_bias(0x4bbf10u);
    forward_surface_bias(0x4bbf10u); /* cached duplicate */
    hle_d3d8_surface_bias_step(.5f);forward_surface_bias(0x4bbf10u);
    forward_surface_bias(0x4bbe20u); /* other pass clears every override */
    hle_d3d8_surface_bias_step(1);forward_surface_bias(0x4bbf10u);
    hle_d3d8_surface_bias_step(-1);forward_surface_bias(0x4bbf10u);
    assert(count==12);deferred=0;record_count=0;hle_d3d8_interp_rec=1;
    /* Queued payloads own their values, independent of changed live params. */
    hle_d3d8_surface_coefficients(.9f,.8f,.7f);
    for(unsigned group=0;group<3;group++) {
        for(unsigned i=0;i<4;i++)queued[group*4+i].op(&queued[group*4+i].payload);
        if(group==2)assert(!constants.mask[0] && !constants.mask[1]);
        else {
            float expected[3],got[4]={0};const unsigned stages[3]={3,5,7};
            float delta=group?.5f:60.f/144;expect(pc_surface_bias(delta));
            pc_surface_coefficients(delta,authored,expected);
            assert(constants.mask[0]==0xa8 && constants.mask[1]==0x40);
            for(unsigned i=0;i<3;i++) {
                assert(pc_combiner_constant_get(&constants,0,stages[i],got));
                assert(got[0]==0 && got[i==0?1:2]==expected[i]);
                assert(got[3]==(i==0?0:expected[i]));
            }
        }
    }
    assert(record_count==12);hle_d3d8_interp_rec=0;
    for(unsigned i=0;i<record_count;i++)recorded[i].op(&recorded[i].payload);
    assert(!constants.mask[0] && !constants.mask[1]);
    /* Same delta with new parameters refreshes coefficients. Invalid input,
       zero/full/catch-up steps and scope exit cannot leak fractional values. */
    hle_d3d8_surface_bias_step(.5f);forward_surface_bias(0x4bbf10u);
    float before[4]={0},after[4]={0};assert(pc_combiner_constant_get(&constants,0,3,before));
    hle_d3d8_surface_coefficients(.2f,.8f,.7f);forward_surface_bias(0x4bbf10u);
    assert(pc_combiner_constant_get(&constants,0,3,after) && before[1]!=after[1]);
    hle_d3d8_surface_coefficients(NAN,.8f,.7f);forward_surface_bias(0x4bbf10u);
    assert(!constants.mask[0] && constants.mask[1]==0x40);
    hle_d3d8_surface_bias_step(-1);forward_surface_bias(0x4bbf10u);
    assert(!constants.mask[0] && !constants.mask[1]);
    const float exits[]={0,1,3,-1,NAN};
    for(unsigned i=0;i<sizeof exits/sizeof exits[0];i++) {
        hle_d3d8_surface_coefficients(.105f,.65f,.98f);
        hle_d3d8_surface_bias_step(.5f);forward_surface_bias(0x4bbf10u);
        assert(constants.mask[0]==0xa8 && constants.mask[1]==0x40);
        hle_d3d8_surface_bias_step(exits[i]);forward_surface_bias(0x4bbf10u);
        assert(!constants.mask[0] && !constants.mask[1]);
    }
    assert(pc_surface_packed_coefficient(.5f)==127.f/255.f);
    assert(pc_surface_packed_coefficient(-1)==0 && pc_surface_packed_coefficient(2)==1);
    assert(pc_combiner_constant_set(&constants,0,7,untouched));
    pc_combiner_constants_reset(&constants);assert(!constants.mask[0] && !constants.mask[1]);
    puts("PASS: queued value ownership, shader scope, clear order, capture round trip and malformed replay");
    return 0;
}
