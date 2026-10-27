"""Project multiply/divide lowering with sampled inputs and checked quotients.

Only multiplication's defined CF/OF are published. Division preserves the
existing snapshots (its flags are undefined) and faults before committing any
result. Guest SEH dispatch remains a separate runtime replacement task.
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

from translation_integer import _width
from translation_operands import _fmt_operand_read, _fmt_operand_write


def _signed(name, bits):
    return f'((int64_t){name} - (({name} & 0x{1 << (bits-1):X}u) ? 0x{1 << bits:X}ll : 0ll))'


def _pair_write(bits, low, high=None):
    if bits == 8:
        value = low if high is None else f'(({low}) & 0xFFu) | ((({high}) & 0xFFu) << 8)'
        return [f'SET_LO16(eax, {value});']
    if high is None:
        high = f'((uint64_t)({low}) >> {bits})'
    if bits == 16:
        return [f'SET_LO16(eax, {low});', f'SET_LO16(edx, {high});']
    return [f'eax = (uint32_t)({low});', f'edx = (uint32_t)({high});']


def _multiply(context, operands, signed):
    count = len(operands)
    if count not in ((1,2,3) if signed else (1,)):
        raise ValueError('Invalid multiply operand count')
    bits, mask = _width(operands[:2] if count > 1 else operands, 2 if count > 1 else 1)
    if count > 1 and (bits == 8 or operands[0].type != 'reg'):
        raise ValueError('Explicit multiply destination must be a word/dword register')
    if count == 3 and operands[2].type != 'imm':
        raise ValueError('Three-operand multiply requires an immediate')
    if count == 1:
        left, right = 'eax', _fmt_operand_read(operands[0])
    elif count == 2:
        left, right = map(_fmt_operand_read, operands)
    else:
        left, right = map(_fmt_operand_read, operands[1:])
    lines = [f'{{ uint32_t _product_left = (uint32_t)({left}) & {mask};',
             f'uint32_t _product_right = (uint32_t)({right}) & {mask};']
    if signed:
        lines.append(f'int64_t _product_wide = {_signed("_product_left", bits)} * {_signed("_product_right", bits)};')
        carry = f'(_product_wide < -0x{1 << (bits-1):X}ll || _product_wide > 0x{(1 << (bits-1))-1:X}ll)'
    else:
        lines.append('uint64_t _product_wide = (uint64_t)_product_left * _product_right;')
        carry = f'(_product_wide > {mask})'
    if getattr(context, 'needs_cf', False):
        lines.append(f'_cf = {carry}; /* multiplication defines CF == OF */')
    if count == 1:
        lines.extend(_pair_write(bits, '_product_wide'))
    else:
        lines.append(_fmt_operand_write(operands[0], '(uint32_t)_product_wide'))
    return [*lines, '}']


def imul(context, instruction, operands):
    return _multiply(context, operands, True)


def _fault(instruction, reason):
    return f'{{ g_ebp = ebp; tsfp_guest_divide_error(0x{instruction.address:X}u, {reason}u); }}'


def muldiv(context, instruction, operands, mnemonic):
    if mnemonic == 'mul':
        return _multiply(context, operands, False)
    if mnemonic not in ('div','idiv'):
        raise ValueError('Unsupported multiply/divide operation')
    bits, mask = _width(operands, 1)
    divisor = _fmt_operand_read(operands[0])
    raw = '(uint64_t)(eax & 0xFFFFu)' if bits == 8 else f'(((uint64_t)(edx & {mask}) << {bits}) | (eax & {mask}))'
    lines = [f'{{ uint32_t _product_divisor = (uint32_t)({divisor}) & {mask};',
             f'uint64_t _product_raw = {raw};',
             f'if (!_product_divisor) {_fault(instruction, 0)}']
    if mnemonic == 'idiv':
        lines += [f'unsigned _product_negative = (unsigned)(_product_raw >> {bits*2-1});',
                  f'unsigned _product_divisor_negative = _product_divisor >> {bits-1};',
                  'unsigned _product_quotient_negative = _product_negative ^ _product_divisor_negative;',
                  f'uint64_t _product_magnitude = (_product_negative ? 0ull - _product_raw : _product_raw) & 0x{(1 << (bits*2))-1:X}ull;',
                  f'uint32_t _product_denominator = (_product_divisor_negative ? 0u - _product_divisor : _product_divisor) & {mask};',
                  'uint64_t _product_quotient = _product_magnitude / _product_denominator;',
                  'uint64_t _product_remainder = _product_magnitude % _product_denominator;',
                  f'if (_product_quotient > (_product_quotient_negative ? 0x{1 << (bits-1):X}ull : 0x{(1 << (bits-1))-1:X}ull)) {_fault(instruction, 1)}',
                  'if (_product_quotient_negative) _product_quotient = 0ull - _product_quotient;',
                  'if (_product_negative) _product_remainder = 0ull - _product_remainder;']
    else:
        lines += ['uint64_t _product_quotient = _product_raw / _product_divisor;',
                  f'if (_product_quotient > {mask}) {_fault(instruction, 1)}',
                  'uint64_t _product_remainder = _product_raw % _product_divisor;']
    return [*lines, *_pair_write(bits, '_product_quotient', '_product_remainder'), '}']
