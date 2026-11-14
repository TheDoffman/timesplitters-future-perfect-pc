"""Build the standalone native mod VM from verified offline Lua source."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_hashes():
    return {p.relative_to(ROOT).as_posix(): sha(p) for p in [
        ROOT / 'tools/build/build-mod-worker.py', ROOT / 'config/mods/lua-source.json',
        *sorted((ROOT / 'src/mods').rglob('*'))] if p.is_file()}


def prepare_lua(output):
    pin = json.loads((ROOT / 'config/mods/lua-source.json').read_text())
    source = ROOT / ('vendor/lua-' + pin['version'])
    for name, expected in pin['source_files'].items():
        if sha(source / name) != expected:
            raise ValueError('Vendored Lua source changed: ' + name)
    for fix in pin['fixes']:
        if sha(ROOT / fix['patch']) != fix['sha256']:
            raise ValueError('Lua fix changed: ' + fix['patch'])
    identity = hashlib.sha256(json.dumps(pin, sort_keys=True).encode()).hexdigest()
    destination = output / ('lua-' + identity[:16])
    if destination.exists():
        receipt = json.loads((destination / 'source-receipt.json').read_text())
        if (receipt['identity'] != identity or receipt['files'] != pin['prepared_files']
                or any(sha(destination / n) != h for n, h in pin['prepared_files'].items())):
            raise ValueError('Prepared Lua source changed; preserve it and choose a new output')
        return destination
    with tempfile.TemporaryDirectory(prefix='.lua-prepare-', dir=output) as temporary:
        stage = Path(temporary)
        if not stage.resolve().is_relative_to(output.resolve()):
            raise ValueError('Lua staging path escaped its selected output')
        for path in (source / 'src').iterdir():
            if path.is_file():
                shutil.copyfile(path, stage / path.name)
        for fix in pin['fixes']:
            # Discovery of the parent checkout otherwise adds a path prefix and
            # can silently skip patches in an ignored build directory.
            environment = dict(os.environ, GIT_CEILING_DIRECTORIES=str(stage.parent.resolve()))
            for name in ('GIT_DIR', 'GIT_WORK_TREE', 'GIT_INDEX_FILE', 'GIT_PREFIX'):
                environment.pop(name, None)
            subprocess.run(['git', '-c', 'core.autocrlf=false', 'apply', '--no-index', str(ROOT / fix['patch'])],
                           cwd=stage, env=environment, check=True)
        files = {p.name: sha(p) for p in stage.iterdir() if p.is_file()}
        if files != pin['prepared_files']:
            changed = sorted(n for n in set(files) | set(pin['prepared_files'])
                             if files.get(n) != pin['prepared_files'].get(n))
            raise ValueError('Prepared Lua source does not match the pinned fixed-source hashes: ' + ', '.join(changed))
        (stage / 'source-receipt.json').write_text(json.dumps({'identity': identity, 'files': files}, indent=2) + '\n')
        stage.rename(destination)
    return destination


def build(output, cmake='cmake', sanitize=False, testing=False, host_sanitize=False):
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    sources = source_hashes()
    lua = prepare_lua(output)
    subprocess.run([cmake, '-S', str(ROOT / 'src/mods'), '-B', str(output / 'cmake'),
                    '-DTSFP_LUA_SOURCE=' + str(lua), '-DCMAKE_BUILD_TYPE=Release',
                    '-DTSFP_MOD_SANITIZE=' + ('ON' if sanitize else 'OFF'),
                    '-DTSFP_MOD_TESTING=' + ('ON' if testing else 'OFF'),
                    '-DTSFP_MOD_HOST_SANITIZE=' + ('ON' if host_sanitize else 'OFF')], check=True)
    subprocess.run([cmake, '--build', str(output / 'cmake'), '--config', 'Release', '--parallel', '2'], check=True)
    name = 'tsfp-mod-worker.exe' if os.name == 'nt' else 'tsfp-mod-worker'
    binary = output / 'cmake' / ('Release' if os.name == 'nt' else '') / name
    if not binary.is_file():
        raise ValueError('Mod worker build did not produce its native executable')
    if source_hashes() != sources:
        raise ValueError('Mod worker sources changed during compilation; rebuild before packaging')
    (output / 'Licenses').mkdir(exist_ok=True)
    shutil.copyfile(ROOT / 'LICENSES/Lua.txt', output / 'Licenses/Lua.txt')
    (output / 'BUILD-worker.json').write_text(json.dumps({
        'schema': 2, 'binary_sha256': sha(binary),
        'runner_sha256': sha(binary.with_name('tsfp-mod-runner.exe' if os.name == 'nt' else 'tsfp-mod-runner')),
        'guardian_sha256': sha(binary.with_name('tsfp-mod-guardian')) if sys.platform == 'darwin' else None,
        'testing': testing, 'sanitize': sanitize, 'host_sanitize': host_sanitize,
        'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'sources': sources}, indent=2) + '\n')
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/mod-worker')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--sanitize-host', action='store_true', help='Instrument trusted native host code; keep sealed child binaries ordinary')
    args = parser.parse_args()
    print(build(args.output, args.cmake, args.sanitize, host_sanitize=args.sanitize_host))
