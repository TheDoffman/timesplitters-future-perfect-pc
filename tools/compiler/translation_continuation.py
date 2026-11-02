"""Original instruction-boundary execution for restored guest continuations.

Normal translation remains unchanged. This cold path yields after each guest
instruction and represents CALL/RET/JMP with guest PCs, never native C calls.
It needs the project's persistent flags ABI and an external admitted dispatcher.
"""
from copy import copy

from translation_context import Lifter
from translation_decode import LOOP_JUMPS, BasicBlock
from translation_function import FP_STACK_MACROS, FP_STACK_UNDEFS
from translation_lifting import block, _status_code
from translation_operands import _fmt_operand_read as read, _fmt_operand_write as write

_FLAG_MASKS = (2048,2048,1,1,64,64,65,65,128,128,4,4,2176,2176,2240,2240)


def _hex(value):
    return f'0x{value & 0xffffffff:08X}u'


def _handoff(expression, event='YIELD'):
    return [f'*context_next = {expression};', f'return TSFP_CONTEXT_STEP_{event};']


def _known(mask):
    return [f'if ((g_status_flags.known & {mask}u) != {mask}u) return TSFP_CONTEXT_STEP_ERROR;']


def _condition(name):
    code = _status_code(name)
    if code is None:
        raise ValueError(f'Unsupported continuation condition: {name}')
    return _known(_FLAG_MASKS[code]), f'tsfp_status_condition(&g_status_flags,{code},0)'


class ContinuationLifter(Lifter):
    def _unimplemented(self, instruction, mnemonic, note=''):
        raise ValueError(f'Unsupported continuation instruction at {instruction.address:#x}: {mnemonic}')


def _control(instruction):
    name, operands = instruction.mnemonic, instruction.operands
    following = _hex(instruction.end_address)
    if name in ('retf','lcall','ljmp','iret','iretd'):
        raise ValueError('Far continuation control transfer is not modeled')
    if instruction.is_call or instruction.is_ret or instruction.is_branch:
        raw = bytes.fromhex(instruction.bytes_hex)
        prefixes = []
        for byte in raw:
            if byte not in (0x66,0x67,0xf0,0xf2,0xf3,0x26,0x2e,0x36,0x3e,0x64,0x65):
                break
            prefixes.append(byte)
        if 0x66 in prefixes:
            raise ValueError('16-bit continuation control transfer is not modeled')
    if name in ('call','jmp'):
        if len(operands)!=1 or operands[0].type not in ('reg','mem','imm'):
            raise ValueError('Continuation target requires one near operand')
        operand=operands[0]
        if operand.type=='reg' and operand.reg not in ('eax','ebx','ecx','edx','esi','edi','esp','ebp'):
            raise ValueError('Continuation target requires a32-bit register')
        if operand.type=='mem' and operand.mem_size!=4:
            raise ValueError('Continuation target requires a32-bit memory cell')
        # The operand, including [ESP], is sampled before the guest stack write.
        result=[f'uint32_t context_target = (uint32_t)({read(operand)});']
        if name=='call':result.append(f'PUSH32(esp, {following});')
        return result+_handoff('context_target')
    if name in ('ret','retn'):
        if len(operands)>1 or (operands and operands[0].type!='imm'):
            raise ValueError('Continuation RET cleanup requires an immediate')
        cleanup=0 if not operands else operands[0].imm & 0xffff
        return ['uint32_t context_target = MEM32(esp);',f'esp += {4+cleanup}u;']+_handoff('context_target','RETURN')
    if instruction.is_cond_jump:
        if instruction.jump_target is None:
            raise ValueError('Continuation conditional branch requires a decoded target')
        before=[]
        if name in ('jcxz','jecxz'):
            condition='LO16(ecx) == 0' if name=='jcxz' else 'ecx == 0'
        elif name in LOOP_JUMPS:
            short=0x67 in bytes.fromhex(instruction.bytes_hex)[:-2]
            counter='LO16(ecx)' if short else 'ecx'
            if name!='loop':before,condition=_condition('je' if name in ('loope','loopz') else 'jne')
            decrement='SET_LO16(ecx, (uint16_t)(ecx - 1u));' if short else 'ecx -= 1u;'
            before.append(decrement)
            condition=f'{counter} != 0'+(f' && ({condition})' if name!='loop' else '')
        else:
            before,condition=_condition(name)
        return before+_handoff(f'({condition}) ? {_hex(instruction.jump_target)} : {following}')
    if name.startswith(('set','cmov')) and _status_code(name) is not None:
        before,condition=_condition(name)
        if name.startswith('set') and len(operands)==1:
            return before+[write(operands[0],f'({condition}) ? 1 : 0')]+_handoff(following)
        if name.startswith('cmov') and len(operands)==2:
            return before+[f'if ({condition}) '+write(operands[0],read(operands[1]))]+_handoff(following)
        raise ValueError('Malformed continuation flag consumer')
    return None


