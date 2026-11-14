"""Bounded workshop metadata signatures. Trust starts with an out-of-band root.

Sign exact bytes, not a reserialized JSON object. The native verifier must use
the same domain separator, RSA parameters and wire limits (see mod-catalog-trust.md).
No uploaded key, TLS response, or self-signed root establishes initial trust.
"""
import base64
import binascii
import json
import re
import time
from urllib.parse import urlsplit

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding, rsa

from mod_package import SHA, ModError, digest, read_json, require

DOMAIN = b'TSFP-WORKSHOP-METADATA-V1\0'
SCHEME = 'rsa-pkcs1-sha256'
MAX_METADATA = 2 * 1024 * 1024
MAX_ROOT = 64 * 1024
MAX_REVOKED = 4000  # Below the shared4096-member JSON collection limit.
MAX_VERSION = 2**53 - 1
CATALOG_TTL = 86400
ROOT_TTL = 366 * 86400


def encode(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'),
                      ensure_ascii=True, allow_nan=False).encode('ascii')


def b64(data):
    return base64.b64encode(data).decode('ascii')


def unb64(value, maximum):
    require(isinstance(value, str) and len(value) <= 4 * ((maximum + 2) // 3),
            'Invalid metadata base64 length')
    try:
        data = base64.b64decode(value, validate=True)
    except (ValueError, binascii.Error) as exc:
        raise ModError('Invalid metadata base64') from exc
    require(len(data) <= maximum and b64(data) == value, 'Noncanonical metadata base64')
    return data


def fields(value, names):
    require(isinstance(value, dict) and set(value) == set(names.split()),
            'Unexpected metadata fields')


def integer(value, minimum=1, maximum=MAX_VERSION):
    require(type(value) is int and minimum <= value <= maximum, 'Invalid metadata integer')
    return value


def origin(value):
    require(isinstance(value, str) and len(value) <= 255 and value.isascii()
            and not any(c.isspace() or ord(c) < 32 for c in value), 'Invalid metadata origin')
    try:
        url = urlsplit(value)
        valid = (url.scheme == 'https' and url.hostname and url.netloc == url.hostname
                 and url.path == '' and not url.query and not url.fragment
                 and re.fullmatch(r'(?=.{1,253}$)(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+'
                                  r'[a-z](?:[a-z0-9-]{0,61}[a-z0-9])?', url.hostname)
                 and '\\' not in value)
    except ValueError:
        valid = False
    require(valid, 'Trust metadata requires a canonical HTTPS origin without a port or path')
    return value


def public_key(private):
    if isinstance(private, dict):
        load_public(private)
        return private
    numbers = private.public_key().public_numbers()
    require(private.key_size == 3072 and numbers.e == 65537, 'Expected RSA-3072 key')
    return {'scheme': SCHEME, 'modulus': b64(numbers.n.to_bytes(384, 'big')), 'exponent': 'AQAB'}


def key_id(key):
    return digest(encode(key))


def load_public(key):
    fields(key, 'scheme modulus exponent')
    require(key['scheme'] == SCHEME and key['exponent'] == 'AQAB', 'Unsupported signing key')
    modulus = unb64(key['modulus'], 384)
    require(len(modulus) == 384 and modulus[0] & 0x80 and modulus[-1] & 1,
            'Invalid RSA-3072 modulus')
    return rsa.RSAPublicNumbers(65537, int.from_bytes(modulus, 'big')).public_key()


def sign(payload, private_keys):
    raw = encode(payload)
    require(len(raw) <= MAX_METADATA // 2, 'Signing payload exceeds budget')
    signatures = []
    for private in private_keys:
        signatures.append({'keyid': key_id(public_key(private)),
                           'signature': b64(private.sign(DOMAIN + raw, padding.PKCS1v15(), hashes.SHA256()))})
    wire = encode({'payload': b64(raw), 'signatures': signatures})
    # Check local signing output using the same bounded envelope parser.
    envelope(wire)
    return wire


def envelope(wire, limit=MAX_METADATA):
    require(isinstance(wire, bytes) and len(wire) <= limit, 'Metadata envelope exceeds budget')
    data = read_json(wire)
    fields(data, 'payload signatures')
    raw = unb64(data['payload'], limit // 2)
    signatures = data['signatures']
    require(isinstance(signatures, list) and 1 <= len(signatures) <= 8, 'Invalid signature count')
    seen = set()
    for signature in signatures:
        fields(signature, 'keyid signature')
        identity = signature['keyid']
        require(isinstance(identity, str) and SHA.fullmatch(identity) and identity not in seen,
                'Duplicate or invalid signing identity')
        seen.add(identity)
        require(len(unb64(signature['signature'], 384)) == 384, 'Invalid signature size')
    return raw, signatures


def verify_role(wire, root, role, limit=MAX_METADATA):
    raw, signatures = envelope(wire, limit)
    policy = root['roles'][role]
    valid = set()
    for signature in signatures:
        identity = signature['keyid']
        if identity not in policy['keyids']:
            continue  # A root rotation carries old AND new root signatures.
        try:
            load_public(root['keys'][identity]).verify(
                unb64(signature['signature'], 384), DOMAIN + raw, padding.PKCS1v15(), hashes.SHA256())
            valid.add(identity)
        except InvalidSignature:
            pass
    require(len(valid) >= policy['threshold'], 'Metadata signature threshold not met')
    return read_json(raw)


def root_document(wire):
    raw, _ = envelope(wire, MAX_ROOT)
    root = read_json(raw)
    fields(root, 'type schema version origin issued expires keys roles')
    require(root['type'] == 'tsfp-workshop-root' and type(root['schema']) is int
            and root['schema'] == 1, 'Unsupported root metadata')
    integer(root['version'])
    origin(root['origin'])
    integer(root['issued'], 0)
    integer(root['expires'], 0)
    require(0 < root['expires'] - root['issued'] <= ROOT_TTL, 'Invalid root lifetime')
    keys = root['keys']
    require(isinstance(keys, dict) and 3 <= len(keys) <= 8, 'Invalid root key count')
    for identity, key in keys.items():
        require(key_id(key) == identity, 'Root key identifier mismatch')
        load_public(key)
    fields(root['roles'], 'root catalog')
    all_ids = set()
    for name, policy in root['roles'].items():
        fields(policy, 'keyids threshold')
        ids = policy['keyids']
        require(isinstance(ids, list) and 1 <= len(ids) <= 4
                and all(isinstance(i, str) and i in keys for i in ids)
                and len(set(ids)) == len(ids) and not all_ids.intersection(ids),
                'Signing roles must use distinct known keys')
        integer(policy['threshold'], 2 if name == 'root' else 1, len(ids))
        all_ids.update(ids)
    require(all_ids == set(keys), 'Unused root keys')
    return root


def trusted_root(pinned_wire, updates=(), now=None):
    """Caller supplies the release-pinned root, NEVER one downloaded as a new pin.

    Expired old roots may authorize sequential rotations; the final root must be
    live. Each successor must meet BOTH the previous and its own root threshold.
    """
    now = int(time.time()) if now is None else integer(now, 0)
    require(isinstance(updates, (list, tuple)) and len(updates) <= 64, 'Too many root rotations')
    root = root_document(pinned_wire)
    verify_role(pinned_wire, root, 'root', MAX_ROOT)
    for wire in updates:
        candidate = root_document(wire)
        require(candidate['version'] == root['version'] + 1 and candidate['origin'] == root['origin']
                and candidate['issued'] >= root['issued'] and candidate['issued'] <= now,
                'Invalid root rotation sequence or origin')
        verify_role(wire, root, 'root', MAX_ROOT)
        verify_role(wire, candidate, 'root', MAX_ROOT)
        root = candidate
    require(root['issued'] <= now < root['expires'], 'Trusted root is expired or not yet valid')
    return root


def make_root(root_keys, catalog_keys, version, service_origin, now, old_root_keys=()):
    """Offline ceremony; catalog_keys may be public descriptors, without secrets."""
    require(len(root_keys) >= 2 and len(catalog_keys) >= 1, 'Root and catalog keys required')
    keys = {key_id(public_key(k)): public_key(k) for k in [*root_keys, *catalog_keys]}
    document = {'type': 'tsfp-workshop-root', 'schema': 1, 'version': version,
                'origin': service_origin, 'issued': now, 'expires': now + ROOT_TTL,
                'keys': keys, 'roles': {
                    'root': {'keyids': [key_id(public_key(k)) for k in root_keys], 'threshold': 2},
                    'catalog': {'keyids': [key_id(public_key(k)) for k in catalog_keys], 'threshold': 1}}}
    # Deduplicate overlapping old/new keys without counting signatures twice.
    signers = {key_id(public_key(k)): k for k in [*old_root_keys, *root_keys]}
    wire = sign(document, signers.values())
    root_document(wire)
    return wire


def checkpoint(value):
    fields(value, 'root_version version metadata_sha256 observed revoked')
    integer(value['root_version'])
    integer(value['version'])
    integer(value['observed'], 0)
    require(isinstance(value['metadata_sha256'], str) and SHA.fullmatch(value['metadata_sha256']),
            'Invalid catalog checkpoint digest')
    revocations(value['revoked'])
    return value


def revocations(values):
    require(isinstance(values, list) and len(values) <= MAX_REVOKED
            and all(isinstance(v, str) and SHA.fullmatch(v) for v in values)
            and values == sorted(set(values)), 'Invalid revocation list')
    return values


def verify_catalog(wire, catalog_bytes, root, previous=None, now=None):
    now = int(time.time()) if now is None else integer(now, 0)
    data = verify_role(wire, root, 'catalog')
    fields(data, 'type schema origin root_version version generated expires catalog_sha256 catalog_length revoked')
    require(data['type'] == 'tsfp-workshop-catalog' and type(data['schema']) is int and data['schema'] == 1
            and data['origin'] == root['origin'] and data['root_version'] == root['version'],
            'Catalog trust identity mismatch')
    integer(data['root_version'])
    integer(data['version'])
    integer(data['generated'], 0)
    integer(data['expires'], 0)
    integer(data['catalog_length'], 1, 4 * 1024 * 1024)
    require(root['issued'] <= now < root['expires'], 'Catalog root has expired')
    require(root['issued'] <= data['generated'] <= now < data['expires']
            and 0 < data['expires'] - data['generated'] <= CATALOG_TTL
            and data['expires'] <= root['expires'], 'Catalog has expired or invalid dates')
    require(isinstance(catalog_bytes, bytes) and len(catalog_bytes) == data['catalog_length']
            and digest(catalog_bytes) == data['catalog_sha256'], 'Signed catalog bytes changed')
    revocations(data['revoked'])
    current_hash = digest(wire)
    if previous is not None:
        checkpoint(previous)
        require(now >= previous['observed'], 'Clock moved backwards since the last trusted catalog')
        require(data['root_version'] >= previous['root_version'] and data['version'] >= previous['version'],
                'Catalog rollback rejected')
        require(data['version'] != previous['version'] or current_hash == previous['metadata_sha256'],
                'Catalog version was reused for different metadata')
        require(set(previous['revoked']).issubset(data['revoked']), 'Catalog forgot an earlier revocation')
    # Validate dates in the hash-bound legacy catalog too. It cannot extend trust.
    catalog = read_json(catalog_bytes)
    require(isinstance(catalog, dict) and type(catalog.get('schema')) is int and catalog['schema'] == 1
            and type(catalog.get('generated')) is int and type(catalog.get('expires')) is int
            and catalog['generated'] == data['generated'] and catalog['expires'] == data['expires'],
            'Catalog dates differ from signed metadata')
    releases = catalog.get('releases')
    require(isinstance(releases, list) and len(releases) <= 1000, 'Invalid signed catalog release count')
    for release in releases:
        require(isinstance(release, dict) and release.get('sha256') not in data['revoked'],
                'Catalog advertises a revoked package')
    state = {'root_version': data['root_version'], 'version': data['version'],
             'metadata_sha256': current_hash, 'observed': now, 'revoked': data['revoked']}
    return catalog, state


class PublicationSigner:
    """Only catalog private keys belong on the online publication host."""
    def __init__(self, pinned_root, private_keys, updates=()):
        self.pinned_root = pinned_root
        self.updates = tuple(updates)
        self.private_keys = tuple(private_keys)

    def publish(self, catalog_bytes, version, revoked, now):
        root = trusted_root(self.pinned_root, self.updates, now)
        catalog = read_json(catalog_bytes)
        data = {'type': 'tsfp-workshop-catalog', 'schema': 1, 'origin': root['origin'],
                'root_version': root['version'], 'version': version,
                'generated': catalog['generated'], 'expires': catalog['expires'],
                'catalog_sha256': digest(catalog_bytes), 'catalog_length': len(catalog_bytes),
                'revoked': sorted(set(revoked))}
        wire = sign(data, self.private_keys)
        verify_catalog(wire, catalog_bytes, root, now=now)
        roots = [self.pinned_root, *self.updates]
        files = {'v1/trust/catalog': wire}
        for root_wire in roots:
            version = root_document(root_wire)['version']
            files['v1/trust/roots/' + str(version)] = root_wire
        return files
