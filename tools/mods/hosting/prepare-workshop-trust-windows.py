"""Off-server Windows root ceremony; export only catalog inputs to the Mac."""
import argparse
import base64
import hashlib
from itertools import islice
import json
import os
from pathlib import Path
import secrets
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from mod_package import ModError, read_json, require
from mod_trust import encode, key_id, make_root, origin as checked_origin, public_key, root_document, sign, verify_role
import mod_windows_recovery as recovery

PRODUCER = 'tsfp-windows-root-ceremony-v1'
MEMBERS = ('root-a/private.pem', 'root-b/private.pem', 'recovery.dpapi',
           'host-input/origin.json', 'host-input/trusted-root.json', 'host-input/catalog-1.pem')
DIRECTORIES = ('root-a', 'root-b', 'host-input')


def exact_entries(path, expected):
    with os.scandir(path) as entries:
        names = [entry.name for entry in islice(entries, len(expected)+1)]
    require(len(names) == len(expected) and set(names) == set(expected), 'Preserving unknown recovery entries')


def check(directory):
    directory = recovery.private(directory)
    exact_entries(directory, {*DIRECTORIES, 'recovery.dpapi', 'ceremony.json'})
    for name in DIRECTORIES:
        recovery.private(directory/name)
        exact_entries(directory/name, {'private.pem'} if name != 'host-input' else {'origin.json', 'trusted-root.json', 'catalog-1.pem'})
    receipt = read_json(recovery.read_file(directory/'ceremony.json', 8192))
    require(isinstance(receipt, dict) and set(receipt) == {'schema', 'producer', 'phase', 'origin', 'trusted_root_sha256', 'root_keyids', 'catalog_keyid', 'files'}
            and type(receipt['schema']) is int and receipt['schema'] == 1 and receipt['producer'] == PRODUCER
            and receipt['phase'] == 'complete' and isinstance(receipt['files'], dict) and set(receipt['files']) == set(MEMBERS),
            'Preserving incomplete or unknown root ceremony')
    checked_origin(receipt['origin'])
    payloads = {name: recovery.read_file(directory/name) for name in MEMBERS}
    require(all(hashlib.sha256(value).hexdigest() == receipt['files'][name] for name, value in payloads.items()),
            'Root ceremony payload changed')
    root_wire = payloads['host-input/trusted-root.json']
    require(hashlib.sha256(root_wire).hexdigest() == receipt['trusted_root_sha256'], 'Root ceremony public pin changed')
    root = root_document(root_wire); verify_role(root_wire, root, 'root')
    require(root['origin'] == receipt['origin'] and root['roles']['root']['keyids'] == receipt['root_keyids']
            and root['roles']['catalog']['keyids'] == [receipt['catalog_keyid']], 'Root ceremony authority changed')
    settings = read_json(payloads['host-input/origin.json'])
    require(settings == dict(schema=1, origin=receipt['origin'], trusted_root_sha256=receipt['trusted_root_sha256']),
            'Root ceremony host settings changed')
    capsule = read_json(recovery.crypt(payloads['recovery.dpapi'], decrypt=True))
    require(isinstance(capsule, dict) and set(capsule) == {'schema', 'producer', 'origin', 'trusted_root_sha256', 'passwords'}
            and type(capsule['schema']) is int and capsule['schema'] == 1 and capsule['producer'] == PRODUCER
            and capsule['origin'] == receipt['origin'] and capsule['trusted_root_sha256'] == receipt['trusted_root_sha256']
            and isinstance(capsule['passwords'], dict) and set(capsule['passwords']) == {'root-a', 'root-b'},
            'Windows recovery capsule identity differs')
    keys = []
    for name in ('root-a', 'root-b'):
        value = payloads[name+'/private.pem']
        require(b'ENCRYPTED PRIVATE KEY' in value, 'Root key is not encrypted')
        password = base64.b64decode(capsule['passwords'][name], validate=True)
        require(len(password) == 32, 'Invalid root recovery password')
        key = serialization.load_pem_private_key(value, password)
        require(key_id(public_key(key)) in receipt['root_keyids'], 'Recovered root key differs')
        keys.append(key)
    online = serialization.load_pem_private_key(payloads['host-input/catalog-1.pem'], None)
    require(key_id(public_key(online)) == receipt['catalog_keyid'], 'Online catalog key differs')
    challenge = dict(type='tsfp-root-recovery-check', root_sha256=receipt['trusted_root_sha256'], nonce=secrets.token_hex(16))
    require(verify_role(sign(challenge, keys), root, 'root') == challenge, 'Recovered root threshold proof failed')
    return dict(schema=1, phase='recovery-verified', origin=receipt['origin'],
                trusted_root_sha256=receipt['trusted_root_sha256'], root_keyids=receipt['root_keyids'],
                catalog_keyid=receipt['catalog_keyid'], root_valid_now=root['issued'] <= int(time.time()) < root['expires'],
                account_bound=True, host_input=str(directory/'host-input'),
                root_keys_exported=False, services_started=False, network_started=False)


