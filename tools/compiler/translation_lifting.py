"""Project instruction routing and block-local condition-state propagation.

The tuple exchanged with the function translator describes captured operands,
not a complete EFLAGS register. Unknown instructions invalidate it; existing
call, SAHF, floating-status and partial-flag policies remain compatibility ABI.
This module has no imports from the bundled toolkit.
"""
import translation_bits as bits
import translation_compare as compare
import translation_conditions as conditions
import translation_control as control
import translation_mmx as mmx
import translation_sse as sse
import translation_strings as strings
import translation_x87 as x87
from translation_decode import LOOP_JUMPS
from translation_moves import _c_string
from translation_operands import _fmt_operand_read as read, _fmt_operand_write as write, _operand_width


# The value is a context method and any additional operation parameter.
_ROUTES = {}
for _method, _names in {
    'mov':'mov', 'movzx':'movzx', 'movsx':'movsx', 'lea':'lea',
    'xchg':'xchg lock_xchg', 'push':'push', 'pop':'pop', 'neg':'neg',
    'not':'not', 'imul':'imul', 'sbb':'sbb', 'adc':'adc', 'sar':'sar',
    'cmp':'cmp', 'test':'test', 'call':'call', 'ret':'ret retn retf',
    'jmp':'jmp', 'swap':'bswap', 'shld':'shld', 'shrd':'shrd',
    'exchange':'xadd lock_xadd cmpxchg lock_cmpxchg',
}.items():
    for _name in _names.split(): _ROUTES[_name.replace('_',' ')] = ('_lift_'+_method, ())
for _method, _names in {
    'alu_binop':'add sub and or xor', 'inc_dec':'inc dec', 'muldiv':'mul div idiv',
    'rotate':'rol ror', 'rotate_carry':'rcl rcr', 'bit_test':'bt bts btr btc',
    'scan':'bsf bsr',
}.items():
    for _name in _names.split(): _ROUTES[_name] = ('_lift_'+_method, (_name,))
for _name, _operator in (('shl','<<'),('sal','<<'),('shr','>>')):
    _ROUTES[_name] = ('_lift_shift', (_operator,))
_STRING_NAMES = frozenset(stem+width for stem in ('movs','stos','lods','cmps','scas') for width in 'bwd')
_FLOAT_FLAGS = frozenset(stem+width for stem in ('comi','ucomi') for width in ('ss','sd'))
_X87_FLAGS = frozenset('fcomi fcomip fcompi fucomi fucomip fucompi'.split())
_SETTERS = frozenset('cmp test add sub adc sbb and or xor inc dec neg shl shr sar imul mul shld shrd rol ror rcl rcr bsf bsr bt bts btr btc cmpxchg xadd'.split()) | _FLOAT_FLAGS
_INVALIDATORS = frozenset('div idiv rdtsc cpuid popfd'.split())
_PRESERVE = frozenset('mov lea push pop nop leave ret movzx movsx xchg bswap cdq cwde cbw cwd lahf not call int3 int wait cld std cli sti pushfd pushal sgdt ljmp sfence'.split()) | LOOP_JUMPS | (_STRING_NAMES - {s for s in _STRING_NAMES if s.startswith(('cmps','scas'))})
# Preserve the established policy even for unsupported operand forms in these
# register families. Supported handlers provide their own precise predicates.
_SIMD_PRESERVE = (sse.MNEMONICS - _FLOAT_FLAGS) | mmx.MNEMONICS | frozenset('paddq pmulhuw pshufd pshufhw pshuflw punpcklqdq punpckhqdq'.split())


def has_xmm(operands):
    return any(getattr(op,'type',None)=='reg' and (getattr(op,'reg',None) or '').startswith('xmm') for op in operands or ())


def repeated_compare(instruction):
    return instruction.mnemonic.startswith('rep') and any(
        name in instruction.mnemonic+' '+(getattr(instruction,'op_str','') or '')
        for name in _STRING_NAMES if name.startswith(('cmps','scas')))


