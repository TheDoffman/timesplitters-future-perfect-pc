/* Read-only room/portal acoustics. The host supplies collision tests on the
 * game thread; no audio callback reads or executes guest memory. */
#ifndef PC_ACOUSTIC_SCENE_H
#define PC_ACOUSTIC_SCENE_H
#include <math.h>
#include <stdint.h>
#include <string.h>
#define PC_ACOUSTIC_ROOMS 201
#define PC_ACOUSTIC_PORTALS 512
typedef struct PcAcousticPortal {unsigned a,b;float center[3];} PcAcousticPortal;
typedef struct PcAcousticScene {
    unsigned rooms,count;
    PcAcousticPortal portals[PC_ACOUSTIC_PORTALS];
} PcAcousticScene;
typedef struct PcAcousticResult {float direction[3],obstruction,room;} PcAcousticResult;
/* -1 means unavailable, 0 obstructed, 1 clear. Invalid geometry fails open. */
typedef int (*PcAcousticRay)(unsigned,const float *,const float *,void *);
static inline int pc_acoustic_bed_kind(unsigned flags,int positioned,const char *asset)
{
    /* The title's effect-stream pool: looping stereo + no world emitter.
       No map or sound IDs. Music has its own pool; dialogue is not looped. */
    if(positioned || (flags&0x210u)!=0x210u)return 0;
    if(!asset || !*asset)return 2; /* unknown bed: do not invent outdoor ownership */
    char name[192];unsigned n=0;
    for(;n<sizeof name-1 && asset[n];n++){char c=asset[n];name[n]=c>='A' && c<='Z' ? (char)(c+32) : c;}
    name[n]=0;
    if(strstr(name,"inside") || strstr(name,"internal") || strstr(name,"_int") ||
       strstr(name,"cave") || strstr(name,"catacomb") || strstr(name,"sewer") ||
       strstr(name,"subamb") || strstr(name,"toilet"))return 2;
    if(strstr(name,"outside") || strstr(name,"_ext") || strstr(name,"amb") ||
       strstr(name,"wind") || strstr(name,"warfare") || strstr(name,"waves") ||
       strstr(name,"seashore"))return 1;
    return 2;
}
static inline float pc_acoustic_distance(const float *a,const float *b)
{float d=0;for(int i=0;i<3;i++){float v=a[i]-b[i];d+=v*v;}return sqrtf(d);}
static inline float pc_acoustic_enclosure(const PcAcousticScene *s,unsigned room,
    const float *listener,PcAcousticRay ray,void *context)
{
    if(!room || room>=s->rooms || s->count>PC_ACOUSTIC_PORTALS)return 0;
    static const float spread[5][2]={{0,0},{40,0},{-40,0},{0,40},{0,-40}};
    unsigned blocked=0;
    for(unsigned i=0;i<5;i++) {
        float sky[3]={listener[0]+spread[i][0],listener[1]+120,listener[2]+spread[i][1]};
        int result=ray(room,listener,sky,context);
        if(result<0)return 0;
        if(!result)blocked++;
    }
    float enclosure=blocked/5.f;
    /* An open entrance makes the exterior bed clearer as it gets closer.
       The mixed stereo recording keeps its authored diffuse direction. */
    unsigned tested=0;
    for(unsigned i=0;i<s->count && tested<12;i++) {
        const PcAcousticPortal *p=&s->portals[i];
        if(p->a!=room && p->b!=room)continue;
        float distance=pc_acoustic_distance(listener,p->center);
        if(distance>12)continue;
        ++tested;if(ray(room,listener,p->center,context)!=1)continue;
        float sky[3]={p->center[0],p->center[1]+120,p->center[2]};
        if(ray(room,p->center,sky,context)==1)enclosure=fminf(enclosure,.25f+.75f*distance/12);
    }
    return enclosure;
}
static inline PcAcousticResult pc_acoustic_route(const PcAcousticScene *s,
    unsigned listener_room,unsigned source_room,const float *listener,const float *source,
    PcAcousticRay ray,void *context)
{
    PcAcousticResult out={{0,0,0},0,0};
    for(int i=0;i<3;i++)out.direction[i]=source[i]-listener[i];
    if(!listener_room || listener_room>=s->rooms || source_room>=s->rooms ||
       s->rooms>PC_ACOUSTIC_ROOMS || s->count>PC_ACOUSTIC_PORTALS ||
       pc_acoustic_distance(listener,source)<2)return out;
    int direct=ray(listener_room,listener,source,context);
    if(direct!=0)return out;
    out.obstruction=1;
    /* Dijkstra over portal centres, rather than room centres. Test each leg:
       a closed door or a wall inside one room must not become an opening. */
    float cost[PC_ACOUSTIC_PORTALS],first[PC_ACOUSTIC_PORTALS][3];
    unsigned arrival[PC_ACOUSTIC_PORTALS];uint8_t done[PC_ACOUSTIC_PORTALS]={0};
    for(unsigned p=0;p<s->count;p++){cost[p]=1e30f;arrival[p]=0;}
    unsigned tests=0;
    for(unsigned p=0;p<s->count;p++) {
        const PcAcousticPortal *v=&s->portals[p];
        if(v->a!=listener_room && v->b!=listener_room)continue;
        if(++tests>64)break;
        if(ray(listener_room,listener,v->center,context)!=1)continue;
        cost[p]=pc_acoustic_distance(listener,v->center);
        arrival[p]=v->a==listener_room?v->b:v->a;
        memcpy(first[p],v->center,12);
    }
    float best=1e30f;int found=-1;
    for(unsigned step=0;step<s->count && tests<96;step++) {
        int pick=-1;float nearest=best;
        for(unsigned p=0;p<s->count;p++)if(!done[p] && cost[p]<nearest){nearest=cost[p];pick=(int)p;}
        if(pick<0)break;
        done[pick]=1;const PcAcousticPortal *v=&s->portals[pick];unsigned room=arrival[pick];
        /* Aircraft/outdoor sources can be above the room-tree bounds. Still
           test their line from each reachable opening instead of bypassing
           the cave roof just because the source has no containing room. */
        if(!source_room || room==source_room) {
            ++tests;
            if(ray(room,v->center,source,context)==1) {
                float d=nearest+pc_acoustic_distance(v->center,source);
                if(d<best){best=d;found=pick;}
            }
        }
        for(unsigned p=0;p<s->count && tests<96;p++) {
            const PcAcousticPortal *w=&s->portals[p];
            if(done[p] || (w->a!=room && w->b!=room))continue;
            float d=nearest+pc_acoustic_distance(v->center,w->center);
            if(d>=cost[p] || d>=best)continue;
            ++tests;if(ray(room,v->center,w->center,context)!=1)continue;
            cost[p]=d;arrival[p]=w->a==room?w->b:w->a;memcpy(first[p],first[pick],12);
        }
    }
    if(found>=0) {
        for(int i=0;i<3;i++)out.direction[i]=first[found][i]-listener[i];
        /* Modest diffraction loss. Keep the already-approved distant range. */
        float extra=best-pc_acoustic_distance(listener,source);
        out.obstruction=fminf(.65f,.3f+fmaxf(0,extra)*.003f);
        out.room=.12f;
    }
    return out;
}
#endif