def prepare(directory, origin):
    checked_origin(origin)
    directory = recovery.private(directory, create=True)
    preparing = encode(dict(schema=1, producer=PRODUCER, phase='preparing', origin=origin))
    recovery.new_file(directory/'ceremony.json', preparing)
    for name in DIRECTORIES: recovery.private(directory/name, create=True)
    roots = [rsa.generate_private_key(public_exponent=65537, key_size=3072) for _ in range(2)]
    online = rsa.generate_private_key(public_exponent=65537, key_size=3072)
    root_wire = make_root(roots, [online], 1, origin, int(time.time()))
    pin = hashlib.sha256(root_wire).hexdigest()
    passwords = {name: secrets.token_bytes(32) for name in ('root-a', 'root-b')}
    payloads = {
        name+'/private.pem': key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                               serialization.BestAvailableEncryption(passwords[name]))
        for name, key in zip(('root-a', 'root-b'), roots)}
    capsule = encode(dict(schema=1, producer=PRODUCER, origin=origin, trusted_root_sha256=pin,
                          passwords={name: base64.b64encode(password).decode('ascii') for name, password in passwords.items()}))
    payloads.update({'recovery.dpapi': recovery.crypt(capsule), 'host-input/trusted-root.json': root_wire,
                     'host-input/origin.json': encode(dict(schema=1, origin=origin, trusted_root_sha256=pin)),
                     'host-input/catalog-1.pem': online.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())})
    for name in MEMBERS: recovery.new_file(directory/name, payloads[name])
    receipt = dict(schema=1, producer=PRODUCER, phase='complete', origin=origin, trusted_root_sha256=pin,
                   root_keyids=[key_id(public_key(k)) for k in roots], catalog_keyid=key_id(public_key(online)),
                   files={name: hashlib.sha256(payloads[name]).hexdigest() for name in MEMBERS})
    recovery.new_file(directory/'ceremony.complete', encode(receipt))
    require(recovery.read_file(directory/'ceremony.json') == preparing, 'Root ceremony changed before commit')
    os.replace(directory/'ceremony.complete', directory/'ceremony.json')
    return check(directory)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', required=True, type=Path)
    parser.add_argument('--origin')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--apply', action='store_true')
    mode.add_argument('--check', action='store_true')
    args = parser.parse_args()
    try:
        require(os.name == 'nt', 'Prepare this off-server recovery store on Windows')
        if args.check:
            require(args.origin is None, 'Recovery check takes its origin from the ceremony')
            result = check(args.directory)
        else:
            checked_origin(args.origin)
            result = prepare(args.directory, args.origin) if args.apply else dict(schema=1, phase='plan-only',
                directory=str(args.directory.absolute()), origin=args.origin, account_bound=True,
                root_keys_exported=False, services_started=False, network_started=False)
        print(json.dumps(result, indent=2)); return 0
    except (OSError, ValueError, ModError) as error:
        print('Windows workshop trust: '+str(error)[:2000], file=sys.stderr); return 2


if __name__ == '__main__':
    raise SystemExit(main())
