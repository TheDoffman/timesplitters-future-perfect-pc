#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
static uint32_t memory[8192];
static ptrdiff_t g_xbox_mem_offset;
static uint32_t g_esp;
int pc_display_weapon_fov(void){return 150;}
#include "graphics/pc_viewmodel_visibility_guest.inc"
#define WORD(a) (*(uint32_t *)((uintptr_t)(a)+g_xbox_mem_offset))
#define BYTE(a) (*(uint8_t *)((uintptr_t)(a)+g_xbox_mem_offset))
int main(void)
{
    const uint32_t player=0x80000100u,arms=0x80002000u,model=0x80002100u;
    const uint32_t base=0x80003000u,header=base+41u*200u;
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory-(ptrdiff_t)0x80000000u;
    WORD(player+0xe84u)=arms;WORD(arms+4u)=model;WORD(model)=arms;
    WORD(model+4u)=header;WORD(header)=41u;BYTE(base+0x4au)=255u;
    /* Captured Cortez rig: shoulders at nodes1/15, bone IDs1/10. */
    const unsigned nodes[]={1,15};
    for(unsigned side=0;side<2;side++) {
        unsigned node=nodes[side];uint32_t address=base+node*200u;
        BYTE(address+0x49u)=side?10:1;BYTE(address+0x4au)=0;
        assert(pc_viewmodel_arm_node_hidden(player,model,node,150));
        assert(!pc_viewmodel_arm_node_hidden(player,model,node,100));
        assert(!pc_viewmodel_arm_node_hidden(player,model,node,80));
        for(unsigned type=1;type<=3;type++) {
            BYTE(address+0x48u)=type;
            assert(!pc_viewmodel_arm_node_hidden(player,model,node,150));
        }
        BYTE(address+0x48u)=0;BYTE(address+0x4au)=1;
        assert(!pc_viewmodel_arm_node_hidden(player,model,node,150));
        BYTE(address+0x4au)=0;
        for(unsigned bone=0;bone<24;bone++) {
            BYTE(address+0x49u)=bone;
            assert(pc_viewmodel_arm_node_hidden(player,model,node,150)==(bone==1||bone==10));
        }
        BYTE(address+0x49u)=side?10:1;
    }
    assert(!pc_viewmodel_arm_node_hidden(player,model,0,150));
    assert(!pc_viewmodel_arm_node_hidden(player,model,41,150));
    assert(!pc_viewmodel_arm_node_hidden(player,model,0xffffffffu,150));
    WORD(model)=arms+0x100;
    assert(!pc_viewmodel_arm_node_hidden(player,model,15,150));
    WORD(model)=arms;WORD(player+0xe84u)=arms+0x100;
    assert(!pc_viewmodel_arm_node_hidden(player,model,15,150));
    WORD(player+0xe84u)=arms;WORD(header)=129;
    assert(!pc_viewmodel_arm_node_hidden(player,model,15,150));
    WORD(header)=41;BYTE(base+0x4au)=0;
    assert(!pc_viewmodel_arm_node_hidden(player,model,15,150));
    assert(!pc_viewmodel_arm_node_hidden(0,model,15,150));
    assert(!pc_viewmodel_arm_node_hidden(0xffffffffu,model,15,150));
    puts("PASS: first-person ownership, shoulder geometry, retained limbs and bounds");
}
