"""Original packed-integer emission for the guest MMX register interface.

All arithmetic is evaluated from captured bit patterns with explicit lane masks.
Signed lanes use mathematical sign extension, avoiding signed shifts/overflow.
The existing separate MMX/x87 storage and host FP environment remain contracts;
this module does not claim x87 tag aliasing or guest exception delivery.
"""
from translation_operands import _fmt_mem, _fmt_operand_read, _fmt_operand_write, _GENERAL

_WRAP = {op+suffix:(bits,op=='padd') for op in ('padd','psub') for suffix,bits in (('b',8),('w',16),('d',32))}
_SAT = {op+kind+suffix:(bits,op=='padd',kind=='s') for op in ('padd','psub') for kind in ('s','us') for suffix,bits in (('b',8),('w',16))}
_COMPARE = {op+suffix:(bits,op=='pcmpeq') for op in ('pcmpeq','pcmpgt') for suffix,bits in (('b',8),('w',16),('d',32))}
_UNPACK = {'punpck'+side+suffix:(bits,side=='h') for side in ('l','h') for suffix,bits in (('bw',8),('wd',16),('dq',32))}
_SHIFT = {op+suffix:(bits,op) for op in ('psll','psrl','psra') for suffix,bits in (('w',16),('d',32),('q',64)) if op!='psra' or bits!=64}
_SPECIAL = frozenset(('pmullw','pmulhw','pmaddwd','pavgb','pavgw','pminsw','pmaxsw','psadbw','pand','pandn','por','pxor',
    'packsswb','packuswb','packssdw','movd','movq','movntq','pinsrw','pextrw','pshufw','pmovmskb',
    'cvtpi2ps','cvtps2pi','cvttps2pi','emms','femms'))
MNEMONICS = frozenset(_WRAP)|frozenset(_SAT)|frozenset(_COMPARE)|frozenset(_UNPACK)|frozenset(_SHIFT)|_SPECIAL


def is_mmx(op):
    return op.type=='reg' and op.reg in {f'mm{i}' for i in range(8)}


def _xmm(op):
    return op.type=='reg' and op.reg in {f'xmm{i}' for i in range(8)}


def preserves_flags(instruction):
    return instruction.mnemonic in MNEMONICS and (instruction.mnemonic in ('emms','femms','cvtpi2ps') or any(is_mmx(op) for op in instruction.operands))


def _constant(value):return f'UINT64_C(0x{value:X})'

def _lane(name,bits,index):return f'(({name} >> {bits*index}) & {_constant((1<<bits)-1)})'

def _signed(value,bits):return f'((int64_t)({value} ^ {_constant(1<<(bits-1))}) - INT64_C(0x{1<<(bits-1):X}))'


def _read(op,name,width=8):
    if is_mmx(op):return [f'uint64_t {name} = {op.reg}.q;']
    if op.type!='mem':raise ValueError('MMX source requires mm/memory')
    code=[f'uint32_t {name}_address = (uint32_t)({_fmt_mem(op)});',f'uint64_t {name} = MEM32({name}_address);']
    if width==8:code += [f'{name} |= (uint64_t)MEM32({name}_address + 4u) << 32;']
    return code


