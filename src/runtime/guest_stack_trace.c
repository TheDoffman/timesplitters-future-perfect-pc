/* Original bounded EBP collection and staged little-endian diagnostic output. */
#include "guest_stack_trace.h"
#include <stdlib.h>
#include <string.h>
static unsigned char *trace_span(const tsfp_guest_vm *vm,uint32_t a,uint32_t n,int write) {
    if(!vm || !vm->resolve || !a || !n || a>UINT32_MAX-(n-1))return NULL;
    return vm->resolve(vm->host,a,n,write);
}
static uint32_t trace_read(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void trace_write(unsigned char *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8));}
static int trace_overlap(const void *a,uint32_t n,const void *b,uint32_t m){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return n && m && (x<=y?y-x<n:x-y<m);}
static int trace_view(const tsfp_guest_stack_view *v){return v && v->low<v->high && v->esp>=v->low && v->esp<v->high && v->code_low<v->code_high;}
static uint32_t trace_collect(const tsfp_guest_vm *vm,const tsfp_guest_stack_view *v,uint32_t skip,uint32_t cap,uint32_t *values) {
    uint32_t used=0,frame=v->ebp,pc=v->caller,floor=v->esp>v->low?v->esp:v->low;
    if(!cap)return 0;
    if(skip)--skip;else {if(pc<v->code_low || pc>=v->code_high)return 0;values[used++]=pc;}
    while(used<cap && frame && frame!=UINT32_MAX) {
        const unsigned char *pair;uint32_t next;
        if((frame&3) || frame<floor || frame>=v->high || v->high-frame<8)break;
        pair=trace_span(vm,frame,8,0);if(!pair)break;
        next=trace_read(pair);pc=trace_read(pair+4);
        if(skip)--skip;else {if(pc<v->code_low || pc>=v->code_high)break;values[used++]=pc;}
        if(next<frame || next-frame<8)break;
        floor=frame+8;frame=next;
    }
    return used;
}
uint32_t tsfp_stack_trace(const tsfp_guest_vm *vm,const tsfp_guest_stack_view *view,uint32_t skip,uint32_t count,uint32_t output,uint32_t hash) {
    tsfp_guest_stack_view v;uint32_t *values=NULL,cap,used,sum=0;unsigned char *body=NULL,*hash_word=NULL;
    if(!trace_view(view))return 0;v=*view;
    cap=(v.high-v.esp)/8+1;if(count<cap)cap=count;
    if(hash){hash_word=trace_span(vm,hash,4,1);if(!hash_word)return 0;}
    if(cap){values=malloc((size_t)cap*sizeof(*values));if(!values)return 0;}
    used=trace_collect(vm,&v,skip,cap,values);
    if(used) {
        body=trace_span(vm,output,used*4,1);
        if(!body || (hash_word && trace_overlap(body,used*4,hash_word,4))){free(values);return 0;}
        for(uint32_t i=0;i<used;++i){sum+=values[i];trace_write(body+i*4,values[i]);}
    }
    if(hash_word)trace_write(hash_word,sum);free(values);return used;
}
int tsfp_stack_callers(const tsfp_guest_vm *vm,const tsfp_guest_stack_view *view,uint32_t first,uint32_t second) {
    tsfp_guest_stack_view v;unsigned char *a=NULL,*b=NULL;uint32_t values[2]={0,0};
    if(!trace_view(view))return 0;v=*view;
    if(first){a=trace_span(vm,first,4,1);if(!a)return 0;}
    if(second){b=trace_span(vm,second,4,1);if(!b)return 0;}
    if(a && b && a!=b && trace_overlap(a,4,b,4))return 0;
    (void)trace_collect(vm,&v,1,2,values);
    if(a)trace_write(a,values[0]);if(b)trace_write(b,values[1]);return 1;
}
