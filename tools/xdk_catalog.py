"""Project adapter for the separately licensed XbSymbolDatabase CLI protocol.

Signature matching belongs to that third-party library. This module owns CLI
selection, protocol decoding, supplemental symbol validation and publication.
"""
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import re
import subprocess
import sys

from analysis_output import atomic_text
from xbe_image import XBEParser

REPO = Path(__file__).resolve().parents[1]


def parse_params(text):
    result = []
    for field in text.split(','):
        field = field.strip()
        if not field:
            continue
        parts = field.split(None, 1)
        result.append(dict(type=parts[0], name=parts[1] if len(parts) == 2 else ''))
    return result


def parse_line(line):
    description, delimiter, number = line.strip().rpartition(' = 0x')
    if not delimiter or not re.fullmatch('[0-9A-Fa-f]{1,8}', number):
        return None
    library, delimiter, payload = description.partition('__')
    if not delimiter or not re.fullmatch('[A-Za-z0-9]{1,8}', library) or not payload:
        return None
    kind, _, body = payload.partition('__')
    record = dict(library=library, address=int(number, 16), kind='unknown')
    if kind == 'FUN':
        call, separator, prototype = body.partition('__')
        name, opening, arguments = prototype.partition('(')
        if (not separator or not re.fullmatch('[a-z]+', call) or not opening
                or not re.fullmatch('[A-Za-z_][A-Za-z0-9_@?$]*', name) or not arguments.endswith(')')):
            return None
        params = parse_params(arguments[:-1])
        record.update(kind='function', name=name, call=call, params=params,
                      stack_bytes=sum({'psh': 4, 'psh2': 8}.get(p['type'], 0) for p in params))
    else:
        name = body if kind in ('VAR', 'UNK') else payload
        if not name or any(ord(c) < 32 for c in name):
            return None
        record.update(name=name, kind='variable' if kind == 'VAR' else 'unknown')
    return record


def parse_output(text):
    return [record for line in text.splitlines() if (record := parse_line(line)) is not None]


def find_cli(explicit=None):
    selected = explicit or os.environ.get('XBSDB_CLI')
    if selected:
        return str(selected)
    filename = 'XbSymbolDatabaseCLI.exe' if os.name == 'nt' else 'XbSymbolDatabaseCLI'
    candidates = (Path(REPO) / 'third_party/XbSymbolDatabase/build').rglob(filename)
    ordered = sorted((path for path in candidates if path.is_file()),
                     key=lambda path: ('release' not in [part.lower() for part in path.parts], str(path)))
    return str(ordered[0]) if ordered else None


def title_id(xbe_path):
    try:
        return XBEParser(xbe_path).parse().certificate.title_id
    except (OSError, ValueError):
        return None


def _validate_symbol(value):
    if not isinstance(value, dict):
        raise ValueError('Supplemental symbols must be objects')
    address = value.get('address')
    if isinstance(address, bool) or not isinstance(address, int) or not 0 <= address < 2**32:
        raise ValueError('Symbol address must be a 32-bit integer')
    for field in ('library', 'name'):
        if not isinstance(value.get(field), str) or not value[field] or any(ord(c) < 32 for c in value[field]):
            raise ValueError(f'Invalid symbol {field}')
    if value.get('kind') not in ('function', 'variable', 'unknown'):
        raise ValueError('Invalid symbol kind')
    if value['kind'] == 'function':
        if not isinstance(value.get('call'), str) or not value['call']:
            raise ValueError('Function calling convention is required')
        size = value.get('stack_bytes')
        if isinstance(size, bool) or not isinstance(size, int) or not 0 <= size <= 0xFFFF or size % 4:
            raise ValueError('Function stack size must be a bounded multiple of four')
    return dict(value)


def load_extra(xbe_path, explicit=None):
    if explicit:
        path = Path(explicit)
    else:
        identifier = title_id(xbe_path)
        if identifier is None:
            return []
        path = Path(REPO) / 'config/extra_symbols' / f'{identifier:08X}.json'
        if not path.exists():
            return []
    if path.stat().st_size > 8 * 1024 * 1024:
        raise ValueError('Supplemental symbol file exceeds 8 MiB')
    data = json.loads(path.read_text(encoding='utf-8'))
    entries = data.get('symbols') if isinstance(data, dict) else None
    if not isinstance(entries, list):
        raise ValueError('Supplemental symbol file must contain a symbols array')
    return [_validate_symbol(entry) for entry in entries]


def merge_symbols(scanned, extra):
    # Preserve the database's aliases, but do not let supplemental guesses
    # displace a detected address or add duplicate supplemental addresses.
    result = list(scanned)
    occupied = {record['address'] for record in result}
    for record in extra:
        if record['address'] not in occupied:
            result.append(record)
            occupied.add(record['address'])
    return sorted(result, key=lambda record: record['address'])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xbe')
    parser.add_argument('--cli')
    parser.add_argument('--out')
    parser.add_argument('--extra')
    args = parser.parse_args(argv)
    try:
        xbe = Path(args.xbe).resolve()
        output = Path(args.out) if args.out else xbe.with_name(xbe.stem + '_xdk_symbols.json')
        if output.resolve() == xbe or (args.extra and output.resolve() == Path(args.extra).resolve()):
            raise ValueError('Symbol output cannot replace an input file')
        XBEParser(xbe).parse()  # Validate before invoking native code.
        extra = load_extra(xbe, args.extra)
        cli = find_cli(args.cli)
        if cli is None:
            raise ValueError('Set --cli or XBSDB_CLI to the XbSymbolDatabase executable')
        process = subprocess.run([cli, str(xbe), '-e'], capture_output=True,
                                 text=True, encoding='utf-8', errors='replace', timeout=300)
        if process.returncode:
            raise ValueError(f'Symbol scanner exited {process.returncode}: {process.stderr.strip()[:400]}')
        symbols = merge_symbols(parse_output(process.stdout), extra)
        if not symbols:
            raise ValueError('Symbol scanner produced no recognized entries')
        with atomic_text(output, output.parent) as stream:
            json.dump(dict(xbe=str(xbe), symbols=symbols), stream, indent=2)
            stream.write('\n')
        print(f'{len(symbols)} library symbols written to {output}')
        for (library, kind), count in sorted(Counter((s['library'], s['kind']) for s in symbols).items()):
            print(f'  {library}: {count} {kind}')
        return 0
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f'Symbol analysis failed: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
