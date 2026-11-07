#include <assert.h>
#include <stdio.h>
#include "gameplay/pc_acoustic_scene.h"
#include "gameplay/pc_acoustic_cache.h"
#include "gameplay/pc_acoustic_dsp.h"
static int door_closed,unavailable;
static int roof,entrance;
static int sky_ray(unsigned room,const float *a,const float *b,void *ctx)
{
    (void)room;(void)ctx;
    if(b[1]>100)return !roof || (entrance && a[0]>=4.5f);
    return !door_closed;
}
static int ray(unsigned room,const float *a,const float *b,void *ctx)
{
    (void)ctx;(void)room;
    if(unavailable)return -1;
    /* A wall on z=0 splits the two points; only opening is at z=5. */
    if(a[2]==0 && b[2]==0)return 0;
    return !door_closed;
}
static float energy(PcAcousticDsp *d,float obstruction,int high)
{
    static int16_t pcm[4800];pc_acoustic_target(d,obstruction,0);
    for(int i=0;i<4800;i++)pcm[i]=(int16_t)(12000*sin((high?2.5:.03)*i));
    pc_acoustic_pcm(d,(uint8_t*)pcm,sizeof pcm,48000,1,16);
    double e=0;for(int i=2400;i<4800;i++)e+=(double)pcm[i]*pcm[i];
    return (float)e;
}
static float bed_level(float level,int bed,int channel)
{
    static PcAcousticDsp d;static int16_t pcm[19200];memset(&d,0,sizeof d);
    for(unsigned i=0;i<9600;i++){pcm[i*2]=(int16_t)(level*32767);pcm[i*2+1]=(int16_t)(level*.5f*32767);}
    pc_acoustic_ambience_target(&d,bed);
    pc_acoustic_pcm(&d,(uint8_t*)pcm,sizeof pcm,48000,2,16);
    return pcm[19198+channel]/32768.f;
}
static void finite_bypass(unsigned rate,unsigned channels,unsigned bits)
{
    static PcAcousticDsp dsp;static uint8_t pcm[384000];memset(&dsp,0,sizeof dsp);
    unsigned bytes=rate*channels*(bits/8);
    memset(pcm,bits==8?160:42,bytes);
    pc_acoustic_target(&dsp,1,.1f);pc_acoustic_ambience_target(&dsp,1);
    pc_acoustic_pcm(&dsp,pcm,bytes,rate,channels,bits);
    assert(dsp.amount>.99f && dsp.bed_amount>.99f);
    pc_acoustic_target(&dsp,0,0);pc_acoustic_ambience_target(&dsp,0);
    /* A one-second release packet must reach exact bypass within that packet,
       not leave an asymptotic state processing (and requantizing) every voice. */
    memset(pcm,42,bytes);pc_acoustic_pcm(&dsp,pcm,bytes,rate,channels,bits);
    for(unsigned i=bytes/2;i<bytes;i++)assert(pcm[i]==42);
    assert(dsp.amount==0 && dsp.bed_amount==0 && dsp.rate==0);
    uint8_t endpoints[]={0,128,255,0,255,127,1,128};
    uint8_t copy[sizeof endpoints];memcpy(copy,endpoints,sizeof copy);
    pc_acoustic_pcm(&dsp,endpoints,sizeof endpoints,rate,channels,bits);
    assert(!memcmp(copy,endpoints,sizeof copy));
    /* Re-enabling must still rebuild useful filtering after history is cleared. */
    pc_acoustic_target(&dsp,1,0);memset(pcm,42,bytes);
    pc_acoustic_pcm(&dsp,pcm,bytes,rate,channels,bits);assert(dsp.amount>.99f);
}
static void cache_contract(void)
{
    static PcAcousticCache cache;
    const float listener[3]={0,0,0},source[3]={10,0,0};
    float moved_listener[3]={2,0,0},moved_source[3]={12,0,0};
    PcAcousticResult out,blocked={{10,0,0},1,0},routed={{5,0,5},.3f,.12f};
    assert(pc_acoustic_cache_request(&cache,0x1000,1,2,3,1,1,2,10000,listener,source,&out));
    pc_acoustic_cache_store(&cache,0x1000,10000,listener,blocked);
    assert(!pc_acoustic_cache_request(&cache,0x1000,1,2,3,1,1,2,10001,moved_listener,moved_source,&out));
    assert(out.obstruction==1 && out.direction[0]==10);
    pc_acoustic_cache_store(&cache,0x1000,10001,listener,routed);
    assert(!pc_acoustic_cache_request(&cache,0x1000,1,2,3,1,1,2,10002,moved_listener,moved_source,&out));
    assert(out.direction[0]==3 && out.direction[2]==5); /* opening stays in world space */
    cache.budget=2;cache.budget_time=120002;
    assert(!pc_acoustic_cache_request(&cache,0x1000,1,2,3,1,1,2,120002,moved_listener,moved_source,&out));
    assert(out.obstruction==.3f && out.direction[0]==3); /* bounded backlog reuse */
    cache.budget=2;cache.budget_time=300002;
    assert(!pc_acoustic_cache_request(&cache,0x1000,1,2,3,1,1,2,300002,listener,source,&out));
    assert(out.obstruction==0 && out.direction[0]==10); /* never reuse indefinitely */
    /* Separate pointers prevent XOR scene collisions; room changes and cache
       slot reuse must not carry obstruction across an exhausted query budget. */
    pc_acoustic_cache_store(&cache,0x1000,300002,listener,blocked);
    assert(!pc_acoustic_cache_request(&cache,0x1000,5,6,3,1,1,2,300003,listener,source,&out));
    assert(out.obstruction==0);
    pc_acoustic_cache_store(&cache,0x1000,300003,listener,blocked);
    assert(!pc_acoustic_cache_request(&cache,0x1000,5,6,4,1,1,2,300004,listener,source,&out));
    assert(out.obstruction==0);
    pc_acoustic_cache_store(&cache,0x1000,300004,listener,blocked);
    assert(!pc_acoustic_cache_request(&cache,0x1000,5,6,4,1,2,2,300005,listener,source,&out));
    assert(out.obstruction==0);
    pc_acoustic_cache_store(&cache,0x1000,300005,listener,blocked);
    assert(!pc_acoustic_cache_request(&cache,0x2000,5,6,4,1,2,2,300006,listener,source,&out));
    assert(out.obstruction==0);
    memset(&cache,0,sizeof cache);
    unsigned refreshes[64]={0};
    /* More voices than fit into one TTL, all sharing the same hash bucket,
       always visited in identical order.
       Every continuously requested voice must make progress within the budget. */
    for(unsigned tick=1;tick<=120;tick++) {
        unsigned queries=0;
        for(unsigned i=0;i<64;i++) {
            uint32_t key=0x1000+i*0x1000;uint64_t now=tick*10000u;
            if(pc_acoustic_cache_request(&cache,key,1,2,3,1,1,2,now,listener,source,&out)) {
                queries++;refreshes[i]++;pc_acoustic_cache_store(&cache,key,now,listener,blocked);
            }
        }
        assert(queries<=2);
    }
    for(unsigned i=0;i<64;i++)assert(refreshes[i]>=3);
    memset(&cache,0,sizeof cache);
    for(unsigned i=0;i<2;i++) {
        assert(pc_acoustic_cache_request(&cache,0x1000+i*16,1,2,3,1,1,2,10000,listener,source,&out));
        pc_acoustic_cache_store(&cache,0x1000+i*16,10000,listener,blocked);
    }
    assert(!pc_acoustic_cache_request(&cache,0x1020,1,2,3,1,1,2,10000,listener,source,&out));
    assert(!pc_acoustic_cache_request(&cache,0x1030,1,2,3,1,1,2,20000,listener,source,&out));
    assert(pc_acoustic_cache_request(&cache,0x1030,1,2,3,1,1,2,110000,listener,source,&out));
}
int main(void)
{
    cache_contract();
    const float a[]={0,0,0},b[]={10,0,0};
    PcAcousticScene s={0};s.rooms=3;s.count=1;
    s.portals[0]=(PcAcousticPortal){1,2,{5,0,5}};
    PcAcousticResult r=pc_acoustic_route(&s,1,2,a,b,ray,NULL);
    assert(r.obstruction>0 && r.obstruction<1 && r.direction[2]==5);
    r=pc_acoustic_route(&s,1,0,a,b,ray,NULL);
    assert(r.obstruction>0 && r.obstruction<1 && r.direction[2]==5);
    door_closed=1;r=pc_acoustic_route(&s,1,2,a,b,ray,NULL);assert(r.obstruction==1);
    unavailable=1;r=pc_acoustic_route(&s,1,2,a,b,ray,NULL);assert(r.obstruction==0);
    unavailable=0;door_closed=0;
    /* Same-room walls still obstruct; unknown/menu rooms do not. */
    r=pc_acoustic_route(&s,1,1,a,b,ray,NULL);assert(r.obstruction==1);
    r=pc_acoustic_route(&s,0,2,a,b,ray,NULL);assert(r.obstruction==0);
    /* Metadata-driven across maps, not a Vietnam/sound-ID exception. */
    assert(pc_acoustic_bed_kind(0x251,0,"l48_vietnam_ambience22.MIB")==1);
    assert(pc_acoustic_bed_kind(0x251,0,"NEW_MAP_OUTSIDE_WIND.MIB")==1);
    assert(pc_acoustic_bed_kind(0x251,0,"new_map_cave_ambience.MIB")==2);
    assert(pc_acoustic_bed_kind(0x251,0,NULL)==2);
    assert(pc_acoustic_bed_kind(0x251,1,"world_emitter.MIB")==0);
    assert(pc_acoustic_bed_kind(0x200,0,"dialogue.MIB")==0);
    assert(pc_acoustic_bed_kind(0x10,0,"mono_loop.VAG")==0);
    assert(pc_acoustic_enclosure(&s,1,a,sky_ray,NULL)==0);
    roof=1;assert(pc_acoustic_enclosure(&s,1,a,sky_ray,NULL)==1);
    entrance=1;float sheltered=pc_acoustic_enclosure(&s,1,a,sky_ray,NULL);
    assert(sheltered>.25f && sheltered<1);
    door_closed=1;assert(pc_acoustic_enclosure(&s,1,a,sky_ray,NULL)==1);
    static PcAcousticDsp clean,blocked,low;
    float original=energy(&clean,0,1),muffled=energy(&blocked,1,1);
    float bass=energy(&low,1,0);
    assert(muffled<original*.05f && bass>muffled*5);
    float quiet=bed_level(.1f,1,0),loud=bed_level(.9f,1,0),right=bed_level(.9f,1,1);
    assert(quiet>.045f && quiet<.055f); /* approximately -6 dB background level */
    assert(loud<.25f && loud>quiet*3); /* peaks softened without flattening everything */
    assert(fabsf(loud-right*2)<.001f); /* linked control preserves stereo balance */
    assert(bed_level(.9f,0,0)>.89f); /* foreground processing bypass */
    /* Disabled processing is byte-identical, including unsigned silence. */
    uint8_t data[]={0,128,255};uint8_t copy[3];memcpy(copy,data,3);
    pc_acoustic_pcm(&clean,data,3,48000,1,8);assert(!memcmp(copy,data,3));
    pc_acoustic_target(&blocked,NAN,NAN);assert(blocked.target==0 && blocked.wet==0);
    finite_bypass(8000,1,8);finite_bypass(48000,2,16);
    /* A reflection-only voice also clears its old echo on bypass. Otherwise
       re-enabling the room send can replay sound from its previous use. */
    static PcAcousticDsp reflection;static int16_t echo_pcm[8000];
    for(unsigned i=0;i<8000;i++)echo_pcm[i]=10000;
    pc_acoustic_target(&reflection,0,.1f);
    pc_acoustic_pcm(&reflection,(uint8_t*)echo_pcm,sizeof echo_pcm,8000,1,16);
    pc_acoustic_target(&reflection,0,0);memset(echo_pcm,0,sizeof echo_pcm);
    pc_acoustic_pcm(&reflection,(uint8_t*)echo_pcm,160,8000,1,16);
    pc_acoustic_target(&reflection,0,.1f);
    pc_acoustic_pcm(&reflection,(uint8_t*)echo_pcm,160,8000,1,16);
    for(unsigned i=0;i<80;i++)assert(echo_pcm[i]==0);
    puts("PASS: walls, opening direction, closed doors, safe fallback and low-pass response");
}
