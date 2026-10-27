"""Project command line for function selection, translation and hook publication.

The runtime directory supplies data, implementations to scan and the current
types header. No Python module is imported from that directory.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys

import translation_layout as layout
import translation_hooks as hooks
from translation_function import BatchTranslator
from translation_names import identifier
from c_source_scan import scan, mask_noncode, strip_disabled
from generation_output import generate_header, print_stats, write_summary, write_if_changed

DEFAULT_RUNTIME = None


def _address(value):
    if isinstance(value, dict):
        value = value['start'] if 'start' in value else value['address']
    address = int(value, 16) if isinstance(value, str) else value
    if type(address) is not int or not 0 <= address < 2**32:
        raise ValueError('Expected a 32-bit guest address')
    return address


def _json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def _load_addrs(path):
    records = _json(path) if path else []
    if not isinstance(records, (list, dict)):
        raise ValueError('Expected an address list or address-to-name object')
    return {_address(value) for value in records}


def _parse_force_returns(items):
    result = {}
    for item in items or ():
        address, separator, value = item.partition('=')
        if not separator:
            raise ValueError('--force-return requires ADDR=VALUE')
        result[_address(int(address, 0))] = int(value, 0) & 0xffffffff
    return result


def find_data_files(disasm_dir=None, func_id_dir=None, abi_dir=None, overrides=None, *, runtime=None):
    tools = Path(runtime or DEFAULT_RUNTIME or Path.cwd()) / 'tools'
    sources = {'functions': (disasm_dir, 'disasm', 'functions'),
               'labels': (disasm_dir, 'disasm', 'labels'),
               'summary': (disasm_dir, 'disasm', 'summary'),
               'identified': (func_id_dir, 'func_id', 'identified_functions'),
               'abi': (abi_dir, 'abi_analysis', 'abi_functions')}
    result = {}
    for key, (directory, stage, filename) in sources.items():
        candidate = Path((overrides or {}).get(key) or
                         (Path(directory or tools / stage / 'output') / (filename + '.json')))
        result[key] = str(candidate) if candidate.is_file() else None
        if result[key] is None and key != 'summary':
            print(f'WARNING: {key} not found at {candidate}', file=sys.stderr)
    return result


def check_data_matches_binary(xbe_path, summary_path):
    # This is a legacy name check. The production analysis cache separately
    # verifies content hashes; a matching basename alone is not image identity.
    if not summary_path:
        return
    recorded = _json(summary_path).get('binary')
    basename = lambda value: re.split(r'[/\\]', str(value))[-1].casefold()
    if recorded and basename(recorded) != basename(xbe_path):
        raise ValueError(f'Disassembly names {recorded!r}, not {str(xbe_path)!r}; regenerate matching analysis')


def game_categories(classification_db):
    return {'unknown', 'crt', 'data_init'} | {
        row['category'] for row in classification_db.values()
        if isinstance(row.get('category'), str) and row['category'].startswith('game_')}


def list_categories(translator):
    counts = Counter(translator.classification_db.get(address, {}).get('category', 'unknown')
                     for address in translator.func_db)
    print(f'Function categories ({len(translator.func_db)} total):')
    for name, count in sorted(counts.items(), key=lambda item: (-item[1], item[0])):
        print(f'{name:<30} {count:8d}')


def _report_unimplemented(stats):
    counts = stats.get('unimplemented') or {}
    if not counts:
        return
    print(f'{sum(map(len, counts.values()))} instruction(s) unimplemented, emitted as no-op comments '
          f'({len(counts)} distinct mnemonics):', file=sys.stderr)
    for name, addresses in sorted(counts.items(), key=lambda item: (-len(item[1]), item[0]))[:20]:
        example = f'0x{min(addresses):08X}' if addresses else 'none'
        print(f'  {name}: {len(addresses)}; example {example}', file=sys.stderr)
    if len(counts) > 20:
        print(f'  {len(counts)-20} further mnemonics', file=sys.stderr)


def _parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xbe_path')
    parser.add_argument('--runtime', help='Runtime data/header root; no toolkit Python is loaded')
    parser.add_argument('--types-header', help='Explicit shared C runtime types header')
    for option in ('game-name', 'gen-dir', 'disasm-dir', 'func-id-dir', 'abi-dir',
                   'functions', 'labels', 'identified', 'abi', 'icall-sites',
                   'manual-functions', 'hle-symbols', 'trace-functions', 'seh-prolog', 'seh-epilog'):
        parser.add_argument('--'+option)
    parser.add_argument('-o', '--output-dir')
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument('-c', '--category')
    selection.add_argument('--game-only', action='store_true', help='Game categories plus unknown, CRT and data initializers')
    selection.add_argument('--all', action='store_true')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('-f', '--function', help='One hexadecimal guest address')
    for flag in ('list-categories', 'header', 'only-hle-thunks'):
        mode.add_argument('--'+flag, action='store_true')
    parser.add_argument('-n', '--max-funcs', type=int)
    parser.add_argument('--split', type=int, default=0)
    parser.add_argument('--hle-impl', action='append')
    parser.add_argument('--exclude-manual', nargs='?', const='')
    parser.add_argument('--force-return', action='append', default=[])
    for flag in ('skip-binary-check', 'trace-all-entries'):
        parser.add_argument('--'+flag, action='store_true')
    parser.add_argument('-v', '--verbose', action='store_true')
    return parser


def _manual_overrides(translator, args, runtime):
    declared, available = set(), set()
    if args.manual_functions:
        records = _json(args.manual_functions)
        declared = _load_addrs(args.manual_functions)
        if isinstance(records, dict):
            for key, name in records.items():
                address = _address(key)
                if address not in translator.func_db:
                    print(f'WARNING: manual address 0x{address:08X} is absent from analysis', file=sys.stderr)
                elif name:
                    translator.func_db[address]['name'] = identifier(address, name)
    available.update(declared)
    if args.exclude_manual is not None:
        source = args.exclude_manual or runtime / 'src/game/recomp/recomp_manual.c'
        if not Path(source).is_file():
            raise ValueError(f'Manual source does not exist: {source}')
        defined, wrappers, referenced = scan(source)
        known = set(translator.func_db)
        for address in (referenced & known) - wrappers:
            translator.func_db[address]['name'] = f'sub_{address:08X}'
        for address in wrappers & known:
            translator.func_db[address].update(name=f'sub_{address:08X}_gen', call_name=f'sub_{address:08X}')
        declared.update((defined - wrappers) & known)
        available.update(defined & known)
    names = {identifier(address, info.get('call_name') or info.get('name'))
             for address in available if (info := translator.func_db.get(address)) is not None}
    return declared, names


def _hook_plan(translator, args, runtime, manual):
    paths = [Path(path) for path in (args.hle_impl or [runtime / 'src/hle']) if Path(path).exists()]
    variables = {name: 0 for name in hooks.imported_variables(paths)}
    wanted = hooks.wanted_originals(paths)
    replacements = {}
    if args.hle_symbols:
        symbols = hooks.load_symbols(args.hle_symbols)
        variables, variable_notes = hooks.resolve_variables(variables, hooks.load_variables(args.hle_symbols) + symbols)
        def cleanup(address):
            info = translator.func_db.get(address, {})
            end = info.get('end')
            end = int(end, 16) if isinstance(end, str) else end if end is not None else address
            if type(end) is not int or not 0 <= end <= 2**32:
                raise ValueError('Invalid function end for hook cleanup')
            data = translator.translator._read_func_bytes(address, end) if end > address else None
            return hooks.stack_cleanup(data, address) if data else None
        replacements, notes = hooks.plan(symbols, hooks.implemented_names(paths), set(translator.func_db), manual, cleanup)
        for address in replacements:
            translator.func_db[address]['name'] = f'sub_{address:08X}'
        for note in variable_notes + notes:
            print('  hle: '+note, file=sys.stderr)
    return replacements, variables, wanted


def _definitions(directory):
    names = set()
    for path in sorted(directory.glob('*.c')):
        if path.name == 'recomp_hle.c':
            continue
        code = mask_noncode(strip_disabled(path.read_text(encoding='utf-8')))
        # Generated definitions have a fixed signature. Ignore comments, strings,
        # literal-disabled branches, prototypes and preprocessor definitions.
        ordinary = []
        continued = False
        for line in code.splitlines():
            if continued or line.lstrip().startswith('#'):
                continued = line.rstrip().endswith('\\')
            else:
                ordinary.append(line)
        code = '\n'.join(ordinary)
        names.update(re.findall(r'^\s*void\s+([A-Za-z_]\w*)\s*\(\s*void\s*\)\s*\{', code, re.M))
    return names


def _refresh(translator, directory, replacements, variables, wanted, manual_names):
    definitions = _definitions(directory)
    active = {address: value for address, value in replacements.items() if f'sub_{address:08X}' not in definitions}
    dispatch = directory / 'recomp_dispatch.c'
    if dispatch.is_file():
        code = mask_noncode(strip_disabled(dispatch.read_text(encoding='utf-8')))
        needed = set(re.findall(r'\(recomp_func_t\)\s*([A-Za-z_]\w*)', code))
        missing = needed - definitions - manual_names - {f'sub_{address:08X}' for address in active}
        if missing:
            raise ValueError('Hook refresh would leave dispatch symbols undefined: '+', '.join(sorted(missing)[:8]))
    originals = {name: None for name in wanted}
    for address, (name, *_rest) in active.items():
        if name in wanted and f'sub_{address:08X}_hle_original' in definitions:
            originals[name] = address
    # Validate both inputs before the first publication; each write is atomic,
    # but these two files do not form a filesystem transaction.
    types = translator._types()
    output = hooks.render_thunks(active, variables, originals)
    write_if_changed(directory / 'recomp_hle.c', output)
    write_if_changed(directory / 'recomp_types.h', types)
    print(f'Refreshed hooks: {len(active)} active, {len(replacements)-len(active)} deferred until translation', file=sys.stderr)


def _run(args):
    if args.split < 0 or (args.max_funcs is not None and args.max_funcs < 0):
        raise ValueError('Split size and maximum function count cannot be negative')
    runtime = Path(args.runtime or DEFAULT_RUNTIME or Path.cwd()).resolve()
    output = Path(args.output_dir or runtime / 'tools/recomp/output')
    files = find_data_files(args.disasm_dir, args.func_id_dir, args.abi_dir,
                           {key: getattr(args, key) for key in ('functions', 'labels', 'identified', 'abi')}, runtime=runtime)
    if not files['functions']:
        raise ValueError('functions.json is required; generate matching analysis first')
    if not args.skip_binary_check:
        check_data_matches_binary(args.xbe_path, files['summary'])
    layout.configure_from_xbe(args.xbe_path)
    if args.game_name:
        layout.set_game_name(args.game_name)
    if args.verbose:
        print(f'Configured {len(layout.SECTIONS)} executable sections', file=sys.stderr)
    translator = BatchTranslator(args.xbe_path, files['functions'], files['labels'], files['identified'], files['abi'],
        output_dir=str(output), trace_functions=_load_addrs(args.trace_functions), trace_all_entries=args.trace_all_entries,
        force_returns=_parse_force_returns(args.force_return),
        seh_prolog=_address(args.seh_prolog) if args.seh_prolog else None,
        seh_epilog=_address(args.seh_epilog) if args.seh_epilog else None,
        icall_sites_json_path=args.icall_sites or str(output / 'icall_sites.json'),
        types_header=args.types_header or runtime / 'templates/runtime/recomp_types.h')
    if args.list_categories:
        list_categories(translator)
        return 0
    if args.function:
        code = translator.translate_single(_address(args.function))
        if not code:
            raise ValueError('The selected function could not be translated')
        print(code)
        return 0
    categories = {args.category} if args.category else None if args.all or (args.header and not args.game_only) else game_categories(translator.classification_db)
    functions = translator.get_functions_by_category(categories=categories)
    if args.header:
        generate_header(functions, output / 'recomp_functions.h', translator.abi_db, layout.banner_name(translator.title))
        return 0
    if args.split or args.only_hle_thunks:
        if args.max_funcs:
            functions = functions[:args.max_funcs]
        directory = Path(args.gen_dir or runtime / 'src/game/recomp/gen')
        manual, manual_names = _manual_overrides(translator, args, runtime)
        replacements, variables, wanted = _hook_plan(translator, args, runtime, manual)
        if args.only_hle_thunks:
            _refresh(translator, directory, replacements, variables, wanted, manual_names)
            return 0
        kept = hooks.keep_originals(replacements, wanted)
        lifted = {address for address, _ in functions} - translator.translator.owned_function_starts
        kept = {address: name for address, name in kept.items() if address in lifted}
        originals = {name: None for name in wanted}
        for address in kept:
            originals[replacements[address][0]] = address
        stats = translator.translate_batch_split(functions, str(directory), chunk_size=args.split,
            verbose=args.verbose, manual=manual | set(replacements), keep_bodies=kept)
        write_if_changed(directory / 'recomp_hle.c', hooks.render_thunks(replacements, variables, originals))
    else:
        stats = translator.translate_batch(functions, max_funcs=args.max_funcs, verbose=args.verbose)
    print_stats(stats)
    _report_unimplemented(stats)
    print('Summary: '+str(write_summary(stats, output)), file=sys.stderr)
    return 0


def main(argv=None):
    args = _parser().parse_args(argv)
    previous_name = layout.GAME_NAME, layout._game_name_explicit
    try:
        return _run(args)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        return 1
    finally:
        layout.GAME_NAME, layout._game_name_explicit = previous_name


if __name__ == '__main__':
    raise SystemExit(main())
