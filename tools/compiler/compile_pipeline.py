"""Project driver for parsing, disassembly, identification and C generation.

All four stages use project tools. The selected runtime tree supplies the
remaining data, title integration and shared C types; it supplies no lifting Python.
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

import argparse
from pathlib import Path
import subprocess
import struct
import sys
import time

from xbe_image import XBEParser

REPO = _layout_root
STAGES = ('parse', 'disasm', 'identify', 'lift')


def xbe_title_id(path):
    """Read only the bounded identifier fields, including for dry-run inputs.

    This is not executable validation; main validates the complete image before
    any real stage is allowed to run.
    """
    try:
        with Path(path).open('rb') as source:
            header = source.read(0x11C)
            if len(header) != 0x11C or header[:4] != b'XBEH':
                return None
            base = struct.unpack_from('<I', header, 0x104)[0]
            certificate = struct.unpack_from('<I', header, 0x118)[0]
            if certificate < base:
                return None
            source.seek(certificate - base + 8)
            identifier = source.read(4)
            return f'{int.from_bytes(identifier, "little"):08X}' if len(identifier) == 4 else None
    except (OSError, ValueError):
        return None


def build_commands(args, xbe, analysis_json):
    selected = (args.only,) if args.only else STAGES[STAGES.index(args.start):]
    own = Path(__file__).resolve().parent
    commands = {
        'parse': [sys.executable, str(own / 'xbe_image.py'), str(xbe), '--json', str(analysis_json)],
        'disasm': [sys.executable, str(own / 'analysis_runner.py'), str(xbe)],
        'identify': [sys.executable, str(own / 'function_catalog.py'), str(xbe)],
        'lift': [sys.executable, str(own / 'translation_cli.py'), str(xbe), '--runtime', str(REPO),
                 '--all' if args.all else '--game-only'],
    }
    parse, disasm, identify, lift = (commands[name] for name in STAGES)
    if getattr(args, 'json', None) or analysis_json.exists() or 'parse' in selected:
        disasm.extend(['--analysis-json', str(analysis_json)])
    if args.text_only:
        disasm.append('--text-only')
    for seed in args.seeds or ():
        disasm.extend(['--seed-functions', str(Path(seed).resolve())])
    if args.work_dir:
        work = Path(args.work_dir).resolve()
        disasm.extend(['-o', str(work / 'disasm')])
        for option, name in (('--functions', 'functions'), ('--strings', 'strings'), ('--xrefs', 'xrefs')):
            identify.extend([option, str(work / 'disasm' / (name + '.json'))])
        identify.extend(['-o', str(work / 'func_id')])
        for option, name in (('--disasm-dir', 'disasm'), ('--func-id-dir', 'func_id'), ('-o', 'recomp')):
            lift.extend([option, str(work / name)])
        lift.extend(['--icall-sites', str(work / 'recomp/icall_sites.json')])
    project = Path(args.project).resolve() if args.project else None
    generated = Path(args.gen_dir) if args.gen_dir else project / 'src/recomp/gen' if project else None
    if generated is not None:
        lift.extend(['--gen-dir', str(generated)])
    if project is not None and (project / 'src/recomp_manual.c').is_file():
        lift.extend(['--exclude-manual', str(project / 'src/recomp_manual.c')])
    if args.split:
        lift.extend(['--split', str(args.split)])
    if args.game_name:
        lift.extend(['--game-name', args.game_name])
    if args.trace_all_entries:
        lift.append('--trace-all-entries')
    symbols = xbe.with_name(xbe.stem + '_xdk_symbols.json')
    if symbols.is_file():
        lift.extend(['--hle-symbols', str(symbols)])
    if args.verbose:
        for command in (disasm, identify, lift):
            command.append('-v')
    return [(name, commands[name]) for name in selected]


def run(name, argv, dry_run=False):
    print(f'{STAGES.index(name) + 1}/{len(STAGES)} {name}: {subprocess.list2cmdline(argv)}', flush=True)
    if dry_run:
        return True
    started = time.monotonic()
    result = subprocess.run(argv, cwd=REPO)
    if result.returncode:
        print(f'{name} failed with exit status {result.returncode}; resume with --from {name}', file=sys.stderr)
        return False
    print(f'{name} completed in {time.monotonic() - started:.2f}s', flush=True)
    return True


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xbe')
    parser.add_argument('--runtime', help='Runtime data, title integration and C types root')
    parser.add_argument('--seeds', action='append')
    parser.add_argument('--no-seeds', action='store_true')
    for flag in ('trace-all-entries', 'text-only', 'all', 'dry-run'):
        parser.add_argument('--' + flag, action='store_true')
    parser.add_argument('--split', type=int, default=1000)
    for option in ('gen-dir', 'work-dir', 'project', 'game-name', 'json'):
        parser.add_argument('--' + option)
    parser.add_argument('--from', dest='start', choices=STAGES, default='parse')
    parser.add_argument('--only', choices=STAGES)
    parser.add_argument('--verbose', '-v', action='store_true')
    args = parser.parse_args(argv)
    global REPO
    previous_root = REPO
    try:
        if args.runtime:
            REPO = Path(args.runtime).resolve()
        xbe = Path(args.xbe).resolve()
        if args.split < 0:
            raise ValueError('--split cannot be negative')
        if not args.dry_run:
            XBEParser(xbe).parse()
        if args.no_seeds:
            args.seeds = []
        elif args.seeds is None:
            identifier = xbe_title_id(xbe)
            candidate = Path(REPO) / 'config/seeds' / f'{identifier}.json' if identifier else None
            args.seeds = [candidate] if candidate is not None and candidate.is_file() else []
        if not args.dry_run:
            for seed in args.seeds:
                if not Path(seed).is_file():
                    raise FileNotFoundError(f'Seed file not found: {seed}')
        metadata = Path(args.json).resolve() if args.json else xbe.with_name(xbe.stem + '_analysis.json')
        if metadata == xbe:
            raise ValueError('Analysis JSON cannot replace the executable')
        if args.gen_dir:
            # Resolve user paths before run() changes the subprocess directory.
            # The public command builder also accepts already-selected paths.
            args.gen_dir = str(Path(args.gen_dir).resolve())
        for name, command in build_commands(args, xbe, metadata):
            if not run(name, command, args.dry_run):
                return 1
        print('Dry run complete; no stages executed' if args.dry_run else 'Pipeline complete')
        return 0
    except KeyboardInterrupt:
        return 130
    except (OSError, ValueError) as error:
        print(f'Pipeline failed: {error}', file=sys.stderr)
        return 1
    finally:
        REPO = previous_root


if __name__ == '__main__':
    raise SystemExit(main())
