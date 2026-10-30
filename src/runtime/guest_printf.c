/* Original format grammar, guest cursor, staging and publication. Standard libc
 * only renders individual typed atoms; it never receives a guest va_list. */
#include "guest_printf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>
#include <limits.h>
_Static_assert(sizeof(double)==8 && DBL_MANT_DIG==53 && DBL_MAX_EXP==1024,"binary64 host double required");
enum {PF_LEFT=1,PF_PLUS=2,PF_SPACE=4,PF_ALT=8,PF_ZERO=16};
enum pf_kind {PF_SIGNED,PF_UNSIGNED,PF_SIGNED64,PF_UNSIGNED64,PF_DOUBLE,PF_STRING};
typedef union pf_value {int s;unsigned u;long long s64;unsigned long long u64;double d;const char *text;} pf_value;
typedef struct pf_count {unsigned char *pointer;unsigned bytes;uint32_t value;} pf_count;
typedef struct pf_state {
    const tsfp_guest_vm *vm;tsfp_printf_allocator allocator;
    uint32_t destination,capacity,used;uint64_t format,cursor;
    char *output;size_t output_capacity;pf_count *counts;size_t count,counts_capacity;
} pf_state;
typedef struct pf_token {unsigned flags,bits;uint32_t width,precision;int have_width,have_precision,width_star,precision_star;char spec;} pf_token;
static void *pf_resize(void *context,void *p,size_t bytes){(void)context;return realloc(p,bytes);}
static void pf_release(void *context,void *p){(void)context;free(p);}
static void pf_free(pf_state *s,void *p){if(p)s->allocator.release(s->allocator.context,p);}
static void *pf_span(pf_state *s,uint64_t address,uint32_t bytes,int writable) {
    if(!address || !bytes || address>UINT32_MAX || bytes-1>UINT32_MAX-address)return NULL;
    return s->vm->resolve(s->vm->host,(uint32_t)address,bytes,writable);
}
static int pf_read(pf_state *s,uint64_t address,unsigned bytes,uint64_t *value) {
    const unsigned char *p=pf_span(s,address,bytes,0);unsigned i;size_t n;uint64_t result=0;
    if(!p)return 0;
    for(i=0;i<bytes;++i) {
        unsigned char byte=p[i];uintptr_t location=(uintptr_t)(p+i);
        /* Overlay staged %n writes in encounter order, including mapped aliases. */
        for(n=0;n<s->count;++n) {
            uintptr_t target=(uintptr_t)s->counts[n].pointer;
            if(location>=target && location-target<s->counts[n].bytes) {
                unsigned offset=(unsigned)(location-target);
                byte=offset<4?(unsigned char)(s->counts[n].value>>(8*offset)):0;
            }
        }
        result|=(uint64_t)byte<<(8*i);
    }
    *value=result;return 1;
}
static int pf_character(pf_state *s,char *c){uint64_t value;if(!pf_read(s,s->format,1,&value))return 0;*c=(char)value;return 1;}
static int pf_argument(pf_state *s,unsigned bytes,uint64_t *value) {
    if(!pf_read(s,s->cursor,bytes,value))return 0;
    s->cursor+=bytes;return 1;
}
static int32_t pf_signed32(uint32_t value){return value>INT32_MAX?-1-(int32_t)(~value):(int32_t)value;}
static int64_t pf_signed64(uint64_t value){return value>INT64_MAX?-1-(int64_t)(~value):(int64_t)value;}
static int pf_reserve(pf_state *s,uint32_t extra) {
    uint64_t needed=(uint64_t)s->used+extra+1;size_t capacity;void *replacement;
    if(needed>(uint64_t)INT32_MAX+1 || needed>s->capacity || !pf_span(s,s->destination,(uint32_t)needed,1))return 0;
    if(needed<=s->output_capacity)return 1;
    capacity=s->output_capacity?s->output_capacity:64;
    while(capacity<needed){if(capacity>SIZE_MAX/2){capacity=(size_t)needed;break;}capacity*=2;}
    replacement=s->allocator.resize(s->allocator.context,s->output,capacity);if(!replacement)return 0;
    s->output=replacement;s->output_capacity=capacity;return 1;
}
static int pf_append(pf_state *s,const char *text,uint32_t bytes) {
    uint32_t remaining=s->capacity-1-s->used;if(bytes>remaining)bytes=remaining;
    if(!pf_reserve(s,bytes))return 0;
    if(bytes)memcpy(s->output+s->used,text,bytes);s->used+=bytes;return 1;
}
static int pf_decimal(pf_state *s,uint32_t *number) {
    char c;uint32_t value=0;
    while(pf_character(s,&c)) {
        if(c<'0' || c>'9'){*number=value;return 1;}
        if(value>((uint32_t)INT32_MAX-(uint32_t)(c-'0'))/10)return 0;
        value=value*10+(uint32_t)(c-'0');++s->format;
    }
    return 0;
}
static int pf_token_read(pf_state *s,pf_token *t) {
    char c;uint64_t value;memset(t,0,sizeof(*t));t->bits=32;
    for(;;) {
        if(!pf_character(s,&c))return 0;
        switch(c){case '-':t->flags|=PF_LEFT;break;case '+':t->flags|=PF_PLUS;break;case ' ':t->flags|=PF_SPACE;break;case '#':t->flags|=PF_ALT;break;case '0':t->flags|=PF_ZERO;break;default:goto width;}
        ++s->format;
    }
width:
    if(c=='*'){t->width_star=t->have_width=1;++s->format;}
    else if(c>='0' && c<='9'){t->have_width=1;if(!pf_decimal(s,&t->width))return 0;}
    if(!pf_character(s,&c))return 0;
    if(c=='.') {
        t->have_precision=1;++s->format;if(!pf_character(s,&c))return 0;
        if(c=='*'){t->precision_star=1;++s->format;}else if(!pf_decimal(s,&t->precision))return 0;
    }
    if(!pf_character(s,&c))return 0;
    if(c=='h') {t->bits=16;++s->format;if(!pf_character(s,&c))return 0;if(c=='h'){t->bits=8;++s->format;}}
    else if(c=='l') {++s->format;if(!pf_character(s,&c))return 0;if(c=='l'){t->bits=64;++s->format;}}
    else if(c=='q' || c=='j' || c=='L'){t->bits=64;++s->format;}
    else if(c=='z' || c=='t'){++s->format;}
    else if(c=='I') {
        char a,b;++s->format;if(!pf_character(s,&a))return 0;++s->format;if(!pf_character(s,&b))return 0;++s->format;
        if(a=='6' && b=='4')t->bits=64;else if(a!='3' || b!='2')return 0;
    }
    if(!pf_character(s,&t->spec))return 0;
    if(!t->spec)return 1;
    ++s->format;
    if(t->width_star) {
        int32_t width_value;if(!pf_argument(s,4,&value))return 0;width_value=pf_signed32((uint32_t)value);
        if(width_value==INT32_MIN)return 0;
        if(width_value<0){t->flags|=PF_LEFT;width_value=-width_value;}t->width=(uint32_t)width_value;
    }
    if(t->precision_star) {
        int32_t precision;if(!pf_argument(s,4,&value))return 0;precision=pf_signed32((uint32_t)value);
        if(precision<0)t->have_precision=0;else t->precision=(uint32_t)precision;
    }
    return 1;
}
static char *pf_digits(char *p,uint32_t value) {
    char reverse[10];unsigned n=0;do{reverse[n++]=(char)('0'+value%10);value/=10;}while(value);
    while(n)*p++=reverse[--n];return p;
}
static void pf_specification(const pf_token *t,enum pf_kind kind,char *output) {
    char *p=output;*p++='%';
    if(t->flags&PF_LEFT)*p++='-';if(t->flags&PF_PLUS)*p++='+';if(t->flags&PF_SPACE)*p++=' ';
    if(t->flags&PF_ALT)*p++='#';if(t->flags&PF_ZERO)*p++='0';
    if(t->have_width)p=pf_digits(p,t->width);
    if(t->have_precision && t->spec!='c'){*p++='.';p=pf_digits(p,t->precision);}
    if(kind==PF_SIGNED64 || kind==PF_UNSIGNED64){*p++='l';*p++='l';}
    *p++=t->spec;*p=0;
}
static int pf_render(char *output,size_t capacity,const char *format,enum pf_kind kind,pf_value value) {
    switch(kind) {
    case PF_SIGNED:return snprintf(output,capacity,format,value.s);
    case PF_UNSIGNED:return snprintf(output,capacity,format,value.u);
    case PF_SIGNED64:return snprintf(output,capacity,format,value.s64);
    case PF_UNSIGNED64:return snprintf(output,capacity,format,value.u64);
    case PF_DOUBLE:return snprintf(output,capacity,format,value.d);
    case PF_STRING:return snprintf(output,capacity,format,value.text);
    }
    return -1;
}
static int pf_string(pf_state *s,uint32_t address,const pf_token *t,char **out) {
    size_t used=0,capacity=0;char *text=NULL;uint64_t value=0;void *replacement;
    for(;;) {
        int finished=t->have_precision && used>=t->precision;
        if(!finished){if(!pf_read(s,(uint64_t)address+used,1,&value))goto failed;finished=value==0;}
        if(used==capacity) {
            size_t next=capacity?capacity*2:64;if(next<capacity || next>UINT32_MAX)goto failed;
            replacement=s->allocator.resize(s->allocator.context,text,next);if(!replacement)goto failed;text=replacement;capacity=next;
        }
        if(finished){text[used]=0;*out=text;return 1;}
        text[used++]=(char)value;
    }
failed:pf_free(s,text);return 0;
}
static int pf_store_count(pf_state *s,uint32_t address,unsigned bytes) {
    pf_count *replacement;unsigned char *pointer;size_t capacity;
    if(!address)return 1;
    pointer=pf_span(s,address,bytes,1);if(!pointer)return 0;
    if(s->count==s->counts_capacity) {
        capacity=s->counts_capacity?s->counts_capacity*2:8;
        if(capacity<s->counts_capacity || capacity>SIZE_MAX/sizeof(*replacement))return 0;
        replacement=s->allocator.resize(s->allocator.context,s->counts,capacity*sizeof(*replacement));if(!replacement)return 0;
        s->counts=replacement;s->counts_capacity=capacity;
    }
    s->counts[s->count].pointer=pointer;s->counts[s->count].bytes=bytes;s->counts[s->count].value=s->used;++s->count;return 1;
}
static int pf_conversion(pf_state *s,pf_token *t) {
    uint64_t raw;pf_value value={0};enum pf_kind kind;char specification[40],small[256],*large=NULL,*string=NULL;
    int length;uint32_t copied,remaining=s->capacity-1-s->used;int ok=0;
    if(t->spec=='n') {if(!pf_argument(s,4,&raw))return 0;return pf_store_count(s,(uint32_t)raw,t->bits/8);}
    if(t->spec=='s') {
        if(!pf_argument(s,4,&raw))return 0;
        if(!raw)value.text="(null)";else{if(!pf_string(s,(uint32_t)raw,t,&string))return 0;value.text=string;}kind=PF_STRING;
    } else if(strchr("fFeEgGaA",t->spec)) {
        if(!pf_argument(s,8,&raw))return 0;memcpy(&value.d,&raw,8);kind=PF_DOUBLE;
    } else if(t->spec=='c') {
        if(!pf_argument(s,4,&raw))return 0;value.s=(int)(raw&255);kind=PF_SIGNED;
    } else if(strchr("diuoxXp",t->spec)) {
        if(t->spec=='p') {t->bits=32;t->spec='x';t->flags=(t->flags&PF_ALT)|PF_ZERO;t->have_width=1;t->width=8;t->have_precision=0;}
        if(!pf_argument(s,t->bits==64?8:4,&raw))return 0;
        if(t->spec=='d' || t->spec=='i') {
            if(t->bits==64){kind=PF_SIGNED64;value.s64=(long long)pf_signed64(raw);}
            else {
                int32_t number;
                if(t->bits==8)number=(int32_t)(raw&255)-((raw&128)?256:0);
                else if(t->bits==16)number=(int32_t)(raw&65535)-((raw&32768)?65536:0);
                else number=pf_signed32((uint32_t)raw);
                kind=PF_SIGNED;value.s=(int)number;
            }
        } else if(t->bits==64){kind=PF_UNSIGNED64;value.u64=(unsigned long long)raw;}
        else {kind=PF_UNSIGNED;value.u=(unsigned)(uint32_t)raw;if(t->bits==8)value.u&=255;else if(t->bits==16)value.u&=65535;}
    } else {
        char literal[2]={'%',t->spec};return pf_append(s,literal,2);
    }
    pf_specification(t,kind,specification);length=pf_render(small,sizeof(small),specification,kind,value);
    if(length<0)goto done;copied=(uint32_t)length;if(copied>remaining)copied=remaining;
    if(!pf_reserve(s,copied))goto done;
    if(copied<sizeof(small))ok=pf_append(s,small,copied);
    else {
        large=s->allocator.resize(s->allocator.context,NULL,(size_t)copied+1);if(!large)goto done;
        if(pf_render(large,(size_t)copied+1,specification,kind,value)<0)goto done;
        ok=pf_append(s,large,copied);
    }
done:pf_free(s,large);pf_free(s,string);return ok;
}
static int pf_overlap(const void *a,size_t an,const void *b,size_t bn) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;
}
uint32_t tsfp_guest_printf(const tsfp_guest_vm *vm,uint32_t destination,uint32_t capacity,uint32_t format,uint32_t arguments,const tsfp_printf_allocator *allocator) {
    pf_state state;char c;unsigned char *output;uint32_t result=0;size_t n;unsigned i;
    if(!capacity || !vm || !vm->resolve || !destination || !format)return 0;
    if(allocator && (!allocator->resize || !allocator->release))return 0;
    memset(&state,0,sizeof(state));state.vm=vm;state.destination=destination;state.capacity=capacity;state.format=format;state.cursor=arguments;
    if(allocator)state.allocator=*allocator;else{state.allocator.resize=pf_resize;state.allocator.release=pf_release;}
    while(state.used<capacity-1) {
        pf_token token;
        if(!pf_character(&state,&c))goto done;if(!c)break;++state.format;
        if(c!='%'){if(!pf_append(&state,&c,1))goto done;continue;}
        if(!pf_character(&state,&c))goto done;
        if(c=='%'){++state.format;if(!pf_append(&state,"%",1))goto done;continue;}
        if(!pf_token_read(&state,&token))goto done;if(!token.spec)break;
        if(!pf_conversion(&state,&token))goto done;
    }
    if(!pf_reserve(&state,0))goto done;output=pf_span(&state,destination,state.used+1,1);if(!output)goto done;
    for(n=0;n<state.count;++n)if(pf_overlap(output,(size_t)state.used+1,state.counts[n].pointer,state.counts[n].bytes))goto done;
    if(state.used)memcpy(output,state.output,state.used);output[state.used]=0;
    for(n=0;n<state.count;++n)for(i=0;i<state.counts[n].bytes;++i)state.counts[n].pointer[i]=i<4?(unsigned char)(state.counts[n].value>>(8*i)):0;
    result=state.used;
done:pf_free(&state,state.output);pf_free(&state,state.counts);return result;
}