def emit(context,instruction,mnemonic,operands):
    if mnemonic not in MNEMONICS:return context._unimplemented(instruction,mnemonic)
    if mnemonic in ('emms','femms'):
        if operands:raise ValueError('EMMS has no operands')
        return [f'(void)0; /* own {mnemonic}: separate MMX/x87 storage */']
    expected=3 if mnemonic in ('pinsrw','pextrw','pshufw') else 2
    if len(operands)!=expected:raise ValueError('Invalid MMX operand count')
    dst,src=operands[:2];body=[]
    if expected==3 and operands[2].type!='imm':raise ValueError('MMX lane selector must be immediate')
    control=(operands[2].imm&255) if expected==3 else 0
    def finish():return [f'{{ /* own {mnemonic} */',*body,'}']
    if mnemonic in ('movd','movq','movntq'):
        width=4 if mnemonic=='movd' else 8
        if mnemonic=='movntq' and (dst.type!='mem' or not is_mmx(src)):raise ValueError('MOVNTQ requires m64,mm')
        if is_mmx(dst):
            if width==4 and src.type=='reg' and src.reg in _GENERAL:
                body += [f'uint64_t _mm_b = (uint32_t)({_fmt_operand_read(src)});']
            else:body += _read(src,'_mm_b',width)
            body += [f'{dst.reg}.q = _mm_b'+(' & UINT64_C(0xFFFFFFFF);' if width==4 else ';')]
        elif is_mmx(src):
            body += _read(src,'_mm_b')
            if dst.type=='mem':
                body += [f'uint32_t _mm_dest = (uint32_t)({_fmt_mem(dst)});','MEM32(_mm_dest) = (uint32_t)_mm_b;']
                if width==8:body += ['MEM32(_mm_dest + 4u) = (uint32_t)(_mm_b >> 32);']
            elif width==4 and dst.type=='reg' and dst.reg in _GENERAL:
                body += [_fmt_operand_write(dst,'(uint32_t)_mm_b')]
            else:raise ValueError('Invalid MMX move destination')
        else:raise ValueError('MMX move requires MMX register')
        return finish()
    if mnemonic=='cvtpi2ps':
        if not _xmm(dst):raise ValueError('CVTPI2PS requires xmm destination')
        body += _read(src,'_mm_b')
        for i in range(2):body += [f'{dst.reg}.f[{i}] = (float){_signed(_lane("_mm_b",32,i),32)};']
        return finish()
    if mnemonic in ('cvtps2pi','cvttps2pi'):
        if not is_mmx(dst):raise ValueError('CVTPS2PI requires mm destination')
        if _xmm(src):body += [f'RecompXmm _mm_float = {src.reg};']
        elif src.type=='mem':
            body += [f'uint32_t _mm_address = (uint32_t)({_fmt_mem(src)});','RecompXmm _mm_float = {0};',
                     '_mm_float.u[0] = MEM32(_mm_address);','_mm_float.u[1] = MEM32(_mm_address + 4u);']
        else:raise ValueError('CVTPS2PI requires xmm/m64 source')
        body += ['uint64_t _mm_result = 0;']
        for i in range(2):
            body += [f'double _mm_rounded{i} = {"trunc" if mnemonic=="cvttps2pi" else "nearbyint"}((double)_mm_float.f[{i}]);',
                     f'uint32_t _mm_integer{i} = (_mm_rounded{i} >= -2147483648.0 && _mm_rounded{i} < 2147483648.0)',
                     f'    ? (uint32_t)(int32_t)_mm_rounded{i} : 0x80000000u;',f'_mm_result |= (uint64_t)_mm_integer{i} << {32*i};']
        body += [f'{dst.reg}.q = _mm_result;'];return finish()
    if mnemonic in ('pextrw','pmovmskb'):
        if not is_mmx(src) or dst.type!='reg' or dst.reg not in _GENERAL:raise ValueError('MMX extraction requires r32,mm')
        body += _read(src,'_mm_b')
        value=_lane('_mm_b',16,control&3) if mnemonic=='pextrw' else ' | '.join(f'(((_mm_b >> {8*i+7}) & 1u) << {i})' for i in range(8))
        body += [_fmt_operand_write(dst,'(uint32_t)('+value+')')];return finish()
    if not is_mmx(dst):return context._unimplemented(instruction,mnemonic,' (dst not mm)')
    body += _read(dst,'_mm_a')
    if mnemonic=='pinsrw':
        if src.type=='mem':value=f'MEM16({_fmt_mem(src)})'
        elif src.type=='reg' and src.reg in _GENERAL:value=_fmt_operand_read(src)
        else:raise ValueError('PINSRW requires r32/m16 source')
        shift=(control&3)*16
        body += [f'uint64_t _mm_word = (uint32_t)({value}) & 0xFFFFu;',f'{dst.reg}.q = (_mm_a & ~({_constant(65535)} << {shift})) | (_mm_word << {shift});']
        return finish()
    if mnemonic in _SHIFT:
        if src.type=='imm':body += [f'uint64_t _mm_count = {src.imm&255}u;']
        else:
            body += _read(src,'_mm_count')
    else:
        # MMX low-unpack operands are m32, unlike the full-width SSE form.
        width=4 if mnemonic in _UNPACK and not _UNPACK[mnemonic][1] else 8
        body += _read(src,'_mm_b',width)
    body += ['uint64_t _mm_result = 0;']
    def put(value,bits,index):
        body.append(f'_mm_result |= ((uint64_t)({value}) & {_constant((1<<bits)-1)}) << {bits*index};')
    def saturate(value,bits,index,signed):
        lo=-(1<<(bits-1)) if signed else 0;hi=(1<<(bits-1))-1 if signed else (1<<bits)-1
        body.append(f'int64_t _mm_value{index} = {value};')
        body.append(f'_mm_value{index} = _mm_value{index} < {lo} ? {lo} : (_mm_value{index} > {hi} ? {hi} : _mm_value{index});')
        put(f'_mm_value{index}',bits,index)
    if mnemonic in _WRAP or mnemonic in _SAT or mnemonic in _COMPARE:
        bits=(_WRAP.get(mnemonic) or _SAT.get(mnemonic) or _COMPARE[mnemonic])[0]
        for i in range(64//bits):
            a,b=_lane('_mm_a',bits,i),_lane('_mm_b',bits,i)
            if mnemonic in _WRAP:put(f'{a} {"+" if _WRAP[mnemonic][1] else "-"} {b}',bits,i)
            elif mnemonic in _SAT:
                _,add,signed=_SAT[mnemonic]
                a,b=(_signed(a,bits),_signed(b,bits)) if signed else (f'(int64_t){a}',f'(int64_t){b}')
                saturate(f'{a} {"+" if add else "-"} {b}',bits,i,signed)
            else:
                equal=_COMPARE[mnemonic][1]
                if not equal:a,b=_signed(a,bits),_signed(b,bits)
                put(f'{a} {"==" if equal else ">"} {b} ? {_constant((1<<bits)-1)} : 0',bits,i)
    elif mnemonic in _SHIFT:
        bits,kind=_SHIFT[mnemonic];mask=_constant((1<<bits)-1)
        for i in range(64//bits):
            a=_lane('_mm_a',bits,i);sign=f'({a} & {_constant(1<<(bits-1))})'
            if kind=='psra':value=f'(_mm_count >= {bits} ? ({sign} ? {mask} : 0) : (({a} >> _mm_count) | ({sign} ? ({mask} ^ ({mask} >> _mm_count)) : 0)))'
            else:value=f'(_mm_count >= {bits} ? 0 : ({a} {"<<" if kind=="psll" else ">>"} _mm_count))'
            put(value,bits,i)
    elif mnemonic in _UNPACK:
        bits,high=_UNPACK[mnemonic];half=32//bits
        for i in range(64//bits):put(_lane('_mm_b' if i%2 else '_mm_a',bits,i//2+(half if high else 0)),bits,i)
    elif mnemonic.startswith('pack'):
        source_bits=32 if mnemonic=='packssdw' else 16;bits=source_bits//2
        for i in range(64//bits):
            value=_signed(_lane('_mm_a' if i<64//source_bits else '_mm_b',source_bits,i%(64//source_bits)),source_bits)
            saturate(value,bits,i,mnemonic!='packuswb')
    elif mnemonic in ('pmullw','pmulhw','pmaddwd'):
        for i in range(2 if mnemonic=='pmaddwd' else 4):
            j=i*2 if mnemonic=='pmaddwd' else i
            value=f'{_signed(_lane("_mm_a",16,j),16)} * {_signed(_lane("_mm_b",16,j),16)}'
            if mnemonic=='pmaddwd':value+=f' + {_signed(_lane("_mm_a",16,j+1),16)} * {_signed(_lane("_mm_b",16,j+1),16)}'
            if mnemonic=='pmulhw':value=f'((uint64_t)({value}) >> 16)'
            put(value,32 if mnemonic=='pmaddwd' else 16,i)
    elif mnemonic in ('pavgb','pavgw','pminsw','pmaxsw'):
        bits=8 if mnemonic=='pavgb' else 16
        for i in range(64//bits):
            a,b=_lane('_mm_a',bits,i),_lane('_mm_b',bits,i)
            value=f'({a} + {b} + 1u) >> 1' if mnemonic.startswith('pavg') else f'{_signed(a,bits)} {"<" if mnemonic=="pminsw" else ">"} {_signed(b,bits)} ? {a} : {b}'
            put(value,bits,i)
    elif mnemonic=='psadbw':
        for i in range(8):
            body += [f'int64_t _mm_delta{i} = (int64_t){_lane("_mm_a",8,i)} - (int64_t){_lane("_mm_b",8,i)};',f'_mm_result += (uint64_t)(_mm_delta{i} < 0 ? -_mm_delta{i} : _mm_delta{i});']
    elif mnemonic in ('pand','pandn','por','pxor'):
        body += [f'_mm_result = {"~" if mnemonic=="pandn" else ""}_mm_a '+{'pand':'&','pandn':'&','por':'|','pxor':'^'}[mnemonic]+' _mm_b;']
    elif mnemonic=='pshufw':
        for i in range(4):put(_lane('_mm_b',16,(control>>(2*i))&3),16,i)
    else:raise ValueError('Missing MMX emission')
    body += [f'{dst.reg}.q = _mm_result;'];return finish()
