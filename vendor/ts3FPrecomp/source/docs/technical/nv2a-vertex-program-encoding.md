# NV2A vertex program encoding — what is derived, and what is not

> **Status, September 2026: applied and audited.** The parser and the CPU
> interpreter live in `src/kernel/nv2a_vsh.c`, moved there so that
> `xbox_kernel` builds without a graphics API. Two tests hold them:
> `tools/vsh_audit/test_vsh_encoding.py` checks the field table's constants
> against the twelve-instruction disassembly below, `tests/nv2a_vsh` runs the
> real C decoder and interpreter over the rules in **Found by audit**, and
> `tests/nv2a_vsh_hlsl` compiles the generated HLSL with D3DCompile. The next
> paragraph describes the table as it was before the fix.

`src/d3d/d3d8_vsh.c` parsed vertex microcode into `NV2AVshProgram`. Its field
table (`VSH_FIELD_*`) was **wrong**, in a way that cannot be partially right:
several fields overlapped, so a destination register was decoded partly from a
write mask and an output selector's low bit was the final-instruction flag.
The consequence was that the interpreter (then `d3d8_vsh_execute`) wrote
nothing at all — `out_written == 0` for every vertex.

Nothing in this repository documented the real encoding, and
`docs/technical/nv2a-shaders.md` pointed at a microcode translator in
`nv2a_pgraph_d3d11.c` that did not exist. So the table below was derived from
Burnout 2's own microcode rather than from recollection, and each row says
what made it believable.

## The sample

`RECOMP_VP_DUMP=<path>` writes the uploaded program. Burnout 2's frontend
uploads twelve instructions, and `dword0` is zero in all of them — which is
the first hint the field table is displaced, since it places the opcodes
there.

```
insn   0 00000000 0020001B 0836106C 2F100FF8
insn   1 00000000 0420061B 083613FC 5011F818
insn   2 00000000 0400001B 083613FC 2070F82C
insn   3 00000000 0240081B 1436186C 2F20F824
insn   4 00000000 0060201B 2436106C 3070F800
insn   5 00000000 00200200 0836106C 2070F830
insn   6 00000000 00200E1B 0836106C 2070F838
insn   7 00000000 0020101B 0836106C 2070F840
insn   8 00000000 0020121B 0836106C 2070F848
insn   9 00000000 0020141B 0836106C 2070F850
insn  10 00000000 0020161B 0836106C 2070F858
insn  11 00000000 0020181B 0836106C 2070F861
```

## Derived, with evidence

| Field | Position | Why it is believed |
|---|---|---|
| `FINAL` | dword3 bit 0 | Set on instruction 11 and on no other. Exactly one final bit is what a valid program has. |
| `OUT_ADDRESS` | dword3 bits 3-10 | Decodes to 7, 8, 9, 10, 11, 12 across instructions 6-11, which are `oB0`, `oB1`, `oT0`, `oT1`, `oT2`, `oT3` in the output enum's own order. Instruction 0 gives `0xFF`. That is not a "no output" code: whether an output is written is `OUT_O_MASK`'s decision, and instruction 0's mask is zero (see **Found by audit**). |
| `CONST` | dword1 bits 9-16 | With this offset the program reads `c3` into `oD0`, `c4` into `oD1`, `c7` into `oB0`, `c8` into `oB1`, and `c9`-`c12` into `oT0`-`oT3`. The output enum numbers those outputs 3, 4, 7, 8, 9-12. Six consecutive `MOV o<N>, c<N>` is not a coincidence. At the previously assumed offset every source read `c0`. |
| `MAC` opcode | dword1 bits 21-24 | Yields MOV for the ten output initialisers, MUL at 3 and ADD at 4 — the only two instructions that compute anything. |
| `ILU` opcode | dword1 bits 25-27 | Yields NOP almost everywhere and RCP at instructions 1-2, which is what a perspective divide looks like. Three bits, not four: the enum has eight entries. |
| A swizzle | dword1 bits 0-7 (w,z,y,x at 0,2,4,6) | Instruction 0's low byte is `0x1B` = `00 01 10 11`, which reads as the identity swizzle `xyzw`. A wrong offset would make the commonest swizzle in any program look like something else. |
| A source bank | dword2 bits 26-27 | Const for the ten initialisers, input for instructions 3 and 4 — the two that read vertex data. |
| A source index | dword2 bits 28-31 | `v1` at instruction 3 and `v2` at instruction 4, consistent with the bank above. |
| Destination masks and mux | dword3: MAC mask 24-27, temp index 20-23, ILU mask 16-19, output mask 12-15, ORB 11, MUX 2 | Instruction 0 reads as `MOV R1, c0` with no output write; instruction 11 as `MOV oT3, c12` with no temp write. Both are coherent, and they are the two ends of the program. |

