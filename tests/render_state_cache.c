/* Complete production state translation with observable immutable RHI objects.
 * Compare emitted descriptors/binds with the pre-cache module, including
 * dynamic stencil references, capacity pressure, failures and device reuse. */
#include "d3d8_internal.h"
#include "d3d8_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %s:%d: %s\n",__FILE__,__LINE__,#x); exit(2); } } while (0)
static unsigned made[4], killed[4], alive[4], peak[4], attempts[4];
static unsigned fail_mask, sets[4];
static uint64_t digest=1469598103934665603ull;
static DWORD render_states[256], texture_states[4][32];
static BOOL scissor_enabled;
static RhiRect scissor_rect;
static D3D8DisplayPolicy policy;
static void hash_bytes(const void *p,size_t n) {
    const unsigned char *s=p;size_t i;
    for(i=0;i<n;i++) digest=(digest^s[i])*1099511628211ull;
}
#define MOCK_STATE(Name, Desc, kind, create, destroy) \
    struct Name { Desc desc; int live; }; \
    Name *create(const Desc *d) { Name *s; ++attempts[kind]; \
        if(fail_mask&(1u<<kind)) { fail_mask&=~(1u<<kind);return NULL; } \
        s=calloc(1,sizeof(*s));CHECK(s);s->desc=*d;s->live=1; \
        ++made[kind];++alive[kind];if(alive[kind]>peak[kind])peak[kind]=alive[kind];return s; } \
    void destroy(Name *s) { if(!s)return;CHECK(s->live);s->live=0; \
        ++killed[kind];CHECK(alive[kind]);--alive[kind]; /* retain tombstone to catch stale use */ }
MOCK_STATE(RhiBlendState,RhiBlendDesc,0,rhi_blend_state_create,rhi_blend_state_destroy)
MOCK_STATE(RhiDepthState,RhiDepthDesc,1,rhi_depth_state_create,rhi_depth_state_destroy)
MOCK_STATE(RhiRasterState,RhiRasterDesc,2,rhi_raster_state_create,rhi_raster_state_destroy)
MOCK_STATE(RhiSampler,RhiSamplerDesc,3,rhi_sampler_create,rhi_sampler_destroy)

int rhi_device_ready(void) { return 1; }
const DWORD *d3d8_GetRenderStates(void) { return render_states; }
const DWORD *d3d8_GetTSS(DWORD stage) { return texture_states[stage]; }
BOOL d3d8_GetScissor(RhiRect *out) { *out=scissor_rect;return scissor_enabled; }
const D3D8DisplayPolicy *d3d8_display_policy(void) { return &policy; }
void rhi_set_blend_state(RhiBlendState *s,const float f[4],uint32_t mask) {
    CHECK(s && s->live);CHECK(f[0]==1 && f[1]==1 && f[2]==1 && f[3]==1);CHECK(mask==0xffffffffu);
    CHECK(s->desc.enable==(render_states[D3DRS_ALPHABLENDENABLE]!=0));
    CHECK(s->desc.write_mask==(render_states[D3DRS_COLORWRITEENABLE]&15));
    ++sets[0];hash_bytes(&s->desc,sizeof(s->desc));
}
void rhi_set_depth_state(RhiDepthState *s,uint32_t ref) {
    CHECK(s && s->live);CHECK(ref==render_states[D3DRS_STENCILREF]);
    CHECK(s->desc.stencil_read_mask==(render_states[D3DRS_STENCILMASK]&255));
    CHECK(s->desc.stencil_write_mask==(render_states[D3DRS_STENCILWRITEMASK]&255));
    CHECK(s->desc.front.func==render_states[D3DRS_STENCILFUNC]);
    CHECK(!memcmp(&s->desc.front,&s->desc.back,sizeof(s->desc.front)));
    ++sets[1];hash_bytes(&s->desc,sizeof(s->desc));hash_bytes(&ref,sizeof(ref));
}
void rhi_set_raster_state(RhiRasterState *s) {
    CHECK(s && s->live);CHECK(s->desc.scissor==(scissor_enabled!=0));
    ++sets[2];hash_bytes(&s->desc,sizeof(s->desc));
}
void rhi_set_samplers(uint32_t slot,uint32_t count,RhiSampler *const *s) {
    CHECK(slot<4 && count==1 && s[0] && s[0]->live);++sets[3];
    hash_bytes(&slot,sizeof(slot));hash_bytes(&s[0]->desc,sizeof(s[0]->desc));
}
void rhi_set_scissor(const RhiRect *r) { CHECK(!memcmp(r,&scissor_rect,sizeof(*r)));hash_bytes(r,sizeof(*r)); }

#include "render_state_impl.h"