def instruction(context, insn):
    """Route decoded operands, resolving overloaded string/SIMD names first."""
    name, operands = insn.mnemonic, insn.operands
    if name=='nop': return ['/* nop */']
    if name=='lea' and len(operands)==2:
        dst,src=operands
        if dst.type=='reg' and src.type=='mem' and src.mem_base==dst.reg and not src.mem_index and not src.mem_disp:
            return ['/* nop */']
    route = _ROUTES.get(name)
    if route:
        method, extra = route
        return getattr(context,method)(insn,operands,*extra)
    if insn.is_cond_jump: return context._lift_jcc(insn)
    if name.startswith(('rep ','repe ','repne ','repz ','repnz ')):
        return context._lift_rep_string(insn,name)
    if name in _STRING_NAMES and not has_xmm(operands): return context._lift_string_op(insn,name)
    if name in ('cbw','cwd','cwde','cdq'): return compare.extend(context,insn,operands)
    if name in ('cld','std'): return strings.direction(context,insn)
    if name=='int' and operands and operands[0].type=='imm' and operands[0].imm==45:
        return ['recomp_debug_service(eax, ecx); /* int 0x2d */']
    if name=='int3': return [f'recomp_int3_reached(0x{insn.address:08X}U); /* int3, stepped over */']
    if name=='leave': return ['esp = ebp;', 'POP32(esp, ebp); /* leave */']
    fixed = {'wait':'/* wait - FPU sync */',
             'lahf':'/* lahf - load AH from flags (used in FPU compare idiom) */',
             'sahf':'/* sahf - store AH to flags */',
             'emms':'(void)0; /* emms - empty MMX state */'}
    if name in fixed: return [fixed[name]]
    if name in ('xlat','xlatb'):
        address = '(uint16_t)(LO16(ebx) + LO8(eax))' if 0x67 in bytes.fromhex(insn.bytes_hex)[:-1] else 'ebx + LO8(eax)'
        return [f'SET_LO8(eax, MEM8({address})); /* xlatb */']
    for prefix,method in (('set','_lift_setcc'),('cmov','_lift_cmovcc')):
        if name.startswith(prefix) and 'j'+name[len(prefix):] in conditions.COND_MAP:
            return getattr(context,method)(insn,operands,name)
    if any(op.type=='reg' and (op.reg or '').startswith('mm') for op in operands) or name in ('femms','cvtpi2ps'):
        return context._lift_mmx(insn,name,operands)
    if name in sse.MNEMONICS:
        return context._lift_sse(insn,'movaps' if name in ('movntps','movntpd','movntdq') else name,operands)
    if name.startswith('f'): return context._lift_fpu(insn,name,operands)
    if name.startswith('prefetch') or name in ('sfence','lfence','mfence'):
        return [f'(void)0; /* {name}: cache/ordering hint, nothing to model */']
    if name in ('stc','clc','cmc'):
        if not context.needs_cf: return [f'/* {name}: no adc/sbb in this function consumes CF */']
        return [f"_cf = {dict(stc='1',clc='0',cmc='!_cf')[name]}; /* {name} */"]
    if name=='rdtsc':
        return ['{ uint64_t _tsc = xbox_ReadTimeStampCounter();',
                '  eax = (uint32_t)_tsc; edx = (uint32_t)(_tsc >> 32); }  /* rdtsc */']
    return context._unimplemented(insn,name)


def result_snapshot(context, operands, operation, src_too=False):
    width = _operand_width(operands[0])
    width = width if width in (1,2,4) else 4
    mask = {1:'0xFFu',2:'0xFFFFu',4:'0xFFFFFFFFu'}[width]
    variable, operand, label = ('_fb',operands[1],operation+' source, before the write') if src_too else ('_fa',operands[0],operation+' result')
    return f'{variable} = (uint32_t)({read(operand)}) & {mask}; {variable}s = (int32_t)(int{width*8}_t)({variable}); /* {label} */'


def unsupported(context, insn, mnemonic, note=''):
    context.unimplemented.setdefault(mnemonic,[]).append(insn.address)
    description=(mnemonic+' '+insn.op_str).strip()
    # Keep diagnostics valid C even for arbitrary decoded text or annotations.
    comment=(description+note).replace('*/','* /').replace('\n',' ').replace('\r',' ')
    return [f'RECOMP_UNIMPL({_c_string(description)}, 0x{insn.address:08X}u); /* TODO: {comment} */']


def fallback_set(context, insn, operands, mnemonic):
    return [write(operands[0],f'_flags /* {mnemonic} */')] if operands else [f'/* {mnemonic}: no operand */']


def fallback_move(context, insn, operands, mnemonic):
    if len(operands)<2: return [f'/* {mnemonic}: bad operands */']
    return [f'if (_flags /* {mnemonic} */) '+write(operands[0],read(operands[1]))]