def emit(start, instructions, lifter=None):
    """Emit a PC membership predicate and one-instruction step over decoded code.

    The caller owns decode/recovery and dispatch admission. Unsupported opcode
    forms raise rather than emit a successful no-op. No normal entry/title hooks
    are executed, and no existing fast function is rewritten by this API.
    """
    instructions=tuple(instructions)
    addresses={i.address for i in instructions}
    if not instructions or start not in addresses or len(addresses)!=len(instructions):
        raise ValueError('Continuation needs a unique decoded entry')
    if any(not 0<i.address<2**32 or not 0<i.size<=15 or i.end_address>2**32 for i in instructions):
        raise ValueError('Invalid continuation instruction span')
    if lifter is not None and not isinstance(lifter,ContinuationLifter):
        raise ValueError('Continuation requires a fail-closed lifter')
    lifter=lifter or ContinuationLifter()
    lifter.func_start=start;lifter.func_end=max(i.end_address for i in instructions)
    lifter.track_status_flags=True;lifter.needs_cf=True
    lifter.publishes_ebp=True;lifter.trace_exit_name=None;lifter.force_return_value=None
    lines=['#include "guest_context_execution.h"',
           f'#define RECOMP_REGISTER_SITE {_hex(start)}','#include "recomp_types.h"',
           '#ifndef RECOMP_STATUS_FLAGS_SUPPORTED',
           '#error Continuation execution requires persistent guest status flags', '#endif',
           f'int tsfp_context_has_{start:08X}(uint32_t context_pc)', '{', '    switch(context_pc) {']
    lines += [f'    case {_hex(address)}:' for address in sorted(addresses)]
    lines += ['        return 1;','    default:return 0;','    }','}',
              '#define ebp g_ebp',
              f'int tsfp_context_step_{start:08X}(uint32_t context_pc,uint32_t *context_next)', '{',
              '    if (!context_next) return TSFP_CONTEXT_STEP_ERROR;',
              f'    RECOMP_DECLARE_GPRS({_hex(start)});',
              '    uint32_t _fa=0,_fb=0; int32_t _fas=0,_fbs=0;',
              '    double _fca=0.0,_fcb=0.0;', '    int _flags=0,_cf=(int)RECOMP_STATUS_CARRY(0);',
              '    (void)_fa;(void)_fb;(void)_fas;(void)_fbs;',
              '    (void)_fca;(void)_fcb;(void)_flags;(void)_cf;',
              *FP_STACK_MACROS,'    switch(context_pc) {']
    for instruction in sorted(instructions,key=lambda i:i.address):
        emitted=_control(instruction)
        if emitted is None:
            # Never substitute zero for an architectural selector/control cell.
            if any(op.type=='reg' and (op.reg in ('cs','ss','ds','es','fs','gs') or
                    (op.reg or '').startswith(('cr','dr','tr'))) for op in instruction.operands):
                raise ValueError('Continuation live selector/control registers are not modeled')
            emitted=[]
            if instruction.mnemonic in ('adc','sbb','rcl','rcr','cmc'):
                emitted+=_known(1)
            if instruction.mnemonic=='lahf':emitted+=_known(0xd5)
            statements,_=block(lifter,BasicBlock(instruction.address,[instruction]))
            emitted+=statements+_handoff(_hex(instruction.end_address))
        lines += [f'    case {_hex(instruction.address)}: {{', *('        '+line for line in emitted),'    }']
    lines += ['    default:return TSFP_CONTEXT_STEP_ERROR;', '    }', *FP_STACK_UNDEFS, '}', '#undef ebp','']
    return '\n'.join(lines)


