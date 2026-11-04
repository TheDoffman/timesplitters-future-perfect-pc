/* Match fragment input widths to the vertex outputs without maintenance4.
 * Receive the full vector and extract the original lanes after each load.
 * Shader arithmetic, semantic locations and interpolation decorations stay intact. */
#ifndef TSFP_VULKAN_INTERFACE_H
#define TSFP_VULKAN_INTERFACE_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef struct { uint32_t variable, components; } TsfpVkInputWidth;
typedef struct { uint32_t type, element, count, storage, variable_type; } TsfpVkSpvType;
typedef struct { uint32_t variable, old_type, new_type, pointer, old_count; } TsfpVkSpvInput;
static int tsfp_vk_widen_inputs(const uint32_t *code,size_t words,
    const TsfpVkInputWidth *widths,size_t count,uint32_t **output,size_t *output_words)
{
    TsfpVkSpvType *types=NULL;
    TsfpVkSpvInput changes[32];size_t nchanges=0,first_function=0,n=0;
    uint32_t declarations[32*8];size_t ndeclarations=0;
    uint32_t *result=NULL,bound,next;
    if(!output||!output_words)return 0;
    *output=NULL;*output_words=0;
    if(!code||words<5||code[0]!=0x07230203u||count>32||(!widths&&count))return 0;
    bound=code[3];if(!bound||bound>1048576||words>16777216)return 0;
    types=(TsfpVkSpvType *)calloc(bound+count*2,sizeof *types);if(!types)return 0;
    next=bound;
    for(size_t i=5;i<words;) {
        uint32_t len=code[i]>>16,op=code[i]&65535;
        if(!len||len>words-i)goto fail;
        if(op==54&&!first_function)first_function=i;
        if(op==21||op==22||op==23||op==32) {
            if(len<3||code[i+1]>=bound)goto fail;
            TsfpVkSpvType *t=&types[code[i+1]];t->type=op;
            if(op==21||op==22)t->count=code[i+2];
            else {if(len!=4)goto fail;t->element=code[i+3];
                if(op==23){t->element=code[i+2];t->count=code[i+3];}
                else t->storage=code[i+2];}
        } else if(op==59) {
            if(len<4||code[i+2]>=bound||code[i+1]>=bound)goto fail;
            types[code[i+2]].variable_type=code[i+1];
        }
        i+=len;
    }
    if(!first_function)goto fail;
    for(size_t i=0;i<count;i++) {
        uint32_t variable=widths[i].variable,to=widths[i].components;
        if(variable>=bound||to<1||to>4)goto fail;
        uint32_t ptr=types[variable].variable_type;
        if(!ptr||types[ptr].type!=32||types[ptr].storage!=1)goto fail;
        uint32_t old=types[ptr].element;if(old>=bound)goto fail;
        uint32_t from=types[old].type==23?types[old].count:1;
        uint32_t scalar=types[old].type==23?types[old].element:old;
        if(scalar>=bound||(types[scalar].type!=21&&types[scalar].type!=22)||types[scalar].count!=32||to<from)goto fail;
        if(to==from)continue;
        for(size_t k=0;k<nchanges;k++)if(changes[k].variable==variable)goto fail;
        uint32_t vector=0,pointer=0;
        for(uint32_t k=1;k<next;k++)if(types[k].type==23&&types[k].element==scalar&&types[k].count==to){vector=k;break;}
        if(!vector){vector=next++;types[vector]=(TsfpVkSpvType){23,scalar,to,0,0};
            declarations[ndeclarations++]=(4u<<16)|23;declarations[ndeclarations++]=vector;
            declarations[ndeclarations++]=scalar;declarations[ndeclarations++]=to;}
        for(uint32_t k=1;k<next;k++)if(types[k].type==32&&types[k].element==vector&&types[k].storage==1){pointer=k;break;}
        if(!pointer){pointer=next++;types[pointer]=(TsfpVkSpvType){32,vector,0,1,0};
            declarations[ndeclarations++]=(4u<<16)|32;declarations[ndeclarations++]=pointer;
            declarations[ndeclarations++]=1;declarations[ndeclarations++]=vector;}
        changes[nchanges++]=(TsfpVkSpvInput){variable,old,vector,pointer,from};
    }
    if(!nchanges){free(types);return 1;}
    /* At most eight extraction words per input load plus relocated globals. */
    result=(uint32_t *)malloc((words*3+ndeclarations+count*4)*sizeof *result);
    if(!result)goto fail;
    memcpy(result,code,5*sizeof *result);n=5;
    for(size_t i=5;i<words;) {
        uint32_t len=code[i]>>16,op=code[i]&65535;size_t change=nchanges;
        if(i==first_function) {
            memcpy(result+n,declarations,ndeclarations*sizeof *result);n+=ndeclarations;
            for(size_t k=0;k<nchanges;k++) {
                result[n++]=(4u<<16)|59;result[n++]=changes[k].pointer;
                result[n++]=changes[k].variable;result[n++]=1;
            }
        }
        if(op==59&&len>=4)for(size_t k=0;k<nchanges;k++)if(code[i+2]==changes[k].variable){change=k;break;}
        if(change<nchanges){if(i>=first_function||len!=4)goto fail;i+=len;continue;}
        if(op==61&&len>=4)for(size_t k=0;k<nchanges;k++)if(code[i+3]==changes[k].variable){change=k;break;}
        if(change<nchanges) {
            TsfpVkSpvInput *c=&changes[change];uint32_t temp=next++;
            if(code[i+1]!=c->old_type||code[i+2]>=bound)goto fail;
            memcpy(result+n,code+i,len*sizeof *result);result[n+1]=c->new_type;result[n+2]=temp;n+=len;
            result[n++]=((c->old_count==1?5u:5u+c->old_count)<<16)|(c->old_count==1?81u:79u);
            result[n++]=c->old_type;result[n++]=code[i+2];result[n++]=temp;
            if(c->old_count>1)result[n++]=temp;
            for(uint32_t lane=0;lane<c->old_count;lane++)result[n++]=lane;
        } else {memcpy(result+n,code+i,len*sizeof *result);n+=len;}
        i+=len;
    }
    result[3]=next;free(types);*output=result;*output_words=n;return 1;
fail:
    free(types);free(result);return 0;
}
#endif