def adjacent_condition(instructions, index, lifter=None):
    if not 0<=index<len(instructions)-1: return None
    producer,consumer=instructions[index:index+2]
    if producer.mnemonic not in ('cmp','test') or len(producer.operands)<2 or not consumer.is_cond_jump: return None
    state=compare.normalise_zero_test(producer.mnemonic,producer.operands)
    condition=conditions.make_condition(consumer.mnemonic,*state)
    if condition is None: return None
    expression,description=condition
    return control.conditional(expression,consumer.mnemonic,description,consumer.jump_target,lifter),2


def preserves(insn):
    return (insn.mnemonic in _PRESERVE or insn.mnemonic in _SIMD_PRESERVE
            or insn.mnemonic.startswith('prefetch') or sse.preserves_flags(insn)
            or mmx.preserves_flags(insn) or x87.preserves_flags(insn))


def successor_state(previous, insn):
    """Advance the captured-condition descriptor after an ordinary instruction."""
    name=insn.mnemonic.removeprefix('lock ')
    if bits.constant_noop(insn): return previous
    if strings.is_compare(insn) and not insn.mnemonic.startswith('rep'):
        return 'cmp',list(insn.operands)
    if name in _SETTERS: return compare.normalise_zero_test(name,list(insn.operands))
    if name in _INVALIDATORS: return None
    if preserves(insn): return previous
    if name in _X87_FLAGS or name=='sahf': return name,list(insn.operands)
    if name.startswith(('f','cmov','j','set')): return previous
    if name.startswith('rep'):
        return (name,list(insn.operands)) if repeated_compare(insn) else previous
    return None


def _consumer(context, insn, state):
    """Return None when a consumer needs the ordinary fallback emitter."""
    name=insn.mnemonic
    if name in ('jecxz','jcxz'): return context._lift_jcc(insn)
    if name in LOOP_JUMPS:
        condition=conditions.make_condition('je' if name in ('loope','loopz') else 'jne',*state) if state and name!='loop' else None
        return context._lift_loop(insn,condition[0] if condition else None)
    if not state: return None
    if insn.is_cond_jump:
        condition=conditions.make_condition(name,*state)
        if condition:
            expression,description=condition
            if state[0] in _FLOAT_FLAGS: expression=f'RECOMP_FLOAT_BRANCH_AT(0x{insn.address:X}, ({expression}))'
            return [control.conditional(expression,name,description,insn.jump_target,context)]
    if name=='lahf':
        value=conditions.lahf(*state)
        return [f'SET_HI8(eax, {value}); /* lahf */'] if value else None
    if name.startswith('set') and insn.operands:
        value=conditions.setcc(name,*state)
        if value is not None: return [write(insn.operands[0],f'({value}) ? 1 : 0')+f' /* {name} */']
    if name.startswith('cmov') and len(insn.operands)>=2:
        value=conditions.cmovcc(name,*state)
        if value is not None: return [f'if ({value}) '+write(insn.operands[0],read(insn.operands[1]))+f' /* {name} */']
    return None


def block(context, basic_block, flag_state=None):
    output=[]; state=flag_state; instructions=basic_block.instructions; index=0
    while index<len(instructions):
        insn=instructions[index]
        pair=adjacent_condition(instructions,index,context)
        if pair:
            state=compare.normalise_zero_test(insn.mnemonic,list(insn.operands))
            output.extend(context._snapshot_flags(insn,state[1],state[0]))
            output.append(pair[0]); index+=pair[1]; continue
        consumed=_consumer(context,insn,state)
        if consumed is not None:
            output.extend(consumed); index+=1; continue
        if insn.mnemonic=='neg':
            following=index+1
            while following<len(instructions):
                candidate=instructions[following]
                if candidate.is_branch or candidate.is_call or candidate.is_ret or not preserves(candidate): break
                following+=1
            carry=following<len(instructions) and instructions[following].mnemonic in ('adc','sbb')
            emitted=context._lift_neg(insn,insn.operands,preserve_carry=carry)
        else:
            emitted=context.lift_instruction(insn)
            if state and repeated_compare(insn):
                condition=conditions.make_condition('je',*state)
                if condition: output.append(f'_flags = ({condition[0]}) ? 1 : 0; /* ZF in: a zero count keeps it */')
        output.extend(emitted)
        state=successor_state(state,insn)
        index+=1
    return output,state
