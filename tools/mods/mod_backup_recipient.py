"""Independent backup recipient custody; not a signing key or copy scheduler.

Only public recipient metadata belongs on the host. Private material stays in
memory, Windows account-scoped DPAPI or a passphrase-authenticated envelope.
"""
import hashlib
from itertools import islice
import os
from pathlib import Path
import secrets

from cryptography.exceptions import InvalidTag
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey,X25519PublicKey
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

from mod_package import ModError,SHA,read_json,require
from mod_portable_recovery import derive,KDF
from mod_trust import b64,unb64,encode,origin

TYPE='tsfp-backup-recipient-v1'
RECOVERY='tsfp-backup-recipient-recovery-v1'
DOMAIN=b'TSFP-BACKUP-RECIPIENT-V1\0'
MAX_PLAIN=4096
MAX_WIRE=8192
FILES={'recipient.json','recipient.dpapi'}


def profile(service_origin,expected_pin):
    origin(service_origin)
    require(isinstance(expected_pin,str) and SHA.fullmatch(expected_pin),'Supply the independently recorded workshop root SHA256')


def key_id(raw):return hashlib.sha256(DOMAIN+raw).hexdigest()


def public(private,service_origin,expected_pin):
    profile(service_origin,expected_pin)
    require(isinstance(private,X25519PrivateKey),'Use an independent X25519 backup recipient key')
    raw=private.public_key().public_bytes(serialization.Encoding.Raw,serialization.PublicFormat.Raw)
    return dict(schema=1,type=TYPE,algorithm='x25519',origin=service_origin,trusted_root_sha256=expected_pin,
                public_key=b64(raw),key_id=key_id(raw))


def checked_public(value,service_origin,expected_pin):
    profile(service_origin,expected_pin)
    require(isinstance(value,dict) and set(value)=={'schema','type','algorithm','origin','trusted_root_sha256','public_key','key_id'}
            and type(value['schema']) is int and value['schema']==1 and value['type']==TYPE and value['algorithm']=='x25519'
            and value['origin']==service_origin and value['trusted_root_sha256']==expected_pin,'Backup recipient authority differs')
    raw=unb64(value['public_key'],32)
    require(len(raw)==32 and b64(raw)==value['public_key'] and value['key_id']==key_id(raw),'Backup recipient public identity differs')
    try:
        key=X25519PublicKey.from_public_bytes(raw)
        X25519PrivateKey.generate().exchange(key)  # Reject low-order/null-shared-secret recipients before export.
    except ValueError as error:raise ModError('Invalid backup recipient public key') from error
    return key


def secret(private,service_origin,expected_pin):
    recipient=public(private,service_origin,expected_pin)
    raw=private.private_bytes(serialization.Encoding.Raw,serialization.PrivateFormat.Raw,serialization.NoEncryption())
    return encode(dict(schema=1,type=TYPE,recipient=recipient,private_key=b64(raw)))


def checked_secret(wire,service_origin,expected_pin):
    require(isinstance(wire,bytes) and 0<len(wire)<=MAX_PLAIN,'Backup recipient secret exceeds budget')
    value=read_json(wire)
    require(isinstance(value,dict) and set(value)=={'schema','type','recipient','private_key'}
            and type(value['schema']) is int and value['schema']==1 and value['type']==TYPE and encode(value)==wire,
            'Unknown backup recipient secret')
    checked_public(value['recipient'],service_origin,expected_pin)
    raw=unb64(value['private_key'],32)
    require(len(raw)==32 and b64(raw)==value['private_key'],'Invalid backup recipient private encoding')
    private=X25519PrivateKey.from_private_bytes(raw)
    require(public(private,service_origin,expected_pin)==value['recipient'],'Backup recipient private/public keys differ')
    return private


def aad(recipient):
    return encode(dict(schema=1,type=RECOVERY,kdf=KDF,cipher='aes256gcm',recipient=recipient))


