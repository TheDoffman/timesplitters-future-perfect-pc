/*
 * nv2a_vsh -- tests for the NV2A vertex-program decoder and interpreter.
 *
 * Runs the real nv2a_vsh_parse() and nv2a_vsh_execute(). tools/vsh_audit
 * checks the field table's constants from Python, which cannot see how the C
 * then resolves those fields -- and an audit found real bugs in exactly that
 * gap: a paired ILU slot writing the wrong temp, OUT_ORB ignored, the oFog
 * mask mapping, and input usage counted for sources no opcode reads. Each has
 * a case here.
 *
 * Ground truth is xemu's hw/xbox/nv2a/pgraph/glsl/vsh-prog.c together with
 * abaire/nv2a_vsh_asm, as recorded in
 * docs/technical/nv2a-vertex-program-encoding.md. No game files are needed:
 * the one real program is twelve instructions of microcode.
 */

#include <stdio.h>
#include <string.h>

#include "nv2a_vsh.h"

static int g_checks;
static int g_failures;

#define CHECK(cond, what)                                              \
    do {                                                               \
        g_checks++;                                                    \
        if (!(cond)) {                                                 \
            g_failures++;                                              \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (what));    \
        }                                                              \
    } while (0)

/* Burnout 2's frontend shader as RECOMP_VP_DUMP wrote it: the canonical Xbox
 * pass-through program.
 *
 *    0  MOV R1.xyzw, v0
 *    1  MOV oD0.xyzw, v3      + RCP R1.w, R1.w
 *    2  RCP oFog.xyzw, v0.w
 *    3  MUL R2.xyzw, R1, c[0] + MOV oD1.xyzw, v4
 *    4  ADD oPos.xyzw, R2, c[1]
 *    5  MOV oPts.xyzw, v1.x
 *    6  MOV oB0.xyzw, v7          9  MOV oT1.xyzw, v10
 *    7  MOV oB1.xyzw, v8         10  MOV oT2.xyzw, v11
 *    8  MOV oT0.xyzw, v9         11  MOV oT3.xyzw, v12
 */
static const uint32_t FRONTEND[12 * 4] = {
    0x00000000, 0x0020001B, 0x0836106C, 0x2F100FF8,
    0x00000000, 0x0420061B, 0x083613FC, 0x5011F818,
    0x00000000, 0x0400001B, 0x083613FC, 0x2070F82C,
    0x00000000, 0x0240081B, 0x1436186C, 0x2F20F824,
    0x00000000, 0x0060201B, 0x2436106C, 0x3070F800,
    0x00000000, 0x00200200, 0x0836106C, 0x2070F830,
    0x00000000, 0x00200E1B, 0x0836106C, 0x2070F838,
    0x00000000, 0x0020101B, 0x0836106C, 0x2070F840,
    0x00000000, 0x0020121B, 0x0836106C, 0x2070F848,
    0x00000000, 0x0020141B, 0x0836106C, 0x2070F850,
    0x00000000, 0x0020161B, 0x0836106C, 0x2070F858,
    0x00000000, 0x0020181B, 0x0836106C, 0x2070F861,
};

/* Absolute bit offsets for building variants of those instructions. Written
 * out from the reference rather than taken from the VSH_FIELD_* macros, which
 * are part of what is under test. */
enum {
    F_INPUT      = 41,
    F_ILU        = 57,
    F_C_SWZ_W    = 66,
    F_C_SWZ_Z    = 68,
    F_C_SWZ_Y    = 70,
    F_C_SWZ_X    = 72,
    F_B_MUX      = 75,
    F_A_MUX      = 90,
    F_FINAL      = 96,
    F_OUT_MUX    = 98,
    F_OUT_ORB    = 107,
    F_OUT_O_MASK = 108,
    F_OUT_TEMP   = 116,
};

static void set_field(uint32_t *insn, int start, int size, uint32_t value)
{
    int word = start / 32, bit = start % 32;
    uint32_t mask = (((size == 32) ? 0xFFFFFFFFu : ((1u << size) - 1u)) << bit);

    insn[word] = (insn[word] & ~mask) | ((value << bit) & mask);
}

/* One instruction of the frontend program, marked as the last. */
static void one_insn(uint32_t *out, int index)
{
    memcpy(out, &FRONTEND[index * 4], 4 * sizeof(uint32_t));
    set_field(out, F_FINAL, 1, 1);
}

typedef const float (*ConstRows)[4];

static NV2AVshProgram g_prog;

