"""Bounded recipient-encrypted files, not an archive parser or transfer service.

Outputs are new-only private staging files. A failed operation leaves its partial
output private and uncommitted. Callers must not restore it. Encryption does not
identify the sender: decryption requires an independently authenticated ciphertext
SHA256, e.g. from the future pinned-host transfer receipt.
"""
from contextlib import contextmanager
import hashlib
import os
from pathlib import Path
import secrets
import shutil
import stat
import struct

from cryptography.exceptions import InvalidTag
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey,X25519PublicKey
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.hkdf import HKDF

import mod_backup_recipient as recipients
from mod_package import ModError,SHA,read_json,require
from mod_trust import b64,unb64,encode

MAGIC=b'TSFPBE1\n'
TYPE='tsfp-backup-file-v1'
DOMAIN=b'TSFP-BACKUP-FILE-V1\0'
CHUNK=1024*1024
MAX_PLAIN=8*1024**3+1024**2+12  # Snapshot payload budget plus bounded archive framing.
MAX_HEADER=4096
MIN_FREE=2*1024**3
MAX_WIRE=MAX_PLAIN+16*((MAX_PLAIN+CHUNK-1)//CHUNK+1)+32+MAX_HEADER+len(MAGIC)+4


def digest(value):return hashlib.sha256(value).hexdigest()


def checked_sha(value):
    require(isinstance(value,str) and SHA.fullmatch(value),'Supply the independently authenticated file SHA256')
    return value


def private_path(path):
    path=Path(os.path.abspath(path))
    if os.name=='nt':
        from mod_windows_recovery import private
        private(path.parent)
    else:
        from mod_backup import private_directory
        private_directory(path.parent)
    return path


def file_stamp(info):
    # Windows path stat reports creation time here while fstat reports change
    # time. Both expose the same identity/size/mtime; content is hash-checked.
    return (info.st_dev,info.st_ino,info.st_size,info.st_mtime_ns)+(() if os.name=='nt' else (info.st_ctime_ns,))


def regular(path,maximum):
    if os.name=='nt':
        from mod_windows_recovery import private
        private(path)
    info=path.lstat()
    require(stat.S_ISREG(info.st_mode) and info.st_nlink==1 and 0<=info.st_size<=maximum
            and (os.name=='nt' or (info.st_uid==os.geteuid() and not info.st_mode&0o077)),
            'Unsafe or oversized private backup file')
    return info


@contextmanager
def incoming(path,maximum):
    path=private_path(path);before=regular(path,maximum)
    with os.fdopen(os.open(path,os.O_RDONLY|getattr(os,'O_NOFOLLOW',0)|getattr(os,'O_NONBLOCK',0)),'rb') as stream:
        require(file_stamp(os.fstat(stream.fileno()))==file_stamp(before),'Backup source changed before opening')
        def stable():
            require(file_stamp(os.fstat(stream.fileno()))==file_stamp(before)
                    and file_stamp(regular(path,maximum))==file_stamp(before),'Backup source changed while processing')
        yield stream,before.st_size,stable
        stable()


@contextmanager
def outgoing(path,expected_size):
    path=private_path(path)
    require(shutil.disk_usage(path.parent).free>=expected_size+MIN_FREE,'Insufficient encrypted backup staging headroom')
    with os.fdopen(os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL|getattr(os,'O_NOFOLLOW',0),0o600),'wb') as stream:
        yield stream
        stream.flush();os.fsync(stream.fileno())
        info=regular(path,MAX_WIRE)
        require(file_stamp(info)==file_stamp(os.fstat(stream.fileno())),'Backup output changed before completion')


def raw(value,length):
    decoded=unb64(value,length)
    require(len(decoded)==length and b64(decoded)==value,'Invalid backup encryption parameter')
    return decoded


def checked_header(wire,service_origin,pin):
    require(isinstance(wire,bytes) and 0<len(wire)<=MAX_HEADER,'Backup header exceeds budget')
    value=read_json(wire)
    require(isinstance(value,dict) and set(value)=={'schema','type','cipher','kdf','recipient','ephemeral','salt','nonce','plain_bytes'}
            and type(value['schema']) is int and value['schema']==1 and value['type']==TYPE
            and value['cipher']=='aes256gcm-chunks' and value['kdf']=='hkdf-sha256' and encode(value)==wire
            and type(value['plain_bytes']) is int and 0<=value['plain_bytes']<=MAX_PLAIN,'Unsupported backup encryption header')
    recipients.checked_public(value['recipient'],service_origin,pin)
    raw(value['ephemeral'],32);raw(value['salt'],32);raw(value['nonce'],8)
    return value


def derive(shared,header,wire):
    return AESGCM(HKDF(algorithm=hashes.SHA256(),length=32,salt=raw(header['salt'],32),info=DOMAIN+hashlib.sha256(wire).digest()).derive(shared))


def parameters(header,wire,index,length):
    counter=struct.pack('>I',index)
    return raw(header['nonce'],8)+counter,DOMAIN+hashlib.sha256(wire).digest()+counter+struct.pack('>I',length)