## Resolved, authoritatively

The derivation above is correct, and the two fields it could not reach are now
known. `abaire/nv2a_vsh_asm` is an assembler and disassembler for this exact
instruction set; its `src/nv2a_vsh/nv2a_vsh_asm/vsh_instruction.py` defines
the encoding as ctypes bitfields, LSB-first per dword, and it agrees with every
field derived above. xemu's `field_mapping[]` in
`hw/xbox/nv2a/pgraph/glsl/vsh-prog.c` agrees too.

Running its disassembler over Burnout 2's twelve instructions gives ground
truth, and the result is the canonical Xbox pass-through shader:

```
 0  MOV R1.xyzw, v0
 1  MOV oD0.xyzw, v3      + RCP R1.w, R1.w
 2  RCP oFog.xyzw, v0.w
 3  MUL R2.xyzw, R1, c[0] + MOV oD1.xyzw, v4
 4  ADD oPos.xyzw, R2, c[1]
 5  MOV oPts.xyzw, v1.x
 6  MOV oB0.xyzw, v7          9  MOV oT1.xyzw, v10
 7  MOV oB1.xyzw, v8         10  MOV oT2.xyzw, v11
 8  MOV oT0.xyzw, v9         11  MOV oT3.xyzw, v12
```

Every attribute passes to the output with the matching number, which is what
confirmed the INPUT field: my own derivation had read those bits as CONST and
produced a plausible-looking `MOV o<N>, c<N>` instead. The one wrong call in
nine, and it looked right.

`oPos = v0 * c[0] + c[1]`, and the constants are `c[0] = (1, 1, 16777215, 1)`
and `c[1] = (0.53125, 0.53125, 0, 0)` -- a half-pixel bias on coordinates that
are already in window space. So for this shader the transform is identity, the
raw attribute is the correct position, and executing the program cannot change
where anything lands.

### The full layout

Absolute bit offsets for `vsh_extract()`, where dword N starts at bit 32*N.
dword0 is unused.

| dword1 | dword2 | dword3 |
|---|---|---|
| A_SWZ_W 32, Z 34, Y 36, X 38 | C_TEMP_HIGH 64 (2) | FINAL 96 |
| A_NEG 40 | C_SWZ_W 66, Z 68, Y 70, X 72 | A0X 97 |
| INPUT 41 (4) | C_NEG 74 | OUT_MUX 98 |
| CONST 45 (8) | B_MUX 75 (2), B_TEMP 77 (4) | OUT_ADDRESS 99 (8) |
| MAC 53 (4) | B_SWZ_W 81, Z 83, Y 85, X 87 | OUT_ORB 107 |
| ILU 57 (3) | B_NEG 89 | OUT_O_MASK 108 (4) |
| | A_MUX 90 (2), A_TEMP 92 (4) | OUT_ILU_MASK 112 (4) |
| | | OUT_TEMP 116 (4) |
| | | OUT_MAC_MASK 120 (4) |
| | | C_MUX 124 (2), C_TEMP_LOW 126 (2) |

Four asymmetries, each of which was a bug in the parser before this table was
applied:

- **A source bank is 1 = temp, 2 = input, 3 = const.** Zero is not a bank. The
  old parser mapped the raw value onto an enum that starts at zero, so every
  operand came out one bank wrong.
- **The ILU opcode is three bits**, read as four, which made unknown opcodes
  reachable.
- **MAC and ILU share one destination temp index** and have a write mask each,
  and there is a *single* output-register write whose source OUT_MUX selects.
  The old parser gave each unit its own output register, which does not exist,
  and carried one mask where there are two. Pairing refines the shared index;
  see **Found by audit**.
- **Source C's temp index is split** across dword2 bits 64-65 and dword3 bits
  126-127, so it cannot be read as one field.

A first rewrite against this table was reverted: a scripted edit removed
declarations it should not have touched. The table was then applied by hand in
`4c69b4c`. For the one shader Burnout 2 uploads so far the transform is
identity, so the fix matters for real 3D rather than for the current image.

## Found by audit

