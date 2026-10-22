#include <assert.h>
#include <stdio.h>
#include "pc_binding_capture.h"
#include "pc_keyboard_menu_model.h"
int main(void) {
    PcBindingCapture c={0};unsigned char keys[256]={0};
    pc_binding_capture_begin(&c,100);keys[1]=1;
    pc_binding_capture_step(&c,keys,1,101);assert(c.phase==1 && !c.event);
    keys[1]=0;pc_binding_capture_step(&c,keys,1,102);assert(c.phase==2);
    keys['E']=1;pc_binding_capture_step(&c,keys,1,103);assert(c.event=='E'+1 && c.phase==3);
    c.event=0;pc_binding_capture_step(&c,keys,1,104);assert(!c.event && c.phase==3);
    keys['E']=0;pc_binding_capture_step(&c,keys,1,105);assert(!c.phase);
    pc_binding_capture_begin(&c,200);pc_binding_capture_step(&c,keys,1,201);
    keys[27]=1;pc_binding_capture_step(&c,keys,1,202);assert(c.event==-1 && c.phase==3);
    keys[27]=0;pc_binding_capture_step(&c,keys,1,203);assert(!c.phase);
    pc_binding_capture_begin(&c,300);pc_binding_capture_step(&c,keys,1,301);
    keys[0x86]=1;pc_binding_capture_step(&c,keys,1,302);assert(c.event==0x87);
    keys[0x86]=0;pc_binding_capture_step(&c,keys,1,303);
    pc_binding_capture_begin(&c,400);pc_binding_capture_step(&c,keys,0,401);assert(c.event==-1 && !c.phase);
    pc_binding_capture_begin(&c,500);pc_binding_capture_step(&c,keys,1,30500);assert(c.event==-1 && !c.phase);
    pc_binding_capture_begin(&c,40000);
    pc_binding_capture_key(&c,'U');assert(!c.event); /* Opening-input guard. */
    pc_binding_capture_step(&c,keys,1,40001);
    pc_binding_capture_key(&c,'U');pc_binding_capture_key(&c,'E');
    pc_binding_capture_step(&c,keys,1,40002); /* Already released at poll time. */
    assert(c.event=='U'+1 && !c.phase);
    pc_binding_capture_begin(&c,50000);pc_binding_capture_step(&c,keys,1,50001);
    pc_binding_capture_key(&c,0xa3);assert(c.event==0x12); /* Right Ctrl shares Ctrl. */
    assert(sizeof pc_keyboard_action_labels/sizeof pc_keyboard_action_labels[0]==34);
    int seen[34]={0};
    for(int g=0;g<4;g++)for(int i=0;i<12;i++) {
        int action=pc_keyboard_groups[g][i];if(action>=0){assert(action<34);seen[action]++;}
    }
    for(int i=0;i<34;i++)assert(seen[i]==1);
    assert(pc_keyboard_sensitivity_step(.1)==500);
    assert(fabs(pc_keyboard_sensitivity(0)-.001)<1e-9);
    assert(fabs(pc_keyboard_sensitivity(1000)-10)<1e-9);
    for(unsigned i=0;i<=1000;i++)assert(pc_keyboard_sensitivity_step(pc_keyboard_sensitivity(i))==i);
    puts("PASS: binding capture isolation, cancel/focus/timeout, complete menu coverage, sensitivity range");
    return 0;
}
