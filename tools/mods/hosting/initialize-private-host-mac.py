"""New-only locked-account host configuration; never starts jobs or network."""
import argparse
import hashlib
import importlib.util
from itertools import islice
import json
import os
from pathlib import Path
import re
import stat
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_backup import private_directory
from mod_package import ModError, read_json, require
from mod_system_host import ROOT, ROLES, paths
from mod_trust import key_id, origin as checked_origin, public_key, trusted_root

PRODUCER = 'tsfp-private-host-v1'
INPUTS = {'origin.json': 8192, 'trusted-root.json': 65536, 'catalog-1.pem': 16384}
CONFIGS = ('api.json', 'publication.json', 'gateway.json')
DIRECTORIES = (*(role+'-state' for role in ROLES), 'root-updates')
PAYLOADS = (*CONFIGS, 'trusted-root.json', 'catalog-1.pem')


def wire(value):
    return (json.dumps(value, sort_keys=True, separators=(',', ':'))+'\n').encode()


def entries(fd, maximum):
    with os.scandir(fd) as directory:
        result = [entry.name for entry in islice(directory, maximum+1)]
    require(len(result) <= maximum, 'Private host entry budget exceeded')
    return set(result)


def read_member(fd, name):
    require(name in {*INPUTS, *PAYLOADS, 'initialization.json'}, 'Invalid fixed host member')
    child = os.open(name, os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK, dir_fd=fd)
    with os.fdopen(child, 'rb') as stream:
        before = os.fstat(stream.fileno())
        require(stat.S_ISREG(before.st_mode) and before.st_uid == os.geteuid() and before.st_nlink == 1
                and stat.S_IMODE(before.st_mode) == 0o600 and 0 < before.st_size <= INPUTS.get(name, 8192),
                'Unsafe private host record')
        value = stream.read(INPUTS.get(name, 8192)+1)
        after = os.fstat(stream.fileno())
        fields = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)
        require(len(value) == before.st_size and fields(before) == fields(after), 'Private host record changed')
    return value


def new_member(fd, name, value):
    require(name in {*PAYLOADS, 'initialization.json', '.initialization-complete'}, 'Invalid private host output')
    require(isinstance(value, bytes) and 0 < len(value) <= INPUTS.get(name, 8192), 'Private host output exceeds budget')
    child = os.open(name, os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW, 0o600, dir_fd=fd)
    with os.fdopen(child, 'wb') as output:
        output.write(value); output.flush(); os.fsync(output.fileno())


def signing(root_wire, key_wire, origin, pin):
    from cryptography.hazmat.primitives import serialization
    require(isinstance(pin, str) and re.fullmatch('[0-9a-f]{64}', pin)
            and hashlib.sha256(root_wire).hexdigest() == pin, 'Private host root pin differs')
    root = trusted_root(root_wire)
    require(root['origin'] == origin, 'Private host root origin differs')
    try:
        private = serialization.load_pem_private_key(key_wire, None)
        identity = key_id(public_key(private))
    except (ValueError, TypeError, AttributeError) as error:
        raise ModError('Invalid noninteractive online catalog key') from error
    require(identity in root['roles']['catalog']['keyids'] and root['roles']['catalog']['threshold'] == 1,
            'Private host key is not an authorized catalog signer')


def configs(release, origin):
    checked_origin(origin)
    require(release.parent == ROOT/'releases' and re.fullmatch('[0-9a-f]{64}', release.name),
            'Use an installed host release identity')
    repository = str(ROOT/'data/scan/repository')
    common = dict(schema=1, caddy=str(release/'runtime/bin/caddy'))
    return {
        'api.json': dict(schema=1, origin=origin, repository=repository, state=str(paths('api')['state'])),
        'gateway.json': dict(common, origin=origin, state=str(paths('gateway')['state'])),
        'publication.json': dict(common, repository=repository, state=str(paths('publication')['state']),
                                 trusted_root=str(ROOT/'data/host/trusted-root.json'),
                                 catalog_keys=[str(ROOT/'data/host/catalog-1.pem')], root_updates=[])}


def marker(release, origin, pin, payloads):
    return dict(schema=1, producer=PRODUCER, release=release.name, origin=origin, trusted_root_sha256=pin,
                files={name: hashlib.sha256(payloads[name]).hexdigest() for name in PAYLOADS})


