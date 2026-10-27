"""Atomic generated-text publication and translation reports for this project."""
import json
import os
from pathlib import Path
import re
import tempfile

C_KEYWORDS = frozenset('auto break case char const continue default do double else enum extern '
    'float for goto if inline int long register restrict return short signed sizeof static struct '
    'switch typedef union unsigned void volatile while _Alignas _Alignof _Atomic _Bool _Complex '
    '_Generic _Imaginary _Noreturn _Static_assert _Thread_local'.split())


def write_if_changed(path, text):
    """Preserve unchanged files and timestamps; publish changed text atomically.

    Text comparison accepts the host's existing newline convention. New output
    uses the native text convention, matching the C compiler's ordinary files.
    A failed temporary write or replacement leaves the previous file intact.
    """
    if not isinstance(text, str):
        raise TypeError('Generated output must be text')
    target = Path(path).resolve()
    if target.is_file() and target.read_text(encoding='utf-8') == text:
        return False
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', dir=target.parent,
                                         prefix='.generated-', suffix='.tmp', delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(text)
            stream.flush()
            os.fsync(stream.fileno())
        if target.exists():
            os.chmod(temporary, target.stat().st_mode)
        temporary.replace(target)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return True


def write_summary(stats, output_dir):
    path = Path(output_dir) / 'summary.json'
    write_if_changed(path, json.dumps(stats, indent=2))
    return str(path)


def print_stats(stats):
    """Present counts without interpreting successful translation as execution."""
    batches = {'translation': stats} if 'total' in stats else stats
    totals = dict(total=0, translated=0, failed=0, total_lines=0)
    for name, record in sorted(batches.items()):
        for key in totals:
            totals[key] += record.get(key, 0)
        print(f"{name}: {record.get('translated', 0)}/{record.get('total', 0)} functions; "
              f"{record.get('failed', 0)} failed; {record.get('total_lines', 0)} lines")
    if len(batches) > 1:
        print(f"Total: {totals['translated']}/{totals['total']} functions; "
              f"{totals['failed']} failed; {totals['total_lines']} lines")


def generate_header(func_list, output_path, abi_db=None, title='Xbox title'):
    """Emit explicit ABI hints for the optional declaration-report interface.

    The executable translator's register-state ABI is separate from this report.
    Inputs describe inferred signatures; this function does not infer an ABI.
    """
    abi_db = abi_db or {}
    title = ' '.join(str(title).replace('*/', '* /').splitlines())
    lines = [f'/* {title}: inferred function declarations. */',
             '#ifndef RECOMP_FUNCTIONS_H', '#define RECOMP_FUNCTIONS_H',
             '#include "recomp_types.h"', '']
    names = set()
    for location, record in sorted(func_list):
        name = record.get('name', f'sub_{location:08X}')
        if (not isinstance(name, str) or not re.fullmatch(r'[A-Za-z_]\w*', name, re.ASCII)
                or name in names or name in C_KEYWORDS):
            raise ValueError('Declaration names must be distinct C identifiers')
        names.add(name)
        abi = abi_db.get(location, {})
        count = abi.get('estimated_params', 0)
        if isinstance(count, bool) or not isinstance(count, int) or not 0 <= count <= 256:
            raise ValueError('Declaration argument count must be between 0 and 256')
        hint = abi.get('return_hint', 'int_or_void')
        result = ('float' if hint in ('float', 'float_sse') else 'int' if hint == 'int_zero'
                  else 'void' if count == 0 and hint == 'int_or_void' else 'uint32_t')
        arguments = ['void *this_ptr'] if abi.get('calling_convention') in ('thiscall', 'thiscall_cdecl') else []
        arguments += [f'uint32_t a{number + 1}' for number in range(count)]
        lines.append(f"{result} {name}({', '.join(arguments) or 'void'});")
    lines += ['', '#endif /* RECOMP_FUNCTIONS_H */', '']
    write_if_changed(output_path, '\n'.join(lines))
    return output_path
