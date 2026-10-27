"""Original SSE/SSE2 emission for the project's 32-bit guest ABI.

Register values and memory addresses are sampled before writes. Memory moves
use integer lanes to preserve payload bits; scalar register destinations retain
their unaffected lanes. Floating exceptions, guest MXCSR and alignment faults
remain execution-model work. Reciprocal estimates retain the existing exact
division policy rather than claiming a particular processor's approximation.
"""
from translation_operands import _fmt_mem, _fmt_operand_read, _fmt_operand_write, _GENERAL

_ARITHMETIC = {'add': '+', 'sub': '-', 'mul': '*', 'div': '/'}
_LOGIC = {'and': '&', 'or': '|', 'xor': '^', 'andn': '&'}
_PREDICATES = ('eq', 'lt', 'le', 'unord', 'neq', 'nlt', 'nle', 'ord')
_SUFFIXES = ('ss', 'sd', 'ps', 'pd')
MNEMONICS = frozenset(
    [op + suffix for op in (*_ARITHMETIC, 'min', 'max', 'sqrt') for suffix in _SUFFIXES]
    + [op + suffix for op in ('rcp', 'rsqrt') for suffix in ('ss', 'ps')]
    + [op + suffix for op in _LOGIC for suffix in ('ps', 'pd')]
    + ['cmp' + predicate + suffix for predicate in ('', *_PREDICATES) for suffix in _SUFFIXES]
    + ['mov'+name for name in ('ss','sd','aps','ups','apd','upd','dqa','dqu','q','d',
                               'lps','hps','lpd','hpd','lhps','hlps','mskps','mskpd',
                               'ntps','ntpd','ntdq')]
    + ['unpck'+side+kind for side in ('l','h') for kind in ('ps','pd')]
    + ['shufps','shufpd','pand','pandn','por','pxor','pcmpgtd']
    + [op+suffix for op in ('comi','ucomi') for suffix in ('ss','sd')]
    + ['cvtsi2ss','cvtsi2sd','cvtss2si','cvttss2si','cvtsd2si','cvttsd2si','cvtss2sd','cvtsd2ss'])


def _xmm(operand):
    return operand.type == 'reg' and operand.reg in {f'xmm{i}' for i in range(8)}


def _general(operand):
    return operand.type == 'reg' and operand.reg in _GENERAL


def preserves_flags(instruction):
    name=instruction.mnemonic
    return (name in MNEMONICS and not name.startswith(('comi','ucomi'))
            and (name!='cmpsd' or any(_xmm(op) for op in instruction.operands)))


