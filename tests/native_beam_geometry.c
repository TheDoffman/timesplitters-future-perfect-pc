/* Original type-32 constructor, geometry update, renderer and effect manager.
 * Capture original vertex writes before GPU submission; replace allocation,
 * RNG and texture/render services. This cannot prove monitor-visible output. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;

enum { FX=0x1000,DATA=0x2000,ORIGIN=0x4000,END=0x4010,PLAYER=0x5000,
       VERTICES=0x10000,STACK=0x30000,HEAD=0x73bb88 };
static unsigned retired,random_calls,draws,visible_vertices,vertex_count,quality;
static float random_value;
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected effect tail")
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected effect callback")
void sub_000FF200(void) {
    assert(MEM32(g_esp+4)==FX && MEM32(HEAD)==FX);
    retired++;MEM32(HEAD)=0;g_esp+=4;
}
void sub_000C6140(void) { g_eax=0;g_esp+=4; }
void sub_00111E40(void) { g_esp+=4; }
void sub_0012B5D0(void) { g_esp+=4; }
void sub_000389B0(void) { g_esp+=4; }
void sub_00344A90(void) { g_esp+=4; }
void sub_001881E0(void) { g_esp+=4; }
void sub_00153BD0(void) {
    random_calls++;g_fp_top=(g_fp_top-1)&7;g_fp_stack[g_fp_top]=random_value;g_esp+=4;
}
void sub_000FFE50(void) { assert(!"Unexpected effect type: 000FFE50"); }
void sub_001067E0(void) { assert(!"Unexpected effect type: 001067E0"); }
void sub_001064B0(void) { assert(!"Unexpected effect type: 001064B0"); }
void sub_00106AD0(void) { assert(!"Unexpected effect type: 00106AD0"); }
void sub_00106BD0(void) { assert(!"Unexpected effect type: 00106BD0"); }
void sub_001070E0(void) { assert(!"Unexpected effect type: 001070E0"); }
void sub_00107240(void) { assert(!"Unexpected effect type: 00107240"); }
void sub_00107500(void) { assert(!"Unexpected effect type: 00107500"); }
void sub_00101700(void) { assert(!"Unexpected effect type: 00101700"); }
void sub_00102650(void) { assert(!"Unexpected effect type: 00102650"); }
void sub_00105750(void) { assert(!"Unexpected effect type: 00105750"); }
void sub_00108780(void) { assert(!"Unexpected effect type: 00108780"); }
void sub_001078A0(void) { assert(!"Unexpected effect type: 001078A0"); }
void sub_000FF2B0(void) {
    assert(g_ebx==32 && MEM32(g_esp+4)==1);
    MEM32(FX)=32;MEM32(FX+0x18)=DATA;MEM32(HEAD)=FX;
    g_eax=FX;g_esp+=4;
}
void sub_001CDAE0(void) { g_eax=quality;g_esp+=4; }
void sub_003C848C(void) {
    g_eax=(uint32_t)(int32_t)g_fp_stack[g_fp_top];
    g_fp_top=(g_fp_top+1)&7;g_esp+=4;
}
void sub_00080F20(void) { assert(MEM32(g_esp+4)==0x33d);g_eax=0x5678;g_esp+=4; }
void sub_00019D70(void) { assert(MEM32(g_esp+4)==2);g_esp+=4; }
void sub_0001C720(void) { assert(MEM32(g_esp+4)==0x5678);g_esp+=4; }
void sub_00018EA0(void) {
    assert(MEM32(g_esp+4)==2 && MEM32(g_esp+8)==13 && MEM32(g_esp+12)==22);
    vertex_count=MEM32(g_esp+16);
    assert(vertex_count==2*(quality<3?16u:8u));
    MEM32(0x55f974)=VERTICES;g_esp+=4;
}
void sub_000459D0(void) {
    assert(MEM32(0x55f974)==VERTICES+vertex_count*24);
    for(unsigned i=0;i<vertex_count;i++) {
        unsigned v=VERTICES+i*24;
        for(unsigned j=0;j<3;j++)assert(isfinite(MEMF(v+j*4)));
        if(MEM32(v+12)>>24)visible_vertices++;
        assert(MEMF(v+16)==0 && (MEMF(v+20)==0 || MEMF(v+20)==1));
    }
    draws++;g_esp+=4;
}
/* GENERATED_ROUTINES */
static void call(void (*fn)(void)) {
    g_esp=STACK;MEM32(STACK)=0x12345678;
    g_esi=0xdead;g_ebx=0xbeef;g_edi=0xfeed;g_ebp=0xabcd;
    if(fn==sub_0010BF5C)g_ebx=1;
    fn();
    assert(g_esp==STACK+4 && g_fp_top==0);
    assert(g_esi==(fn==sub_0010BF5C?0u:0xdeadu));
    assert(g_ebx==(fn==sub_0010BF5C?1u:0xbeefu) && g_edi==0xfeed && g_ebp==0xabcd);
}
static void construct(unsigned strands) {
    memset((void *)((uintptr_t)FX+g_xbox_mem_offset),0,0x68);
    memset((void *)((uintptr_t)DATA+g_xbox_mem_offset),0,0xa00);
    MEMF(ORIGIN)=10;MEMF(ORIGIN+4)=20;MEMF(ORIGIN+8)=30;
    MEMF(END)=110;MEMF(END+4)=50;MEMF(END+8)=90;
    MEM32(STACK+4)=MEM32(STACK+8)=0;
    MEM32(STACK+12)=ORIGIN;MEM32(STACK+16)=END;
    MEM32(STACK+20)=0xff0080ff;MEM32(STACK+24)=0xff5522ff;
    MEM32(STACK+28)=strands;MEM32(STACK+32)=0xffff;
    random_calls=retired=0;call(sub_00104F30);
    assert(g_eax==FX && MEM32(DATA)==strands);
    assert(MEMF(FX+0xc)==0 && fabsf(MEMF(FX+0x10)-1.2f)<.00001f);
    assert(random_calls==5*(strands-1));
}
static void tick(float delta,unsigned whole) {
    MEMF(0x4e794c)=delta;MEM32(0x4e7944)=whole;
    call(sub_0010A760);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static unsigned render_expected(unsigned expected) {
    draws=visible_vertices=0;call(sub_0010BF5C);
    assert(draws==expected);
    return visible_vertices;
}
static unsigned render(void) { return render_expected(2*(MEM32(DATA)-1)); }
static float run(int fps,int native,float speed,unsigned strands,unsigned q,float rng) {
    g_tsfp_native_timing_active=native;quality=q;random_value=rng;
    construct(strands);
    unsigned first=0,last=0,frames=0,draw_frames=0;
    double carry=0,elapsed=0;float dt=60.f/fps*speed;
    while(!retired && frames<fps*8u) {
        carry+=dt;unsigned whole=(unsigned)carry;carry-=whole;
        tick(dt,whole);frames++;elapsed+=(double)dt/60;
        if(retired)break;
        unsigned visible=render();
        if(visible) { if(!first)first=frames;last=frames;draw_frames++; }
        unsigned char before[0xa00];
        memcpy(before,(void *)((uintptr_t)DATA+g_xbox_mem_offset),sizeof before);
        float age=MEMF(FX+0xc);
        tick(0,0);
        assert(MEMF(FX+0xc)==age);
        assert(!memcmp(before,(void *)((uintptr_t)DATA+g_xbox_mem_offset),sizeof before));
        assert(render()==visible);
    }
    assert(first==1 && retired==1 && draw_frames>0);
    assert(render_expected(0)==0); /* Retired effects cannot reappear. */
    assert(fabs(elapsed-1.2)<=2*dt/60+.00001);
    assert(last*dt/60<=1.2+.00001);
    assert(random_calls==5*(strands-1));
    printf("beam_geometry native=%d fps=%d speed=%.2f strands=%u quality=%u rng=%.3f first=%.6f last=%.6f expiry=%.6f draw_frames=%u\n",
        native,fps,speed,strands,q,rng,first*dt/60,last*dt/60,elapsed,draw_frames);
    return last*dt/60;
}
static void visibility_cases(unsigned fps,float speed) {
    g_tsfp_native_timing_active=1;quality=1;random_value=.5f;
    float dt=60.f/fps*speed;double carry=0;
    construct(4);
    /* Original manager computes per-player room visibility every live
     * interval; only the upstream camera/room query result is controlled. */
    MEM32(FX+8)=0x80000;MEM32(FX+0x98)=1;MEM32(FX+0x9c)=0;
    MEM32(0x7b0ce0)=1;MEM32(0x7b0ce4)=PLAYER;
    MEM32(PLAYER+0x21c)=0x80000001;MEM8(PLAYER+0xd2c)=0;
    for(unsigned i=0;i<fps/(4*speed);i++) {
        carry+=dt;unsigned whole=(unsigned)carry;carry-=whole;
        tick(dt,whole);assert(render_expected(0)==0 && !retired);
    }
    MEM8(PLAYER+0xd2c)=1;
    tick(dt,0);assert(render()>0); /* No whole tick required for visibility. */
    unsigned normal=MEM32(FX+8);
    MEM32(FX+8)=normal|0x200000;assert(render_expected(0)==0);
    MEM32(FX+8)=normal;
    MEM32(FX+0x34)=2;assert(render_expected(0)==0);
    MEM32(FX+0x34)=1;assert(render()>0);
    MEM32(FX+8)=normal&~0x10000u;assert(render_expected(0)==0);
    tick(0,0);assert(render()>0); /* Zero-time visibility refresh, frozen age. */
    MEM32(FX+0x98)=0;MEM32(PLAYER+0x21c)=0;
    construct(4);MEMF(FX+0x38)=.3f;
    unsigned first=0,frames=0;carry=0;
    while(!retired && frames<fps*8u) {
        carry+=dt;unsigned whole=(unsigned)carry;carry-=whole;
        tick(dt,whole);frames++;
        if(retired) { assert(render_expected(0)==0);break; }
        if(MEMF(FX+0x38)>0)assert(render_expected(0)==0);
        else if(render()>0 && !first)first=frames;
    }
    assert(first && fabsf(first*dt/60-.3f)<=dt/60+.00001f);
    printf("beam_visibility fps=%u speed=%.2f delayed_first=%.6f retired=%u\n",
           fps,speed,first*dt/60,retired);
}
int main(void) {
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x4e8cec)=60;
    MEM32(0x7b0cc0)=PLAYER;MEM32(PLAYER+0x214)=0;
    /* Controlled trig service table, shared by all comparison schedules. */
    for(unsigned i=0;i<2048;i++)MEMF(0x7a38a0+i*4)=sinf((float)(i*6.283185307179586/2048));
    setvbuf(stdout,NULL,_IONBF,0);
    for(unsigned strands=2;strands<=4;strands+=2)
        for(unsigned q=1;q<=3;q+=2)
            for(unsigned r=0;r<3;r++) {
                float rng=r==0?0:r==1?.5f:.999f;
                float reference=run(60,0,1,strands,q,rng);
                unsigned char original[0xa00];
                memcpy(original,(void *)((uintptr_t)DATA+g_xbox_mem_offset),sizeof original);
                const unsigned rates[]={60,120,144};
                for(unsigned i=0;i<3;i++)for(unsigned slow=0;slow<2;slow++) {
                    float last=run(rates[i],1,slow?.25f:1,strands,q,rng);
                    assert(fabsf(last-reference)<=1.f/60+.00001f);
                    if(i==0 && !slow)assert(!memcmp(original,
                        (void *)((uintptr_t)DATA+g_xbox_mem_offset),sizeof original));
                }
            }
    const unsigned rates[]={60,120,144};
    for(unsigned i=0;i<3;i++)for(unsigned slow=0;slow<2;slow++)
        visibility_cases(rates[i],slow?.25f:1);
    puts("PASS: original beam construction, geometry, vertex alpha onset/fade, pause and retirement");
    free(memory);return 0;
}
