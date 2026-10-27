/*
 * nv2a_vsh_hlsl -- compile the HLSL the vertex-program translator generates.
 *
 * d3d8_vsh_generate_hlsl() writes shader source as text. A mistake in it is
 * invisible until a title uploads a program that reaches the broken path, and
 * then it surfaces as a shader that fails to compile in the middle of a run.
 * This runs the generator over Burnout 2's real program and over synthetic
 * instructions that reach every MAC and ILU opcode, paired slots, a paired ARL
 * feeding an a0-relative constant read, an oFog write, and temp writes to R12
 * (which is oPos) and R13 (which does not exist), and a read of R13. Each
 * result goes to
 * D3DCompile with the profile and entry point the runtime uses.
 *
 * It also checks the shape the generator promises: inside a slot, every value
 * is computed into a local before anything is written, because the MAC and
 * ILU run in parallel on the hardware.
 *
 * Windows only, for D3DCompile. No device, window or game files.
 */

#include "d3d8_internal.h"
#include <d3d11.h>   /* this test drives D3D11 itself */
#include <d3dcompiler.h>
#include <stdio.h>
#include <string.h>

/* d3d8_vsh.c's links to the rest of the D3D8 layer. Nothing here creates a
 * device; only the generator and the compiler run. */
ID3D11Device        *d3d8_GetD3D11Device(void)  { return NULL; }
ID3D11DeviceContext *d3d8_GetD3D11Context(void) { return NULL; }
DWORD                d3d8_GetCurrentFVF(void)   { return 0; }
BOOL                 d3d8_target_is_screen(void) { return TRUE; }   /* camera zoom: d3d8_device.c */

static int g_checks;
static int g_failures;

static void check(int cond, const char *program, const char *what)
{
    g_checks++;
    if (!cond) {
        g_failures++;
        printf("FAIL %s: %s\n", program, what);
    }
}

/* Burnout 2's frontend shader, the canonical Xbox pass-through program. */
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

/* Absolute bit offsets, from docs/technical/nv2a-vertex-program-encoding.md.
 * Written out rather than taken from VSH_FIELD_*, which are under test. */
enum {
    F_A_SWZ_W = 32, F_A_SWZ_Z = 34, F_A_SWZ_Y = 36, F_A_SWZ_X = 38,
    F_INPUT = 41, F_CONST = 45, F_MAC = 53, F_ILU = 57,
    F_C_TEMP_HI = 64,
    F_C_SWZ_W = 66, F_C_SWZ_Z = 68, F_C_SWZ_Y = 70, F_C_SWZ_X = 72,
    F_B_MUX = 75, F_B_TEMP = 77,
    F_B_SWZ_W = 81, F_B_SWZ_Z = 83, F_B_SWZ_Y = 85, F_B_SWZ_X = 87,
    F_A_MUX = 90, F_A_TEMP = 92,
    F_FINAL = 96, F_A0X = 97, F_OUT_MUX = 98, F_OUT_ADDRESS = 99,
    F_OUT_ORB = 107, F_OUT_O_MASK = 108, F_OUT_ILU_MASK = 112,
    F_OUT_TEMP = 116, F_OUT_MAC_MASK = 120, F_C_MUX = 124, F_C_TEMP_LO = 126,
};

enum { BANK_TEMP = 1, BANK_INPUT = 2, BANK_CONST = 3 };

static void set_field(uint32_t *insn, int start, int size, uint32_t value)
{
    int word = start / 32, bit = start % 32;
    uint32_t mask = (((size == 32) ? 0xFFFFFFFFu : ((1u << size) - 1u)) << bit);

    insn[word] = (insn[word] & ~mask) | ((value << bit) & mask);
}