static void defaults(void) {
    memset(render_states,0,sizeof(render_states));
    render_states[D3DRS_SRCBLEND]=D3DBLEND_SRCALPHA;
    render_states[D3DRS_DESTBLEND]=D3DBLEND_INVSRCALPHA;
    render_states[D3DRS_BLENDOP]=1;render_states[D3DRS_COLORWRITEENABLE]=15;
    render_states[D3DRS_ZENABLE]=1;render_states[D3DRS_ZWRITEENABLE]=1;
    render_states[D3DRS_ZFUNC]=D3DCMP_LESSEQUAL;
    render_states[D3DRS_STENCILFUNC]=D3DCMP_ALWAYS;
    render_states[D3DRS_STENCILMASK]=255;render_states[D3DRS_STENCILWRITEMASK]=255;
    render_states[D3DRS_STENCILFAIL]=1;render_states[D3DRS_STENCILZFAIL]=1;render_states[D3DRS_STENCILPASS]=1;
    render_states[D3DRS_FILLMODE]=D3DFILL_SOLID;render_states[D3DRS_CULLMODE]=D3DCULL_CCW;
}
static void material(unsigned i) {
    render_states[D3DRS_ALPHABLENDENABLE]=i&1;
    render_states[D3DRS_COLORWRITEENABLE]=(i&2)?7:15;
    render_states[D3DRS_ZWRITEENABLE]=(i>>1)&1;
    render_states[D3DRS_STENCILENABLE]=(i>>2)&1;
    render_states[D3DRS_CULLMODE]=1+i%3;
    render_states[D3DRS_STENCILREF]=i*13;
    scissor_enabled=(i>>3)&1;
    scissor_rect.left=(int32_t)i;scissor_rect.top=2;scissor_rect.right=320;scissor_rect.bottom=200;
}
int main(void) {
    unsigned i,j,initial[3];defaults();CHECK(d3d8_states_init()==S_OK);
    for(i=0;i<2000;i++) { material(i%16);d3d8_states_apply(); }
    fprintf(stderr,"reuse-phase creates=%u,%u,%u\n",made[0],made[1],made[2]);
#ifdef EXPECT_CACHE
    CHECK(made[0]==4 && made[1]==4 && made[2]==6);
    CHECK(killed[0]==0 && killed[1]==0 && killed[2]==0);
#endif
    /* Vary every depth/stencil field, force replacement, then revisit evicted
     * descriptors. Reference is dynamic and must be bound on every apply. */
    for(j=0;j<3;j++) for(i=0;i<96;i++) {
        material(i);
        render_states[D3DRS_STENCILMASK]=i;
        render_states[D3DRS_STENCILWRITEMASK]=95-i;
        render_states[D3DRS_STENCILFUNC]=1+i%8;
        render_states[D3DRS_ZENABLE]=i&1;
        render_states[D3DRS_ZFUNC]=1+(i/8)%8;
        render_states[D3DRS_STENCILFAIL]=1+(i/16)%6;
        render_states[D3DRS_STENCILZFAIL]=1+(i/8)%8;
        render_states[D3DRS_STENCILPASS]=1+i%8;
        render_states[D3DRS_SRCBLEND]=1+i%11;
        render_states[D3DRS_DESTBLEND]=1+(i/11)%8;
        render_states[D3DRS_BLENDOP]=1+i%5;
        d3d8_states_apply();
    }
    for(i=0;i<3;i++) initial[i]=attempts[i];
    /* Force fresh descriptors; failed creates must not be cached as hits. */
    defaults();render_states[D3DRS_STENCILMASK]=211;
    render_states[D3DRS_STENCILWRITEMASK]=213;
    render_states[D3DRS_SRCBLEND]=D3DBLEND_SRCCOLOR;
    render_states[D3DRS_DESTBLEND]=D3DBLEND_DESTCOLOR;
    render_states[D3DRS_COLORWRITEENABLE]=3;
    render_states[D3DRS_FILLMODE]=D3DFILL_WIREFRAME;
    fail_mask=7;d3d8_states_apply();CHECK(fail_mask==0);
    d3d8_states_apply();
    for(i=0;i<3;i++) CHECK(attempts[i]==initial[i]+2);
    d3d8_states_shutdown();
    for(i=0;i<4;i++) CHECK(!alive[i] && made[i]==killed[i]);
    d3d8_states_shutdown(); /* Idempotent, no double ownership. */
    defaults();CHECK(d3d8_states_init()==S_OK);d3d8_states_apply();d3d8_states_shutdown();
    for(i=0;i<4;i++) CHECK(!alive[i] && made[i]==killed[i]);
#ifdef EXPECT_CACHE
    for(i=0;i<3;i++) CHECK(peak[i]<=33);
#endif
    printf("bind-digest=%llu sets=%u,%u,%u,%u\n",(unsigned long long)digest,sets[0],sets[1],sets[2],sets[3]);
    return 0;
}
