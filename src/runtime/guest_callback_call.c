#include "guest_callback_call.h"
#include <string.h>
typedef struct {
    uint32_t eax,ebx,ecx,edx,esi,edi,esp,ebp,seh,fs,saved_esp,dispatch;
    RecompXmm xmm[8];RecompMmx mm[8];double fp[8];
    int top,cmp,df;uint16_t control,cc;
} callback_state;
static void capture(callback_state *s) {
    s->eax=g_eax;s->ebx=g_ebx;s->ecx=g_ecx;s->edx=g_edx;
    s->esi=g_esi;s->edi=g_edi;s->esp=g_esp;s->ebp=g_ebp;s->seh=g_seh_ebp;s->fs=g_fs_base;
    s->saved_esp=g_icall_saved_esp;s->dispatch=g_icall_dispatch_form;
    s->xmm[0]=g_xmm0;s->xmm[1]=g_xmm1;s->xmm[2]=g_xmm2;s->xmm[3]=g_xmm3;
    s->xmm[4]=g_xmm4;s->xmm[5]=g_xmm5;s->xmm[6]=g_xmm6;s->xmm[7]=g_xmm7;
    s->mm[0]=g_mm0;s->mm[1]=g_mm1;s->mm[2]=g_mm2;s->mm[3]=g_mm3;
    s->mm[4]=g_mm4;s->mm[5]=g_mm5;s->mm[6]=g_mm6;s->mm[7]=g_mm7;
    memcpy(s->fp,g_fp_stack,sizeof(s->fp));s->top=g_fp_top;s->cmp=g_fp_cmp;
    s->df=g_df;s->control=g_fp_control_word;s->cc=g_fp_cc;
}
static void restore(const callback_state *s) {
    g_eax=s->eax;g_ebx=s->ebx;g_ecx=s->ecx;g_edx=s->edx;
    g_esi=s->esi;g_edi=s->edi;g_esp=s->esp;g_ebp=s->ebp;g_seh_ebp=s->seh;g_fs_base=s->fs;
    g_icall_saved_esp=s->saved_esp;g_icall_dispatch_form=s->dispatch;
    g_xmm0=s->xmm[0];g_xmm1=s->xmm[1];g_xmm2=s->xmm[2];g_xmm3=s->xmm[3];
    g_xmm4=s->xmm[4];g_xmm5=s->xmm[5];g_xmm6=s->xmm[6];g_xmm7=s->xmm[7];
    g_mm0=s->mm[0];g_mm1=s->mm[1];g_mm2=s->mm[2];g_mm3=s->mm[3];
    g_mm4=s->mm[4];g_mm5=s->mm[5];g_mm6=s->mm[6];g_mm7=s->mm[7];
    memcpy(g_fp_stack,s->fp,sizeof(s->fp));g_fp_top=s->top;g_fp_cmp=s->cmp;
    g_df=s->df;g_fp_control_word=s->control;g_fp_cc=s->cc;
}
static void put(unsigned char *p,uint32_t v) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8));}
int tsfp_guest_callback_one(const tsfp_guest_callback_ops *ops,uint32_t routine,uint32_t argument) {
    callback_state saved;tsfp_guest_entry_t entry;unsigned char *frame;uint32_t address;int balanced;
    if(!ops || !ops->lookup || !ops->output || !routine || g_esp<8)return 0;
    entry=ops->lookup(ops->context,routine);if(!entry)return 0;
    address=g_esp-8;frame=(unsigned char *)ops->output(ops->context,address,8);
    if(!frame)return 0;
    capture(&saved);put(frame,0);put(frame+4,argument);
    g_esp=address;g_seh_ebp=address;entry();
    balanced=g_esp==saved.esp;restore(&saved);return balanced;
}

int tsfp_guest_callback_prepare(const tsfp_guest_callback_ops *ops,uint32_t routine,
    const uint32_t *arguments,unsigned count,tsfp_guest_callback_plan *out) {
    tsfp_guest_callback_plan candidate;tsfp_guest_entry_t entry;unsigned char *frame;
    uint32_t address,bytes;unsigned i;
    if(!out || !ops || !ops->lookup || !ops->output || !routine || count>4 || (count && !arguments))return 0;
    bytes=4+4*count;if(g_esp<bytes)return 0;
    memset(&candidate,0,sizeof(candidate));candidate.esp=g_esp;
    for(i=0;i<count;++i)candidate.words[i]=arguments[i];
    entry=ops->lookup(ops->context,routine);if(!entry)return 0;
    address=g_esp-bytes;frame=(unsigned char *)ops->output(ops->context,address,bytes);
    if(!frame)return 0;
    candidate.entry=entry;candidate.frame=frame;candidate.address=address;
    candidate.bytes=bytes;candidate.count=count;*out=candidate;return 1;
}
int tsfp_guest_callback_invoke(tsfp_guest_callback_plan *prepared,int (*admit)(void *),void *context,uint32_t *result) {
    callback_state saved;tsfp_guest_entry_t entry;unsigned char *frame;
    uint32_t address,value,words[4];unsigned i,count;int balanced;
    if(!prepared)return 0;
    entry=prepared->entry;prepared->entry=NULL;
    if(!entry || !prepared->frame || prepared->count > 4 || g_esp!=prepared->esp ||
       prepared->bytes!=4+4*prepared->count || prepared->esp<prepared->bytes ||
       prepared->address!=prepared->esp-prepared->bytes)return 0;
    if(admit && !admit(context))return 0;
    frame=prepared->frame;address=prepared->address;count=prepared->count;
    memcpy(words,prepared->words,sizeof(words));capture(&saved);put(frame,0);
    for(i=0;i<count;++i)put(frame+4+4*i,words[i]);
    g_esp=address;g_seh_ebp=address;entry();
    value=g_eax;balanced=g_esp==saved.esp;restore(&saved);
    if(balanced && result)*result=value;
    return balanced;
}
int tsfp_guest_callback_stdcall(const tsfp_guest_callback_ops *ops,uint32_t routine,
    const uint32_t *arguments,unsigned count,uint32_t *result) {
    tsfp_guest_callback_plan prepared;
    return tsfp_guest_callback_prepare(ops,routine,arguments,count,&prepared) &&
        tsfp_guest_callback_invoke(&prepared,NULL,NULL,result);
}
