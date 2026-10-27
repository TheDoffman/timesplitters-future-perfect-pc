"""Original x87 emission for the current binary64 guest stack interface.

The emitted operations preserve stack ordering and the existing control/status
ABI. Internal extended precision, stack tags and exception delivery belong to
the execution model; libm transcendental results are not x87 bit-exact claims.
"""
import re
from translation_operands import _fmt_mem, _fmt_operand_read, _fmt_operand_write

_ARITHMETIC={}
for stem,operator in (('add','+'),('sub','-'),('mul','*'),('div','/')):
    for reverse in (False,True) if stem in ('sub','div') else (False,):
        core=stem+('r' if reverse else '')
        for pop in (False,True):_ARITHMETIC['f'+core+('p' if pop else '')]=(operator,reverse,pop,False)
        _ARITHMETIC['fi'+core]=(operator,reverse,False,True)
_CONSTANTS={'fldz':'0.0','fld1':'1.0','fldpi':'0x1.921fb54442d18p+1',
            'fldl2e':'0x1.71547652b82fep+0','fldl2t':'0x1.a934f0979a371p+1',
            'fldlg2':'0x1.34413509f79ffp-2','fldln2':'0x1.62e42fefa39efp-1'}
_UNARY={'fabs':'fabs(_x87_value)','fchs':'-_x87_value','fsqrt':'sqrt(_x87_value)',
        'fsin':'sin(_x87_value)','fcos':'cos(_x87_value)','f2xm1':'exp2(_x87_value) - 1.0'}
_COMPARE=frozenset(('fcom','fcomp','fcompp','fucom','fucomp','fucompp','fcomi','fcomip','fcompi','fucomi','fucomip','fucompi'))
MNEMONICS=frozenset(_ARITHMETIC)|frozenset(_CONSTANTS)|frozenset(_UNARY)|_COMPARE|frozenset((
    'fld','fst','fstp','fild','fist','fistp','fisttp','fsincos','fptan','fpatan','fyl2x','fyl2xp1',
    'fprem','fprem1','fscale','frndint','ftst','fxam','fxch','fnstsw','fstsw','fnstcw','fstcw','fldcw'))


def stack_index(register):
    match=re.fullmatch(r'st(?:\(([0-7])\)|([0-7]))',register or '')
    if not match:raise ValueError('Expected an x87 stack register')
    return int(match[1] or match[2])


def stack(index):
    if not isinstance(index,int) or not 0<=index<8:raise ValueError('x87 stack index outside 0..7')
    return ('fp_top()','fp_st1()')[index] if index<2 else f'g_fp_stack[(g_fp_top + {index}) & 7]'


def preserves_flags(instruction):
    return instruction.mnemonic in MNEMONICS and instruction.mnemonic not in _COMPARE-{'fcom','fcomp','fcompp','fucom','fucomp','fucompp'}


def _address(operand,name='_fp_address'):
    if operand.type!='mem':raise ValueError('Expected x87 memory operand')
    return f'uint32_t {name} = (uint32_t)({_fmt_mem(operand)});'


def _load(operand,name,integer=False,observe=False):
    if operand.type=='reg' and not integer:return [f'double {name} = {stack(stack_index(operand.reg))};']
    code=[_address(operand,name+'_address')];address=name+'_address';width=operand.mem_size
    if integer:
        if width not in (2,4,8):raise ValueError('x87 integer memory width must be 2, 4 or 8')
        code += [f'uint64_t {name}_bits = MEM{width*8}({address});']
        if width==4 and observe:
            code += [f'double {name} = RECOMP_I32_TO_F64({address}, (uint32_t){name}_bits);']
        else:
            mask=(1<<(width*8))-1;sign=1<<(width*8-1)
            code += [f'double {name} = ({name}_bits & UINT64_C(0x{sign:X}))',
                     f'    ? -(double)((~{name}_bits + 1u) & UINT64_C(0x{mask:X})) : (double){name}_bits;']
        return code
    if width in (4,8):return code+[f'double {name} = {"MEMF" if width==4 else "MEMD"}({address});']
    if width!=10:raise ValueError('x87 floating memory width must be 4, 8 or 10')
    code += [f'uint64_t {name}_mantissa = MEM32({address});',
             f'{name}_mantissa |= (uint64_t)MEM32({address} + 4u) << 32;',
             f'uint16_t {name}_se = MEM16({address} + 8u);',
             f'unsigned {name}_exponent = {name}_se & 0x7FFFu;',
             f'double {name};',
             f'if ({name}_exponent == 0x7FFFu) {name} = {name}_mantissa == UINT64_C(0x8000000000000000) ? INFINITY : NAN;',
             f'else if ({name}_exponent && !({name}_mantissa >> 63)) {name} = NAN;',
             f'else {name} = ldexp((double){name}_mantissa, (int)({name}_exponent ? {name}_exponent : 1u) - 16446);',
             f'if ({name}_se & 0x8000u) {name} = -{name};']
    return code


