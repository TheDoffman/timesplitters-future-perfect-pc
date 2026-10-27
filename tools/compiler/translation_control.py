"""Project control-transfer emission over the shared guest/native call ABI.

Guest operands are sampled before a CALL pushes its return address. Native
functions retain their current guest-stack, frame, dispatch and diagnostic
interfaces; exception unwinding and complete cross-block EFLAGS are separate.
"""
from translation_moves import _c_string
from translation_operands import _fmt_operand_read
from translation_conditions import COND_MAP
from translation_decode import LOOP_JUMPS
from translation_hooks import stack_cleanup
import translation_layout as layout


def _hex(address):
    return f'0x{address & 0xFFFFFFFF:08X}u'


def external(context,address):
    return not context.func_start <= address < context.func_end


def _force(context):
    value=context.force_return_value
    return '' if value is None else f'if (g_force_return) eax = 0x{value & 0xFFFFFFFF:X}U; '


def _trace(context,event):
    return [] if not context.trace_exit_name else [f'RECOMP_TRACE_ESP({_c_string(context.trace_exit_name)}, {_c_string(event)});']


def _tail_body(context,address=None,expression=None):
    if expression is not None:call=f'RECOMP_ITAIL({expression});'
    elif address in context.manual_functions:call=f'RECOMP_ITAIL({_hex(address)});'
    else:call=f'{context._call_target_name(address)}();'
    return f'g_seh_ebp = ebp; {call} {_force(context)}return;'


def conditional(condition,mnemonic,description,target,context=None):
    note=f'{mnemonic}: {description}'
    if target is None:return f'if ({condition}) {{ /* {note} - indirect */ }}'
    if context is not None and external(context,target):
        suffix=', manual tail' if target in context.manual_functions else ''
        return f'if ({condition}) {{ {_tail_body(context,address=target)} }} /* {note}{suffix} */'
    return f'if ({condition}) goto loc_{target:08X}; /* {note} */'


def callee_cleanup(context,target):
    if target not in context._pop_cache:
        result=None;info=context.func_db.get(target);data=context.xbe_data
        if info is not None and data:
            end=info.get('end')
            if isinstance(end,str):
                try:end=int(end,16)
                except ValueError:end=None
            offset=layout.va_to_file_offset(target)
            # An incomplete body cannot establish consensus about its returns.
            if isinstance(end,int) and end>target and offset is not None and 0<=offset<=len(data)-(end-target):
                result=stack_cleanup(data[offset:offset+end-target],target)
        context._pop_cache[target]=result
    return context._pop_cache[target]


def call(context,instruction,operands):
    target=instruction.call_target;site=instruction.address;following=instruction.end_address
    push=f'PUSH32(esp, {_hex(following)});'
    if target is None:
        if not operands:return ['/* call: no target */']
        if len(operands)!=1:raise ValueError('Indirect CALL requires one target')
        result=[f'{{ uint32_t _icall_target = {_fmt_operand_read(operands[0])}; {push}']
        observed=context.icall_site_targets.get(site,())
        guarded=bool(observed) and len(observed)<=4 and all(a in context.func_db and a not in context.manual_functions for a in observed)
        if guarded:
            for index,address in enumerate(sorted(set(observed))):
                result.append(f'{"else " if index else ""}if (_icall_target == {_hex(address)}) {{ RECOMP_ICALL_GUARD_HIT(); RECOMP_ABI_CALL({_hex(address)}, {context._call_target_name(address)}); }}')
            result.append('else { RECOMP_ICALL_GUARD_MISS();')
        result.append(f'RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, {_hex(site)});')
        if guarded:result.append('}')
        suffix=f', guarded {len(set(observed))}' if guarded else ''
        result.append('} /* indirect call'+suffix+' */')
        # The existing stack-restore postpass consumes one complete call per
        # statement, so it must see the target capture and push with fallback.
        return [' '.join(result)]
    result=[]
    seh=target in context.SEH_PROLOGS or target in context.SEH_EPILOGS
    if seh:result.append('g_seh_ebp = ebp; /* publish frame to SEH helper */')
    if context.publishes_ebp:result+=['g_ebp = ebp; /* frame stays current across calls */','g_seh_ebp = ebp;']
    name=context._call_target_name(target)
    if target==context.SETJMP_FN:
        result += ['{ int _flow_resume = setjmp(*recomp_setjmp_slot(MEM32(esp))); '
                   'if (_flow_resume) { ebp = g_seh_ebp; eax = (uint32_t)_flow_resume; } '
                   f'else {{ {push} {name}(); }} }} /* setjmp {_hex(target)} */']
    elif target==context.LONGJMP_FN:
        result += [f'if (!recomp_guest_longjmp(MEM32(esp), MEM32(esp + 4))) {{ {push} {name}(); }} /* longjmp {_hex(target)} */']
    elif target in context.manual_functions:
        result += [f'{push} RECOMP_ICALL_SAFE({_hex(target)}, _icall_esp); /* manual call 0x{target:08X} */']
    else:
        cleanup=context.callee_pop(target);resolved=f'RECOMP_DIRECT_CALLEE_AT(0x{site:X}, {name})'
        invocation=(f'RECOMP_ABI_CALL({_hex(target)}, {resolved});' if cleanup is None else f'RECOMP_ABI_CALL_POP({_hex(target)}, {resolved}, {cleanup}u);')
        result += [f'{push} {invocation} /* call 0x{target:08X} */']
    result+=_trace(context,f'after call 0x{target:08X}')
    if seh:result.append('ebp = g_seh_ebp; /* read back frame from SEH helper */')
    return result