An independent audit compared the parser with xemu (`decode_opcode()` in
`hw/xbox/nv2a/pgraph/glsl/vsh-prog.c`) and with abaire
(`_disassemble_outputs()` in `vsh_instruction.py`), both read from source.
Every offset and width above survived. How the fields are *resolved* did not,
in the places below. Each is now fixed in `nv2a_vsh_parse()` or
`nv2a_vsh_execute()` and tested in `tests/nv2a_vsh`; a second audit checked
that each test fails when its fix is removed. None of them changes
Burnout 2's frontend shader, which is why the twelve instructions above could
not show them.

| Rule | Evidence |
|---|---|
| **A paired ILU writes R1.** When MAC and ILU both run, the ILU's temp write goes to R1 whatever `OUT_TEMP` says. | xemu: `/* Paired ILU opcodes can only write to R1 */`. abaire: "ILU will write to R1 regardless of the encoded target". |
| **A paired MAC write aimed at R1 is dropped.** | xemu only: `/* Ignore paired MAC opcodes that write to R1 */`. abaire keeps the write. The parser follows xemu, which runs real titles; not verified on hardware. |
| **`OUT_O_MASK` decides whether there is an output write**, not `OUT_ADDRESS`, and only a unit that runs makes it. The address is read as its low four bits, where 15 is a0.x. | xemu gates the write on `FLD_OUT_O_MASK != 0`, calls `decode_opcode()` only for non-NOP units, and indexes `out_reg_name[ADDRESS & 0xF]`. |
| **`OUT_ORB` = 0 targets a constant register**, not an output. Recorded as `out_const_index`; not emulated. | xemu: `OUTPUT_C = 0`, and `assert(!"TODO: Emulate writeable const registers")`. abaire writes `c[address]`. |
| **An oFog write fills the first *k* components** for a *k*-bit mask, from the same-named components of the result. | xemu `fog_mask_str`, used on both sides of its GLSL macros (`dest.mask = _MOV(_in(src)).mask`). Its comment describes taking the most significant masked component instead; the code is followed. |
| **Only the sources an opcode reads count as inputs.** MOV and ARL read A; MUL, DP3, DPH, DP4, DST, MIN, MAX, SLT and SGE read A and B; ADD reads A and C; MAD reads all three; the ILU reads C. | xemu `mac_opcode_params`. |

The same audit found two faults in the HLSL generator in `src/d3d/d3d8_vsh.c`,
independent of the field table. It wrote the MAC's temp before evaluating the
ILU, so an ILU source could read a value the slot had just changed. And it
evaluated each expression twice, with the temp written in between. Both units
now compute into locals first and then write, as the CPU interpreter always
did.

### Where this deliberately differs from xemu

Unverified on hardware either way, and recorded so they are not mistaken for
agreement:

- **Timing inside a paired slot.** xemu's GLSL writes a paired MAC's *output*
  before evaluating the ILU; only the MAC's temp write is deferred. Here both
  units read the register file as it was at the start of the slot.
- **ILU scalar sources.** For RCP, RCC, RSQ, EXP and LOG, xemu replicates
  source C's x swizzle (`ilu_force_scalar`), and a paired ADD or MAD then reads
  that scalar C. Here the MAC keeps C's full swizzle.
- **ARL** is `floor(A.x + 0.001)` in xemu and plain `floor(A.x)` here.
- **EXP and LOG** produce vector results in xemu and one replicated value here.

## Previously unresolved

Both questions below were settled by the references in **Resolved,
authoritatively**. Kept for the record.

**Source B and source C field positions.** Only one instruction in this
program uses B (the `MUL` at 3) and four use C, so the sample does not
constrain them: a search for two-bit fields whose value is always a legal
bank (0, 1 or 2) leaves sixteen candidates. Fitting a position to four data
points is how a plausible wrong answer gets adopted.

**Whether an input register index is shared per instruction or per source.**
The hardware is documented elsewhere as having one `v#` per instruction, but
here the per-source index field (dword2 bits 28-31) already produces the right
answer, and this program never reads two different inputs in one slot. The two
readings are indistinguishable on this sample.

## How the remaining question was going to be resolved

(Kept for the record; the reference above settled it.) Capture a program that
transforms world geometry. It will contain a four-
instruction `DP4` chain against consecutive constants — the matrix multiply —
and a `DP4` uses A and B together with known operand roles, which pins both
fields immediately. Burnout 2 uploads such a program once it draws something
other than its frontend; the frontend shader above is close to a pass-through,
which is also why its vertices arrive already in pixels.

Until then `fetch_position()` in `nv2a_pb_exec.c` falls back to the raw
attribute whenever the program writes no position, and counts every time it
does so.
