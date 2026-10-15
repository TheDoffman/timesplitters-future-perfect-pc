/* Shared PC/Mac classification of FP's direct screen-position programs.
 * Matrix-transformed world/weapon vertices must keep the widened viewport. */
#ifndef TSFP_PC_SCREEN_SPACE_H
#define TSFP_PC_SCREEN_SPACE_H
static int pc_screen_position_write(const NV2AVshInstruction *in,int previous) {
    if(in->mac_op!=NV2A_VSH_MAC_NOP && (unsigned)in->mac_op<NV2A_VSH_MAC_COUNT &&
       in->mac_dst.output_reg==NV2A_VSH_OUT_POS && (nv2a_vsh_output_write_mask(&in->mac_dst)&12))
        previous=in->mac_op==NV2A_VSH_MAC_MOV && in->mac_src[0].reg_type==NV2A_VSH_REG_INPUT &&
                 (nv2a_vsh_output_write_mask(&in->mac_dst)&12)==12 ? in->mac_src[0].reg_index:-1;
    if(in->ilu_op!=NV2A_VSH_ILU_NOP && in->ilu_dst.output_reg==NV2A_VSH_OUT_POS &&
       (nv2a_vsh_output_write_mask(&in->ilu_dst)&12))
        previous=in->ilu_op==NV2A_VSH_ILU_MOV && in->ilu_src.reg_type==NV2A_VSH_REG_INPUT &&
                 (nv2a_vsh_output_write_mask(&in->ilu_dst)&12)==12 ? in->ilu_src.reg_index:-1;
    return previous;
}
#endif