def inputs(release):
    root = private_directory(ROOT/'data/host-input')
    fd = os.open(root, os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:
        require(entries(fd, len(INPUTS)) == set(INPUTS), 'Preserving unknown host input entries')
        value = {name: read_member(fd, name) for name in INPUTS}
    finally: os.close(fd)
    settings = read_json(value['origin.json'])
    require(isinstance(settings, dict) and set(settings) == {'schema', 'origin', 'trusted_root_sha256'}
            and type(settings['schema']) is int and settings['schema'] == 1, 'Invalid private host inputs')
    origin, pin = settings['origin'], settings['trusted_root_sha256']
    expected = configs(release, origin)
    signing(value['trusted-root.json'], value['catalog-1.pem'], origin, pin)
    payloads = {name: wire(data) for name, data in expected.items()}
    payloads.update({name: value[name] for name in ('trusted-root.json', 'catalog-1.pem')})
    return payloads, marker(release, origin, pin, payloads)


def check(release):
    root = private_directory(ROOT/'data/host')
    fd = os.open(root, os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:
        expected_entries = {*PAYLOADS, *DIRECTORIES, 'initialization.json'}
        require(entries(fd, len(expected_entries)) == expected_entries, 'Preserving unknown private host entries')
        saved = read_member(fd, 'initialization.json')
        value = read_json(saved)
        require(isinstance(value, dict) and value.get('phase') == 'complete', 'Preserving incomplete private host setup')
        expected = configs(release, value.get('origin'))
        payloads = {name: read_member(fd, name) for name in PAYLOADS}
        for name, data in expected.items():
            require(payloads[name] == wire(data), 'Preserving changed private host configuration')
        current = marker(release, value['origin'], value.get('trusted_root_sha256'), payloads)
        require(saved == wire(dict(current, phase='complete')), 'Preserving changed or different host generation')
        signing(payloads['trusted-root.json'], payloads['catalog-1.pem'], current['origin'], current['trusted_root_sha256'])
        for name in DIRECTORIES: private_directory(root/name)
        updates = os.open('root-updates', os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW, dir_fd=fd)
        try:
            with os.scandir(updates) as directory:
                require(next(directory, None) is None, 'Root updates require an explicit preserving generation migration')
        finally: os.close(updates)
    finally: os.close(fd)
    return dict(schema=1, phase='ready', release=release.name, origin=current['origin'],
                trusted_root_sha256=current['trusted_root_sha256'], services_started=False, network_started=False)


def initialize(release):
    data = private_directory(ROOT/'data')
    parent = os.open(data, os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:
        try: os.stat('host', dir_fd=parent, follow_symlinks=False)
        except FileNotFoundError: pass
        else: return check(release)
        payloads, value = inputs(release)
        os.mkdir('host', mode=0o700, dir_fd=parent)
        fd = os.open('host', os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW, dir_fd=parent)
        try:
            new_member(fd, 'initialization.json', wire(dict(value, phase='preparing')))
            for name in DIRECTORIES: os.mkdir(name, mode=0o700, dir_fd=fd)
            for name in PAYLOADS: new_member(fd, name, payloads[name])
            require(read_member(fd, 'initialization.json') == wire(dict(value, phase='preparing')),
                    'Private host setup changed before commit')
            new_member(fd, '.initialization-complete', wire(dict(value, phase='complete')))
            os.replace('.initialization-complete', 'initialization.json', src_dir_fd=fd, dst_dir_fd=fd)
            os.fsync(fd)
        finally: os.close(fd)
        os.fsync(parent)
    finally: os.close(parent)
    return check(release)


def installed_release():
    source = Path(__file__).with_name('initialize-private-scan-mac.py')
    spec = importlib.util.spec_from_file_location('host_scan_initializer', source)
    initializer = importlib.util.module_from_spec(spec); spec.loader.exec_module(initializer)
    release = initializer.installed_release()
    require(Path(__file__).absolute() == release/'source/tools/mods/hosting/initialize-private-host-mac.py',
            'Use the immutable installed host initializer')
    initializer.check(release)
    return release


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--apply', action='store_true')
    mode.add_argument('--check', action='store_true')
    args = parser.parse_args()
    try:
        if args.apply or args.check:
            release = installed_release()
            result = initialize(release) if args.apply else check(release)
        else:
            result = dict(schema=1, phase='plan-only', account='_tsfpworkshop', root=str(ROOT/'data/host'),
                          input=str(ROOT/'data/host-input'), services_started=False, network_started=False)
        print(json.dumps(result, indent=2)); return 0
    except (OSError, ValueError, ModError) as error:
        print('Private host initialization: '+str(error)[:2000], file=sys.stderr); return 2


if __name__ == '__main__':
    raise SystemExit(main())
