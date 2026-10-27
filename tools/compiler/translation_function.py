"""Original function-to-C assembly over the project's decoder and lifter.

The guest register/runtime ABI remains an external interface. Flag states are
captured-condition descriptions, not complete EFLAGS. Existing diagnostic and
title hooks remain ordered before guest execution.
"""
import os
import re

import translation_batch as batch
import translation_context as context
import translation_layout as _config
import translation_strings as strings
from generation_output import write_if_changed
from translation_bits import constant_noop
from translation_conditions import zf_expression
from translation_decode import Disassembler
from translation_discovery import FunctionDiscovery
from translation_lifting import block as lift_basic_block
from translation_names import identifier
from translation_operands import _GENERAL, _PARTS, _operand_width
from xbe_image import XBEParser as _ProjectXBEParser

Lifter = context.Lifter
xbe_title = batch.xbe_title
va_to_file_offset = _config.va_to_file_offset
is_code_address = _config.is_code_address
_FLOAT_COMPARE = frozenset(('comiss','ucomiss','comisd','ucomisd'))
_CARRY_CC = frozenset('b nae c ae nb nc be na a nbe'.split())


def _merge_zero_flag(states):
    if not states: return None
    destination = None
    for state in states:
        if not state or state[0] not in context.ZF_FROM_DEST or not state[1]: return None
        operand = state[1][0]
        if operand.type != 'reg' or not operand.reg: return None
        if destination is not None and destination != operand.reg: return None
        destination = operand.reg
    return '__zf_from_dest', [states[0][1][0]]


def _merge_flag_states(states):
    if not states or any(not state or not state[0] for state in states): return None
    first = states[0]
    if all(state == first for state in states): return first
    if first[0] in ('cmp','test') and len(first[1]) == 2:
        width = lambda operands: _operand_width(operands[0]) or _operand_width(operands[1])
        if all(kind == first[0] and len(operands) == 2 and width(operands) == width(first[1]) for kind,operands in states): return first
        return None
    if all(kind in _FLOAT_COMPARE and len(operands) == 2 for kind,operands in states): return first
    return _merge_zero_flag(states)


def _incoming_flag_state(sources, known, is_entry):
    if is_entry or not sources or not set(sources) <= known.keys(): return None
    states = [known[address] for address in sources]
    merged = _merge_flag_states(states)
    if merged is not None: return merged
    return ('__zf_snapshot', []) if all(zf_expression(state) is not None for state in states) else None


def _before_terminator(statements):
    index = len(statements)
    for statement in reversed(statements):
        text = statement.lstrip()
        if not (text.startswith(('goto ','return')) or 'return;' in text or (text.startswith('if (') and 'goto' in text)): break
        index -= 1
    return index


_ICALL = re.compile(r'\bRECOMP_ICALL_SAFE(?:_AT)?\(')
_LABEL = re.compile(r'^loc_[0-9a-fA-F]+:')
_CLEANUP = re.compile(r'^esp = esp \+ (?:0x[0-9a-fA-F]+|[0-9]+);$')


def _fixup_icall_esp_save(lines):
    """Place failed-call stack captures around the existing statement ABI.

    First pushes of restored nonvolatile registers are frame saves. Later
    pushes are arguments. A nearby caller cleanup retains the arguments; other
    sites capture before them. This bounded textual policy is not ABI inference.
    """
    sites = [index for index,line in enumerate(lines) if _ICALL.search(line)]
    if not sites: return lines
    stripped = [line.strip() for line in lines]
    frame_saves = set()
    for register in ('ebx','esi','edi','ebp'):
        if any(f'POP32(esp, {register})' in line for line in lines):
            first = next((i for i,line in enumerate(stripped) if line.startswith(f'PUSH32(esp, {register})')),None)
            if first is not None: frame_saves.add(first)
    openings = set()
    for site in sites:
        following = next((line for line in stripped[site+1:site+7] if line and not _LABEL.match(line)), '')
        capture = site
        if not _CLEANUP.fullmatch(following):
            for index in range(site-1,-1,-1):
                line = stripped[index]
                if not line: continue
                if index in frame_saves or '/* call 0x' in line: break
                if line.startswith('PUSH32(esp,'):
                    capture = index
                    continue
                if (_LABEL.match(line) or 'goto ' in line or 'RECOMP_ICALL' in line or
                    'return;' in line or line.startswith(('if (','POP32('))): break
        openings.add(capture)
    result = []
    for index,line in enumerate(lines):
        indent = line[:len(line)-len(line.lstrip())]
        if index in openings: result.append(indent+'{ uint32_t _icall_esp = g_esp;')
        result.append(line)
        if index in sites: result.append(indent+'}')
    return result


