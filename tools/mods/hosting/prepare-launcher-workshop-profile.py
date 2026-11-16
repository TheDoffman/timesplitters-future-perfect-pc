"""Verify a source-pinned public root and stage the launcher's existing settings."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_package import ModError, read_json, require
from mod_trust import origin as checked_origin, trusted_root

REPOSITORY = Path(__file__).resolve().parents[3]
PROFILE = REPOSITORY/'config/mods/public-workshop.json'
PUBLIC_ROOT = REPOSITORY/'config/mods/public-workshop-root.json'


def read_public(path, maximum):
    path = Path(path).absolute()
    require(all(not p.is_symlink() and not p.is_junction() for p in (path, *path.parents)), 'Aliased public profile path')
    with path.open('rb') as source:
        before = os.fstat(source.fileno())
        require(stat.S_ISREG(before.st_mode) and before.st_nlink == 1 and 0 < before.st_size <= maximum,
                'Invalid public profile file')
        wire = source.read(maximum+1)
        after = os.fstat(source.fileno())
        fields = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)
        require(len(wire) == before.st_size and fields(before) == fields(after), 'Public profile changed')
    return wire


def validate(now=None):
    profile = read_json(read_public(PROFILE, 8192))
    require(isinstance(profile, dict) and set(profile) == {'schema','origin','trusted_root_sha256','status'}
            and type(profile['schema']) is int and profile['schema'] == 1
            and profile['status'] in ('pending-public-acceptance','active'), 'Invalid public workshop profile')
    checked_origin(profile['origin'])
    wire = read_public(PUBLIC_ROOT, 65536)
    require(hashlib.sha256(wire).hexdigest() == profile['trusted_root_sha256'], 'Public workshop root fingerprint differs')
    root = trusted_root(wire, now=now)
    require(root['origin'] == profile['origin'], 'Public workshop root origin differs')
    return profile, wire


def prepare(output, acceptance_only=False, now=None):
    profile, root = validate(now)
    require(profile['status'] == 'active' or acceptance_only, 'Public workshop acceptance is incomplete; use only private acceptance staging')
    output = Path(output).absolute()
    require(output.parent.is_dir() and all(not p.is_symlink() and not p.is_junction() for p in (output, *output.parents)),
            'Choose an unaliased existing staging parent')
    require(not output.exists(), 'Preserving existing launcher profile output')
    output.mkdir(mode=0o700)
    files = {'service.json': (json.dumps(dict(service=profile['origin']), separators=(',',':'))+'\n').encode(),
             'trusted-root.json': root}
    for name, value in files.items():
        with (output/name).open('xb') as target:
            target.write(value); target.flush(); os.fsync(target.fileno())
    result = dict(schema=1, phase='acceptance-only' if acceptance_only else 'release-profile',
                  origin=profile['origin'], trusted_root_sha256=profile['trusted_root_sha256'],
                  destination='Support/Mods', files={name: hashlib.sha256(value).hexdigest() for name,value in files.items()},
                  public_acceptance_complete=profile['status']=='active', services_started=False, network_started=False)
    with (output/'profile-receipt.json').open('x') as target:
        json.dump(result,target,indent=2); target.write('\n'); target.flush(); os.fsync(target.fileno())
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--acceptance-only', action='store_true')
    args = parser.parse_args()
    try:
        if args.apply:
            require(args.output is not None, 'Choose a new profile output directory')
            result = prepare(args.output, args.acceptance_only)
        else:
            profile, _ = validate()
            result = dict(profile, phase='plan-only', services_started=False, network_started=False)
        print(json.dumps(result,indent=2)); return 0
    except (OSError, ValueError, ModError) as error:
        print('Launcher workshop profile: '+str(error)[:2000],file=sys.stderr); return 2


if __name__ == '__main__':
    raise SystemExit(main())