def return_from(context,instruction,operands):
    if len(operands)>1 or (operands and operands[0].type!='imm'):raise ValueError('RET cleanup must be an immediate')
    cleanup=0 if not operands else operands[0].imm & 0xFFFF
    code=_force(context)
    if context.trace_exit_name:code+=f'RECOMP_TRACE_EXIT({_c_string(context.trace_exit_name)}, 0x{context.func_start:08X}); '
    if context.func_start in context.SEH_PROLOGS or context.func_start in context.SEH_EPILOGS:code+='g_seh_ebp = ebp; '
    note=f'ret {cleanup}' if operands else 'ret'
    return [f'{code}esp += {4+cleanup}; return; /* {note} */']


def forced_tail(context,source):
    # Compatibility for existing callers that already assembled a terminal tail.
    return source.replace('; return;', '; '+_force(context)+'return;',1)


def read_table(context,address,max_entries=256):
    if max_entries<=0 or not context.xbe_data:return []
    data=context.xbe_data;offset=layout.va_to_file_offset(address)
    if offset is None or not 0<=offset<=len(data):return []
    def slot(position):
        if not 0<=position<=len(data)-4:return None
        value=int.from_bytes(data[position:position+4],'little')
        return value if layout.is_code_address(value) else None
    def run(position,count):
        found=[]
        for _ in range(count):
            value=slot(position)
            if value is None:break
            found.append(value);position+=4
        return found
    def displacement(position):
        # FF /4, SIB scale four, no base: disp32 is instruction data, not a slot.
        return position>=3 and data[position-3:position-1]==b'\xff\x24' and data[position-1]&0xC7==0x85
    values=run(offset,max_entries)
    if len(values)>=2:return values
    left=[]
    if not displacement(offset-4):
        for index in range(1,max_entries+1):
            position=offset-4*index;value=slot(position)
            if value is None or (value==address and displacement(position)):break
            left.append(value)
    values=list(reversed(left))+values
    if len(values)<2:
        for skipped in range(1,4):
            candidate=run(offset+4*skipped,max(0,max_entries-skipped))
            if len(candidate)>=2:return candidate
    return values


def switch_targets(context,operands):
    if not operands or operands[0].type!='mem':return []
    operand=operands[0]
    if not operand.mem_disp or not (operand.mem_index or operand.mem_base):return []
    values=context.jump_table_targets.get(operand.mem_disp)
    if values is None:values=context._read_jump_table(operand.mem_disp)
    inside=[]
    for target in values or ():
        if external(context,target):break
        inside.append(target)
    return inside if len(inside)>=2 else []


def jump(context,instruction,operands):
    target=instruction.jump_target
    if target is not None:
        if not external(context,target):return [f'goto loc_{target:08X};']
        note='manual tail jmp' if target in context.manual_functions else 'tail jmp'
        return _trace(context,f'tail 0x{target:08X}')+[f'{_tail_body(context,address=target)} /* {note} 0x{target:08X} */']
    if not operands:return ['/* jmp: no target */']
    if len(operands)!=1:raise ValueError('JMP requires one target')
    entries=context._analyze_switch_table(operands)
    if not entries and operands[0].type=='reg':entries=sorted(t for t in context.imm_code_refs if not external(context,t))
    value=_fmt_operand_read(operands[0])
    if entries:
        note='intra-function indirect jmp' if operands[0].type=='reg' else f'switch: {len(entries)} entries'
        result=[f'{{ uint32_t _jt = {value}; /* {note} */']
        result += [f'if (_jt == {_hex(t)}) goto loc_{t:08X};' for t in sorted(set(entries))]
        result += [_tail_body(context,expression='_jt')+' }']
        return result
    return [_tail_body(context,expression=value)+' /* indirect tail jmp */']


def loop(context,instruction,zf_expr=None):
    name=instruction.mnemonic
    raw=bytes.fromhex(getattr(instruction,'bytes_hex','') or '')
    short=bool(raw) and 0x67 in raw[:-2]
    count='LO16(ecx)' if short else 'ecx'
    decrement='SET_LO16(ecx, (uint16_t)(ecx - 1u));' if short else 'ecx -= 1;'
    result=[decrement+f' /* {name}: count down */']
    condition=f'{count} != 0'
    if name!='loop':condition=f'({condition}) && ({zf_expr or "_flags"})'
    description='count remains nonzero' if name=='loop' else 'count and captured zero flag'
    if instruction.jump_target is None:result.append(f'/* {name}: {description} - no target */')
    else:result.append(conditional(condition,name,description,instruction.jump_target,context))
    return result


def branch(context,instruction):
    name=instruction.mnemonic;target=instruction.jump_target
    if name in LOOP_JUMPS:return loop(context,instruction)
    if name in ('jecxz','jcxz'):
        condition='ecx == 0' if name=='jecxz' else 'LO16(ecx) == 0'
        description='counter is zero'
    else:
        # The existing join probe recognizes this marker before inserting
        # predecessor zero-flag snapshots. Preserve it until that pass is replaced.
        condition='_flags /* fallback flag consumer */'
        if context.needs_cf and name in ('jb','jnae','jc'):condition='_cf'
        if context.needs_cf and name in ('jae','jnb','jnc'):condition='!_cf'
        description=COND_MAP.get(name,(None,None,name))[2]
    return [conditional(condition,name,description,target,context)]
