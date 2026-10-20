/* Execute the production camera-feed scope, including batch boundaries. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_TLS
static int ui_placement, expected, calls, flushes, changes;
static uint32_t ui_tag, camera=0x1000, flags;
static uint32_t *word(uint32_t a) {
    if(a==0x007B0CC0u)return &camera;
    if(a==camera+0x21cu)return &flags;
    abort();
}
#define WORD_AT(a) (*word(a))
static int guest_writable(uint32_t a,size_t n){return a==0x1000 && n<=0x224;}
static void sub_00022FF0(void){flushes++;}
static void guest_call(void (*f)(void),int n){(void)n;f();}
static void host_SetTwoDPlacement(int place,uint32_t tag){
    (void)place;(void)tag;
    if(flushes!=changes+1){fprintf(stderr,"FAIL: placement changed before batch flush\n");exit(1);}
    changes++;
}
/* PRODUCTION */
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: line %d %s\n",__LINE__,#x);exit(1);}}while(0)
static void draw(void){
    calls++;CHECK(ui_placement==expected);
    if(expected==1)CHECK(ui_tag==0x000c8120u);
}
static void nested(void){
    CHECK(ui_placement==1);CHECK(ui_tag==0x000c8120u);
    pc_display_camera_feed(draw);
    CHECK(ui_placement==1);CHECK(ui_tag==0x000c8120u);
}
int main(void){
    const uint32_t modes[]={0,1,8,9,0x400009,0x400001};
    for(unsigned i=0;i<sizeof modes/sizeof modes[0];i++){
        flags=modes[i];camera=0x1000;ui_placement=4;ui_tag=0x12345;
        flushes=changes=calls=0;expected=(flags&9)==9?1:4;
        pc_display_camera_feed(draw);
        CHECK(calls==1);CHECK(ui_placement==4);CHECK(ui_tag==0x12345);
        CHECK(changes==(expected==1?2:0));CHECK(flushes==changes);
    }
    camera=0;flags=9;expected=4;calls=changes=flushes=0;
    pc_display_camera_feed(draw);CHECK(calls==1 && changes==0);
    camera=0x1000;flags=0x400009;expected=1;changes=flushes=0;
    pc_display_camera_feed(nested);
    CHECK(changes==4 && flushes==4);CHECK(ui_placement==4 && ui_tag==0x12345);
    puts("PASS: texture camera placement, disabled/world cameras, invalid camera, nested restoration and batch flushing");
    return 0;
}
