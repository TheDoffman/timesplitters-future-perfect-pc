"""Original exchange lowering and captured arithmetic conditions.

Memory forms use sequentially consistent host atomics, including implicit-lock
XCHG. Unlocked memory XADD/CMPXCHG retain the existing stronger atomic policy.
Guest fault delivery, MMIO and other LOCK instruction families remain separate.
"""
from translation_integer import _width
from translation_operands import _fmt_mem, _fmt_operand_read, _fmt_operand_write


def exchange(context, instruction, operands, operation=None):
    name = operation or instruction.mnemonic
    locked = name.startswith('lock ')
    name = name.removeprefix('lock ')
    if name not in ('xchg', 'xadd', 'cmpxchg'):
        raise ValueError('Unsupported exchange operation')
    bits, mask = _width(operands, 2)
    if any(op.type == 'imm' for op in operands):
        raise ValueError('Exchange requires registers or memory')
    destination, source = operands
    if name == 'xchg' and source.type == 'mem':
        destination, source = source, destination
    if source.type != 'reg' or (locked and destination.type != 'mem'):
        raise ValueError('LOCK requires a memory destination and register source')
    lines = [f'{{ uint32_t _exchange_source = (uint32_t)({_fmt_operand_read(source)}) & {mask};']
    if name == 'cmpxchg':
        lines.append(f'uint32_t _exchange_compare = eax & {mask};')
    if destination.type == 'mem':
        lines.append(f'uint32_t _exchange_address = (uint32_t)({_fmt_mem(destination)});')
        primitive = {'xchg':'exchange', 'xadd':'add', 'cmpxchg':'cas'}[name]
        arguments = ('_exchange_compare, ' if name == 'cmpxchg' else '') + '_exchange_source'
        lines.append(f'uint32_t _exchange_old = tsfp_atomic_{primitive}{bits}((void *)XBOX_PTR(_exchange_address), {arguments});')
    else:
        lines.append(f'uint32_t _exchange_old = (uint32_t)({_fmt_operand_read(destination)}) & {mask};')
    if name == 'xchg':
        if destination.type == 'reg':
            lines.append(_fmt_operand_write(destination, '_exchange_source'))
        return [*lines, _fmt_operand_write(source, '_exchange_old'), '}']
    left = '_exchange_compare' if name == 'cmpxchg' else '_exchange_old'
    right = '_exchange_old' if name == 'cmpxchg' else '_exchange_source'
    symbol = '-' if name == 'cmpxchg' else '+'
    lines += [f'uint32_t _exchange_result = ({left} {symbol} {right}) & {mask};',
              '_fa = _exchange_result;', f'_fb = {right};',
              f'_fas = (int32_t)(int{bits}_t){left};',
              f'_fbs = (int32_t)(int{bits}_t){right};']
    if getattr(context, 'needs_cf', False):
        cf = (f'{left} < {right}' if name == 'cmpxchg' else
              f'(((uint64_t){left} + {right}) >> {bits}) != 0')
        lines.append(f'_cf = ({cf});')
    if name == 'xadd':
        # Source must be written first: XADD r,r leaves the sum in that register.
        lines.append(_fmt_operand_write(source, '_exchange_old'))
        if destination.type == 'reg':
            lines.append(_fmt_operand_write(destination, '_exchange_result'))
    else:
        lines.append('if (_exchange_compare != _exchange_old) {')
        accumulator = {'8':'al','16':'ax','32':'eax'}[str(bits)]
        from types import SimpleNamespace
        lines.append(_fmt_operand_write(SimpleNamespace(type='reg',reg=accumulator), '_exchange_old'))
        if destination.type == 'reg':
            lines += ['} else {', _fmt_operand_write(destination, '_exchange_source')]
        lines.append('}')
    return [*lines, '}']


def condition(jump, setter, operands):
    if setter not in ('xadd','cmpxchg'):
        return None
    bits, _ = _width(operands, 2)
    z = '!_fa'
    s = f'((_fa & 0x{1 << (bits-1):X}u) != 0)'
    relation = '!=' if setter == 'cmpxchg' else '=='
    o = f'(((_fas < 0) {relation} (_fbs < 0)) && ((_fas < 0) != {s}))'
    p = '((0x9669u >> ((_fa ^ (_fa >> 4)) & 15u)) & 1u)'
    groups = [('e z', z), ('ne nz', f'!{z}'), ('b c nae', '_cf'),
              ('ae nb nc', '!_cf'), ('be na', f'(_cf || {z})'),
              ('a nbe', f'(!_cf && !{z})'), ('s', s), ('ns', f'!{s}'),
              ('o', o), ('no', f'!{o}'), ('p pe', p), ('np po', f'!{p}'),
              ('l nge', f'({s} != {o})'), ('ge nl', f'({s} == {o})'),
              ('le ng', f'({z} || ({s} != {o}))'),
              ('g nle', f'(!{z} && ({s} == {o}))')]
    return next((value for aliases,value in groups if jump.removeprefix('j') in aliases.split()), None)
