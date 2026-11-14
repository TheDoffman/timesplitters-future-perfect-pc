"""Local key ceremony and private-key loading; never reads keys from an upload."""
import argparse
import getpass
import os
from pathlib import Path
import stat
import sys
import time

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa

from mod_package import ModError, read_json, require
from mod_trust import (MAX_ROOT, PublicationSigner, encode, key_id, load_public,
                       make_root, public_key, root_document, trusted_root)


def bounded_file(path, limit):
    path = Path(path)
    require(path.is_file() and not path.is_symlink() and not path.is_junction()
            and path.stat().st_size <= limit, 'Missing, linked or oversized signing file')
    with path.open('rb') as source:
        data = source.read(limit + 1)
    require(len(data) <= limit, 'Signing file exceeds budget')
    return data


def private_directory(path):
    require(os.name == 'posix' and os.geteuid() != 0,
            'Manage production signing secrets as a non-root Unix operator')
    path = Path(path).absolute()
    # Refuse directory aliases before resolving: secret locations must be explicit.
    require(all(not p.is_symlink() for p in (path, *path.parents)), 'Linked key directory forbidden')
    status = path.stat()
    require(stat.S_ISDIR(status.st_mode) and status.st_uid == os.geteuid()
            and stat.S_IMODE(status.st_mode) == 0o700, 'Key directory must be owned by the operator with mode0700')
    return path


def load_private(path, interactive=False):
    path = Path(path).absolute()
    private_directory(path.parent)
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(descriptor, 'rb') as source:
        status = os.fstat(source.fileno())
        require(stat.S_ISREG(status.st_mode) and status.st_uid == os.geteuid()
                and stat.S_IMODE(status.st_mode) == 0o600 and status.st_size <= 16384,
                'Signing key must be a private0600 regular file')
        raw = source.read(16385)
    require(len(raw) <= 16384, 'Signing key exceeds budget')
    password = None
    if b'ENCRYPTED PRIVATE KEY' in raw:
        require(interactive, 'Online signing needs its dedicated noninteractive catalog key')
        password = getpass.getpass('Offline key passphrase: ').encode('utf-8')
    try:
        private = serialization.load_pem_private_key(raw, password)
        public_key(private)
    except (TypeError, ValueError, AttributeError) as exc:
        raise ModError('Cannot load the RSA-3072 signing key') from exc
    return private


def load_signer(root_path, key_paths, updates=()):
    pinned = bounded_file(root_path, MAX_ROOT)
    history = [bounded_file(path, MAX_ROOT) for path in updates]
    require(1 <= len(key_paths) <= 4, 'One to four catalog signing keys required')
    authority = trusted_root(pinned, history)
    keys = [load_private(path) for path in key_paths]
    require(all(key_id(public_key(k)) in authority['roles']['catalog']['keyids'] for k in keys),
            'Only catalog-role private keys may be used by the online publisher')
    return PublicationSigner(pinned, keys, history)


def new_file(path, data, mode=0o644):
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, mode)
    with os.fdopen(descriptor, 'wb') as target:
        target.write(data)
        target.flush()
        os.fsync(target.fileno())


def generate_key(directory, offline=False):
    directory = Path(directory).absolute()
    require(os.name == 'posix' and os.geteuid() != 0,
            'Generate signing keys as a non-root Unix operator')
    require(all(not p.is_symlink() for p in (directory.parent, *directory.parent.parents)),
            'Linked key directory forbidden')
    directory.mkdir(mode=0o700)  # New directory only. Never overwrite or clean up old secrets.
    private_directory(directory)
    encryption = serialization.NoEncryption()
    if offline:
        first = getpass.getpass('New offline key passphrase (at least20 characters): ')
        second = getpass.getpass('Confirm offline key passphrase: ')
        require(first == second and len(first) >= 20 and len(first.encode('utf-8')) <= 1024,
                'Offline key passphrases must match and be20 to1024 UTF8 bytes')
        encryption = serialization.BestAvailableEncryption(first.encode('utf-8'))
    private = rsa.generate_private_key(public_exponent=65537, key_size=3072)
    new_file(directory / 'private.pem', private.private_bytes(
        serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, encryption), 0o600)
    descriptor = public_key(private)
    new_file(directory / 'public.json', encode(descriptor))
    return {'directory': str(directory), 'keyid': key_id(descriptor), 'offline': offline}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    keygen = commands.add_parser('keygen')
    keygen.add_argument('directory')
    keygen.add_argument('--offline', action='store_true')
    root = commands.add_parser('root', help='Offline bootstrap or cross-signed sequential root rotation')
    root.add_argument('--root-key', action='append', required=True)
    root.add_argument('--catalog-public', action='append', required=True)
    root.add_argument('--previous-root')
    root.add_argument('--old-root-key', action='append', default=[])
    root.add_argument('--origin', required=True)
    root.add_argument('--output', required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == 'keygen':
            result = generate_key(args.directory, args.offline)
        else:
            require(2 <= len(args.root_key) <= 4 and 1 <= len(args.catalog_public) <= 4
                    and len(args.old_root_key) <= 4, 'Invalid ceremony key count')
            keys = [load_private(path, interactive=True) for path in args.root_key]
            # Offline roots must actually be encrypted, not just called offline.
            require(all(b'ENCRYPTED PRIVATE KEY' in bounded_file(path, 16384)
                        for path in [*args.root_key, *args.old_root_key]),
                    'Root ceremony requires encrypted offline keys')
            public = [read_json(bounded_file(path, 4096)) for path in args.catalog_public]
            for descriptor in public:
                load_public(descriptor)
            old_wire = bounded_file(args.previous_root, MAX_ROOT) if args.previous_root else None
            old = root_document(old_wire) if old_wire else None
            require(old is not None or not args.old_root_key, 'Bootstrap has no old signing role')
            old_keys = [load_private(path, interactive=True) for path in args.old_root_key]
            now = int(time.time())
            wire = make_root(keys, public, old['version'] + 1 if old else 1, args.origin, now, old_keys)
            trusted_root(old_wire or wire, [wire] if old else [], now)
            new_file(args.output, wire)
            result = {'root_file': str(Path(args.output).absolute()), 'version': root_document(wire)['version']}
        print(encode(result).decode('ascii'))
        return 0
    except (ModError, OSError) as exc:
        print('Workshop signing: ' + str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
