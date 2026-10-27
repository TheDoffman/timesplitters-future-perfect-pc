"""Original comparison snapshots and implicit accumulator sign extension.

Snapshots retain the existing branch-consumer ABI. General condition lowering,
full EFLAGS/AF and guest memory-fault delivery are separate components.
"""
from translation_decode import Operand
from translation_integer import _width
from translation_operands import _fmt_operand_read, _fmt_mem


def normalise_zero_test(mnemonic, operands):
    if mnemonic == 'test' and len(operands) == 2:
        a, b = operands
        if a.type == b.type == 'reg' and a.reg and a.reg == b.reg:
            _width(operands, 2)
            return 'cmp', [a, Operand('imm', imm=0, mem_size=a.mem_size)]
    return mnemonic, operands


def snapshot(context, instruction, operands, kind):
    if kind not in ('cmp', 'test'):
        raise ValueError('Comparison must be CMP or TEST')
    bits, mask = _width(operands, 2)
    expressions = []
    for operand in operands:
        value = _fmt_operand_read(operand)
        if (kind == 'cmp' and operand.type == 'mem' and bits == 32
                and not any((operand.mem_base, operand.mem_index, operand.mem_seg))):
            value = f'RECOMP_LOAD_GLOBAL_U32(0x{instruction.address:X}, {_fmt_mem(operand)}, {value})'
        expressions.append(value)
    result = [f'_fa = (uint32_t)({expressions[0]}) & {mask};',
              f'_fb = (uint32_t)({expressions[1]}) & {mask};',
              f'_fas = (int32_t)(int{bits}_t)_fa;',
              f'_fbs = (int32_t)(int{bits}_t)_fb; /* own {kind} snapshot */']
    if getattr(context, 'needs_cf', False):
        result.append('_cf = (int)(_fa < _fb);' if kind == 'cmp' and not
                      (operands[1].type == 'imm' and not operands[1].imm) else '_cf = 0;')
    return result


def cmp(context, instruction, operands):
    return snapshot(context, instruction, operands, 'cmp')


def test(context, instruction, operands):
    kind, operands = normalise_zero_test('test', operands)
    return snapshot(context, instruction, operands, kind)


def extend(context, instruction, operands, operation=None):
    operation = operation or instruction.mnemonic
    if operands or operation not in ('cbw', 'cwd', 'cwde', 'cdq'):
        raise ValueError('Implicit sign extension has no explicit operands')
    # Unsigned masks avoid signed shifts and out-of-range signed conversions.
    return {'cbw': ['eax = (eax & 0xFFFF0000u) | (eax & 0xFFu) | ((0u - ((eax >> 7) & 1u)) & 0xFF00u);'],
            'cwd': ['edx = (edx & 0xFFFF0000u) | ((0u - ((eax >> 15) & 1u)) & 0xFFFFu);'],
            'cwde': ['eax = (eax & 0xFFFFu) | ((0u - ((eax >> 15) & 1u)) & 0xFFFF0000u);'],
            'cdq': ['edx = 0u - (eax >> 31);']}[operation]