static void test_frontend_decode(void)
{
    static const NV2AVshOutputReg mov_out[6] = {
        NV2A_VSH_OUT_B0, NV2A_VSH_OUT_B1, NV2A_VSH_OUT_T0,
        NV2A_VSH_OUT_T1, NV2A_VSH_OUT_T2, NV2A_VSH_OUT_T3,
    };
    const NV2AVshInstruction *in = g_prog.insns;
    int i;

    nv2a_vsh_parse(FRONTEND, 12, &g_prog);
    CHECK(g_prog.length == 12, "the program is twelve instructions");
    for (i = 0; i < 12; i++)
        CHECK(in[i].is_final == (i == 11), "only instruction 11 is final");

    /* 0  MOV R1, v0 */
    CHECK(in[0].mac_op == NV2A_VSH_MAC_MOV && in[0].ilu_op == NV2A_VSH_ILU_NOP,
          "0: MOV, no ILU");
    CHECK(in[0].mac_dst.temp_reg == 1 && in[0].mac_dst.write_mask == 0xF,
          "0: writes R1.xyzw");
    CHECK(in[0].mac_dst.output_reg == NV2A_VSH_OUT_NONE, "0: writes no output");
    CHECK(in[0].mac_src[0].reg_type == NV2A_VSH_REG_INPUT
          && in[0].mac_src[0].reg_index == 0, "0: reads v0");

    /* 1  MOV oD0, v3  +  RCP R1.w, R1.w -- a paired slot */
    CHECK(in[1].mac_op == NV2A_VSH_MAC_MOV && in[1].ilu_op == NV2A_VSH_ILU_RCP,
          "1: MOV + RCP");
    CHECK(in[1].mac_dst.output_reg == NV2A_VSH_OUT_D0
          && in[1].mac_dst.output_mask == 0xF, "1: the MAC writes oD0");
    CHECK(in[1].ilu_dst.temp_reg == 1 && in[1].ilu_dst.write_mask == 0x1,
          "1: the ILU writes R1.w");
    CHECK(in[1].ilu_src.reg_type == NV2A_VSH_REG_TEMP
          && in[1].ilu_src.reg_index == 1, "1: the ILU reads R1");

    /* 2  RCP oFog, v0.w */
    CHECK(in[2].mac_op == NV2A_VSH_MAC_NOP && in[2].ilu_op == NV2A_VSH_ILU_RCP,
          "2: RCP only");
    CHECK(in[2].ilu_dst.output_reg == NV2A_VSH_OUT_FOG, "2: the ILU writes oFog");

    /* 3  MUL R2, R1, c[0]  +  MOV oD1, v4 */
    CHECK(in[3].mac_op == NV2A_VSH_MAC_MUL && in[3].ilu_op == NV2A_VSH_ILU_MOV,
          "3: MUL + MOV");
    CHECK(in[3].mac_dst.temp_reg == 2 && in[3].mac_dst.write_mask == 0xF,
          "3: the MAC writes R2");
    CHECK(in[3].mac_src[0].reg_type == NV2A_VSH_REG_TEMP
          && in[3].mac_src[0].reg_index == 1, "3: A is R1");
    CHECK(in[3].mac_src[1].reg_type == NV2A_VSH_REG_CONST
          && in[3].mac_src[1].reg_index == 0, "3: B is c[0]");
    CHECK(in[3].ilu_dst.output_reg == NV2A_VSH_OUT_D1, "3: the ILU writes oD1");

    /* 4  ADD oPos, R2, c[1] */
    CHECK(in[4].mac_op == NV2A_VSH_MAC_ADD, "4: ADD");
    CHECK(in[4].mac_dst.output_reg == NV2A_VSH_OUT_POS, "4: writes oPos");
    CHECK(in[4].mac_src[0].reg_type == NV2A_VSH_REG_TEMP
          && in[4].mac_src[0].reg_index == 2, "4: A is R2");
    CHECK(in[4].mac_src[2].reg_type == NV2A_VSH_REG_CONST
          && in[4].mac_src[2].reg_index == 1, "4: C is c[1]");

    /* 5  MOV oPts, v1.x    6-11  MOV oB0..oT3, v7..v12 */
    CHECK(in[5].mac_dst.output_reg == NV2A_VSH_OUT_PTS && in[5].input_index == 1,
          "5: MOV oPts, v1");
    for (i = 0; i < 6; i++) {
        CHECK(in[6 + i].mac_dst.output_reg == mov_out[i], "6-11: output register");
        CHECK(in[6 + i].input_index == 7 + i, "6-11: input register");
    }

    for (i = 0; i < 12; i++)
        CHECK(in[i].out_const_index == -1, "no slot writes a constant register");

    CHECK(g_prog.inputs_read == 0x1F9B, "reads v0 v1 v3 v4 and v7-v12");
}