def _round(name,truncate=False):
    mode='3u' if truncate else '((g_fp_control_word >> 10) & 3u)'
    return [f'double {name}_rounded = {name};',
            f'if (isfinite({name})) {{',
            f'    double _fp_integral; double _fp_fraction = modf({name}, &_fp_integral);',
            f'    unsigned _fp_mode = {mode};',
            '    double _fp_increment = 0.0;',
            '    if (_fp_mode == 1u && _fp_fraction < 0.0) _fp_increment = -1.0;',
            '    if (_fp_mode == 2u && _fp_fraction > 0.0) _fp_increment = 1.0;',
            '    if (_fp_mode == 0u && (fabs(_fp_fraction) > 0.5 ||',
            '        (fabs(_fp_fraction) == 0.5 && fmod(fabs(_fp_integral), 2.0) == 1.0)))',
            '        _fp_increment = copysign(1.0, _fp_fraction);',
            f'    {name}_rounded = _fp_integral + _fp_increment;',
            f'    if ({name}_rounded == 0.0) {name}_rounded = copysign(0.0, {name});',
            '}']


def emit(context,instruction,mnemonic,operands):
    if mnemonic not in MNEMONICS:return context._unimplemented(instruction,mnemonic)
    body=[]
    def finish():return [f'{{ /* own {mnemonic} */',*body,'}']
    def arity(count):
        if len(operands)!=count:raise ValueError('Invalid x87 operand count')
    if mnemonic in _CONSTANTS:
        arity(0);body += [f'fp_push({_CONSTANTS[mnemonic]});'];return finish()
    if mnemonic in ('fld','fild'):
        arity(1);body += _load(operands[0],'_x87_value',mnemonic=='fild',observe=True)+['fp_push(_x87_value);'];return finish()
    if mnemonic in ('fst','fstp'):
        arity(1);dst=operands[0];body += ['double _x87_value = fp_top();']
        if dst.type=='reg':body += [f'{stack(stack_index(dst.reg))} = _x87_value;']
        else:
            body += [_address(dst)]
            if dst.mem_size in (4,8):body += [f'{"MEMF" if dst.mem_size==4 else "MEMD"}(_fp_address) = {"(float)" if dst.mem_size==4 else ""}_x87_value;']
            elif dst.mem_size==10:
                body += ['uint16_t _fp_se = signbit(_x87_value) ? 0x8000u : 0u;',
                         'uint64_t _fp_mantissa = 0;',
                         'if (isnan(_x87_value)) { _fp_se |= 0x7FFFu; _fp_mantissa = UINT64_C(0xC000000000000000); }',
                         'else if (isinf(_x87_value)) { _fp_se |= 0x7FFFu; _fp_mantissa = UINT64_C(0x8000000000000000); }',
                         'else if (_x87_value != 0.0) {',
                         '    int _fp_exponent; double _fp_fraction = frexp(fabs(_x87_value), &_fp_exponent);',
                         '    _fp_mantissa = (uint64_t)ldexp(_fp_fraction, 64);',
                         '    _fp_se |= (uint16_t)(_fp_exponent + 16382);',
                         '}',
                         'MEM32(_fp_address) = (uint32_t)_fp_mantissa;',
                         'MEM32(_fp_address + 4u) = (uint32_t)(_fp_mantissa >> 32);',
                         'MEM16(_fp_address + 8u) = _fp_se;']
            else:raise ValueError('Invalid x87 store width')
        if mnemonic=='fstp':body += ['fp_pop();']
        return finish()
    if mnemonic in ('fist','fistp','fisttp'):
        arity(1);dst=operands[0];width=dst.mem_size
        if width not in (2,4,8):raise ValueError('Invalid x87 integer store width')
        body += [_address(dst),'double _x87_value = fp_top();']+_round('_x87_value',mnemonic=='fisttp')
        limit=f'0x1p{width*8-1}'
        body += [f'uint64_t _fp_integer = (_x87_value_rounded >= -{limit} && _x87_value_rounded < {limit})',
                 f'    ? (uint64_t)(int64_t)_x87_value_rounded : UINT64_C(0x{1<<(width*8-1):X});',
                 f'MEM{width*8}(_fp_address) = (uint{width*8}_t)_fp_integer;']
        if mnemonic!='fist':body += ['fp_pop();']
        return finish()
    if mnemonic in _ARITHMETIC:
        operator,reverse,pop,integer=_ARITHMETIC[mnemonic]
        if operands and operands[0].type=='mem':
            arity(1);destination=stack(0);body += _load(operands[0],'_fp_right',integer)
            pop=False
        elif integer:raise ValueError('Integer x87 arithmetic requires memory')
        elif len(operands)==2:
            destination=stack(stack_index(operands[0].reg));body += _load(operands[1],'_fp_right')
        elif len(operands)==1:
            destination=stack(stack_index(operands[0].reg)) if pop else stack(0)
            body += [f'double _fp_right = {stack(0) if pop else stack(stack_index(operands[0].reg))};']
        elif not operands:
            destination=stack(1);pop=True;body += [f'double _fp_right = {stack(0)};']
        else:raise ValueError('Invalid x87 arithmetic operands')
        body += [f'double _fp_left = {destination};',f'{destination} = '+('_fp_right' if reverse else '_fp_left')+f' {operator} '+('_fp_left' if reverse else '_fp_right')+';']
        if pop:body += ['fp_pop();']
        return finish()
    if mnemonic in _UNARY or mnemonic=='frndint':
        arity(0);body += ['double _x87_value = fp_top();']
        if mnemonic=='frndint':body += _round('_x87_value')+['fp_top() = _x87_value_rounded;']
        else:body += [f'fp_top() = {_UNARY[mnemonic]};']
        return finish()
    if mnemonic in ('fsincos','fptan'):
        arity(0);body += ['double _x87_value = fp_top();',f'fp_top() = {"sin" if mnemonic=="fsincos" else "tan"}(_x87_value);',
                         'fp_push(cos(_x87_value));' if mnemonic=='fsincos' else 'fp_push(1.0);'];return finish()
    if mnemonic in ('fpatan','fyl2x','fyl2xp1','fprem','fprem1','fscale'):
        arity(0);body += ['double _fp_top = fp_top();','double _fp_next = fp_st1();']
        if mnemonic in ('fpatan','fyl2x','fyl2xp1'):
            expression={'fpatan':'atan2(_fp_next, _fp_top)','fyl2x':'_fp_next * log2(_fp_top)','fyl2xp1':'_fp_next * log2(_fp_top + 1.0)'}[mnemonic]
            body += [f'fp_st1() = {expression};','fp_pop();']
        elif mnemonic in ('fprem','fprem1'):
            body += [f'fp_top() = {"fmod" if mnemonic=="fprem" else "remainder"}(_fp_top, _fp_next);']
        else:
            body += ['if (isnan(_fp_top) || isnan(_fp_next)) fp_top() = NAN;',
                     'else if (isinf(_fp_next)) {',
                     '    if (_fp_next > 0.0) fp_top() = _fp_top == 0.0 ? NAN : copysign(INFINITY, _fp_top);',
                     '    else fp_top() = isinf(_fp_top) ? NAN : copysign(0.0, _fp_top);',
                     '} else {',
                     '    int _fp_scale = _fp_next > 4096.0 ? 4096 : (_fp_next < -4096.0 ? -4096 : (int)_fp_next);',
                     '    fp_top() = ldexp(_fp_top, _fp_scale);',
                     '}']
        return finish()
    if mnemonic=='fxch':
        if len(operands)>2:raise ValueError('Invalid FXCH operands')
        target=stack(stack_index(operands[-1].reg)) if operands else stack(1)
        body += ['double _x87_value = fp_top();',f'fp_top() = {target};',f'{target} = _x87_value;'];return finish()
    if mnemonic=='fxam':
        arity(0);body += ['double _x87_value = fp_top();','uint16_t _fp_class = _x87_value == 0.0 ? 0x4000u : 0x0400u;',
                         'if (isinf(_x87_value)) _fp_class = 0x0500u;','if (isnan(_x87_value)) _fp_class = 0x0100u;',
                         'g_fp_cc = (uint16_t)(_fp_class | (signbit(_x87_value) ? 0x0200u : 0u));'];return finish()
    if mnemonic in _COMPARE or mnemonic=='ftst':
        if len(operands)>2:raise ValueError('Invalid x87 compare operands')
        if mnemonic=='ftst':arity(0);body += ['double _fp_right = 0.0;']
        elif operands:body += _load(operands[-1],'_fp_right')
        else:body += ['double _fp_right = fp_st1();']
        body += ['double _fp_left = fp_top();',
                 'g_fp_cmp = (isnan(_fp_left) || isnan(_fp_right)) ? 2 : (_fp_left < _fp_right ? -1 : (_fp_left > _fp_right ? 1 : 0));']
        integer_flags=mnemonic.endswith(('i','ip'))
        if not integer_flags:body += ['g_fp_cc = (uint16_t)(g_fp_cmp == 2 ? 0x4500u : (g_fp_cmp < 0 ? 0x0100u : (g_fp_cmp == 0 ? 0x4000u : 0u)));']
        pops=2 if mnemonic.endswith('pp') else int(mnemonic.endswith(('p','pi')))
        body += ['fp_pop();']*pops;return finish()
    if mnemonic in ('fnstsw','fstsw','fnstcw','fstcw','fldcw'):
        arity(1);op=operands[0]
        if mnemonic=='fldcw':
            body += ([f'g_fp_control_word = MEM16({_fmt_mem(op)});'] if op.type=='mem' else [f'g_fp_control_word = (uint16_t)({_fmt_operand_read(op)});'])
        else:
            value='(uint16_t)(((g_fp_top & 7u) << 11) | g_fp_cc)' if mnemonic.endswith('sw') else 'g_fp_control_word'
            body += ([f'MEM16({_fmt_mem(op)}) = {value};'] if op.type=='mem' else [_fmt_operand_write(op,value)])
        return finish()
    raise ValueError('Missing x87 emission')
