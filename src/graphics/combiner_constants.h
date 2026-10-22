#ifndef TSFP_COMBINER_CONSTANTS_H
#define TSFP_COMBINER_CONSTANTS_H
#include <stdint.h>
#include <string.h>
#include <math.h>
/* Host float overrides are separate from the guest's packed render states. */
typedef struct { uint32_t mask[2]; float value[2][8][4]; } PcCombinerConstants;
static inline void pc_combiner_constants_reset(PcCombinerConstants *s) {memset(s,0,sizeof *s);}
static inline int pc_combiner_constant_set(PcCombinerConstants *s,unsigned bank,unsigned stage,const float *value)
{
    if(bank>=2 || stage>=8)return 0;
    if(value) {
        for(unsigned c=0;c<4;c++)if(!isfinite(value[c]))return 0;
        memcpy(s->value[bank][stage],value,4*sizeof(float));s->mask[bank]|=1u<<stage;
    } else s->mask[bank]&=~(1u<<stage);
    return 1;
}
static inline int pc_combiner_constant_get(const PcCombinerConstants *s,unsigned bank,unsigned stage,float *value)
{
    if(bank>=2 || stage>=8 || !(s->mask[bank]&(1u<<stage)))return 0;
    if(value)memcpy(value,s->value[bank][stage],4*sizeof(float));
    return 1;
}
#endif