static void test_frontend_execute(void)
{
    float inputs[NV2A_VS_MAX_INPUTS][4];
    float consts[2][4] = {
        { 1.0f, 1.0f, 16777215.0f, 1.0f },
        { 0.53125f, 0.53125f, 0.0f, 0.0f },
    };
    NV2AVshState st;
    int i;

    memset(inputs, 0, sizeof inputs);
    inputs[0][0] = 100.0f;
    inputs[0][1] = 200.0f;
    inputs[0][3] = 1.0f;
    inputs[3][0] = 0.25f;
    inputs[3][1] = 0.5f;
    inputs[3][2] = 0.75f;
    inputs[3][3] = 1.0f;
    for (i = 7; i <= 12; i++)
        inputs[i][0] = (float)i;

    nv2a_vsh_parse(FRONTEND, 12, &g_prog);
    nv2a_vsh_execute(&g_prog, (ConstRows)inputs, (ConstRows)consts, 2, &st);

    CHECK(st.out_written & (1u << NV2A_VSH_OUT_POS), "oPos is written");
    CHECK(st.out[NV2A_VSH_OUT_POS][0] == 100.53125f
          && st.out[NV2A_VSH_OUT_POS][1] == 200.53125f,
          "oPos.xy is v0.xy plus the half-pixel bias");
    CHECK(st.out[NV2A_VSH_OUT_POS][2] == 0.0f && st.out[NV2A_VSH_OUT_POS][3] == 1.0f,
          "oPos.zw is (0, 1)");
    CHECK(memcmp(st.out[NV2A_VSH_OUT_D0], inputs[3], sizeof inputs[3]) == 0,
          "oD0 is v3");
    CHECK(st.out[NV2A_VSH_OUT_FOG][0] == 1.0f, "oFog is 1 / v0.w");
    CHECK(st.out[NV2A_VSH_OUT_T3][0] == 12.0f, "oT3 is v12");
}

static void test_paired_ilu_writes_r1(void)
{
    uint32_t w[4];

    one_insn(w, 1);                      /* MOV oD0, v3 + RCP R1.w, R1.w */
    set_field(w, F_OUT_TEMP, 4, 5);      /* encode R5 as the temp */
    nv2a_vsh_parse(w, 1, &g_prog);
    CHECK(g_prog.insns[0].ilu_dst.temp_reg == 1,
          "a paired ILU writes R1 whatever OUT_TEMP says");
}

static void test_paired_mac_write_to_r1_is_dropped(void)
{
    uint32_t w[4];

    one_insn(w, 3);                      /* MUL R2, R1, c[0] + MOV oD1, v4 */
    set_field(w, F_OUT_TEMP, 4, 1);      /* aim the MAC's temp write at R1 */
    nv2a_vsh_parse(w, 1, &g_prog);
    CHECK(g_prog.insns[0].mac_dst.temp_reg == -1
          && g_prog.insns[0].mac_dst.write_mask == 0,
          "xemu drops a paired MAC write aimed at R1");
    CHECK(g_prog.insns[0].ilu_dst.output_reg == NV2A_VSH_OUT_D1,
          "the slot's output write is unaffected");
}

static void test_orb_selects_a_constant_register(void)
{
    float inputs[NV2A_VS_MAX_INPUTS][4];
    float consts[2][4];
    NV2AVshState st;
    uint32_t w[4];

    one_insn(w, 4);                      /* ADD oPos, R2, c[1] */
    set_field(w, F_OUT_ORB, 1, 0);       /* OUTPUT_C: the target is c[OUT_ADDRESS] */
    nv2a_vsh_parse(w, 1, &g_prog);
    CHECK(g_prog.insns[0].mac_dst.output_reg == NV2A_VSH_OUT_NONE,
          "a constant-register write is not an output write");
    CHECK(g_prog.insns[0].out_const_index == 0, "the constant index is recorded");

    memset(inputs, 0, sizeof inputs);
    memset(consts, 0, sizeof consts);
    nv2a_vsh_execute(&g_prog, (ConstRows)inputs, (ConstRows)consts, 2, &st);
    CHECK(!(st.out_written & (1u << NV2A_VSH_OUT_POS)), "oPos is not written");
}