static void identity_swizzles(uint32_t *w)
{
    set_field(w, F_A_SWZ_X, 2, 0); set_field(w, F_A_SWZ_Y, 2, 1);
    set_field(w, F_A_SWZ_Z, 2, 2); set_field(w, F_A_SWZ_W, 2, 3);
    set_field(w, F_B_SWZ_X, 2, 0); set_field(w, F_B_SWZ_Y, 2, 1);
    set_field(w, F_B_SWZ_Z, 2, 2); set_field(w, F_B_SWZ_W, 2, 3);
    set_field(w, F_C_SWZ_X, 2, 0); set_field(w, F_C_SWZ_Y, 2, 1);
    set_field(w, F_C_SWZ_Z, 2, 2); set_field(w, F_C_SWZ_W, 2, 3);
}

/* A MAC-only slot: op(A = v0, B = c[0], C = R1) -> R<temp>.xyzw and oT0. */
static void mac_slot(uint32_t *w, unsigned op, unsigned temp)
{
    memset(w, 0, 4 * sizeof *w);
    identity_swizzles(w);
    set_field(w, F_MAC, 4, op);
    set_field(w, F_A_MUX, 2, BANK_INPUT);
    set_field(w, F_INPUT, 4, 0);
    set_field(w, F_B_MUX, 2, BANK_CONST);
    set_field(w, F_CONST, 8, 0);
    set_field(w, F_C_MUX, 2, BANK_TEMP);
    set_field(w, F_C_TEMP_LO, 2, 1);
    set_field(w, F_OUT_TEMP, 4, temp);
    set_field(w, F_OUT_MAC_MASK, 4, 0xF);
    set_field(w, F_OUT_MUX, 1, 0);
    set_field(w, F_OUT_ORB, 1, 1);
    set_field(w, F_OUT_ADDRESS, 8, 9);          /* oT0 */
    set_field(w, F_OUT_O_MASK, 4, 0xF);
    set_field(w, F_FINAL, 1, 1);
}

/* An ILU-only slot: op(C = v0) -> R3.xyzw and oT1. */
static void ilu_slot(uint32_t *w, unsigned op)
{
    memset(w, 0, 4 * sizeof *w);
    identity_swizzles(w);
    set_field(w, F_ILU, 3, op);
    set_field(w, F_C_MUX, 2, BANK_INPUT);
    set_field(w, F_INPUT, 4, 0);
    set_field(w, F_OUT_TEMP, 4, 3);
    set_field(w, F_OUT_ILU_MASK, 4, 0xF);
    set_field(w, F_OUT_MUX, 1, 1);
    set_field(w, F_OUT_ORB, 1, 1);
    set_field(w, F_OUT_ADDRESS, 8, 10);         /* oT1 */
    set_field(w, F_OUT_O_MASK, 4, 0xF);
    set_field(w, F_FINAL, 1, 1);
}

/* ARL a0.x = floor(v0.x), paired with MOV oT2, c[a0 + 4]. The ILU must read
 * a0 as it was at the start of the slot. */
static void paired_arl_slot(uint32_t *w)
{
    memset(w, 0, 4 * sizeof *w);
    identity_swizzles(w);
    set_field(w, F_MAC, 4, 13);
    set_field(w, F_A_MUX, 2, BANK_INPUT);
    set_field(w, F_INPUT, 4, 0);
    set_field(w, F_ILU, 3, 1);
    set_field(w, F_C_MUX, 2, BANK_CONST);
    set_field(w, F_CONST, 8, 4);
    set_field(w, F_A0X, 1, 1);
    set_field(w, F_OUT_TEMP, 4, 4);
    set_field(w, F_OUT_ILU_MASK, 4, 0xF);
    set_field(w, F_OUT_MUX, 1, 1);
    set_field(w, F_OUT_ORB, 1, 1);
    set_field(w, F_OUT_ADDRESS, 8, 11);         /* oT2 */
    set_field(w, F_OUT_O_MASK, 4, 0xF);
    set_field(w, F_FINAL, 1, 1);
}

/* One instruction of the frontend program, marked as the last. */
static void frontend_slot(uint32_t *w, int index)
{
    memcpy(w, &FRONTEND[index * 4], 4 * sizeof *w);
    set_field(w, F_FINAL, 1, 1);
}

/* Inside every slot block of the program body, no write comes before the last
 * local is declared. Locals are "float4 _mac", "float4 _ilu" and "int _a0";
 * writes are assignments from them. */
