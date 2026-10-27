"""Project width-aware shift and rotate lowering against the current flag ABI.

Undefined narrow double-shift counts leave state unchanged. Complete dynamic
flag provenance and overflow-condition lowering are separate integration work.
"""
from translation_integer import _width
from translation_operands import _fmt_operand_read, _fmt_operand_write

_OPERATIONS = frozenset(('shl','sal','shr','sar','shld','shrd','rol','ror','rcl','rcr'))


def constant_noop(instruction):
    """Known zero counts retain the predecessor's flag interpretation."""
    if instruction.mnemonic not in _OPERATIONS:
        return False
    operands = instruction.operands
    if not operands or operands[-1].type != 'imm':
        return False
    count = operands[-1].imm & 31
    if instruction.mnemonic in ('rcl','rcr','shld','shrd'):
        width, _ = _width(operands[:1], 1)
        if instruction.mnemonic in ('rcl','rcr'):
            count %= width + 1
        elif count > width:
            return True  # Explicit no-change policy for undefined narrow counts.
    return count == 0


def _count(operand):
    if operand.type != 'imm' and not (operand.type == 'reg' and operand.reg == 'cl'):
        raise ValueError('Shift/rotate count must be immediate or CL')
    return _fmt_operand_read(operand)


def _inputs(operands, double=False):
    if len(operands) != (3 if double else 2):
        raise ValueError('Invalid shift/rotate operand count')
    bits, mask = _width(operands[:2] if double else operands[:1], 2 if double else 1)
    if double and (bits not in (16,32) or operands[1].type != 'reg'):
        raise ValueError('Double shift requires a word/dword register source')
    return bits, mask, _count(operands[-1])


def _result(operand, bits):
    return [_fmt_operand_write(operand, '_bit_result'), '_fa = _bit_result;',
            f'_fas = (int32_t)(int{bits}_t)_fa;']


def _shift(context, instruction, operands, operation):
    bits, mask, count = _inputs(operands)
    value = _fmt_operand_read(operands[0])
    lines = [f'{{ uint32_t _bit_value = (uint32_t)({value}) & {mask};',
             f'unsigned _bit_count = (unsigned)({count}) & 31u;',
             'if (_bit_count) {']
    if operation == 'sar':
        lines += [f'uint32_t _bit_sign = (_bit_value >> {bits-1}) & 1u;',
                  f'uint32_t _bit_result = _bit_count >= {bits}u ? (_bit_sign ? {mask} : 0u)',
                  f': (_bit_value >> _bit_count) | (_bit_sign ? ({mask} ^ ({mask} >> _bit_count)) : 0u);']
        carry = f'(_bit_count >= {bits}u ? _bit_sign : ((_bit_value >> (_bit_count - 1u)) & 1u))'
    else:
        operator = '<<' if operation == 'shl' else '>>'
        lines.append(f'uint32_t _bit_result = (_bit_value {operator} _bit_count) & {mask};')
        distance = f'{bits}u - _bit_count' if operation == 'shl' else '_bit_count - 1u'
        carry = f'(_bit_count <= {bits}u ? ((_bit_value >> ({distance})) & 1u) : 0u)'
    if getattr(context, 'needs_cf', False):
        lines.append(f'_cf = (int){carry};')
    return [*lines, *_result(operands[0], bits), '} }']


def shift(context, instruction, operands, operator):
    if operator not in ('<<','>>'):
        raise ValueError('Invalid shift operator')
    return _shift(context, instruction, operands, 'shl' if operator == '<<' else 'shr')


def sar(context, instruction, operands):
    return _shift(context, instruction, operands, 'sar')


def double_shift(context, instruction, operands, mnemonic):
    if mnemonic not in ('shld','shrd'):
        raise ValueError('Invalid double shift')
    bits, mask, count = _inputs(operands, True)
    left, right = map(_fmt_operand_read, operands[:2])
    high, low = ('_bit_value','_bit_source') if mnemonic == 'shld' else ('_bit_source','_bit_value')
    expression = f'((_bit_pair << _bit_count) >> {bits})' if mnemonic == 'shld' else '(_bit_pair >> _bit_count)'
    distance = f'{bits}u - _bit_count' if mnemonic == 'shld' else '_bit_count - 1u'
    lines = [f'{{ unsigned _bit_count = (unsigned)({count}) & 31u;',
             f'if (_bit_count && _bit_count <= {bits}u) {{',
             f'uint32_t _bit_value = (uint32_t)({left}) & {mask};',
             f'uint32_t _bit_source = (uint32_t)({right}) & {mask};',
             f'uint64_t _bit_pair = ((uint64_t){high} << {bits}) | {low};',
             f'uint32_t _bit_result = (uint32_t){expression} & {mask};']
    if getattr(context, 'needs_cf', False):
        lines.append(f'_cf = (int)((_bit_value >> ({distance})) & 1u);')
    return [*lines, *_result(operands[0],bits), '} }']


def shld(context, instruction, operands):
    return double_shift(context, instruction, operands, 'shld')


def shrd(context, instruction, operands):
    return double_shift(context, instruction, operands, 'shrd')


def rotate(context, instruction, operands, mnemonic):
    if mnemonic not in ('rol','ror'):
        raise ValueError('Invalid rotate')
    bits, mask, count = _inputs(operands)
    value = _fmt_operand_read(operands[0])
    first, second = ('<<','>>') if mnemonic == 'rol' else ('>>','<<')
    lines = [f'{{ uint32_t _bit_value = (uint32_t)({value}) & {mask};',
             f'unsigned _bit_count = (unsigned)({count}) & 31u;',
             f'unsigned _bit_distance = _bit_count % {bits}u;',
             'if (_bit_count) {',
             'uint32_t _bit_result = _bit_value;',
             f'if (_bit_distance) _bit_result = ((_bit_value {first} _bit_distance)',
             f'| (_bit_value {second} ({bits}u - _bit_distance))) & {mask};']
    if getattr(context, 'needs_cf', False):
        bit = '0' if mnemonic == 'rol' else str(bits-1)
        lines.append(f'_cf = (int)((_bit_result >> {bit}) & 1u);')
    return [*lines, _fmt_operand_write(operands[0], '_bit_result'), '} }']


def rotate_carry(context, instruction, operands, mnemonic):
    if mnemonic not in ('rcl','rcr'):
        raise ValueError('Invalid carry rotate')
    bits, mask, count = _inputs(operands)
    value = _fmt_operand_read(operands[0])
    first, second = ('<<','>>') if mnemonic == 'rcl' else ('>>','<<')
    ring_mask = f'0x{(1<<(bits+1))-1:X}ull'
    return [f'{{ unsigned _bit_count = ((unsigned)({count}) & 31u) % {bits+1}u;',
            'if (_bit_count) {',
            f'uint64_t _bit_ring = ((uint64_t)(_cf != 0) << {bits}) | ((uint32_t)({value}) & {mask});',
            f'_bit_ring = ((_bit_ring {first} _bit_count) | (_bit_ring {second} ({bits+1}u - _bit_count))) & {ring_mask};',
            f'uint32_t _bit_result = (uint32_t)_bit_ring & {mask};',
            f'_cf = (int)((_bit_ring >> {bits}) & 1u);',
            _fmt_operand_write(operands[0], '_bit_result'), '} }']