def _decode(translator, start, info):
    if start in translator.force_returns:
        raise ValueError('Continuation requires a forced-return policy adapter')
    if start in translator.lifter.manual_functions:
        raise ValueError('Continuation manual entry requires an opaque dispatch adapter')
    clone=copy(translator)
    old=translator.lifter
    # Reuse immutable configuration and already discovered helper identities.
    # Constructing a full configured Lifter here would scan the entire title
    # for C++ helper signatures once per function (quadratic generation work).
    clone.lifter=ContinuationLifter()
    vars(clone.lifter).update(vars(old))
    for field in ('referenced_calls','unimplemented','_pop_cache','jump_table_targets','icall_site_targets'):
        setattr(clone.lifter,field,{})
    clone.lifter.imm_code_refs=set()
    recovered=clone._recovered_cfg.get(start)
    end=(recovered['end'] if recovered else info.get('end')) or start+info.get('size',0)
    if end<=start:return None,()
    data=clone._read_func_bytes(start,end)
    if not data:return None,()
    clone.lifter.func_start=start;clone.lifter.func_end=end
    clone.lifter.jump_table_targets=recovered['jump_tables'] if recovered else {}
    instructions,_=clone._decode_body(start,end,data,recovered)
    return clone.lifter,instructions


def translate_function(translator, start, info):
    """Reuse actual function recovery without mutating the normal lifter state."""
    lifter,instructions=_decode(translator,start,info)
    return emit(start,instructions,lifter) if instructions else None


def emit_dispatch(functions):
    """Resolve exact decoded PCs through a fixed host table, never a guest cast.

    Functions must have disjoint ownership spans. Each range's generated
    membership predicate checks instruction boundaries and holes before dispatch.
    Manual/kernel capabilities belong to the caller's separate opaque adapter.
    """
    entries=[];last=0
    for start,instructions in sorted(functions.items()):
        instructions=tuple(instructions)
        addresses=[i.address for i in instructions]
        if (not addresses or start!=min(addresses) or start<=last or
                len(set(addresses))!=len(addresses) or
                any(not 0<i.address<2**32 or not 0<i.size<=15 or i.end_address>2**32 for i in instructions)):
            raise ValueError('Continuation dispatch needs disjoint decoded ownership spans')
        last=max(i.end_address for i in instructions)-1
        entries.append((start,max(addresses)))
    if not entries:raise ValueError('Continuation dispatch needs decoded functions')
    lines=['#include "guest_context_execution.h"']
    for start,_ in entries:
        lines += [f'int tsfp_context_has_{start:08X}(uint32_t);',
                  f'int tsfp_context_step_{start:08X}(uint32_t,uint32_t *);']
    lines += ['typedef struct tsfp_context_code_entry {', '    uint32_t first,last;',
              '    int (*has)(uint32_t);', '    int (*step)(uint32_t,uint32_t *);',
              '} tsfp_context_code_entry;',
              'static const tsfp_context_code_entry tsfp_context_code[] = {']
    lines += [f'    {{{_hex(start)},{_hex(end)},tsfp_context_has_{start:08X},tsfp_context_step_{start:08X}}},' for start,end in entries]
    lines += ['};',
              'static const tsfp_context_code_entry *tsfp_context_code_find(uint32_t pc) {',
              '    size_t low=0,high=sizeof tsfp_context_code/sizeof tsfp_context_code[0];',
              '    while(low<high) {', '        size_t middle=low+(high-low)/2;',
              '        if(tsfp_context_code[middle].first<=pc)low=middle+1;',
              '        else high=middle;', '    }', '    if(!low)return NULL;',
              '    const tsfp_context_code_entry *entry=&tsfp_context_code[low-1];',
              '    return pc<=entry->last && entry->has(pc)?entry:NULL;', '}',
              'int tsfp_context_generated_admit(uint32_t pc) {',
              '    return tsfp_context_code_find(pc)!=NULL;', '}',
              'int tsfp_context_generated_step(uint32_t pc,uint32_t *next) {',
              '    const tsfp_context_code_entry *entry=tsfp_context_code_find(pc);',
              '    return entry && next?entry->step(pc,next):TSFP_CONTEXT_STEP_ERROR;', '}', '']
    return '\n'.join(lines)


def translate_functions(translator, functions):
    """Generate an opt-in owned-code bundle and its admitted-PC dispatcher."""
    decoded={};sources=[]
    for start,info in sorted(functions.items()):
        lifter,instructions=_decode(translator,start,info)
        if not instructions:raise ValueError(f'Undecodable continuation function: {start:#x}')
        sources.append(emit(start,instructions,lifter));decoded[start]=instructions
    return '\n'.join(sources+[emit_dispatch(decoded)])