def _read(operand, name, width):
    if _xmm(operand):
        return [f'RecompXmm {name} = {operand.reg};']
    if operand.type != 'mem':
        raise ValueError('SIMD source must be an XMM register or memory')
    return [f'uint32_t {name}_address = (uint32_t)({_fmt_mem(operand)});',
            f'RecompXmm {name} = {{0}};',
            *(f'{name}.u[{i}] = MEM32({name}_address + {i*4}u);' for i in range(width//4))]


def _write(operand, value, width):
    if _xmm(operand):
        return [f'{operand.reg} = {value};']
    if operand.type != 'mem':
        raise ValueError('SIMD destination must be an XMM register or memory')
    return [f'uint32_t _simd_dest = (uint32_t)({_fmt_mem(operand)});',
            *(f'MEM32(_simd_dest + {i*4}u) = {value}.u[{i}];' for i in range(width//4))]


def _shape(mnemonic):
    suffix = mnemonic[-2:]
    double = suffix in ('sd','pd')
    return ('d' if double else 'f', 'q' if double else 'u',
            1 if suffix[0]=='s' else (2 if double else 4), 8 if double else 4)


def emit(context, instruction, mnemonic, operands):
    if mnemonic not in MNEMONICS:
        return context._unimplemented(instruction, mnemonic)
    if len(operands) not in (2,3):
        raise ValueError('SSE requires two operands and an optional immediate')
    dst,src = operands[:2]
    if dst.type=='mem' and src.type=='mem':
        raise ValueError('SSE does not encode two memory operands')
    body = []
    def finish():
        return [f'{{ /* own {mnemonic} */', *body, '}']
    def require_xmm():
        if not _xmm(dst): raise ValueError('SSE arithmetic destination must be XMM')
    if mnemonic in ('movd','movq'):
        width = 4 if mnemonic=='movd' else 8
        if _xmm(src) or src.type=='mem': body += _read(src,'_simd_b',width)
        elif width==4 and _general(src):
            body += ['RecompXmm _simd_b = {0};',f'_simd_b.u[0] = {src.reg};']
        else: raise ValueError('Invalid MOVD/MOVQ source')
        if _general(dst) and width==4:
            body += [_fmt_operand_write(dst,'_simd_b.u[0]')]
        else:
            for lane in range(width//4,4): body += [f'_simd_b.u[{lane}] = 0;']
            body += _write(dst,'_simd_b',width)
        return finish()
    if mnemonic in ('movaps','movups','movapd','movupd','movdqa','movdqu','movntps','movntpd','movntdq'):
        body += _read(src,'_simd_b',16) + _write(dst,'_simd_b',16)
        return finish()
    if mnemonic in ('movss','movsd','movlps','movhps','movlpd','movhpd','movlhps','movhlps'):
        width = 4 if mnemonic=='movss' else 8
        high = mnemonic in ('movhps','movhpd','movlhps')
        body += _read(src,'_simd_b',width)
        if _xmm(dst):
            body += [f'RecompXmm _simd_result = {dst.reg};']
            if src.type=='mem' and mnemonic in ('movss','movsd'):
                body += ['_simd_result.q[0] = _simd_result.q[1] = 0;']
            for i in range(width//4):
                body += [f'_simd_result.u[{i+(2 if high else 0)}] = _simd_b.u[{i+(2 if mnemonic=="movhlps" else 0)}];']
            body += _write(dst,'_simd_result',16)
        else:
            if high: body += ['_simd_b.q[0] = _simd_b.q[1];']
            body += _write(dst,'_simd_b',width)
        return finish()
    if mnemonic in ('cvtsi2ss','cvtsi2sd'):
        require_xmm();double=mnemonic.endswith('sd');ctype='double' if double else 'float'
        if not (_general(src) or src.type=='mem'): raise ValueError('Integer conversion requires r32/m32')
        value=_fmt_operand_read(src)
        if src.type=='mem':
            body += [f'uint32_t _simd_address = (uint32_t)({_fmt_mem(src)});']
            value='MEM32(_simd_address)'
        body += [f'uint32_t _simd_integer = (uint32_t)({value});']
        value=f'({ctype})(int32_t)_simd_integer'
        if src.type=='mem':
            value=(f'RECOMP_I32_TO_F64(_simd_address, _simd_integer)' if double else
                   f'RECOMP_I32_TO_F32_AT(0x{instruction.address:X}, _simd_address, _simd_integer)')
        body += [f'{dst.reg}.{"d" if double else "f"}[0] = {value};']
        return finish()
    if mnemonic in ('cvtss2si','cvttss2si','cvtsd2si','cvttsd2si','cvtss2sd','cvtsd2ss'):
        double='sd2' in mnemonic;field='d' if double else 'f'
        body += _read(src,'_simd_b',8 if double else 4)
        if mnemonic.endswith('si'):
            if not _general(dst): raise ValueError('Float conversion requires a 32-bit general destination')
            rounding='trunc' if mnemonic.startswith('cvtt') else 'nearbyint'
            body += [f'double _simd_rounded = {rounding}((double)_simd_b.{field}[0]);',
                     'uint32_t _simd_integer = (_simd_rounded >= -2147483648.0 && _simd_rounded < 2147483648.0)',
                     '    ? (uint32_t)(int32_t)_simd_rounded : 0x80000000u;',
                     _fmt_operand_write(dst,'_simd_integer')]
        else:
            require_xmm()
            body += [f'{dst.reg}.{"f" if double else "d"}[0] = ({"float" if double else "double"})_simd_b.{field}[0];']
        return finish()
    if mnemonic.startswith(('comi','ucomi')):
        require_xmm();field,_,_,width=_shape(mnemonic)
        body += _read(dst,'_simd_a',width)+_read(src,'_simd_b',width)
        body += [f'_fca = _simd_a.{field}[0]; _fcb = _simd_b.{field}[0];']
        return finish()
    if mnemonic.startswith('movmsk'):
        if not _general(dst) or not _xmm(src): raise ValueError('MOVMSK requires r32,xmm')
        double=mnemonic.endswith('pd');field='q' if double else 'u';bits=64 if double else 32
        value=' | '.join(f'((uint32_t)({src.reg}.{field}[{i}] >> {bits-1}) << {i})' for i in range(128//bits))
        body += [_fmt_operand_write(dst,value)]
        return finish()
    require_xmm()
    field,bits_field,lanes,width=_shape(mnemonic) if mnemonic[-2:] in _SUFFIXES else ('f','u',4,4)
    body += _read(dst,'_simd_a',16)+_read(src,'_simd_b',lanes*width)
    body += ['RecompXmm _simd_result = _simd_a;']
    if mnemonic.startswith(('shuf','unpck')):
        if mnemonic.startswith('shuf'):
            if len(operands)!=3 or operands[2].type!='imm': raise ValueError('Shuffle requires an immediate')
            control=operands[2].imm&255
            for i in range(lanes):
                source='_simd_a' if i<lanes//2 else '_simd_b'
                index=(control>>(i*(1 if width==8 else 2))) & (lanes-1)
                body += [f'_simd_result.{bits_field}[{i}] = {source}.{bits_field}[{index}];']
        else:
            offset=lanes//2 if mnemonic.startswith('unpckh') else 0
            for i in range(lanes):
                body += [f'_simd_result.{bits_field}[{i}] = _simd_{"a" if i%2==0 else "b"}.{bits_field}[{offset+i//2}];']
    elif mnemonic in ('pand','pandn','por','pxor','pcmpgtd') or mnemonic[:-2] in _LOGIC:
        op=mnemonic[1:] if mnemonic.startswith('p') else mnemonic[:-2]
        for i in range(4):
            a=f'_simd_a.u[{i}]';b=f'_simd_b.u[{i}]'
            expression=(f'((int32_t){a} > (int32_t){b} ? 0xFFFFFFFFu : 0u)' if op=='cmpgtd' else
                        f'{"~" if op=="andn" else ""}{a} {_LOGIC[op]} {b}')
            body += [f'_simd_result.u[{i}] = {expression};']
    else:
        op=mnemonic[:-2]
        for i in range(lanes):
            a=f'_simd_a.{field}[{i}]';b=f'_simd_b.{field}[{i}]'
            if op in _ARITHMETIC:
                value=f'{a} {_ARITHMETIC[op]} {b}'
                if mnemonic.endswith('ss'):
                    value=f'RECOMP_F32_{op.upper()}_AT(0x{instruction.address:X}, {a}, {b})'
            elif op in ('min','max'):
                comparison='<' if op=='min' else '>'
                body += [f'_simd_result.{bits_field}[{i}] = ({a} {comparison} {b}) ? _simd_a.{bits_field}[{i}] : _simd_b.{bits_field}[{i}];']
                continue
            elif op in ('sqrt','rsqrt','rcp'):
                value=b if op=='rcp' else f'{"sqrt" if width==8 else "sqrtf"}({b})'
                if op!='sqrt': value=f'1.0f / {value}'
            elif op.startswith('cmp'):
                predicate=op[3:]
                if not predicate:
                    if len(operands)!=3 or operands[2].type!='imm': raise ValueError('CMP requires predicate immediate')
                    predicate=_PREDICATES[operands[2].imm&7]
                conditions=(f'{a} == {b}',f'{a} < {b}',f'{a} <= {b}',f'isnan({a}) || isnan({b})',
                            f'{a} != {b}',f'!({a} < {b})',f'!({a} <= {b})',f'!isnan({a}) && !isnan({b})')
                value=conditions[_PREDICATES.index(predicate)]
                body += [f'_simd_result.{bits_field}[{i}] = ({value}) ? {"UINT64_MAX" if width==8 else "UINT32_MAX"} : 0;']
                continue
            else: raise ValueError(f'Unhandled SIMD operation {mnemonic}')
            body += [f'_simd_result.{field}[{i}] = {value};']
    body += _write(dst,'_simd_result',16)
    return finish()
