"""Project symbol-to-runtime hook planning and guest ABI thunk emission.

Markers are lexical C observations, not a full preprocessing result. Return
cleanup is consensus over decoded near returns in the supplied function span;
it does not infer tail-call conventions or prove instruction/data separation.
"""
from pathlib import Path
import json
import re

from c_source_scan import mask_noncode, strip_disabled
from x86_decode import make_decoder

_IDENTIFIER = re.compile(r'[A-Za-z_][A-Za-z_0-9]*\Z')
_REGISTERS = frozenset(('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp'))
# Capacity of the current shared runtime's RECOMP_HLE_LTCG_CALL argument frame.
MAX_REGISTER_FRAME_ARGUMENTS = 24


def _name(value):
    if not isinstance(value, str) or not _IDENTIFIER.fullmatch(value):
        raise ValueError('Hook name must be a C identifier')
    return value


def _address(value):
    if type(value) is not int or not 0 <= value < 2**32:
        raise ValueError('Hook address must be a 32-bit integer')
    return value


def _markers(paths, marker):
    result = set()
    files = set()
    for supplied in paths:
        path = Path(supplied)
        files.update(path.rglob('*.c') if path.is_dir() else (path,))
    pattern = re.compile(r'^\s*' + marker + r'\s*\(\s*([A-Za-z_][A-Za-z_0-9]*)\s*\)', re.M)
    for path in sorted(files):
        source = mask_noncode(strip_disabled(path.read_text(encoding='utf-8')))
        source = '\n'.join(line for line in source.splitlines() if not line.lstrip().startswith('#'))
        result.update(pattern.findall(source))
    return result


def implemented_names(paths):
    return _markers(paths, 'HLE_EXPORT')


def imported_variables(paths):
    return _markers(paths, 'HLE_IMPORT_VAR')


def wanted_originals(paths):
    return _markers(paths, 'HLE_ORIGINAL')


def _load_kind(path, kind):
    data = json.loads(Path(path).read_text(encoding='utf-8'))
    if not isinstance(data, dict) or not isinstance(data.get('symbols', []), list):
        raise ValueError('Expected a symbol catalog object')
    result = []
    for symbol in data.get('symbols', []):
        if not isinstance(symbol, dict):
            raise ValueError('Expected a symbol record')
        if symbol.get('kind') == kind:
            _name(symbol.get('name'))
            _address(symbol.get('address'))
            result.append(symbol)
    return result


def load_symbols(path):
    return _load_kind(path, 'function')


def load_variables(path):
    return _load_kind(path, 'variable')


def _by_name(symbols):
    indexed = {}
    for symbol in symbols:
        indexed.setdefault(_name(symbol['name']), set()).add(_address(symbol['address']))
    return indexed


def resolve_variables(names, symbols):
    indexed, values, notes = _by_name(symbols), {}, []
    for name in sorted(names):
        _name(name)
        candidates = indexed.get(name, set())
        values[name] = next(iter(candidates)) if len(candidates) == 1 else 0
        if len(candidates) != 1:
            reason = 'not found in this XBE' if not candidates else f'{len(candidates)} different addresses, left 0'
            notes.append(f'import {name}: {reason}')
    return values, notes


def stack_cleanup(code, start_va):
    _address(start_va)
    if start_va + len(code) > 2**32:
        return None
    counts = set()
    for instruction in make_decoder().disasm(code, start_va):
        if instruction.mnemonic == 'ret':
            counts.add(instruction.operands[0].imm if instruction.operands else 0)
    return next(iter(counts)) if len(counts) == 1 else None


def parse_ltcg(name):
    """Read the XbSymbolDatabase register-position name protocol strictly."""
    _name(name)
    prefix, separator, suffix = name.partition('__LTCG_')
    if not separator:
        return name, {}
    base, separator, stack = prefix.rpartition('_')
    if not separator or not base or not stack.isdecimal() or int(stack) > 65535:
        return name, {}
    registers = {}
    for field in suffix.split('_'):
        register, position = field[:3], field[3:]
        if register not in _REGISTERS or not position.isdecimal():
            return name, {}
        position = int(position)
        if not 1 <= position <= MAX_REGISTER_FRAME_ARGUMENTS or position in registers or register in registers.values():
            return name, {}
        registers[position] = register
    return base, registers


