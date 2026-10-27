"""Project lowering for data transfer, address calculation and stack moves.

Policy hooks remain explicit call sites;
arithmetic flags and instruction dispatch are outside this module.
"""
from dataclasses import replace

from translation_operands import (_fmt_mem, _fmt_operand_read, _fmt_operand_write,
                                  _operand_width, _SEGMENTS)


def _arity(operands, count):
    if len(operands) != count:
        raise ValueError(f'Expected {count} instruction operands')


def _c_string(text):
    """Encode UTF-8 bytes in a C literal without surrogate or escape ambiguity."""
    return '"' + ''.join(chr(byte) if 32 <= byte < 127 and byte not in (34,92)
                         else f'\\{byte:03o}' for byte in text.encode('utf-8')) + '"'


def _global_word(op):
    return (op.type == 'mem' and op.mem_size == 4 and not op.mem_base
            and not op.mem_index and not op.mem_seg)


def mov(context, instruction, operands):
    _arity(operands, 2)
    destination, source = operands
    value = _fmt_operand_read(source)
    site = f'0x{instruction.address:X}'
    if destination.type == 'reg' and _global_word(source):
        value = f'RECOMP_LOAD_GLOBAL_U32({site}, {_fmt_mem(source)}, {value})'
    if destination.type == 'reg' and source.type == 'mem' and source.mem_size == 1:
        value = f'RECOMP_LOAD_U8_AT({site}, {value})'
    if _global_word(destination):
        value = f'RECOMP_STORE_GLOBAL_U32({site}, {_fmt_mem(destination)}, {value})'
    lines = [_fmt_operand_write(destination, value)]
    if destination.type == source.type == 'reg' and (destination.reg, source.reg) == ('ebp', 'esp'):
        lines.extend(['g_ebp = ebp; /* expose the new frame to callees */', 'g_seh_ebp = ebp;'])
    if destination.type == 'mem' and destination.mem_size == 4 and not destination.mem_seg:
        lines.append(f'RECOMP_AFTER_STORE32({site}, {_fmt_mem(destination)});')
    return lines


def _extend(operands, signed):
    _arity(operands, 2)
    destination, source = operands
    width, target = _operand_width(source), _operand_width(destination)
    if source.type not in ('mem','reg') or destination.type != 'reg' or width not in (1,2) or target not in (2,4) or width >= target:
        raise ValueError('Extension requires a narrower byte/word source and a word/dword register')
    value = f'{"SX" if signed else "ZX"}{width*8}({_fmt_operand_read(source)})'
    return [_fmt_operand_write(destination, value)]


def movzx(context, instruction, operands):
    return _extend(operands, False)


def movsx(context, instruction, operands):
    return _extend(operands, True)


def lea(context, instruction, operands):
    _arity(operands, 2)
    destination, source = operands
    if destination.type != 'reg' or source.type != 'mem':
        raise ValueError('LEA requires a register and an address expression')
    # LEA computes an offset; segment overrides do not add a segment base.
    return [_fmt_operand_write(destination, _fmt_mem(replace(source, mem_seg=None)))]


def _stack_width(instruction, operand):
    prefixes = {0x26,0x2e,0x36,0x3e,0x64,0x65,0x66,0x67,0xf0,0xf2,0xf3}
    for value in bytes.fromhex(instruction.bytes_hex):
        if value not in prefixes:
            break
        if value == 0x66:
            return 2
    width = _operand_width(operand)
    if width not in (None,2,4):
        raise ValueError('Stack transfer must be a word or dword')
    return width or 4


def push(context, instruction, operands):
    _arity(operands, 1)
    source = operands[0]
    width, value = _stack_width(instruction, source), _fmt_operand_read(source)
    if width == 4:
        if source.type == 'imm':
            value = f'RECOMP_PUSH_IMM_U32_AT(0x{instruction.address:X}, {value})'
        return [f'PUSH32(esp, {value});']
    return [f'{{ uint16_t _pushed = (uint16_t)({value});',
            'esp -= 2; MEM16(esp) = _pushed; }']


def pop(context, instruction, operands):
    _arity(operands, 1)
    destination = operands[0]
    if destination.type not in ('reg','mem'):
        raise ValueError('POP destination must be writable')
    width = _stack_width(instruction, destination)
    trace = getattr(context, 'trace_exit_name', None)
    lines = []
    if trace and destination.type == 'reg' and destination.reg not in _SEGMENTS:
        lines.append(f'RECOMP_TRACE_ESP({_c_string(trace)}, "pop {destination.reg}");')
    if width == 4 and destination.type == 'reg' and destination.reg not in {*_SEGMENTS, 'esp'}:
        return [*lines, f'POP32(esp, {destination.reg});']
    lines += [f'{{ uint32_t _popped = MEM{width*8}(esp); esp += {width};']
    if destination.type == 'reg' and destination.reg in _SEGMENTS:
        lines.append('(void)_popped; /* segment selector remains unmodeled */ }')
    else:
        # Resolve a memory destination after updating ESP; POP ESP overwrites
        # the increment with the value just read, and POP SP preserves its high bits.
        lines.append(_fmt_operand_write(destination, '_popped') + ' }')
    return lines
