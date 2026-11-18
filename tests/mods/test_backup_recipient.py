"""Independent recipient custody and portable recovery; no production secrets."""
import copy
import os
from pathlib import Path
import sys

import pytest
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey
from cryptography.hazmat.primitives.asymmetric import rsa

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_backup_recipient as recipient
import mod_windows_recovery as windows
from mod_package import ModError
from mod_trust import encode,b64,unb64

ORIGIN='https://workshop.example.test:8443'
PIN='a'*64
PASSWORD='backup recipient fixture passphrase'


@pytest.fixture
def pair():
    key=X25519PrivateKey.generate()
    return key,recipient.public(key,ORIGIN,PIN)


@pytest.fixture
def envelope(pair):return recipient.seal(pair[0],PASSWORD,ORIGIN,PIN)


def test_public_and_private_round_trip_are_bound_to_profile_and_independent_key(pair,envelope):
    key,public=pair
    assert recipient.checked_public(public,ORIGIN,PIN)
    loaded=recipient.unseal(envelope,PASSWORD,ORIGIN,PIN)
    assert recipient.public(loaded,ORIGIN,PIN)==public
    peer=X25519PrivateKey.generate()
    assert key.exchange(peer.public_key())==loaded.exchange(peer.public_key())
    assert recipient.secret(key,ORIGIN,PIN) not in envelope
    assert recipient.seal(key,PASSWORD,ORIGIN,PIN)!=envelope


@pytest.mark.parametrize('fault',['schema','type','algorithm','origin','pin','id','key','low-order','unknown'])
def test_invalid_public_identity_refuses_before_private_recovery(pair,fault):
    value=copy.deepcopy(pair[1])
    if fault=='schema':value['schema']=True
    elif fault=='type':value['type']='tsfp-portable-trust-recovery-v1'
    elif fault=='algorithm':value['algorithm']='rsa'
    elif fault=='origin':value['origin']='https://other.example.test'
    elif fault=='pin':value['trusted_root_sha256']='b'*64
    elif fault=='id':value['key_id']='b'*64
    elif fault=='key':value['public_key']=b64(b'a'*33)
    elif fault=='low-order':value.update(public_key=b64(bytes(32)),key_id=recipient.key_id(bytes(32)))
    else:value['secret']='unexpected'
    with pytest.raises(ModError):recipient.checked_public(value,ORIGIN,PIN)


@pytest.mark.parametrize('origin,pin',[(ORIGIN,True),('http://workshop.example.test',PIN),
    (ORIGIN,'../bad'),(ORIGIN+'/',PIN),('https://user@workshop.example.test',PIN)])
def test_invalid_profile_never_creates_recipient_key_store(tmp_path,origin,pin):
    with pytest.raises(ModError):recipient.create_windows(tmp_path/'not-created',origin,pin)
    assert not (tmp_path/'not-created').exists()


@pytest.mark.parametrize('fault',['schema','type','kdf','cipher','salt','nonce','ciphertext','public','unknown','oversized','noncanonical'])
def test_malformed_envelope_refuses_before_scrypt(envelope,monkeypatch,fault):
    value=recipient.read_json(envelope)
    if fault=='schema':value['schema']=True
    elif fault=='type':value['type']='tsfp-portable-trust-recovery-v1'
    elif fault=='kdf':value['kdf']='arbitrary-memory'
    elif fault=='cipher':value['cipher']='none'
    elif fault=='salt':value['salt']=b64(bytes(31))
    elif fault=='nonce':value['nonce']=b64(bytes(11))
    elif fault=='ciphertext':value['ciphertext']=b64(bytes(16))
    elif fault=='public':value['recipient']['key_id']='f'*64
    elif fault=='unknown':value['private_key']='unexpected'
    wire=b'x'*(recipient.MAX_WIRE+1) if fault=='oversized' else encode(value)+ (b'\n' if fault=='noncanonical' else b'')
    monkeypatch.setattr(recipient,'derive',lambda *_:pytest.fail('KDF before bounded envelope validation'))
    with pytest.raises(ModError):recipient.unseal(wire,PASSWORD,ORIGIN,PIN)


@pytest.mark.parametrize('fault',['password','tag','salt','nonce','different-valid-public'])
def test_authentication_failure_never_returns_a_recipient_key(envelope,fault):
    value=recipient.read_json(envelope);password=PASSWORD
    if fault=='password':password='a different sufficiently long fixture password'
    elif fault=='different-valid-public':value['recipient']=recipient.public(X25519PrivateKey.generate(),ORIGIN,PIN)
    else:
        field='ciphertext' if fault=='tag' else fault
        raw=bytearray(unb64(value[field],recipient.MAX_PLAIN+16));raw[-1]^=1;value[field]=b64(bytes(raw))
    with pytest.raises(ModError,match='authenticate'):recipient.unseal(encode(value),password,ORIGIN,PIN)


