#ifndef TSFP_PC_PHYSICAL_STATE_H
#define TSFP_PC_PHYSICAL_STATE_H
#include <stdint.h>
#include <math.h>
/* Numeric rigid-body state. Handles, collision shapes and contacts stay local. */
typedef struct PcRigidBodyState {
    uint32_t present;
    float rotation[9],position[3],velocity[3],angular[3];
} PcRigidBodyState;
static inline int pc_rigid_body_valid(const PcRigidBodyState *p)
{
    if(p->present>1u) return 0;
    if(!p->present) return 1;
    for(unsigned i=0;i<9;i++) if(!isfinite(p->rotation[i]) || fabsf(p->rotation[i])>1000000.f) return 0;
    /* Reject degenerate/skewed rotations before the original quaternion setter. */
    for(unsigned r=0;r<3u;r++) {
        float length=0.f;
        for(unsigned c=0;c<3u;c++) length+=p->rotation[r*3u+c]*p->rotation[r*3u+c];
        if(length<.75f || length>1.25f) return 0;
        for(unsigned q=0;q<r;q++) {
            float dot=0.f;for(unsigned c=0;c<3u;c++) dot+=p->rotation[r*3u+c]*p->rotation[q*3u+c];
            if(fabsf(dot)>.125f) return 0;
        }
    }
    const float *m=p->rotation;
    const float determinant=m[0]*(m[4]*m[8]-m[5]*m[7])-m[1]*(m[3]*m[8]-m[5]*m[6])+m[2]*(m[3]*m[7]-m[4]*m[6]);
    if(determinant<.75f || determinant>1.25f) return 0;
    for(unsigned i=0;i<3;i++) if(!isfinite(p->position[i]) || fabsf(p->position[i])>1000000.f ||
        !isfinite(p->velocity[i]) || fabsf(p->velocity[i])>100000.f ||
        !isfinite(p->angular[i]) || fabsf(p->angular[i])>100000.f) return 0;
    return 1;
}
#define PC_RAGDOLL_PARTS 8u
typedef struct PcRagdollState {
    uint32_t count,kind;
    PcRigidBodyState parts[PC_RAGDOLL_PARTS];
} PcRagdollState;
static inline int pc_ragdoll_valid(const PcRagdollState *s)
{
    static const unsigned counts[7]={8,6,5,4,3,3,5};
    if(!s->count) return s->kind==0;
    if(s->kind>=7u || s->count!=counts[s->kind]) return 0;
    for(unsigned i=0;i<s->count;i++)
        if(!s->parts[i].present || !pc_rigid_body_valid(&s->parts[i])) return 0;
    return 1;
}
void pc_character_ragdoll_capture(PcRagdollState *state,uint32_t entity);
void pc_character_ragdoll_present(uint32_t entity,const PcRagdollState *state);
void pc_character_ragdoll_release(uint32_t entity);
void pc_character_ragdoll_evaluate(uint32_t entity);
int32_t pc_character_ragdoll_clip(uint32_t entity,int32_t clip);
void pc_rigid_body_capture(PcRigidBodyState *state,uint32_t entity);
void pc_character_ragdoll_preserve(uint32_t entity,uint32_t corpse);
void pc_rigid_body_pause(uint32_t entity);
void pc_rigid_body_remove(uint32_t entity);
int pc_rigid_body_present(uint32_t entity,const PcRigidBodyState *state);
#endif