static void test_fog_mask(void)
{
    float inputs[NV2A_VS_MAX_INPUTS][4];
    float consts[1][4];
    NV2AVshDstOperand d;
    NV2AVshState st;
    uint32_t w[4];

    memset(&d, 0, sizeof d);
    d.output_reg = NV2A_VSH_OUT_FOG;
    d.output_mask = 0x1;
    CHECK(nv2a_vsh_output_write_mask(&d) == 0x8, "an oFog mask of w fills x");
    d.output_mask = 0x3;
    CHECK(nv2a_vsh_output_write_mask(&d) == 0xC, "an oFog mask of zw fills xy");
    d.output_mask = 0xF;
    CHECK(nv2a_vsh_output_write_mask(&d) == 0xF, "an oFog mask of xyzw is unchanged");
    d.output_reg = NV2A_VSH_OUT_D0;
    d.output_mask = 0x1;
    CHECK(nv2a_vsh_output_write_mask(&d) == 0x1, "other outputs keep their mask");

    /* A vector result, so the component rule is visible: RCP replicates one
     * value and could not tell them apart. xemu's GLSL takes the same-named
     * component of the result (x), not the "most significant masked" one
     * its comment describes (w). This follows the code. */
    one_insn(w, 2);                      /* RCP oFog, v0.w ... */
    set_field(w, F_ILU, 3, 1);           /* ... made MOV oFog, v0 */
    set_field(w, F_C_SWZ_X, 2, 0);
    set_field(w, F_C_SWZ_Y, 2, 1);
    set_field(w, F_C_SWZ_Z, 2, 2);
    set_field(w, F_C_SWZ_W, 2, 3);
    set_field(w, F_OUT_O_MASK, 4, 0x1);  /* mask w */
    nv2a_vsh_parse(w, 1, &g_prog);
    memset(inputs, 0, sizeof inputs);
    memset(consts, 0, sizeof consts);
    inputs[0][0] = 1.0f; inputs[0][1] = 2.0f; inputs[0][2] = 3.0f; inputs[0][3] = 4.0f;
    nv2a_vsh_execute(&g_prog, (ConstRows)inputs, (ConstRows)consts, 1, &st);
    CHECK(st.out[NV2A_VSH_OUT_FOG][0] == 1.0f, "a w-masked oFog write puts result.x in x");
    CHECK(st.out[NV2A_VSH_OUT_FOG][3] == 0.0f, "and leaves w alone");
}

static void test_output_write_gating(void)
{
    uint32_t w[4];

    one_insn(w, 4);                      /* ADD oPos, R2, c[1] */
    set_field(w, F_OUT_O_MASK, 4, 0);    /* output mask cleared */
    nv2a_vsh_parse(w, 1, &g_prog);
    CHECK(g_prog.insns[0].mac_dst.output_reg == NV2A_VSH_OUT_NONE,
          "no output write without OUT_O_MASK");

    one_insn(w, 2);                      /* RCP oFog: ILU only, OUT_MUX = ILU */
    set_field(w, F_OUT_MUX, 1, 0);       /* name the MAC, which is a NOP */
    nv2a_vsh_parse(w, 1, &g_prog);
    CHECK(g_prog.insns[0].mac_dst.output_reg == NV2A_VSH_OUT_NONE
          && g_prog.insns[0].ilu_dst.output_reg == NV2A_VSH_OUT_NONE,
          "no output write when OUT_MUX names a unit that does not run");
}

static void test_inputs_read_ignores_unread_sources(void)
{
    uint32_t w[4];

    one_insn(w, 0);                      /* MOV R1, v0 */
    set_field(w, F_A_MUX, 2, 3);         /* A now reads a constant */
    set_field(w, F_B_MUX, 2, 2);         /* B is encoded as an input ... */
    set_field(w, F_INPUT, 4, 5);         /* ... v5, which MOV never reads */
    nv2a_vsh_parse(w, 1, &g_prog);
    CHECK(g_prog.inputs_read == 0, "a MOV from a constant reads no input");
}

int main(void)
{
    test_frontend_decode();
    test_frontend_execute();
    test_paired_ilu_writes_r1();
    test_paired_mac_write_to_r1_is_dropped();
    test_orb_selects_a_constant_register();
    test_fog_mask();
    test_output_write_gating();
    test_inputs_read_ignores_unread_sources();

    printf("nv2a_vsh: %d checks, %d failed\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