def seal(private,password,service_origin,expected_pin):
    recipient=public(private,service_origin,expected_pin);plain=secret(private,service_origin,expected_pin)
    salt,nonce=secrets.token_bytes(32),secrets.token_bytes(12)
    ciphertext=AESGCM(derive(password,salt)).encrypt(nonce,plain,aad(recipient))
    wire=encode(dict(schema=1,type=RECOVERY,kdf=KDF,cipher='aes256gcm',recipient=recipient,
                     salt=b64(salt),nonce=b64(nonce),ciphertext=b64(ciphertext)))
    require(len(wire)<=MAX_WIRE,'Backup recipient recovery exceeds budget')
    return wire


def unseal(wire,password,service_origin,expected_pin):
    profile(service_origin,expected_pin)
    require(isinstance(wire,bytes) and 0<len(wire)<=MAX_WIRE,'Backup recipient recovery exceeds budget')
    value=read_json(wire)
    require(isinstance(value,dict) and set(value)=={'schema','type','kdf','cipher','recipient','salt','nonce','ciphertext'}
            and type(value['schema']) is int and value['schema']==1 and value['type']==RECOVERY
            and value['kdf']==KDF and value['cipher']=='aes256gcm' and wire==encode(value),'Unknown backup recipient recovery envelope')
    checked_public(value['recipient'],service_origin,expected_pin)
    salt,nonce,ciphertext=unb64(value['salt'],32),unb64(value['nonce'],12),unb64(value['ciphertext'],MAX_PLAIN+16)
    require(len(salt)==32 and len(nonce)==12 and 16<len(ciphertext)<=MAX_PLAIN+16
            and all(b64(raw)==value[key] for key,raw in (('salt',salt),('nonce',nonce),('ciphertext',ciphertext))),
            'Invalid backup recipient envelope sizes/encoding')
    try:plain=AESGCM(derive(password,salt)).decrypt(nonce,ciphertext,aad(value['recipient']))
    except InvalidTag as error:raise ModError('Cannot authenticate backup recipient recovery') from error
    private=checked_secret(plain,service_origin,expected_pin)
    require(public(private,service_origin,expected_pin)==value['recipient'],'Recovered backup recipient differs')
    return private


def write_windows(private,destination,service_origin,expected_pin):
    """New-only private store; partial output remains for inspection on IO error."""
    from mod_windows_recovery import private as private_path,crypt,new_file
    require(os.name=='nt','Manage backup recipient custody on Windows')
    recipient=public(private,service_origin,expected_pin);plain=secret(private,service_origin,expected_pin)
    destination=private_path(Path(destination).absolute(),create=True)
    encrypted=crypt(plain)
    new_file(destination/'recipient.dpapi',encrypted)
    new_file(destination/'recipient.json',encode(recipient))  # Completion record last; never plaintext private material.
    return recipient


def create_windows(destination,service_origin,expected_pin):
    profile(service_origin,expected_pin)
    require(os.name=='nt','Manage backup recipient custody on Windows')
    return write_windows(X25519PrivateKey.generate(),destination,service_origin,expected_pin)


def load_windows(destination,service_origin,expected_pin):
    from mod_windows_recovery import private as private_path,crypt,read_file
    profile(service_origin,expected_pin)
    destination=private_path(destination)
    with os.scandir(destination) as stream:names=[item.name for item in islice(stream,len(FILES)+1)]
    require(len(names)==len(FILES) and set(names)==FILES,'Preserving incomplete or foreign backup recipient store')
    wire=read_file(destination/'recipient.json',MAX_PLAIN);recipient=read_json(wire)
    require(encode(recipient)==wire,'Noncanonical backup recipient record')
    checked_public(recipient,service_origin,expected_pin)
    private=checked_secret(crypt(read_file(destination/'recipient.dpapi'),decrypt=True),service_origin,expected_pin)
    require(public(private,service_origin,expected_pin)==recipient,'Stored backup recipient differs')
    return private


def export_windows(destination,password,service_origin,expected_pin):
    return seal(load_windows(destination,service_origin,expected_pin),password,service_origin,expected_pin)


def import_windows(wire,password,destination,service_origin,expected_pin):
    require(os.name=='nt','Manage backup recipient custody on Windows')
    private=unseal(wire,password,service_origin,expected_pin)
    return write_windows(private,destination,service_origin,expected_pin)
