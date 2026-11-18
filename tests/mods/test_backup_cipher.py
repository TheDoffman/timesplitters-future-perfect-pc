"""Real private files and bounded authenticated chunk framing; disposable keys."""
import hashlib
import os
from pathlib import Path
import struct
import sys
from contextlib import contextmanager
from types import SimpleNamespace

import pytest
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_backup_cipher as cipher
import mod_backup_recipient as recipient
from mod_package import ModError
from mod_trust import encode,b64

ORIGIN='https://workshop.example.test:8443'
PIN='a'*64


@pytest.fixture
def case(tmp_path):
    folder=tmp_path.resolve()/'private'
    if os.name=='nt':
        from mod_windows_recovery import private
        private(folder,create=True)
    else:folder.mkdir(mode=0o700)
    key=X25519PrivateKey.generate()
    return folder,key,recipient.public(key,ORIGIN,PIN)


def write(path,data):path.write_bytes(data);path.chmod(0o600)


def encrypt(case,data):
    folder,key,public=case;source=folder/'plain';write(source,data)
    output=folder/'encrypted'
    report=cipher.encrypt_file(source,output,public,ORIGIN,PIN,plain_bytes=len(data),plain_sha256=cipher.digest(data))
    return source,output,report


@pytest.mark.parametrize('length',[0,1,1024,cipher.CHUNK,cipher.CHUNK+1,2*cipher.CHUNK+17])
def test_round_trip_private_files_empty_and_chunk_boundaries(case,length):
    data=(bytes(range(251))*((length+250)//251))[:length]
    source,encrypted,report=encrypt(case,data);output=case[0]/'restored'
    assert report['bytes']==encrypted.stat().st_size and report['cipher_sha256']==cipher.digest(encrypted.read_bytes())
    result=cipher.decrypt_file(encrypted,output,case[1],ORIGIN,PIN,cipher_sha256=report['cipher_sha256'])
    assert output.read_bytes()==source.read_bytes()==data and result['plain_sha256']==cipher.digest(data)
    assert result['plain_bytes']==length
    with pytest.raises((OSError,ValueError)):cipher.decrypt_file(encrypted,output,case[1],ORIGIN,PIN,cipher_sha256=report['cipher_sha256'])
    assert output.read_bytes()==data


@pytest.mark.parametrize('fault',['hash','key','origin','pin','magic','header-limit','truncated','trailing'])
def test_foreign_receipt_recipient_or_bad_framing_never_creates_plaintext(case,fault):
    source,path,report=encrypt(case,b'private fixture'*100);wire=path.read_bytes();key=case[1];origin,pin=ORIGIN,PIN
    if fault=='hash':report['cipher_sha256']='b'*64
    elif fault=='key':key=X25519PrivateKey.generate()
    elif fault=='origin':origin='https://other.example.test'
    elif fault=='pin':pin='b'*64
    elif fault=='magic':wire=b'UNKNOWN!'+wire[len(cipher.MAGIC):]
    elif fault=='header-limit':wire=cipher.MAGIC+struct.pack('>I',cipher.MAX_HEADER+1)+wire[len(cipher.MAGIC)+4:]
    elif fault=='truncated':wire=wire[:-1]
    else:wire+=b'extra'
    if fault not in ('hash','key','origin','pin'):write(path,wire);report['cipher_sha256']=cipher.digest(wire)
    output=case[0]/'not-created'
    with pytest.raises(ModError):cipher.decrypt_file(path,output,key,origin,pin,cipher_sha256=report['cipher_sha256'])
    assert not output.exists() and source.read_bytes()==b'private fixture'*100


@pytest.mark.parametrize('fault',['first-tag','last-tag','terminal-tag','reorder','header-binding'])
def test_tampering_with_updated_transport_hash_still_fails_chunk_authentication(case,fault):
    source,path,_=encrypt(case,b'A'*cipher.CHUNK+b'B'*cipher.CHUNK+b'end')
    wire=bytearray(path.read_bytes());offset=len(cipher.MAGIC)+4;size=struct.unpack('>I',wire[len(cipher.MAGIC):offset])[0];start=offset+size
    if fault=='first-tag':wire[start+cipher.CHUNK]^=1
    elif fault=='last-tag':wire[-17]^=1
    elif fault=='terminal-tag':wire[-1]^=1
    elif fault=='reorder':
        length=cipher.CHUNK+16;first=wire[start:start+length];second=wire[start+length:start+2*length]
        wire[start:start+2*length]=second+first
    else:
        header=cipher.read_json(bytes(wire[offset:start]));header['salt']=b64(bytes(32));body=encode(header)
        wire=bytearray(cipher.MAGIC+struct.pack('>I',len(body))+body+wire[start:])
    write(path,bytes(wire));output=case[0]/'partial-private'
    with pytest.raises(ModError,match='authenticate'):cipher.decrypt_file(path,output,case[1],ORIGIN,PIN,cipher_sha256=cipher.digest(wire))
    if output.exists():
        assert output.read_bytes()==source.read_bytes()[:output.stat().st_size]
        if os.name=='posix':assert output.stat().st_mode&0o077==0


def test_wrong_verified_plaintext_digest_leaves_incomplete_ciphertext_not_a_valid_export(case):
    folder,key,public=case;source=folder/'source';write(source,b'private bytes')
    target=folder/'incomplete'
    with pytest.raises(ModError):cipher.encrypt_file(source,target,public,ORIGIN,PIN,plain_bytes=13,plain_sha256='a'*64)
    assert target.exists()
    with pytest.raises(ModError):cipher.decrypt_file(target,folder/'not-created',key,ORIGIN,PIN,cipher_sha256=cipher.digest(target.read_bytes()))
    assert not (folder/'not-created').exists()


@pytest.mark.parametrize('fault',['schema','size','oversize','unknown','ephemeral'])
def test_invalid_header_or_ephemeral_refuses_before_plaintext_output(case,fault):
    _,path,_=encrypt(case,b'fixture');wire=path.read_bytes();offset=len(cipher.MAGIC)+4;length=struct.unpack('>I',wire[len(cipher.MAGIC):offset])[0]
    header=cipher.read_json(wire[offset:offset+length])
    if fault=='schema':header['schema']=True
    elif fault=='size':header['plain_bytes']=True
    elif fault=='oversize':header['plain_bytes']=cipher.MAX_PLAIN+1
    elif fault=='unknown':header['secret']='unexpected'
    else:header['ephemeral']=b64(bytes(32))
    body=encode(header);changed=cipher.MAGIC+struct.pack('>I',len(body))+body+wire[offset+length:];write(path,changed)
    output=case[0]/'not-created'
    with pytest.raises(ModError):cipher.decrypt_file(path,output,case[1],ORIGIN,PIN,cipher_sha256=cipher.digest(changed))
    assert not output.exists()


def test_missing_external_cipher_hash_and_invalid_size_refuse_without_creating_output(case):
    _,path,_=encrypt(case,b'fixture');output=case[0]/'not-created'
    for sha in (None,True,'../bad'):
        with pytest.raises(ModError):cipher.decrypt_file(path,output,case[1],ORIGIN,PIN,cipher_sha256=sha)
    with pytest.raises(ModError):cipher.encrypt_file(path,output,case[2],ORIGIN,PIN,plain_bytes=True,plain_sha256='a'*64)
    assert not output.exists()


def test_same_plaintext_encrypts_differently_and_existing_destinations_are_preserved(case):
    source,path,first=encrypt(case,b'fixture');other=case[0]/'second'
    second=cipher.encrypt_file(source,other,case[2],ORIGIN,PIN,plain_bytes=7,plain_sha256=cipher.digest(b'fixture'))
    assert first['cipher_sha256']!=second['cipher_sha256']
    before=path.read_bytes()
    with pytest.raises(OSError):cipher.encrypt_file(source,path,case[2],ORIGIN,PIN,plain_bytes=7,plain_sha256=cipher.digest(b'fixture'))
    assert path.read_bytes()==before


def test_public_header_does_not_expose_plaintext_checksum(case):
    data=b'private low-entropy fixture';_,path,_=encrypt(case,data);wire=path.read_bytes()
    offset=len(cipher.MAGIC)+4;length=struct.unpack('>I',wire[len(cipher.MAGIC):offset])[0]
    header=wire[offset:offset+length]
    assert 'plain_sha256' not in cipher.read_json(header) and cipher.digest(data).encode() not in header


def test_valid_replacement_between_hash_pass_and_decrypt_is_not_accepted(case,monkeypatch):
    source,path,report=encrypt(case,b'original');replacement=case[0]/'replacement';write(source,b'attacker')
    cipher.encrypt_file(source,replacement,case[2],ORIGIN,PIN,plain_bytes=8,plain_sha256=cipher.digest(b'attacker'))
    changed=replacement.read_bytes();assert len(changed)==path.stat().st_size
    real=cipher.incoming
    @contextmanager
    def replace_after_hash(source_path,maximum):
        with real(source_path,maximum) as (stream,size,stable):
            class Reader:
                def read(self,count):return stream.read(count)
                def seek(self,position):write(path,changed);return stream.seek(position)
            # Model a filesystem whose timestamps remain cached while another
            # writer is open. Content consumed during decryption must still be
            # checked against the independently authenticated original receipt.
            yield Reader(),size,lambda:None
    monkeypatch.setattr(cipher,'incoming',replace_after_hash)
    with pytest.raises(ModError,match='changed after receipt'):
        cipher.decrypt_file(path,case[0]/'uncommitted',case[1],ORIGIN,PIN,cipher_sha256=report['cipher_sha256'])


def test_storage_headroom_refuses_before_creating_either_output(case,monkeypatch):
    source,path,report=encrypt(case,b'fixture');output=case[0]/'not-created'
    monkeypatch.setattr(cipher.shutil,'disk_usage',lambda *_:SimpleNamespace(free=0))
    with pytest.raises(ModError,match='headroom'):
        cipher.encrypt_file(source,output,case[2],ORIGIN,PIN,plain_bytes=7,plain_sha256=cipher.digest(b'fixture'))
    with pytest.raises(ModError,match='headroom'):
        cipher.decrypt_file(path,output,case[1],ORIGIN,PIN,cipher_sha256=report['cipher_sha256'])
    assert not output.exists() and source.read_bytes()==b'fixture'


def test_plaintext_changed_during_streaming_never_gets_completion_footer(case,monkeypatch):
    folder,key,public=case;data=b'A'*(2*cipher.CHUNK+1);source=folder/'mutable';write(source,data)
    parameters=cipher.parameters
    def changed(header,wire,index,length):
        if index==0:write(source,b'B'*len(data))
        return parameters(header,wire,index,length)
    monkeypatch.setattr(cipher,'parameters',changed);target=folder/'partial'
    with pytest.raises(ModError):cipher.encrypt_file(source,target,public,ORIGIN,PIN,plain_bytes=len(data),plain_sha256=cipher.digest(data))
    assert target.exists()
    with pytest.raises(ModError):cipher.decrypt_file(target,folder/'not-created',key,ORIGIN,PIN,cipher_sha256=cipher.digest(target.read_bytes()))
    assert not (folder/'not-created').exists()
