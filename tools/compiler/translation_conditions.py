"""Project condition expressions over captured guest instruction results.

Only known flags are exposed. The producer ABI does not yet preserve a complete
EFLAGS register across mixed setters, dynamic zero shifts, calls or joins.
SAR keeps the established OF=0 policy for longer (architecturally undefined)
counts. Undefined logic/shift AF is materialized as zero by LAHF.
"""
from translation_operands import _operand_width, _fmt_operand_read

_GROUPS = ('o', 'no', 'b c nae', 'ae nb nc', 'e z', 'ne nz', 'be na', 'a nbe',
           's', 'ns', 'p pe', 'np po', 'l nge', 'ge nl', 'le ng', 'g nle')
_CANONICAL = {alias: group.split()[0] for group in _GROUPS for alias in group.split()}
_DESCRIPTIONS = ('overflow', 'no overflow', 'unsigned below', 'unsigned at least',
                 'equal / zero', 'not equal / nonzero', 'unsigned at most', 'unsigned above',
                 'negative', 'nonnegative', 'even parity', 'odd parity',
                 'signed less', 'signed at least', 'signed at most', 'signed greater')
COND_MAP = {'j'+alias: (None, None, description)
            for group, description in zip(_GROUPS, _DESCRIPTIONS) for alias in group.split()}
COND_MAP.update(jecxz=(None, None, 'ECX is zero'), jcxz=(None, None, 'CX is zero'))
_FLOAT = frozenset(('comiss', 'comisd', 'ucomiss', 'ucomisd'))
_X87 = frozenset(('fcomi', 'fcomip', 'fcompi', 'fucomi', 'fucomip', 'fucompi'))
_RESULT = frozenset(('add','sub','adc','sbb','and','or','xor','inc','dec','neg',
                     'shl','sal','shr','sar','shld','shrd'))


def _not(value):
    if value is None:
        return None
    if value in ('0','1'):
        return str(1-int(value))
    if value.isidentifier():
        return '!'+value
    return f'!({value})'


def _combine(a, b, symbol):
    if a is None or b is None:
        return None
    if symbol == '&&':
        if '0' in (a,b): return '0'
        if a == '1': return b
        if b == '1': return a
    if symbol == '||':
        if '1' in (a,b): return '1'
        if a == '0': return b
        if b == '0': return a
    return f'(({a}) {symbol} ({b}))'


def _parity(result):
    return f'((0x9669u >> ((({result}) ^ (({result}) >> 4)) & 15u)) & 1u)'


