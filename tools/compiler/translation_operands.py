"""Project C expressions for the 32-bit guest register and memory model.

Integer subregister writes preserve the unaffected bits. FS accesses retain
the runtime's thread-base mapping; other segment bases remain flat.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

_GENERAL = tuple('eax ebx ecx edx esi edi ebp esp'.split())
_SEGMENTS = frozenset('cs ds es ss fs gs'.split())
_SYSTEM_REGS = frozenset(f'{kind}{n}' for kind, count in (('cr',16),('dr',16),('tr',8))
                         for n in range(count))
_PARTS = {}
for parent in _GENERAL:
    _PARTS[parent[1:]] = (parent, 16, 'LO16')
for parent in _GENERAL[:4]:
    _PARTS[parent[1]+'l'] = (parent, 8, 'LO8')
    _PARTS[parent[1]+'h'] = (parent, 8, 'HI8')
_VECTOR = frozenset(f'{kind}{n}' for kind in ('xmm','mm') for n in range(8))
_FLOAT = {'st': 0, **{f'st({n})': n for n in range(8)}}
_KNOWN = frozenset((*_GENERAL, *_PARTS, *_VECTOR, *_SYSTEM_REGS, *_SEGMENTS, *_FLOAT))


def _register(name):
    if not isinstance(name, str):
        raise ValueError('Register name must be a string')
    name = name.lower()
    if name not in _KNOWN:
        raise ValueError(f'Unsupported register: {name!r}')
    return name


def _fmt_reg(name, size=4):
    if not name:
        return '0'
    name = _register(name)
    if name in _FLOAT:
        return f'g_fp_stack[(g_fp_top + {_FLOAT[name]}) & 7]'
    if name in _PARTS:
        parent, _, macro = _PARTS[name]
        return f'{macro}({parent})'
    if name in _SEGMENTS:
        return f'0 /* seg:{name} */'
    if name in _SYSTEM_REGS:
        return f'0 /* {name}: system register, not modelled */'
    return name


def _fmt_set_reg(name, value_expr):
    name = _register(name)
    if name in _FLOAT:
        return f'g_fp_stack[(g_fp_top + {_FLOAT[name]}) & 7] = {value_expr};'
    if name in _PARTS:
        parent, _, macro = _PARTS[name]
        return f'SET_{macro}({parent}, {value_expr});'
    if name in _SEGMENTS or name in _SYSTEM_REGS:
        kind = 'segment' if name in _SEGMENTS else 'system'
        return f'/* mov {name}, {value_expr} - {kind} register */;'
    return f'{name} = {value_expr};'


def _fmt_imm(value):
    if type(value) is not int or not -(2**31) <= value < 2**32:
        raise ValueError('Immediate must fit the 32-bit guest domain')
    if value < 10:
        return str(value)
    return f'0x{value:08X}u' if value > 0x7fffffff else f'0x{value:X}'


def _mem_accessor(size):
    if type(size) is not int or size not in (1,2,4):
        raise ValueError('Integer access requires 1, 2 or 4 bytes')
    return f'MEM{8*size}'


def _smem_accessor(size):
    if type(size) is not int or size not in (1,2,4,8):
        raise ValueError('Signed access requires 1, 2, 4 or 8 bytes')
    return f'SMEM{8*size}'


def _fmt_mem(op):
    segment = getattr(op, 'mem_seg', None)
    if segment and segment not in _SEGMENTS:
        raise ValueError('Invalid address segment')
    address_size = getattr(op, 'mem_addr_size', 4)
    if type(address_size) is not int or address_size not in (2,4):
        raise ValueError('Address width must be 16 or 32 bits')
    _fmt_imm(op.mem_disp)  # Validate before normalizing absolute addresses.
    terms = ['XBOX_FS_BASE'] if segment == 'fs' and address_size == 4 else []
    for register, scale in ((op.mem_base, 1), (op.mem_index, op.mem_scale)):
        if register:
            if type(scale) is not int or scale not in (1,2,4,8):
                raise ValueError('Invalid x86 address scale')
            if register not in _GENERAL and register not in ('bx','bp','si','di'):
                raise ValueError('Invalid address register')
            expression = _fmt_reg(register)
            terms.append(expression if scale == 1 else f'{expression} * {scale}')
    if op.mem_disp:
        displacement = op.mem_disp if terms else op.mem_disp & 0xffffffff
        terms.append(_fmt_imm(displacement))
    result = ' + '.join(terms) if terms else '0'
    if address_size == 2:
        result = f'(uint16_t)({result})'
        if segment == 'fs':
            result = f'XBOX_FS_BASE + {result}'
    return result


def _fmt_mem_read(op):
    return f'{_mem_accessor(op.mem_size)}({_fmt_mem(op)})'


def _fmt_mem_write(op, value_expr):
    return f'{_fmt_mem_read(op)} = {value_expr};'


def _operand_width(op):
    if op.type == 'mem':
        return op.mem_size or 4
    if op.type == 'reg':
        name = _register(op.reg)
        if name in _PARTS:
            return _PARTS[name][1] // 8
        if name in _FLOAT:
            return 10  # Architectural x87 width; runtime storage is separate.
        if name in _VECTOR:
            return 16 if name.startswith('xmm') else 8
        return 4
    return None


def _fmt_operand_read(op):
    if op.type == 'reg':
        return _fmt_reg(op.reg)
    if op.type == 'imm':
        return _fmt_imm(op.imm)
    if op.type == 'mem':
        return _fmt_mem_read(op)
    raise ValueError(f'Unsupported operand: {op.type!r}')


def _fmt_operand_write(op, value_expr):
    if op.type == 'reg':
        return _fmt_set_reg(op.reg, value_expr)
    if op.type == 'mem':
        return _fmt_mem_write(op, value_expr)
    raise ValueError(f'Operand is not writable: {op.type!r}')


def _fmt_fpu_operand(op):
    if op.type == 'mem':
        if op.mem_size not in (4,8):
            raise ValueError('Floating load requires 4 or 8 bytes')
        return f'{"MEMF" if op.mem_size == 4 else "MEMD"}({_fmt_mem(op)})'
    if op.type == 'reg' and op.reg in _FLOAT:
        return _fmt_reg(op.reg)
    return None
