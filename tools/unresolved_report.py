"""Describe unresolved addresses against an explicit executable and function map.

This is diagnostic evidence, not a function-discovery pass. A gap or a recorded
end does not establish a new function, its extent, or the identity of a library.
"""
import argparse
from bisect import bisect_right
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import sys

from branch_feedback import _address, _separate
from generation_output import write_if_changed
from recomp_image import load_image


def read_symbols(text):
    result = set()
    for number, line in enumerate(text.splitlines(), 1):
        value = line.strip()
        if not value or value.startswith('#'):
            continue
        if not re.fullmatch(r'(?:sub_|0[xX])?[0-9A-Fa-f]{1,8}', value):
            raise ValueError(f'Invalid unresolved symbol on line {number}')
        result.add(_address(value.removeprefix('sub_')))
    return sorted(result)


class FunctionMap:
    def __init__(self, records):
        if not isinstance(records, list):
            raise ValueError('Function metadata must be an array')
        self.entries, self.ends = defaultdict(list), defaultdict(list)
        unique = set()
        for record in records:
            start = _address(record['start'])
            if 'end' in record:
                end = _address(record['end'], endpoint=True)
            else:
                size = record.get('size', 0)
                if type(size) is not int or size < 0:
                    raise ValueError('Invalid function size')
                end = _address(start + size, endpoint=True)
            name = record.get('name', f'sub_{start:08X}')
            if end < start or not isinstance(name, str):
                raise ValueError('Invalid function extent or name')
            unique.add((start, end, name))
        self.records = sorted(unique)
        self.starts, self.prefix = [], []
        maximum = 0
        for record in self.records:
            start, end, _ = record
            self.entries[start].append(record)
            if end > start:
                self.ends[end].append(record)
            self.starts.append(start)
            maximum = max(maximum, end)
            self.prefix.append(maximum)

    def owners(self, address):
        index, result = bisect_right(self.starts, address) - 1, []
        while index >= 0 and self.prefix[index] > address:
            record = self.records[index]
            if record[0] < address < record[1]:
                result.append(record)
            index -= 1
        return sorted(result)


def _record(record):
    start, end, name = record
    return dict(start=f'0x{start:08X}', end=f'0x{end:08X}', name=name)


def describe(image, functions, addresses):
    rows = []
    for address in sorted({_address(value) for value in addresses}):
        section = image.get_section_at_va(address)
        entries = functions.entries.get(address, [])
        owners = functions.owners(address)
        ends = functions.ends.get(address, [])
        relation = ('known_entry' if entries else 'inside_recorded_body' if owners
                    else 'recorded_end' if ends else 'outside_recorded_bodies')
        offset = image.va_to_file_offset(address)
        mapping = ('section' if section else 'header' if offset is not None else 'unmapped')
        rows.append(dict(address=f'0x{address:08X}', mapping=mapping,
                         section=section.name if section else None,
                         section_executable=section.executable if section else None,
                         file_offset=offset, file_backed=offset is not None,
                         relation=relation, entries=list(map(_record, entries)),
                         containing_bodies=list(map(_record, owners)),
                         ending_bodies=list(map(_record, ends))))
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xbe', help='Executable supplying the actual section layout')
    parser.add_argument('--functions', required=True, help='Explicit function metadata for this image')
    parser.add_argument('--unresolved', required=True, help='One sub_HEX or hex address per line')
    parser.add_argument('--out', required=True, help='Diagnostic JSON; never a seed list')
    args = parser.parse_args(argv)
    try:
        _separate(args.out, [args.xbe, args.functions, args.unresolved])
        image = load_image(args.xbe)
        raw_functions = Path(args.functions).read_bytes()
        raw_symbols = Path(args.unresolved).read_bytes()
        functions = FunctionMap(json.loads(raw_functions))
        rows = describe(image, functions, read_symbols(raw_symbols.decode('utf-8')))
        counts = dict(sorted(Counter(row['relation'] for row in rows).items()))
        report = dict(schema_version=1, executable_sha256=hashlib.sha256(image.raw_data).hexdigest(),
                      functions_sha256=hashlib.sha256(raw_functions).hexdigest(),
                      unresolved_sha256=hashlib.sha256(raw_symbols).hexdigest(),
                      metadata_binding='User-selected function map; image identity not independently verified',
                      counts=counts, observations=rows)
        write_if_changed(args.out, json.dumps(report, indent=2) + '\n')
        print(f'Unresolved addresses: {len(rows)}; relations: {json.dumps(counts, sort_keys=True)}')
        print('Recorded bounds and section flags are evidence only; no function ends or seeds inferred.')
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f'Unresolved report: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