def wire_size(header,length):
    return len(MAGIC)+4+length+header['plain_bytes']+16*((header['plain_bytes']+CHUNK-1)//CHUNK+1)+32


def encrypt_file(source,destination,recipient,service_origin,pin,*,plain_bytes,plain_sha256):
    with incoming(source,MAX_PLAIN) as (input,size,stable):
        require(size==plain_bytes,'Verified backup size differs before encryption')
        return encrypt_stream(input,destination,recipient,service_origin,pin,
                              plain_bytes=plain_bytes,plain_sha256=plain_sha256,verify_source=stable)


def encrypt_stream(input,destination,recipient,service_origin,pin,*,plain_bytes,plain_sha256,verify_source):
    """Encrypt a trusted bounded reader, checking its preverified digest at EOF.

    Readers must return requested lengths (or EOF); verify_source must refuse any
    source/inventory change. No plaintext archive or unbounded buffer is created.
    """
    require(callable(verify_source),'A source stability check is required')
    checked_sha(plain_sha256)
    require(type(plain_bytes) is int and 0<=plain_bytes<=MAX_PLAIN,'Invalid backup plaintext size')
    recipients.checked_public(recipient,service_origin,pin)
    recipient=read_json(encode(recipient))  # Freeze a canonical caller-owned public record before deriving the key.
    public=recipients.checked_public(recipient,service_origin,pin);ephemeral=X25519PrivateKey.generate()
    header=dict(schema=1,type=TYPE,cipher='aes256gcm-chunks',kdf='hkdf-sha256',recipient=recipient,
        ephemeral=b64(ephemeral.public_key().public_bytes(serialization.Encoding.Raw,serialization.PublicFormat.Raw)),
        salt=b64(secrets.token_bytes(32)),nonce=b64(secrets.token_bytes(8)),plain_bytes=plain_bytes)
    wire=encode(header);checked_header(wire,service_origin,pin);cipher=derive(ephemeral.exchange(public),header,wire)
    encrypted=hashlib.sha256();plain=hashlib.sha256()
    with outgoing(destination,wire_size(header,len(wire))) as output:
        def write(data):output.write(data);encrypted.update(data)
        write(MAGIC+struct.pack('>I',len(wire))+wire)
        index=0;remaining=plain_bytes
        while remaining:
            length=min(CHUNK,remaining);data=input.read(length)
            require(len(data)==length,'Backup plaintext was truncated')
            nonce,aad=parameters(header,wire,index,length);write(cipher.encrypt(nonce,data,aad));plain.update(data)
            remaining-=length;index+=1
        require(input.read(1)==b'' and plain.hexdigest()==plain_sha256,'Verified backup plaintext changed')
        verify_source();nonce,aad=parameters(header,wire,index,0);write(cipher.encrypt(nonce,plain.digest(),aad))
    return dict(schema=1,phase='backup-file-encrypted',bytes=wire_size(header,len(wire)),plain_bytes=plain_bytes,
                plain_sha256=plain_sha256,cipher_sha256=encrypted.hexdigest(),key_id=recipient['key_id'])


def decrypt_file(source,destination,private,service_origin,pin,*,cipher_sha256):
    checked_sha(cipher_sha256);expected=recipients.public(private,service_origin,pin)
    with incoming(source,MAX_WIRE) as (input,size,stable):
        # Authenticate the transport receipt before creating plaintext, including
        # when corruption occurs late in a large file. No sender identity is
        # inferred from possession of the public encryption key.
        checksum=hashlib.sha256();remaining=size
        while remaining:
            data=input.read(min(CHUNK,remaining));require(data,'Truncated encrypted backup')
            checksum.update(data);remaining-=len(data)
        require(input.read(1)==b'' and checksum.hexdigest()==cipher_sha256,'Encrypted backup differs from authenticated receipt')
        stable();input.seek(0)
        consumed=hashlib.sha256()
        def read(length):
            value=input.read(length);consumed.update(value);return value
        require(read(len(MAGIC))==MAGIC,'Unknown encrypted backup magic')
        prefix=read(4);require(len(prefix)==4,'Truncated encrypted backup header')
        length=struct.unpack('>I',prefix)[0];require(0<length<=MAX_HEADER,'Backup header exceeds budget')
        wire=read(length);header=checked_header(wire,service_origin,pin)
        require(header['recipient']==expected and size==wire_size(header,length),'Encrypted backup recipient or file size differs')
        try:shared=private.exchange(X25519PublicKey.from_public_bytes(raw(header['ephemeral'],32)))
        except ValueError as error:raise ModError('Invalid backup ephemeral key') from error
        cipher=derive(shared,header,wire);plain=hashlib.sha256();remaining=header['plain_bytes'];index=0
        with outgoing(destination,header['plain_bytes']) as output:
            while True:
                length=min(CHUNK,remaining);frame_length=length+16 if remaining else 48
                data=read(frame_length)
                require(len(data)==frame_length,'Truncated encrypted backup chunk')
                nonce,aad=parameters(header,wire,index,length)
                try:decoded=cipher.decrypt(nonce,data,aad)
                except InvalidTag as error:raise ModError('Cannot authenticate encrypted backup chunk') from error
                if not remaining:
                    require(decoded==plain.digest(),'Encrypted backup terminal plaintext digest differs')
                    break  # Mandatory encrypted terminal digest; no plaintext hash is exposed in the header.
                output.write(decoded);plain.update(decoded);remaining-=length;index+=1
            require(read(1)==b'','Backup final boundary differs')
            require(consumed.hexdigest()==cipher_sha256,'Encrypted backup changed after receipt verification')
            stable()
    return dict(schema=1,phase='backup-file-decrypted',plain_bytes=header['plain_bytes'],plain_sha256=plain.hexdigest(),
                cipher_sha256=cipher_sha256,key_id=expected['key_id'])