@pytest.mark.parametrize('fault',['other-key','schema','unknown','raw-length','noncanonical'])
def test_malformed_or_mismatched_private_payload_is_refused(pair,fault):
    value=recipient.read_json(recipient.secret(pair[0],ORIGIN,PIN))
    if fault=='other-key':value['recipient']=recipient.public(X25519PrivateKey.generate(),ORIGIN,PIN)
    elif fault=='schema':value['schema']=True
    elif fault=='unknown':value['catalog_key']='not-allowed'
    elif fault=='raw-length':value['private_key']=b64(bytes(31))
    with pytest.raises(ModError):recipient.checked_secret(encode(value)+(b'\n' if fault=='noncanonical' else b''),ORIGIN,PIN)


def test_wrong_expected_profile_refuses_before_kdf(envelope,monkeypatch):
    monkeypatch.setattr(recipient,'derive',lambda *_:pytest.fail('KDF for foreign workshop'))
    for service_origin,pin in ((ORIGIN,'b'*64),('https://other.example.test',PIN)):
        with pytest.raises(ModError):recipient.unseal(envelope,PASSWORD,service_origin,pin)


def test_signing_key_cannot_be_used_as_backup_recipient():
    signing=rsa.generate_private_key(public_exponent=65537,key_size=2048)
    with pytest.raises(ModError):recipient.public(signing,ORIGIN,PIN)
    with pytest.raises(ModError):recipient.seal(signing,PASSWORD,ORIGIN,PIN)


@pytest.mark.skipif(os.name=='nt',reason='Windows-only import refusal on other hosts')
def test_windows_import_refuses_other_hosts_before_unsealing(tmp_path,monkeypatch):
    monkeypatch.setattr(recipient,'unseal',lambda *_:pytest.fail('Unsealed before Windows import admission'))
    with pytest.raises(ModError):recipient.import_windows(b'ciphertext',PASSWORD,tmp_path/'not-created',ORIGIN,PIN)
    assert not (tmp_path/'not-created').exists()


@pytest.mark.skipif(os.name!='nt',reason='Actual Windows DPAPI and NTFS private ACLs')
def test_windows_account_store_export_and_new_store_import_preserve_key_and_existing_output(tmp_path):
    first,second=tmp_path/'original',tmp_path/'recovered'
    public=recipient.create_windows(first,ORIGIN,PIN)
    key=recipient.load_windows(first,ORIGIN,PIN)
    assert recipient.public(key,ORIGIN,PIN)==public
    raw=recipient.secret(key,ORIGIN,PIN)
    assert raw not in (first/'recipient.dpapi').read_bytes() and b'private_key' not in (first/'recipient.json').read_bytes()
    wire=recipient.export_windows(first,PASSWORD,ORIGIN,PIN)
    assert recipient.import_windows(wire,PASSWORD,second,ORIGIN,PIN)==public
    assert recipient.public(recipient.load_windows(second,ORIGIN,PIN),ORIGIN,PIN)==public
    before={p.name:p.read_bytes() for p in first.iterdir()}
    with pytest.raises(ModError):recipient.create_windows(first,ORIGIN,PIN)
    with pytest.raises(ModError):recipient.import_windows(wire,PASSWORD,first,ORIGIN,PIN)
    assert before=={p.name:p.read_bytes() for p in first.iterdir()}
    with pytest.raises(ModError):recipient.load_windows(first,ORIGIN,'b'*64)
    windows.private(first);windows.private(second)


@pytest.mark.skipif(os.name!='nt',reason='Actual Windows DPAPI and NTFS private ACLs')
def test_partial_windows_store_is_preserved_without_completion_or_adoption(tmp_path,monkeypatch):
    folder=tmp_path/'partial';write=windows.new_file
    def interrupted(path,value):
        if path.name=='recipient.json':raise OSError('fixture incomplete completion write')
        return write(path,value)
    with monkeypatch.context() as patch:
        patch.setattr(windows,'new_file',interrupted)
        with pytest.raises(OSError):recipient.create_windows(folder,ORIGIN,PIN)
    assert {p.name for p in folder.iterdir()}=={'recipient.dpapi'}
    before=(folder/'recipient.dpapi').read_bytes()
    with pytest.raises(ModError):recipient.load_windows(folder,ORIGIN,PIN)
    with pytest.raises(ModError):recipient.create_windows(folder,ORIGIN,PIN)
    assert (folder/'recipient.dpapi').read_bytes()==before


@pytest.mark.skipif(os.name!='nt',reason='Actual Windows DPAPI and NTFS private ACLs')
def test_windows_store_refuses_a_different_valid_protected_key_or_unknown_member(tmp_path):
    first,other=tmp_path/'first',tmp_path/'other'
    assert recipient.create_windows(first,ORIGIN,PIN)!=recipient.create_windows(other,ORIGIN,PIN)
    swapped=(other/'recipient.dpapi').read_bytes();(first/'recipient.dpapi').write_bytes(swapped)
    with pytest.raises(ModError,match='Stored backup recipient differs'):recipient.load_windows(first,ORIGIN,PIN)
    assert (first/'recipient.dpapi').read_bytes()==swapped
    (other/'unknown').write_bytes(b'preserve')
    with pytest.raises(ModError,match='foreign'):recipient.load_windows(other,ORIGIN,PIN)
    assert (other/'unknown').read_bytes()==b'preserve'
