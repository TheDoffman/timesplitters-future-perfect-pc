"""Passphrase-encrypted off-server trust recovery; no network or plaintext export."""
import hashlib
import importlib.util
import os
from pathlib import Path
import secrets
import time

from cryptography.exceptions import InvalidTag
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.scrypt import Scrypt

from mod_package import ModError, SHA, read_json, require
from mod_trust import b64, encode, key_id, public_key, root_document, sign, unb64, verify_role

TYPE = 'tsfp-portable-trust-recovery-v1'
KDF = 'scrypt-n131072-r8-p1'
AAD = encode(dict(schema=1, type=TYPE, kdf=KDF, cipher='aes256gcm'))
MAX_WIRE = 65536
MAX_PLAIN = 32768
MEMBERS = ('root-a/private.pem', 'root-b/private.pem', 'host-input/origin.json',
           'host-input/trusted-root.json', 'host-input/catalog-1.pem', 'passwords.json')


def ceremony():
    spec = importlib.util.spec_from_file_location('portable_windows_ceremony',
        Path(__file__).with_name('hosting')/'prepare-workshop-trust-windows.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module


def pin(value):
    require(isinstance(value, str) and SHA.fullmatch(value), 'Supply the independently recorded public root SHA256')
    return value


def password_bytes(password):
    require(isinstance(password, str) and 16 <= len(password) <= 256,
            'Recovery passphrase must contain 16 to 256 characters')
    value = password.encode('utf-8')
    require(len(value) <= 1024, 'Recovery passphrase exceeds budget')
    return value


def derive(password, salt):
    return Scrypt(salt=salt, length=32, n=131072, r=8, p=1).derive(password_bytes(password))


def validate(payloads, expected_pin):
    pin(expected_pin)
    require(isinstance(payloads, dict) and set(payloads) == set(MEMBERS)
            and all(isinstance(v, bytes) and 0 < len(v) <= 8192 for v in payloads.values()),
            'Invalid portable recovery inventory')
    wire = payloads['host-input/trusted-root.json']
    require(hashlib.sha256(wire).hexdigest() == expected_pin, 'Portable recovery public pin differs')
    root = root_document(wire); verify_role(wire, root, 'root')
    require(root['roles']['root']['threshold'] == 2 and len(root['roles']['root']['keyids']) == 2
            and len(root['roles']['catalog']['keyids']) == 1, 'Unexpected recovery authority')
    settings = read_json(payloads['host-input/origin.json'])
    require(settings == dict(schema=1, origin=root['origin'], trusted_root_sha256=expected_pin),
            'Recovery origin differs')
    capsule = read_json(payloads['passwords.json'])
    require(isinstance(capsule, dict) and set(capsule) == {'schema', 'producer', 'origin', 'trusted_root_sha256', 'passwords'}
            and type(capsule['schema']) is int and capsule['schema'] == 1
            and capsule['producer'] == 'tsfp-windows-root-ceremony-v1'
            and capsule['origin'] == root['origin'] and capsule['trusted_root_sha256'] == expected_pin
            and isinstance(capsule['passwords'], dict) and set(capsule['passwords']) == {'root-a', 'root-b'},
            'Recovery password capsule differs')
    keys = []
    for name in ('root-a', 'root-b'):
        password = unb64(capsule['passwords'][name], 32)
        require(len(password) == 32 and b'ENCRYPTED PRIVATE KEY' in payloads[name+'/private.pem'],
                'Recovery root key must remain encrypted')
        key = serialization.load_pem_private_key(payloads[name+'/private.pem'], password)
        require(isinstance(key, rsa.RSAPrivateKey), 'Recovery requires RSA root keys')
        keys.append(key)
    ids = [key_id(public_key(k)) for k in keys]
    require(len(set(ids)) == 2 and ids == root['roles']['root']['keyids'], 'Recovered root keys differ')
    online = serialization.load_pem_private_key(payloads['host-input/catalog-1.pem'], None)
    require(isinstance(online, rsa.RSAPrivateKey), 'Recovery requires an RSA catalog key')
    catalog_id = key_id(public_key(online))
    require(root['roles']['catalog']['keyids'] == [catalog_id] and catalog_id not in ids,
            'Recovered catalog key differs')
    challenge = dict(type='tsfp-portable-recovery-check', root_sha256=expected_pin, nonce=secrets.token_hex(16))
    require(verify_role(sign(challenge, keys), root, 'root') == challenge, 'Root threshold recovery proof failed')
    return dict(schema=1, phase='portable-recovery-verified', origin=root['origin'],
                trusted_root_sha256=expected_pin, root_keyids=ids, catalog_keyid=catalog_id,
                root_valid_now=root['issued'] <= int(time.time()) < root['expires'],
                account_bound=False, services_started=False, network_started=False)


def seal(payloads, password, expected_pin):
    validate(payloads, expected_pin)
    plain = encode(dict(schema=1, type=TYPE, files={k: b64(v) for k, v in payloads.items()}))
    require(len(plain) <= MAX_PLAIN, 'Portable recovery exceeds plaintext budget')
    salt, nonce = secrets.token_bytes(32), secrets.token_bytes(12)
    encrypted = AESGCM(derive(password, salt)).encrypt(nonce, plain, AAD)
    result = encode(dict(schema=1, type=TYPE, kdf=KDF, cipher='aes256gcm',
                         salt=b64(salt), nonce=b64(nonce), ciphertext=b64(encrypted)))
    require(len(result) <= MAX_WIRE, 'Portable recovery exceeds wire budget')
    return result


def unseal(wire, password, expected_pin):
    pin(expected_pin)
    require(isinstance(wire, bytes) and 0 < len(wire) <= MAX_WIRE, 'Portable recovery exceeds wire budget')
    value = read_json(wire)
    require(isinstance(value, dict) and set(value) == {'schema', 'type', 'kdf', 'cipher', 'salt', 'nonce', 'ciphertext'}
            and type(value['schema']) is int and value['schema'] == 1
            and value['type'] == TYPE and value['kdf'] == KDF and value['cipher'] == 'aes256gcm',
            'Unsupported portable recovery format')
    salt, nonce = unb64(value['salt'], 32), unb64(value['nonce'], 12)
    encrypted = unb64(value['ciphertext'], MAX_PLAIN+16)
    require(len(salt) == 32 and len(nonce) == 12 and len(encrypted) > 16, 'Invalid recovery encryption fields')
    try:
        plain = AESGCM(derive(password, salt)).decrypt(nonce, encrypted, AAD)
    except InvalidTag as error:
        raise ModError('Recovery passphrase or encrypted file is incorrect') from error
    inner = read_json(plain)
    require(isinstance(inner, dict) and set(inner) == {'schema', 'type', 'files'}
            and type(inner['schema']) is int and inner['schema'] == 1 and inner['type'] == TYPE
            and isinstance(inner['files'], dict) and set(inner['files']) == set(MEMBERS),
            'Invalid encrypted recovery inventory')
    payloads = {k: unb64(v, 8192) for k, v in inner['files'].items()}
    return payloads, validate(payloads, expected_pin)


def export_windows(store, password, expected_pin):
    import mod_windows_recovery as recovery
    checked = ceremony().check(store)
    require(checked['trusted_root_sha256'] == pin(expected_pin), 'Source recovery public pin differs')
    payloads = {name: recovery.read_file(Path(store)/name) for name in MEMBERS if name != 'passwords.json'}
    payloads['passwords.json'] = recovery.crypt(recovery.read_file(Path(store)/'recovery.dpapi'), decrypt=True)
    return seal(payloads, password, expected_pin)


def import_windows(wire, password, expected_pin, destination):
    # Authenticate, pin and prove both roots BEFORE any filesystem mutation.
    payloads, result = unseal(wire, password, expected_pin)
    import mod_windows_recovery as recovery
    helper = ceremony()
    directory = recovery.private(destination, create=True)
    preparing = encode(dict(schema=1, producer=helper.PRODUCER, phase='preparing', origin=result['origin']))
    recovery.new_file(directory/'ceremony.json', preparing)
    for name in helper.DIRECTORIES: recovery.private(directory/name, create=True)
    files = {name: value for name, value in payloads.items() if name != 'passwords.json'}
    files['recovery.dpapi'] = recovery.crypt(payloads['passwords.json'])
    for name in helper.MEMBERS: recovery.new_file(directory/name, files[name])
    receipt = dict(schema=1, producer=helper.PRODUCER, phase='complete', origin=result['origin'],
        trusted_root_sha256=expected_pin, root_keyids=result['root_keyids'], catalog_keyid=result['catalog_keyid'],
        files={name: hashlib.sha256(value).hexdigest() for name, value in files.items()})
    recovery.new_file(directory/'ceremony.complete', encode(receipt))
    require(recovery.read_file(directory/'ceremony.json') == preparing, 'Recovery store changed before commit')
    os.replace(directory/'ceremony.complete', directory/'ceremony.json')
    return helper.check(directory)


def read_export(path):
    if os.name == 'nt':
        import mod_windows_recovery as recovery
        return recovery.read_file(path, MAX_WIRE)
    from mod_backup import private_directory, regular
    path = Path(os.path.abspath(path)); private_directory(path.parent)
    before = regular(path, MAX_WIRE)
    require(before.st_mode & 0o077 == 0, 'Recovery export must be private')
    with os.fdopen(os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK), 'rb') as stream:
        held = os.fstat(stream.fileno())
        fields = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)
        wire = stream.read(MAX_WIRE+1)
        require(fields(before) == fields(held) == fields(os.fstat(stream.fileno()))
                and len(wire) == held.st_size, 'Recovery export changed')
    return wire


def write_export(path, wire):
    require(isinstance(wire, bytes) and 0 < len(wire) <= MAX_WIRE, 'Portable recovery exceeds wire budget')
    if os.name == 'nt':
        import mod_windows_recovery as recovery
        recovery.new_file(path, wire)
        return
    from mod_backup import private_directory, sync_directory
    path = Path(os.path.abspath(path)); private_directory(path.parent)
    with os.fdopen(os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY | os.O_NOFOLLOW, 0o600), 'wb') as stream:
        stream.write(wire); stream.flush(); os.fsync(stream.fileno())
    sync_directory(path.parent)