def _flags(setter, operands):
    if setter == '__zf_snapshot': return {'z':'_flags'}
    if setter == '__zf_from_dest':
        return {'z':f'!({_fmt_operand_read(operands[0])})'} if operands else {}
    if setter in ('bt','bts','btr','btc','rol','ror','rcl','rcr'):
        return {'c':'_cf'}
    if setter in ('bsf','bsr'): return {'z':'!_fa'}
    if setter in ('mul','imul'): return {'c':'_cf','o':'_cf'}
    if setter in _FLOAT:
        unordered = '(_fca != _fca || _fcb != _fcb)'
        return {'z':f'(_fca == _fcb || {unordered})',
                'c':f'(_fca < _fcb || {unordered})', 'p':unordered,
                's':'0','o':'0','a':'0'}
    if setter in _X87 or setter == 'sahf':
        flags = {'z':'(g_fp_cmp == 0 || g_fp_cmp == 2)',
                 'c':'(g_fp_cmp == -1 || g_fp_cmp == 2)', 'p':'!!(g_fp_cmp == 2)'}
        if setter in _X87: flags.update(s='0',o='0',a='0')
        return flags
    if setter and setter.startswith('rep') and any(stem in setter for stem in ('cmps','scas')):
        return {'z':'_flags','c':'_cf'}
    if setter not in _RESULT | {'cmp','test','xadd','cmpxchg'} or not operands:
        return {}
    width = _operand_width(operands[0])
    if width not in (1,2,4):
        return {}
    bits = width*8; mask = f'0x{(1<<bits)-1:X}u'; sign = f'0x{1<<(bits-1):X}u'
    left = right = None
    if setter in ('cmp','test'):
        if len(operands)<2: return {}
        left,right = '_fa','_fb'
        operator = '-' if setter=='cmp' else '&'
        result = f'((uint{bits}_t)(_fa {operator} _fb))'
    else:
        result = '_fa'
    flags = {'z':f'!({result})', 's':f'!!(({result}) & {sign})',
             'p':_parity(result)}
    if setter in ('test','and','or','xor'):
        flags.update(c='0',o='0',a='0')
    elif setter in ('cmp','add','sub','adc','sbb','xadd','cmpxchg'):
        subtract = setter in ('cmp','sub','sbb','cmpxchg')
        if setter in ('add','sub'):
            reverse = '-' if setter=='add' else '+'
            left,right = f'((_fa {reverse} _fb) & {mask})','_fb'
        elif setter in ('adc','sbb'):
            left,right = '(uint32_t)_fbs','_fb'
        elif setter in ('xadd','cmpxchg'):
            left,right = '(uint32_t)_fas','(uint32_t)_fbs'
        flags['c'] = '(_fa < _fb)' if setter=='cmp' else '_cf'
        inversion = '' if subtract else '~'
        flags['o'] = f'!!({inversion}(({left}) ^ ({right})) & (({left}) ^ ({result})) & {sign})'
        flags['a'] = f'!!((({left}) ^ ({right}) ^ ({result})) & 0x10u)'
    elif setter in ('inc','dec'):
        previous = '_fa - 1u' if setter=='inc' else '_fa + 1u'
        flags.update(c='_cf',o='!!_fb',a=f'!!((({previous}) ^ _fa ^ 1u) & 0x10u)')
    elif setter=='neg':
        flags.update(c='!!_fa',o=f'!!(_fa == {sign})',a='!!(((0u - _fa) ^ _fa) & 0x10u)')
    else:
        flags.update(c='_cf',a='0')
        if setter=='sar': flags['o']='0'
    return flags


def _predicate(code, flags):
    c,z,s,o,p = (flags.get(n) for n in ('c','z','s','o','p'))
    if code in ('b','e','s','o','p'):
        return {'b':c,'e':z,'s':s,'o':o,'p':p}[code]
    if code in ('ae','ne','ns','no','np'):
        return _not({'ae':c,'ne':z,'ns':s,'no':o,'np':p}[code])
    if code=='be': return _combine(c,z,'||')
    if code=='a': return _combine(_not(c),_not(z),'&&')
    less = None if s is None or o is None else (s if o=='0' else f'!!(({s}) != ({o}))')
    if code=='l': return less
    if code=='ge': return _not(less)
    if code=='le': return _combine(z,less,'||')
    if code=='g': return _combine(_not(z),_not(less),'&&')
    return None


def make_condition(jump, setter, operands):
    if not isinstance(jump,str) or not jump.startswith('j'):
        return None
    code = _CANONICAL.get(jump[1:])
    if code is None: return None
    expression = _predicate(code,_flags(setter,operands))
    return (expression,COND_MAP[jump][2]) if expression is not None else None


def setcc(mnemonic, setter, operands):
    if not mnemonic.startswith('set'): return None
    result = make_condition('j'+mnemonic[3:],setter,operands)
    return result[0] if result else None


def cmovcc(mnemonic, setter, operands):
    if not mnemonic.startswith('cmov'): return None
    result = make_condition('j'+mnemonic[4:],setter,operands)
    return result[0] if result else None


def lahf(setter, operands):
    flags = _flags(setter,operands)
    if not all(flag in flags for flag in ('s','z','a','p','c')):
        return None
    pieces = ['0x02']
    for flag,bit in (('s',128),('z',64),('a',16),('p',4),('c',1)):
        if flags[flag]!='0': pieces.append(f'(({flags[flag]}) ? 0x{bit:02X} : 0)')
    return '(uint8_t)('+' | '.join(pieces)+')'


def zf_expression(state):
    if not state: return None
    setter,operands = state
    if setter=='__zf_snapshot': return '_flags'
    if setter=='__zf_from_dest':
        return f'!({_fmt_operand_read(operands[0])})' if operands else None
    if setter=='cmp' and len(operands)>=2: return '(_fa == _fb)'
    if setter=='test' and len(operands)>=2: return '!(_fa & _fb)'
    if setter in _RESULT | {'xadd','cmpxchg'}: return '!_fa'
    return None
