/* US 45410066 first-person arm nodes. The title creates this separate entity
 * at D35B6/D35C0 and animates it through 5FCF0; world actors use another model.
 * 8F5E0 traverses children independently of each 18EA90 geometry submission. */
#ifndef PC_VIEWMODEL_VISIBILITY_GUEST_INC
#define PC_VIEWMODEL_VISIBILITY_GUEST_INC
#include "pc_display.h"
#define VM_WORD(a) (*(const uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define VM_BYTE(a) (*(const uint8_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static int pc_viewmodel_address(uint32_t address,uint32_t bytes)
{ return address>=0x80000000u && bytes<=0x04000000u && address<=0x84000000u-bytes; }
static int pc_viewmodel_arm_node_hidden(uint32_t player,uint32_t model,uint32_t node,int weapon_fov)
{
    if(weapon_fov<=100)return 0;
    if(!pc_viewmodel_address(player,0xe88u))return 0;
    uint32_t arms=VM_WORD(player+0xe84u);
    if(!pc_viewmodel_address(arms,8u))return 0;
    if(model!=VM_WORD(arms+4u) || !pc_viewmodel_address(model,8u) || VM_WORD(model)!=arms)return 0;
    uint32_t header=VM_WORD(model+4u);
    if(!pc_viewmodel_address(header,4u))return 0;
    uint32_t count=VM_WORD(header);
    if(!count || count>128u || node>=count || header<count*200u)return 0;
    uint32_t base=header-count*200u,address=base+node*200u;
    if(!pc_viewmodel_address(base,count*200u) || !node)return 0;
    /* Only the two rigid shoulder roots, never their skinned upper arms,
     * elbows, wrists or fingers. Node indices vary with character geometry. */
    return VM_BYTE(base+0x4au)==255u && VM_BYTE(address+0x48u)==0u &&
        VM_BYTE(address+0x4au)==0u &&
        (VM_BYTE(address+0x49u)==1u || VM_BYTE(address+0x49u)==10u);
}
static int pc_viewmodel_node_hidden(void)
{
    return pc_viewmodel_arm_node_hidden(VM_WORD(0x7b0c7cu),VM_WORD(g_esp+4u),
        VM_WORD(g_esp+8u),pc_display_weapon_fov());
}
#undef VM_WORD
#undef VM_BYTE
#endif