def _abi(pop, registers):
    if type(pop) is not int or not 0 <= pop <= 65535:
        return False
    if not registers:
        return True
    total = pop // 4 + len(registers)
    return (pop % 4 == 0 and total <= MAX_REGISTER_FRAME_ARGUMENTS and len(set(registers.values())) == len(registers)
            and all(type(position) is int and 1 <= position <= total and register in _REGISTERS
                    for position, register in registers.items()))


def plan(symbols, implemented, known_addrs, manual, cleanup):
    indexed = _by_name(symbols)
    variants = {}
    for full, addresses in indexed.items():
        base, registers = parse_ltcg(full)
        if registers:
            variants.setdefault(base, []).extend((address, registers, full) for address in sorted(addresses))
    proposed, notes = {}, []
    for name in sorted(implemented):
        _name(name)
        registers, candidates = {}, sorted(indexed.get(name, ()))
        if not candidates and name in variants:
            options = variants[name]
            if len(options) != 1:
                notes.append(f'{name}: {len(options)} LTCG variants, skipped')
                continue
            address, registers, full = options[0]
            candidates = [address]
            notes.append(f'{name}: matched {full} (LTCG)')
        if len(candidates) != 1:
            reason = f'{len(candidates)} matches, skipped' if candidates else 'not found in this XBE'
            notes.append(f'{name}: {reason}')
            continue
        address = candidates[0]
        if address in manual:
            notes.append(f'{name}: 0x{address:08X} is hand-written in the title, which wins')
            continue
        if address not in known_addrs:
            notes.append(f'{name}: 0x{address:08X} is not a detected function')
            continue
        pop = cleanup(address)
        if not _abi(pop, registers):
            notes.append(f'{name}: 0x{address:08X} has no single valid return ABI, skipped')
            continue
        proposed.setdefault(address, []).append((name, pop, registers))
    replacements = {}
    for address, choices in sorted(proposed.items()):
        if len(choices) == 1:
            replacements[address] = choices[0]
        else:
            notes.append(f'0x{address:08X}: conflicting implemented names, skipped')
    return replacements, notes


def original_name(address):
    return f'sub_{_address(address):08X}_hle_original'


def keep_originals(replace, wanted):
    return {address: original_name(address) for address, (name, _, _) in replace.items() if name in wanted}


def render_thunks(replace, variables=None, originals=None):
    lines = ['/* Project-generated runtime hooks; addresses and return cleanup come from the title. */',
             '#include "recomp_types.h"', '']
    for address, (name, pop, registers) in sorted(replace.items()):
        _address(address)
        _name(name)
        if not _abi(pop, registers):
            raise ValueError('Invalid hook return/register ABI')
        lines.append(f'void hle_{name}(void);')
        if not registers:
            body = f'hle_{name}(); g_esp += {pop + 4};'
            comment = ''
        else:
            count, stack_index, assignments = pop // 4 + len(registers), 0, []
            for position in range(1, count + 1):
                register = registers.get(position)
                value = 'g_' + register if register else f'RECOMP_HLE_STACK({stack_index})'
                if not register:
                    stack_index += 1
                assignments.append(f'a[{position - 1}] = {value};')
            body = (f'RECOMP_HLE_LTCG_CALL({count}, hle_{name}, {pop + 4}) {{ '
                    + ' '.join(assignments) + ' } RECOMP_HLE_LTCG_END')
            comment = ' /* ' + ', '.join(f'arg{p} in {r}' for p, r in sorted(registers.items())) + ' */'
        lines.append(f'void sub_{address:08X}(void) {{ {body} }}{comment}')
        lines.append('')
    for name, address in sorted((variables or {}).items()):
        lines.append(f'const uint32_t hle_var_{_name(name)} = 0x{_address(address):08X}u;')
    for name, address in sorted((originals or {}).items()):
        _name(name)
        target = original_name(address) if address is not None else '0'
        if address is not None:
            lines.append(f'void {target}(void);')
        lines.append(f'void (*const hle_original_{name})(void) = {target};')
    return '\n'.join(lines) + '\n'