static int locals_before_writes(const char *hlsl)
{
    const char *body = strstr(hlsl, "/* --- Program body");
    const char *stop = body ? strstr(body, "#undef R12") : NULL;
    const char *p = body;

    if (!body || !stop)
        return 0;
    while ((p = strstr(p, "    {\n")) != NULL && p < stop) {
        const char *end = strstr(p, "    }\n");
        const char *last_decl = NULL, *first_write = NULL, *q;

        if (!end || end > stop)
            return 0;
        for (q = p; q < end; q++) {
            if (!strncmp(q, "float4 _", 8) || !strncmp(q, "int _a0", 7))
                last_decl = q;
            if (!first_write && (!strncmp(q, "= (_", 4) || !strncmp(q, "= _a0", 5)))
                first_write = q;
        }
        if (last_decl && first_write && first_write < last_decl)
            return 0;
        p = end + 6;
    }
    return 1;
}

static void compile_program(const char *name, const uint32_t *words, int count)
{
    static NV2AVshProgram prog;
    static char hlsl[1 << 17];
    ID3DBlob *code = NULL;
    ID3DBlob *errors = NULL;
    HRESULT hr;
    int len;

    nv2a_vsh_parse(words, count, &prog);
    len = d3d8_vsh_generate_hlsl(&prog, hlsl, (int)sizeof hlsl);
    check(len > 0, name, "the generator produces source");
    if (len <= 0)
        return;

    hr = D3DCompile(hlsl, (SIZE_T)len, name, NULL, NULL, "main", "vs_5_0",
                    D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (FAILED(hr))
        printf("---- %s: D3DCompile failed ----\n%s\n---- source ----\n%s\n",
               name,
               errors ? (const char *)ID3D10Blob_GetBufferPointer(errors) : "(no message)",
               hlsl);
    check(SUCCEEDED(hr), name, "the generated HLSL compiles");
    check(locals_before_writes(hlsl), name, "every slot computes before it writes");

    if (code)
        ID3D10Blob_Release(code);
    if (errors)
        ID3D10Blob_Release(errors);
}

int main(void)
{
    uint32_t w[4];
    char name[64];
    unsigned op;

    compile_program("frontend", FRONTEND, 12);

    for (op = 1; op < NV2A_VSH_MAC_COUNT; op++) {
        mac_slot(w, op, 2);
        snprintf(name, sizeof name, "mac_op_%u", op);
        compile_program(name, w, 1);
    }
    for (op = 1; op < NV2A_VSH_ILU_COUNT; op++) {
        ilu_slot(w, op);
        snprintf(name, sizeof name, "ilu_op_%u", op);
        compile_program(name, w, 1);
    }

    frontend_slot(w, 1);                        /* MOV oD0 + RCP R1.w, paired */
    set_field(w, F_OUT_TEMP, 4, 5);
    compile_program("paired_ilu_temp_r5", w, 1);

    frontend_slot(w, 3);                        /* MUL R2 + MOV oD1, paired */
    set_field(w, F_OUT_TEMP, 4, 1);
    compile_program("paired_mac_temp_r1", w, 1);

    paired_arl_slot(w);
    compile_program("paired_arl_relative", w, 1);

    frontend_slot(w, 2);                        /* RCP oFog, masked to w */
    set_field(w, F_OUT_O_MASK, 4, 0x1);
    compile_program("fog_mask_w", w, 1);

    mac_slot(w, 1, 12);                         /* MOV into R12, which is oPos */
    compile_program("temp_r12_is_opos", w, 1);

    mac_slot(w, 1, 13);                         /* R13 does not exist */
    compile_program("temp_r13_skipped", w, 1);

    mac_slot(w, 1, 2);                          /* MOV R2, R13: a read of it */
    set_field(w, F_A_MUX, 2, BANK_TEMP);
    set_field(w, F_A_TEMP, 4, 13);
    compile_program("read_r13_is_zero", w, 1);

    printf("nv2a_vsh_hlsl: %d checks, %d failed\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
