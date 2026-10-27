"""Project ledger for observed indirect branches and conservative seed export.

Observed flags and per-site targets accumulate across runs. Seed filtering is
a separate output: a rejected candidate never erases its recorded observation.
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

from bisect import bisect_right
from collections import Counter
from pathlib import Path
import argparse
import json
import sys

from generation_output import write_if_changed

SEEN_RESOLVED, SEEN_UNRESOLVED = 1, 2
FLAG_NAMES = {1: 'resolved', 2: 'unresolved', 3: 'both'}
_PIPELINE = _layout_root / 'private/pipeline'
DEFAULT_DB = str(_PIPELINE / 'recomp/icall_targets.json')
DEFAULT_SITES_DB = str(_PIPELINE / 'recomp/icall_sites.json')
FUNCTIONS_PATH = str(_PIPELINE / 'disasm/functions.json')


def _address(value, endpoint=False):
    if isinstance(value, str):
        value = int(value, 16)
    if type(value) is not int or not 0 <= value <= (2**32 if endpoint else 2**32 - 1):
        raise ValueError('Branch address is outside the guest address space')
    return value


def _flags(value):
    if type(value) is not int or value not in FLAG_NAMES:
        raise ValueError('Branch observation flags must be 1, 2 or 3')
    return value


def _json(path, empty):
    path = Path(path)
    return json.loads(path.read_text(encoding='utf-8')) if path.exists() else empty


def parse_dump(path):
    result = {}
    for line in Path(path).read_text(encoding='utf-8', errors='replace').splitlines():
        fields = line.split()
        if len(fields) != 2 or line.lstrip().startswith('#'):
            continue
        try:
            address, flags = _address(fields[0]), _flags(int(fields[1], 10))
        except ValueError:
            continue
        result[address] = result.get(address, 0) | flags
    return result


def _merge_site(result, site, targets, saturated):
    previous, full = result.get(site, (set(), False))
    result[site] = (previous | targets, full or saturated)


def parse_sites_dump(path):
    result = {}
    for line in Path(path).read_text(encoding='utf-8', errors='replace').splitlines():
        fields = line.split()
        if len(fields) < 2 or line.lstrip().startswith('#'):
            continue
        try:
            site, saturated = _address(fields[0]), fields[-1] == '+'
            targets = {_address(value) for value in fields[1:-1 if saturated else None]}
        except ValueError:
            continue
        if site:
            _merge_site(result, site, targets, saturated)
    return result


def load_db(path):
    records, result = _json(path, []), {}
    if not isinstance(records, list):
        raise ValueError('Branch target ledger must be an array')
    for record in records:
        if isinstance(record, dict):
            address, flags = _address(record['start']), _flags(record.get('flags', SEEN_RESOLVED))
        else:
            address, flags = _address(record), SEEN_RESOLVED
        result[address] = result.get(address, 0) | flags
    return result


def save_db(path, targets):
    records = [dict(start=f'0x{_address(address):08X}', flags=_flags(flags),
                    seen=FLAG_NAMES[flags], source='icall-feedback') for address, flags in sorted(targets.items())]
    write_if_changed(path, json.dumps(records, indent=2) + '\n')


def load_sites(path=None):
    records, result = _json(path or DEFAULT_SITES_DB, {}), {}
    if not isinstance(records, dict):
        raise ValueError('Branch site ledger must be an object')
    for raw, record in records.items():
        site = _address(raw)
        if not site or not isinstance(record, dict) or not isinstance(record.get('targets', []), list):
            raise ValueError('Invalid branch site record')
        saturated = record.get('saturated', False)
        if type(saturated) is not bool:
            raise ValueError('Site saturation must be a boolean')
        _merge_site(result, site, {_address(value) for value in record.get('targets', [])}, saturated)
    return result


def save_sites(path, sites):
    records = {}
    for site, (targets, saturated) in sorted(sites.items()):
        if not _address(site) or type(saturated) is not bool:
            raise ValueError('Invalid site or saturation')
        records[f'0x{site:08X}'] = dict(targets=[f'0x{_address(value):08X}' for value in sorted(targets)],
                                     saturated=saturated)
    write_if_changed(path, json.dumps(records, indent=2) + '\n')


def sites_dump_beside(targets_dump):
    name = str(targets_dump)
    return name[:-len('targets.dump')] + 'sites.dump' if name.endswith('targets.dump') else name + '.sites'


def _collect_sites(db_path, dumps):
    result, seen = load_sites(db_path), 0
    for path in dumps:
        if not Path(path).exists():
            continue
        observed = parse_sites_dump(path)
        seen += len(observed)
        for site, (targets, saturated) in observed.items():
            _merge_site(result, site, targets, saturated)
    return result, seen


def merge_sites(db_path, dumps):
    result, seen = _collect_sites(db_path, dumps)
    if seen:
        save_sites(db_path, result)
    return result, seen


def load_function_starts(path=None):
    records = _json(path or FUNCTIONS_PATH, None)
    return None if records is None else {_address(record['start']) for record in records}


def load_function_bodies(path=None):
    records = _json(path or FUNCTIONS_PATH, None)
    if records is None:
        return None
    bodies = {}
    for record in records:
        start = _address(record['start'])
        if 'end' in record:
            end = _address(record['end'], endpoint=True)
        else:
            size = record.get('size', 0)
            if type(size) is not int or size < 0:
                raise ValueError('Invalid function size')
            end = _address(start + size, endpoint=True)
        if end < start:
            raise ValueError('Inverted function range')
        if end > start:
            bodies[start] = max(bodies.get(start, start), end)
    return sorted(bodies.items())


class BodyIndex:
    """Exact overlap lookup; never limit the search to a fixed prior-window."""
    def __init__(self, bodies):
        self.bodies = sorted(bodies)
        self.starts = [start for start, _ in self.bodies]
        greatest, self.prefix = 0, []
        for _, end in self.bodies:
            greatest = max(greatest, end)
            self.prefix.append(greatest)
        self.entries = set(self.starts)

    def interior(self, address):
        if address in self.entries:
            return None
        index = bisect_right(self.starts, address) - 1
        while index >= 0 and self.prefix[index] > address:
            start, end = self.bodies[index]
            if start < address < end:
                return start
            index -= 1
        return None


def _interior_of(va, bodies):
    return BodyIndex(bodies).interior(va)


def _decode_probe(xbe_path):
    if not xbe_path:
        return None
    from recomp_image import DATA_SECTION_NAMES, load_image
    from x86_recovery import CodeRecovery
    image = load_image(xbe_path)
    recovery = CodeRecovery(image)
    for section in image.sections:
        if section.executable and section.name not in DATA_SECTION_NAMES:
            recovery.linear_sweep(section)
    return recovery.probes_as_function_body


def _separate(output, inputs):
    destination = Path(output).resolve()
    if any(path and Path(path).resolve() == destination for path in inputs):
        raise ValueError('Output must not replace an input or observation ledger')


def cmd_merge(args):
    site_dumps = [sites_dump_beside(path) for path in args.dumps]
    inputs = [*args.dumps, *site_dumps]
    sites_path = getattr(args, 'sites_db', None) or DEFAULT_SITES_DB
    _separate(args.db, inputs)
    _separate(sites_path, [args.db, *inputs])
    targets, observed = load_db(args.db), {}
    for path in args.dumps:
        if not Path(path).exists():
            print(f'Missing dump: {path}', file=sys.stderr)
            continue
        for address, flags in parse_dump(path).items():
            observed[address] = observed.get(address, 0) | flags
    sites, seen = _collect_sites(sites_path, site_dumps)
    if not observed and not seen:
        print('No branch observations to merge', file=sys.stderr)
        return 1
    added = len(observed.keys() - targets.keys())
    for address, flags in observed.items():
        targets[address] = targets.get(address, 0) | flags
    if observed:
        save_db(args.db, targets)
    if seen:
        save_sites(sites_path, sites)
    print(f'Targets: {len(targets)}; new: {added}; sites: {len(sites)}; observed sites: {seen}')
    return 0


def cmd_seeds(args):
    _separate(args.out, [args.db, args.functions, args.xbe, DEFAULT_SITES_DB])
    targets = load_db(args.db)
    if not targets:
        print('Empty target ledger', file=sys.stderr)
        return 1
    if args.align < 0:
        raise ValueError('Alignment must be nonnegative')
    # An implicit database may describe another title. Only explicit bounds
    # may filter a seed export; observations remain independent of that choice.
    bodies = BodyIndex((load_function_bodies(args.functions) or ()) if args.functions else ())
    known = (load_function_starts(args.functions) or set()) if args.functions else set()
    probe = _decode_probe(args.xbe)
    kept = {}
    for address, flags in sorted(targets.items()):
        if address not in known and bodies.interior(address) is not None:
            continue
        accepted = probe(address) if probe else not args.align or address % args.align == 0
        if accepted:
            kept[address] = flags
    save_db(args.out, kept)
    print(f'Seeds: {len(kept)} of {len(targets)} observations; ledger preserved')
    return 0


def cmd_report(args):
    targets = load_db(args.db)
    if not targets:
        print('Empty target ledger', file=sys.stderr)
        return 1
    print(f'Targets: {len(targets)}')
    for flags, count in sorted(Counter(targets.values()).items()):
        print(f'  {FLAG_NAMES[flags]}: {count}')
    known = load_function_starts(args.functions) if args.functions else None
    if known is not None:
        print(f'Known starts: {len(targets.keys() & known)}')
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--db', default=DEFAULT_DB)
    parser.add_argument('--functions')
    modes = parser.add_subparsers(dest='command', required=True)
    merge = modes.add_parser('merge', help='Accumulate target and site observations')
    merge.add_argument('dumps', nargs='+')
    merge.add_argument('--sites-db', default=DEFAULT_SITES_DB)
    merge.set_defaults(run=cmd_merge)
    seeds = modes.add_parser('seeds', help='Export candidates without modifying observations')
    seeds.add_argument('--out', required=True)
    seeds.add_argument('--xbe')
    seeds.add_argument('--align', type=int, default=16)
    seeds.set_defaults(run=cmd_seeds)
    modes.add_parser('report', help='Summarize observations').set_defaults(run=cmd_report)
    args = parser.parse_args(argv)
    try:
        return args.run(args)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f'Branch feedback: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
