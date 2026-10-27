"""Project bit-string, scan and byte-swap lowering for the 32-bit guest.

Register and immediate indices wrap at operand width. Memory/register indices
are signed bit-string offsets. Zero scans retain the destination as an explicit
policy for the architecturally undefined result. LOCK operations belong to the
separate atomic-instruction replacement, not these non-atomic handlers.
"""
from translation_integer import _width
from translation_operands import _fmt_mem, _fmt_operand_read, _fmt_operand_write


def bit_test(context, instruction, operands, operation):
    if operation not in ('bt', 'bts', 'btr', 'btc'):
        raise ValueError('Unsupported bit operation')
    bits, mask = _width(operands, 2)
    destination, index = operands
    if bits not in (16, 32) or index.type not in ('imm', 'reg'):
        raise ValueError('Bit test requires word/dword and register/immediate index')
    lines = [f'{{ uint32_t _bitop_offset = (uint32_t)({_fmt_operand_read(index)}) & {mask};',
             f'unsigned _bitop_index = _bitop_offset & {bits-1}u;']
    if destination.type == 'mem':
        lines.append(f'uint32_t _bitop_address = (uint32_t)({_fmt_mem(destination)});')
        if index.type == 'reg':
            shift = 4 if bits == 16 else 5
            # Unsigned subtraction expresses a floor-divided signed offset
            # without implementation-defined signed casts/right shifts.
            lines += [f'uint32_t _bitop_word = (_bitop_offset >> {shift}) -',
                      f'((_bitop_offset & 0x{1 << (bits-1):X}u) ? 0x{1 << (bits-shift):X}u : 0u);',
                      f'_bitop_address += _bitop_word * {bits//8}u;']
        value = f'MEM{bits}(_bitop_address)'
    else:
        value = _fmt_operand_read(destination)
    lines.append(f'uint32_t _bitop_value = (uint32_t)({value}) & {mask};')
    if getattr(context, 'needs_cf', False):
        lines.append('_cf = (int)((_bitop_value >> _bitop_index) & 1u);')
    if operation != 'bt':
        operator = {'bts': '|', 'btr': '& ~', 'btc': '^'}[operation]
        result = f'(_bitop_value {operator} (1u << _bitop_index))'
        lines.append(f'{value} = {result};' if destination.type == 'mem' else
                     _fmt_operand_write(destination, result))
    else:
        lines.append('(void)_bitop_value;')
    return [*lines, '}']


def scan(context, instruction, operands, operation):
    if operation not in ('bsf', 'bsr'):
        raise ValueError('Unsupported bit scan')
    bits, mask = _width(operands, 2)
    if bits not in (16, 32) or operands[0].type != 'reg' or operands[1].type == 'imm':
        raise ValueError('Bit scan requires word/dword register and register/memory source')
    lines = [f'{{ uint32_t _bitop_value = (uint32_t)({_fmt_operand_read(operands[1])}) & {mask};',
             '_fa = _bitop_value; _fb = 0; _fas = (int32_t)_fa; _fbs = 0;',
             'if (_bitop_value) {', 'unsigned _bitop_index = 0;']
    if operation == 'bsf':
        lines.append('while (!(_bitop_value & 1u)) { _bitop_value >>= 1; ++_bitop_index; }')
    else:
        lines.append('while ((_bitop_value >>= 1) != 0u) ++_bitop_index;')
    return [*lines, _fmt_operand_write(operands[0], '_bitop_index'), '} }']


def swap(context, instruction, operands):
    bits, _ = _width(operands, 1)
    if bits != 32 or operands[0].type != 'reg':
        raise ValueError('BSWAP requires a dword register')
    return [f'{{ uint32_t _bitop_value = (uint32_t)({_fmt_operand_read(operands[0])});',
            _fmt_operand_write(operands[0], '((_bitop_value << 24) | ((_bitop_value & 0xFF00u) << 8) | ((_bitop_value >> 8) & 0xFF00u) | (_bitop_value >> 24))'), '}']


def condition(jump, setter):
    """Consume captured flags, including after a tested operand is overwritten."""
    if setter in ('bt', 'bts', 'btr', 'btc'):
        if jump in ('jb', 'jnae', 'jc'): return '_cf'
        if jump in ('jae', 'jnb', 'jnc'): return '!_cf'
    if setter in ('bsf', 'bsr'):
        if jump in ('je', 'jz'): return '(_fa == 0)'
        if jump in ('jne', 'jnz'): return '(_fa != 0)'
    return None