# Evaluate a pushed value before changing the circular stack index.
FP_STACK_MACROS = [
    '    #define fp_push(value) do { const double _fp_input = (value); g_fp_top = (g_fp_top - 1u) & 7u; g_fp_stack[g_fp_top] = _fp_input; } while (0)',
    '    #define fp_pop() (g_fp_top = (g_fp_top + 1u) & 7u)',
    '    #define fp_top() g_fp_stack[g_fp_top]',
    '    #define fp_st(index) g_fp_stack[(g_fp_top + (index)) & 7u]',
    '    #define fp_st1() fp_st(1)',
]
FP_STACK_UNDEFS = ['    #undef '+name for name in ('fp_push','fp_pop','fp_top','fp_st','fp_st1')]


def _comment(value):
    return ' '.join(str(value).replace('*/','* /').splitlines())


class FunctionTranslator(FunctionDiscovery):
    _CARRY_CC = _CARRY_CC

    @staticmethod
    def _function_needs_cf(instructions):
        producer = None
        for insn in instructions:
            name = insn.mnemonic
            if (name in ('lahf','rcl','rcr','mul','imul','bt','bts','btr','btc') or
                name.removeprefix('lock ') in ('xadd','cmpxchg') or
                (not name.startswith('rep') and strings.is_compare(insn))): return True
            if constant_noop(insn): continue
            if name in ('adc','sbb','stc','clc','cmc'): return True
            condition = next((name[len(prefix):] for prefix in ('cmov','set','j') if name.startswith(prefix)),None)
            if condition in _CARRY_CC and producer in (context.CF_TRACKED | context.BT_MODIFY | {'inc','dec','rep-cmps','rol','ror'}): return True
            if name.startswith('rep') and ('cmps' in name or 'scas' in name): producer = 'rep-cmps'
            elif name in context.FLAG_SETTERS or name in context._EFLAGS_SETTERS: producer = name
            elif name in context._FLAGS_UNDEFINED: producer = None
        return False

    def _find_used_registers(self, instructions):
        found = set()
        for insn in instructions:
            for operand in insn.operands:
                names = (operand.reg,) if operand.type == 'reg' else (operand.mem_base,operand.mem_index) if operand.type == 'mem' else ()
                found.update(_PARTS.get(name,(name,))[0] for name in names if name in _PARTS or name in _GENERAL)
        return found

    def _find_used_xmm(self, instructions):
        return {op.reg for insn in instructions for op in insn.operands if op.type == 'reg' and op.reg and op.reg.startswith(('xmm','mm'))}

    def _decode_body(self, start, end, data, recovered):
        instructions = recovered['instructions'] if recovered else self.disasm.disassemble_function(data,start,end)
        indirect_register = any(i.mnemonic == 'jmp' and i.jump_target is None and i.operands and i.operands[0].type == 'reg' for i in instructions)
        immediate = {i.operands[1].imm for i in instructions if indirect_register and i.mnemonic == 'mov' and len(i.operands)>=2
                     and i.operands[0].type == 'reg' and i.operands[1].type == 'imm' and start <= i.operands[1].imm < end}
        self.lifter.imm_code_refs = immediate
        def leaders(code):
            return immediate | {target for i in code if i.mnemonic == 'jmp' and i.jump_target is None and i.operands
                                for target in self.lifter._analyze_switch_table(i.operands) if start <= target < end}
        targets = leaders(instructions)
        if recovered is None:
            anchors = set()
            for _ in range(8):
                missing = targets - {i.address for i in instructions}
                if not missing: break
                anchors.update(missing)
                instructions = self.disasm.disassemble_function(data,start,end,resync=anchors)
                targets = leaders(instructions)
        if os.environ.get('RECOMP_DEBUG_FUNC','').upper() == f'{start:08X}':
            print(f'[debug] sub_{start:08X}: end 0x{end:08X} recovered={recovered is not None} insns={len(instructions)} switch_leaders={sorted(hex(t) for t in targets)} undecoded={sorted(hex(t) for t in targets-{i.address for i in instructions})}')
        blocks = self.disasm.build_basic_blocks(instructions,start,end,extra_leaders=targets or None)
        return instructions,blocks

    def _emit_blocks(self, blocks, instructions, start, end, conditional):
        labels = {successor for block in blocks for successor in block.successors} | self.lifter.imm_code_refs | {start}
        labels.update(i.jump_target for i in instructions if i.jump_target is not None and start <= i.jump_target < end)
        for insn in instructions:
            if insn.mnemonic == 'jmp' and insn.jump_target is None and insn.operands:
                labels.update(self.lifter._analyze_switch_table(insn.operands))
        predecessors = {block.start:set() for block in blocks}
        for index,block in enumerate(blocks):
            if not block.instructions: continue
            last = block.instructions[-1]
            if last.jump_target in predecessors: predecessors[last.jump_target].add(block.start)
            if not last.is_ret and last.mnemonic not in ('jmp','int3','ud2','hlt') and index+1 < len(blocks):
                predecessors[blocks[index+1].start].add(block.start)
        outgoing = {}
        settled = outgoing
        def incoming(block,states):
            return _incoming_flag_state(predecessors[block.start],states,block.start == start)
        def probe(action):
            saved = {key:list(value) for key,value in self.lifter.unimplemented.items()}
            try: return action()
            finally:
                self.lifter.unimplemented.clear()
                self.lifter.unimplemented.update(saved)
        def settle():
            # Preserve the current bounded prepass contract during migration.
            for _ in range(3):
                changed = False
                for block in blocks:
                    _,state = lift_basic_block(self.lifter,block,flag_state=incoming(block,outgoing))
                    if outgoing.get(block.start) != state:
                        outgoing[block.start] = state
                        changed = True
                if not changed: break
        if any(len(parents)>1 or any(parent>=address for parent in parents) for address,parents in predecessors.items()):
            probe(settle)
            outgoing = {}
        need_zero = set()
        for block in blocks:
            state = incoming(block,settled)
            if conditional and state == ('__zf_snapshot',[]): need_zero.update(predecessors[block.start])
            if block.start != start and len(predecessors[block.start])>1 and not state:
                statements,_ = probe(lambda:lift_basic_block(self.lifter,block,flag_state=None))
                if any('_flags /*' in statement for statement in statements): need_zero.update(predecessors[block.start])
        lines = []
        for block in blocks:
            if block.start in labels: lines.append(f'loc_{block.start:08X}: ;')
            statements,outgoing[block.start] = lift_basic_block(self.lifter,block,flag_state=incoming(block,settled))
            if block.start in need_zero:
                zero = zf_expression(outgoing[block.start])
                if zero and zero != '_flags': statements.insert(_before_terminator(statements),f'_flags = {zero}; /* outgoing ZF snapshot */')
            lines.extend('    '+statement for statement in statements)
            lines.append('')
        return lines

    def translate_function(self, func_addr, func_info):
        start = func_addr
        recovered = self._recovered_cfg.get(start)
        end = (recovered['end'] if recovered else func_info.get('end')) or start+func_info.get('size',0)
        if end <= start: return None
        data = self._read_func_bytes(start,end)
        if not data: return None
        lifter = self.lifter
        lifter.func_start,lifter.func_end = start,end
        lifter.jump_table_targets = recovered['jump_tables'] if recovered else {}
        lifter.icall_site_targets = self.icall_sites
        instructions,blocks = self._decode_body(start,end,data,recovered)
        if not instructions or not blocks: return None
        name = identifier(start,func_info.get('name'))
        abi = self.abi_db.get(start,{})
        classified = self.classification_db.get(start,{})
        frame = abi.get('frame_type','fpo_leaf')
        used = self._find_used_registers(instructions)
        prologue = self._func_has_prologue(instructions)
        floating = any(i.mnemonic.startswith('f') for i in instructions)
        conditional = any(i.is_cond_jump or i.mnemonic.startswith(('set','cmov')) for i in instructions)
        last = instructions[-1]
        fallthrough = end if not last.is_terminator and last.mnemonic not in ('int3','ud2','hlt') and end in self.func_db and end not in self.owned_function_starts else None
        tail = any((i.is_jump and (i.jump_target is None or not start <= i.jump_target < end)) or
                   (i.is_cond_jump and i.jump_target is not None and not start <= i.jump_target < end) for i in instructions)
        if tail or fallthrough is not None or any(i.mnemonic in ('leave','div','idiv') or i.call_target in lifter.SEH_PROLOGS | lifter.SEH_EPILOGS for i in instructions): used.add('ebp')
        lifter.trace_exit_name = name if start in self.trace_functions else None
        lifter.force_return_value = self.force_returns.get(start)
        lifter.needs_cf = self._function_needs_cf(instructions)
        lifter.publishes_ebp = self._func_owns_a_frame(instructions)
        description = f'{name}: guest 0x{start:08X}..0x{end:08X}; {len(instructions)} instructions; category={classified.get("category","unknown")}; source={classified.get("source_file","")}; ABI={abi.get("calling_convention","cdecl")}, {abi.get("estimated_params",0)} parameters, {abi.get("return_hint","int_or_void")}; frame={frame}'
        lines = ['/* '+_comment(description)+' */','#ifdef RECOMP_GPR_REFS_SUPPORTED',
                 f'#define RECOMP_REGISTER_SITE 0x{start:08X}u','#include "recomp_types.h"','#endif',f'void {name}(void)','{']
        if start in self.trace_functions or self.trace_all_entries: lines.append(f'    RECOMP_TRACE_ENTER("{name}", 0x{start:08X});')
        if start not in self.trace_functions and start not in self.force_returns: lines.append(f'    if (RECOMP_FAST_ENTRY(0x{start:08X}u)) return;')
        lines += ['#ifdef RECOMP_GPR_REFS_SUPPORTED',f'    RECOMP_DECLARE_GPRS(0x{start:08X}u);','#endif']
        if start == lifter.CXX_THROW: lines.append('    recomp_cxx_throw(MEM32(esp + 4), MEM32(esp + 8));')
        if 'ebp' in used: lines += ['    uint32_t ebp = 0;','    ebp = g_ebp; /* incoming frame register */']
        if conditional or any(strings.is_compare(i) for i in instructions): lines.append('    int _flags = 0; /* captured fallback condition */')
        snapshots = {'cmp','test','bsf','bsr','cmpxchg','lock cmpxchg','xadd','lock xadd','inc','dec'} | context._RESULT_SNAPSHOT_SETTERS
        if any(i.mnemonic in snapshots or (strings.is_compare(i) and not i.mnemonic.startswith('rep')) for i in instructions):
            lines += ['    uint32_t _fa = 0, _fb = 0;','    int32_t _fas = 0, _fbs = 0;','    (void)_fa; (void)_fb; (void)_fas; (void)_fbs;']
        if any(i.mnemonic in _FLOAT_COMPARE for i in instructions): lines += ['    double _fca = 0.0, _fcb = 0.0;','    (void)_fca; (void)_fcb;']
        if lifter.needs_cf: lines.append('    int _cf = 0; /* captured carry */')
        if floating: lines.extend(FP_STACK_MACROS)
        if frame == 'fpo_leaf' and 'ebp' in used and not prologue: lines.append('    ebp = g_seh_ebp; /* inherited shared frame */')
        lines += ['']+self._emit_blocks(blocks,instructions,start,end,conditional)
        if fallthrough is not None:
            target = lifter._call_target_name(fallthrough)
            lines += [f'    g_seh_ebp = ebp; {target}(); return; /* fallthrough 0x{fallthrough:08X} */','']
        lines = _fixup_icall_esp_save(lines)
        labels = {match[1] for line in lines if (match := re.match(r'^(loc_[0-9A-Fa-f]+):',line))}
        def resolve(match):
            return match[0] if match[1] in labels else f'(void)0; /* unavailable local label {match[1]} */'
        lines = [re.sub(r'goto (loc_[0-9A-Fa-f]+);',resolve,line) for line in lines]
        # Every emitted label already has its required C null statement.
        if floating: lines.extend(FP_STACK_UNDEFS)
        return '\n'.join(lines+['}',''])


class BatchTranslator(batch.BatchTranslator):
    function_translator_type = FunctionTranslator
