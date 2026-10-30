"""Project string-instruction lowering for the existing mapped-memory ABI.

Each slow-path iteration completes one element before moving the indices.
Comparisons publish ZF/CF for repeat consumers; the surrounding translator
retains responsibility for incoming flags when the count is zero. Complete
EFLAGS, segmented protection and resumable guest faults remain runtime work.
"""

_BASES = frozenset(stem + suffix for stem in ('movs', 'stos', 'lods', 'cmps', 'scas')
                   for suffix in 'bwd')
_REPEAT = frozenset(('rep', 'repe', 'repz', 'repne', 'repnz'))


def operation(instruction, mnemonic=None):
    words = (mnemonic or instruction.mnemonic).split()
    if len(words) == 1 and words[0] in _BASES:
        prefix, base = '', words[0]
    elif len(words) == 2 and words[0] in _REPEAT and words[1] in _BASES:
        prefix, base = words
    else:
        raise ValueError('Expected a supported string instruction')
    if any((getattr(op, 'reg', None) or '').startswith(('xmm', 'mm'))
           for op in getattr(instruction, 'operands', ())):
        raise ValueError('SIMD instructions are not string operations')
    return prefix, base


def is_compare(instruction):
    try:
        return operation(instruction)[1][:4] in ('cmps', 'scas')
    except ValueError:
        return False


def direction(context, instruction, operands=None):
    if instruction.mnemonic not in ('cld', 'std'):
        raise ValueError('Expected CLD or STD')
    return [f'g_df = {int(instruction.mnemonic == "std")}; /* {instruction.mnemonic} */']


def emit(context, instruction, mnemonic=None):
    prefix, base = operation(instruction, mnemonic)
    stem, size = base[:4], {'b': 1, 'w': 2, 'd': 4}[base[-1]]
    # Prefix bytes precede the opcode; do not search instruction immediates.
    prefixes = []
    for value in bytes.fromhex(getattr(instruction, 'bytes_hex', '') or ''):
        if value not in (0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65, 0x66, 0x67, 0xf2, 0xf3):
            break
        prefixes.append(value)
    narrow = 0x67 in prefixes
    segments = [p for p in prefixes if p in (0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65)]
    fs = bool(segments and segments[-1] == 0x64)
    bits = size * 8
    mask = {1: '0xFFu', 2: '0xFFFFu', 4: '0xFFFFFFFFu'}[size]
    source = '(esi & 0xFFFFu)' if narrow else 'esi'
    destination = '(edi & 0xFFFFu)' if narrow else 'edi'
    if fs:
        source = f'(uint32_t)(XBOX_FS_BASE + {source})'
    count = '(ecx & 0xFFFFu)' if narrow else 'ecx'

    def advance(register, amount):
        if narrow:
            return f'{register} = ({register} & 0xFFFF0000u) | (({register} + {amount}) & 0xFFFFu);'
        return f'{register} += {amount};'

    lines = [f'{{ /* own {prefix + " " if prefix else ""}{base} */']
    # Copy/fill bulk paths are legal only for nonwrapping, forward flat spans.
    # Range comparisons use widened guest offsets, never unrelated host pointers.
    bulk = bool(prefix and not narrow and stem in ('movs', 'stos') and not fs)
    if bulk:
        lines += [f'uint64_t _str_bytes = (uint64_t)ecx * {size}u;',
                  'uint64_t _str_end_d = (uint64_t)edi + _str_bytes;']
        guard = '!g_df && ecx && _str_end_d <= UINT64_C(0x100000000)'
        if stem == 'movs':
            lines.append('uint64_t _str_end_s = (uint64_t)esi + _str_bytes;')
            guard += ' && _str_end_s <= UINT64_C(0x100000000) && (_str_end_s <= edi || _str_end_d <= esi)'
        elif size != 1:
            bulk = False
            lines = lines[:1]
        if bulk:
            lines.append(f'if ({guard}) {{')
            if stem == 'movs':
                lines += ['memcpy((void *)XBOX_PTR(edi), (const void *)XBOX_PTR(esi), (size_t)_str_bytes);',
                          'esi += (uint32_t)_str_bytes;']
            else:
                lines.append('memset((void *)XBOX_PTR(edi), (int)(eax & 0xFFu), (size_t)_str_bytes);')
            lines += ['edi += (uint32_t)_str_bytes;', 'ecx = 0;', '} else {']
    lines.append(f'uint32_t _str_step = (uint32_t)RECOMP_DF_STEP({size});')
    if prefix:
        lines.append(f'while ({count} != 0) {{')
    left = f'MEM{bits}({source})' if stem in ('movs', 'lods', 'cmps') else f'(eax & {mask})'
    lines.append(f'uint32_t _str_value = {left};')
    if stem in ('movs', 'stos'):
        lines.append(f'MEM{bits}({destination}) = (uint{bits}_t)_str_value;')
    elif stem == 'lods':
        lines.append('eax = _str_value;' if size == 4 else
                     f'eax = (eax & ~{mask}) | _str_value;')
    else:
        lines += [f'uint32_t _str_other = MEM{bits}({destination});',
                  '_flags = (_str_value == _str_other);']
        if getattr(context, 'needs_cf', False):
            lines.append('_cf = (_str_value < _str_other);')
        if not prefix:
            lines += ['_fa = _str_value;', '_fb = _str_other;',
                      f'_fas = (int32_t)(int{bits}_t)_str_value;',
                      f'_fbs = (int32_t)(int{bits}_t)_str_other;']
    if stem in ('movs', 'lods', 'cmps'):
        lines.append(advance('esi', '_str_step'))
    if stem != 'lods':
        lines.append(advance('edi', '_str_step'))
    if prefix:
        lines.append(advance('ecx', '0xFFFFFFFFu'))
        if stem in ('cmps', 'scas'):
            lines.append('if (_flags) break;' if prefix in ('repne', 'repnz') else 'if (!_flags) break;')
        lines.append('}')
    if bulk:
        lines.append('}')
    return [*lines, '}']
