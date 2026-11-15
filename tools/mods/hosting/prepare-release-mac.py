"""Freeze tracked hosting source and bind an existing verified runtime; no root changes."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

spec = importlib.util.spec_from_file_location('host_release', Path(__file__).with_name('host_release.py'))
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)
ROOT = Path(__file__).resolve().parents[3]


def git(*args):
    return subprocess.check_output(['/opt/homebrew/bin/git', '-C', str(ROOT), *args], timeout=20)


def prepare(runtime, output):
    release.require(sys.platform == 'darwin' and os.getuid() != 0, 'Prepare as the ordinary Mac user without sudo')
    uid = os.getuid()
    output = Path(os.path.abspath(output))
    fd = release.directory(output.parent, uid)
    os.close(fd)
    release.require(output.parent.stat().st_uid == uid and output.parent.stat().st_mode & 0o077 == 0,
                    'Choose a private preparation parent')
    release.require(not git('status', '--porcelain', '--', 'tools/mods', 'config/mods'), 'Hosting source has local changes')
    revision = git('rev-parse', 'HEAD').decode().strip()
    paths = git('ls-tree', '-r', '--name-only', 'HEAD', '--', 'tools/mods', 'config/mods').decode().splitlines()
    release.require(0 < len(paths) <= 240, 'Hosting source selection exceeded budget')
    runtime = Path(os.path.abspath(runtime))
    runtime_wire = release.read_file(runtime, 'runtime.json', uid, 4 * 1024**2)
    output.mkdir(mode=0o700)
    snapshot = output / 'source'
    snapshot.mkdir(mode=0o700)
    records = {}
    for key in paths:
        release.name(key)
        wire = release.read_file(ROOT, key, uid, 8 * 1024**2)
        canonical = git('show', revision + ':' + key)
        release.require(wire == canonical, 'Hosting source bytes differ from Git revision')
        path = snapshot / key
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open('xb') as stream:
            stream.write(wire)
        mode = 0o755 if (ROOT / key).stat().st_mode & 0o111 else 0o644
        path.chmod(mode & ~0o200)
        records[key] = dict(bytes=len(wire), sha256=hashlib.sha256(wire).hexdigest(), mode=mode)
    for key, mode, maximum in [('scan-guardian-mac', 0o755, 1024**2), ('receipt.json', 0o644, 4096)]:
        wire = release.read_file(ROOT / 'build/scan-guardian', key, uid, maximum)
        relative = 'build/scan-guardian/' + key
        path = snapshot / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open('xb') as stream:
            stream.write(wire)
        path.chmod(mode & ~0o200)
        records[relative] = dict(bytes=len(wire), sha256=hashlib.sha256(wire).hexdigest(), mode=mode)
    for path in sorted([p for p in snapshot.rglob('*') if p.is_dir()] + [snapshot], key=lambda p: len(p.parts), reverse=True):
        path.chmod(0o555)
    data = dict(schema=1, producer='tsfp-host-release-v1', source_revision=revision, owner_uid=uid,
                runtime_root=str(runtime), runtime_sha256=hashlib.sha256(runtime_wire).hexdigest(),
                source_root=str(snapshot), source_files=records)
    wire = (json.dumps(data, sort_keys=True, indent=2) + '\n').encode()
    bundle = output / 'release.json'
    with bundle.open('xb') as stream:
        stream.write(wire)
    bundle.chmod(0o600)
    digest = hashlib.sha256(wire).hexdigest()
    plan = release.plan(bundle, digest)
    return dict(descriptor=str(bundle), sha256=digest, source_revision=revision, source_files=len(records),
                runtime_files=len(plan['components'][0][2]), bytes=plan['bytes'],
                root_modified=False, services_started=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(prepare(args.runtime, args.output), indent=2))
        return 0
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print('Host release preparation: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
