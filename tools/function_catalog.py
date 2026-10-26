"""Project function classification from bounded code, strings and call evidence.

The input analyzer owns function discovery. This tool enriches those records;
pointer-table candidates are separate evidence, never guessed function extents.
No CRT names are asserted from byte prefixes. Classification is diagnostic and
does not replace an executable body with a native implementation.
"""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import sys

from analysis_output import atomic_text
from function_categories import (source_identifiers, direct_source_labels,
                                 contextual_labels, propagate_categories)
from function_evidence import (FunctionRanges, address, collect_operands,
                               find_pointer_tables, scalar_initializers,
                               entry_blocks, numeric_observations)
from recomp_image import DATA_SECTION_NAMES, load_image, read_code_window

DEFAULT_FUNCTIONS_JSON = 'tools/disasm/output/functions.json'
DEFAULT_STRINGS_JSON = 'tools/disasm/output/strings.json'
DEFAULT_XREFS_JSON = 'tools/disasm/output/xrefs.json'
DEFAULT_OUTPUT_DIR = 'tools/func_id/output'
OUTPUT_NAMES = ('identified_functions.json', 'rw_modules.json', 'crt_functions.json',
                'pointer_tables.json', 'entry_blocks.json', 'summary.json')


def read_records(path):
    path = Path(path)
    if path.stat().st_size > 128 * 1024 * 1024:
        raise ValueError(f'Catalog input exceeds 128 MiB: {path.name}')
    records = json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(records, list) or len(records) > 2_000_000 or any(
            not isinstance(record, dict) for record in records):
        raise ValueError('Catalog input must contain a bounded array of objects')
    return records


def validate_inputs(image, functions, strings, xrefs):
    ranges = FunctionRanges(functions)
    result = []
    for start in ranges.starts:
        source = ranges.records[start]
        size = source['end'] - start
        section = image.get_section_at_va(start)
        if section is None or not section.executable or len(read_code_window(image, start, size)) != size:
            raise ValueError(f'Function 0x{start:08X} is not fully backed executable code')
        name = source.get('name', f'sub_{start:08X}')
        if not isinstance(name, str):
            raise ValueError('Function names must be text')
        calls = source.get('calls_to', [])
        if not isinstance(calls, list):
            raise ValueError('Function calls must be an array')
        result.append(dict(start=f'0x{start:08X}', end=f"0x{source['end']:08X}", size=size,
            name=name, section=section.name, calls_to=[address(target) for target in calls]))
    for record in strings:
        location = address(record['address'])
        if not isinstance(record.get('string'), str) or image.get_section_at_va(location) is None:
            raise ValueError('Strings require text and a mapped address')
    for record in xrefs:
        address(record['from'])
        address(record['to'])
        if not isinstance(record.get('type'), str):
            raise ValueError('Cross references require a type')
    return result


def merge_data_references(image, functions, references, xrefs):
    ranges = FunctionRanges(functions)
    merged = defaultdict(set, {key: set(values) for key, values in references.items()})
    for record in xrefs:
        if record['type'] != 'data_read':
            continue
        target = address(record['to'])
        section = image.get_section_at_va(target)
        owner = ranges.containing(address(record['from']))
        if owner is not None and section and (not section.executable or section.name in DATA_SECTION_NAMES):
            merged[target].add(owner)
    return {key: sorted(values) for key, values in sorted(merged.items())}


