"""Project integer arithmetic, logic and carry lowering.

Inputs are sampled once at operand width. Results feed the existing translator
snapshot interface; conditional-branch lowering remains a separate component.
"""
from translation_operands import (_operand_width, _fmt_operand_read,
                                  _fmt_operand_write, _GENERAL, _PARTS)

_INTEGER_REGISTERS = frozenset((*_GENERAL, *_PARTS))


def _width(operands, count):
    if len(operands) != count or operands[0].type not in ('reg','mem'):
        raise ValueError('Invalid integer ALU operands')
    for operand in operands:
        if operand.type not in ('reg','mem','imm'):
            raise ValueError('Invalid integer ALU operand kind')
        if operand.type == 'reg' and (operand.reg or '').lower() not in _INTEGER_REGISTERS:
            raise ValueError('Integer ALU requires general registers')
    if count == 2 and operands[0].type == operands[1].type == 'mem':
        raise ValueError('Integer ALU cannot use two memory operands')
    width = _operand_width(operands[0])
    if width not in (1,2,4):
        raise ValueError('Integer ALU destination must be 8, 16 or 32 bits')
    if count == 2 and operands[1].type != 'imm' and _operand_width(operands[1]) != width:
        raise ValueError('Integer source and destination widths must agree')
    return width*8, f'0x{(1<<(width*8))-1:X}u'


def _snapshot(bits):
    return ['_fa = _alu_result;', f'_fas = (int32_t)(int{bits}_t)_fa;']


def _binary(context, instruction, operands, operation):
    bits, mask = _width(operands, 2)
    left, right = map(_fmt_operand_read, operands)
    lines = [f'{{ uint32_t _alu_left = (uint32_t)({left}) & {mask};',
             f'uint32_t _alu_right = (uint32_t)({right}) & {mask};']
    if operation in ('adc','sbb'):
        lines.append('uint32_t _alu_carry = (_cf != 0);')
    if operation in ('add','adc','sub','sbb'):
        operator = '+' if operation in ('add','adc') else '-'
        extra = f' {operator} _alu_carry' if operation in ('adc','sbb') else ''
        lines.append(f'uint64_t _alu_wide = (uint64_t)_alu_left {operator} _alu_right{extra};')
        lines.append(f'uint32_t _alu_result = (uint32_t)_alu_wide & {mask};')
        if operation in ('add','sub'):
            lines += ['_fb = _alu_right;', f'_fbs = (int32_t)(int{bits}_t)_fb;']
        elif operation in ('adc','sbb'):
            # Preserve both inputs for condition reconstruction after clobbers.
            lines += ['_fb = _alu_right;', f'_fbs = (int32_t)(int{bits}_t)_alu_left;']
        if getattr(context, 'needs_cf', False) or operation in ('adc','sbb'):
            if operator == '+':
                lines.append(f'_cf = (int)((_alu_wide >> {bits}) & 1u);')
            else:
                extra = ' + _alu_carry' if operation == 'sbb' else ''
                lines.append(f'_cf = (int)((uint64_t)_alu_left < (uint64_t)_alu_right{extra});')
    else:
        operator = {'and':'&','or':'|','xor':'^'}[operation]
        lines.append(f'uint32_t _alu_result = (_alu_left {operator} _alu_right) & {mask};')
        if getattr(context, 'needs_cf', False):
            lines.append('_cf = 0;')
    lines.append(_fmt_operand_write(operands[0], '_alu_result'))
    return [*lines, *_snapshot(bits), '}']


def alu_binop(context, instruction, operands, mnemonic):
    if mnemonic not in ('add','sub','and','or','xor'):
        raise ValueError('Unsupported binary integer operation')
    return _binary(context, instruction, operands, mnemonic)


def adc(context, instruction, operands):
    return _binary(context, instruction, operands, 'adc')


def sbb(context, instruction, operands):
    return _binary(context, instruction, operands, 'sbb')


def inc_dec(context, instruction, operands, mnemonic):
    bits, mask = _width(operands, 1)
    if mnemonic not in ('inc','dec'):
        raise ValueError('Unsupported increment/decrement operation')
    value = _fmt_operand_read(operands[0])
    if mnemonic == 'dec' and operands[0].type == 'mem' and bits == 8:
        value = f'RECOMP_LOAD_U8_AT(0x{instruction.address:X}, {value})'
    operation = '+' if mnemonic == 'inc' else '-'
    overflow = (1<<(bits-1)) - (mnemonic == 'dec')
    return [f'{{ uint32_t _alu_result = ((uint32_t)({value}) {operation} 1u) & {mask};',
            _fmt_operand_write(operands[0], '_alu_result'), *_snapshot(bits),
            f'_fb = (_alu_result == 0x{overflow:X}u);', '}']


def neg(context, instruction, operands, preserve_carry=False):
    bits, mask = _width(operands, 1)
    value = _fmt_operand_read(operands[0])
    lines = [f'{{ uint32_t _alu_left = (uint32_t)({value}) & {mask};',
             f'uint32_t _alu_result = (0u - _alu_left) & {mask};']
    if preserve_carry or getattr(context, 'needs_cf', False):
        lines.append('_cf = (_alu_left != 0);')
    return [*lines, _fmt_operand_write(operands[0], '_alu_result'), *_snapshot(bits), '}']


def bit_not(context, instruction, operands):
    _width(operands, 1)
    return [_fmt_operand_write(operands[0], f'~({_fmt_operand_read(operands[0])})')]
