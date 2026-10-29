#include <assert.h>
#include <stdio.h>
#include "gameplay/pc_acoustic_scene.h"
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
int main(void)
{
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
    puts("PASS: walls, opening direction, closed doors, safe fallback and low-pass response");
}