def classify(image, functions, strings, xrefs):
    functions = validate_inputs(image, functions, strings, xrefs)
    operands = collect_operands(image, functions)
    references = merge_data_references(image, functions, operands.data_references, xrefs)
    sources = source_identifiers(strings)
    labels = contextual_labels(image, functions, strings, references)
    labels.update(direct_source_labels(sources, references))
    initializers = scalar_initializers(image, functions)
    for start, facts in initializers.items():
        labels.setdefault(start, dict(category='data_init', confidence=0.95,
            method='scalar_initialization', stub_type='scalar_sse', evidence=facts))
    # Only subsystem/source hypotheses propagate, never an initializer shape.
    seeds = {key: value for key, value in labels.items() if value['category'] != 'data_init'}
    for start, label in propagate_categories(functions, seeds).items():
        labels.setdefault(start, label)
    tables = find_pointer_tables(image, functions, operands)
    memberships = defaultdict(list)
    for table in tables:
        for slot, target in enumerate(table['entries']):
            memberships[target].append(dict(address=table['address'], slot=slot))
        for constructor in table['constructors']:
            labels.setdefault(constructor, dict(category='game_vtable', confidence=0.7,
                method='pointer_table_store'))
    numeric = numeric_observations(image, functions)
    enriched = []
    for record in functions:
        start = address(record['start'])
        entry = {key: value for key, value in record.items() if key != 'calls_to'}
        fallback = (dict(category='game_vtable', confidence=0.6, method='pointer_table_entry')
                    if start in memberships else dict(category='unknown', confidence=0.0, method='none'))
        entry.update(labels.get(start, fallback))
        if start in memberships:
            entry['pointer_tables'] = memberships[start]
        if start in numeric:
            entry['numeric_observations'] = numeric[start]
        enriched.append(entry)
    modules = {}
    for source in sources:
        key = source['path']
        module = modules.setdefault(key, dict(path=key, category=source['category'],
            addresses=[], functions=[]))
        module['addresses'].append(f"0x{source['address']:08X}")
        module['functions'].extend(f'0x{value:08X}' for value in references.get(source['address'], ()))
    for module in modules.values():
        module['addresses'] = sorted(set(module['addresses']))
        module['functions'] = sorted(set(module['functions']))
        module['num_functions'] = len(module['functions'])
    counts = Counter(entry['category'] for entry in enriched)
    totals = dict(renderware=sum(count for name, count in counts.items() if name.startswith('rw_')),
        crt=counts['crt'], data_init=counts['data_init'],
        game_classified=sum(count for name, count in counts.items() if name.startswith('game_')),
        vtable_methods=sum(entry['method'].startswith('pointer_table') for entry in enriched),
        unknown=counts['unknown'])
    summary = dict(total_functions=len(enriched), classification=totals,
        percentages={name: round(count * 100 / len(enriched), 1) if enriched else 0
                     for name, count in totals.items()}, by_category=dict(sorted(counts.items())),
        by_method=dict(sorted(Counter(entry['method'] for entry in enriched).items())),
        rw_modules=modules, rw_module_count=len(modules), pointer_tables=len(tables),
        policy='bounded_evidence_v1', crt_identity='No symbol identity inferred from byte prefixes')
    return dict(zip(OUTPUT_NAMES, (enriched, modules, [], tables,
                                  entry_blocks(image, tables, functions), summary)))


def run(xbe_path, functions_path=None, strings_path=None, xrefs_path=None,
        output_dir=None, verbose=False):
    paths = [Path(xbe_path), Path(functions_path or DEFAULT_FUNCTIONS_JSON),
             Path(strings_path or DEFAULT_STRINGS_JSON), Path(xrefs_path or DEFAULT_XREFS_JSON)]
    output = Path(output_dir or DEFAULT_OUTPUT_DIR)
    inputs = {path.resolve() for path in paths}
    if any((output / name).resolve() in inputs for name in OUTPUT_NAMES):
        raise ValueError('Catalog output must not replace an input')
    image = load_image(paths[0])
    if verbose:
        print('Project classification: collecting bounded instruction and reference evidence', flush=True)
    documents = classify(image, *(read_records(path) for path in paths[1:]))
    # Validate/serialize everything before publishing. Each file replacement is
    # atomic; the successful CLI exit, not a partially updated directory, is the
    # handoff contract for the sequential generation driver.
    serialized = {name: json.dumps(value, indent=2, ensure_ascii=True) + '\n'
                  for name, value in documents.items()}
    for name, text in serialized.items():
        with atomic_text(output / name, output) as stream:
            stream.write(text)
    summary = documents['summary.json']
    if verbose:
        print(f"Project classification: {summary['total_functions']} measured functions; "
              f"{summary['pointer_tables']} corroborated pointer arrays", flush=True)
        print(json.dumps(summary['by_category'], sort_keys=True), flush=True)
    return summary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xbe_path')
    for option in ('functions', 'strings', 'xrefs'):
        parser.add_argument('--' + option)
    parser.add_argument('-o', '--output')
    parser.add_argument('-v', '--verbose', action='store_true')
    args = parser.parse_args(argv)
    try:
        run(args.xbe_path, args.functions, args.strings, args.xrefs, args.output, args.verbose)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f'Classification failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
