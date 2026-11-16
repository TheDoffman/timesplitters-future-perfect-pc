"""Private launcher wire to the real immutable native audio loader."""
import base64
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

import pytest
from test_script_process import process_tools

ROOT=Path(__file__).resolve().parents[2]
VECTOR=json.loads((ROOT/'tests/launcher/mod-launch-vectors.json').read_text())['audioVectors'][0]


def setup(tmp_path):
    root=tmp_path/'cache';root.mkdir(mode=0o700)
    wav=base64.b64decode(VECTOR['wav'])
    payload=root/(hashlib.sha256(wav).hexdigest()+'.wav');payload.write_bytes(wav)
    path=tmp_path/'audio.bin';path.write_bytes(base64.b64decode(VECTOR['wire']))
    return path,root,payload


def call(tools,path,root,packages=None,digest=None,mode='capsule'):
    worker=tools['worker'];driver=worker.with_name('tsfp-mod-audio-assets-driver'+worker.suffix)
    packages=VECTOR['packages'] if packages is None else packages
    digest=hashlib.sha256(path.read_bytes()).hexdigest() if digest is None else digest
    result=subprocess.run([str(driver),mode,str(path),digest,str(root),*packages],capture_output=True,timeout=15)
    assert result.returncode==0 and not result.stderr,(result.returncode,result.stdout,result.stderr)
    return json.loads(result.stdout)


def test_shared_launcher_vector_loads_real_wave_and_sorts_independent_authority(process_tools,tmp_path):
    path,root,_=setup(tmp_path)
    assert call(process_tools,path,root,list(reversed(VECTOR['packages'])),VECTOR['sha256'])==dict(accepted=True,count=2,bytes=120)


@pytest.mark.parametrize('kind',['magic','truncated','trailing','zero-assets','many-assets','owner-count',
    'owner-order','duplicate-owner','foreign-authority','duplicate-authority','bad-authority',
    'foreign-index','duplicate-key','unsorted-keys','reserved-key','zero-size','huge-size','aggregate',
    'wrong-capsule-hash','wrong-payload-hash','cancel'])
def test_invalid_capsules_never_publish_partial_assets(process_tools,tmp_path,kind):
    path,root,payload=setup(tmp_path);data=bytearray(path.read_bytes());packages=None;digest=None;mode='capsule'
    at=16+64
    if kind=='magic':data[0]=0
    elif kind=='truncated':data=data[:-1]
    elif kind=='trailing':data+=b'x'
    elif kind=='zero-assets':struct.pack_into('<I',data,12,0)
    elif kind=='many-assets':struct.pack_into('<I',data,12,8193)
    elif kind=='owner-count':struct.pack_into('<I',data,8,33)
    elif kind=='owner-order':data[16:80]=data[48:80]+data[16:48]
    elif kind=='duplicate-owner':data[48:80]=data[16:48]
    elif kind=='foreign-authority':packages=['03'*32,'02'*32]
    elif kind=='duplicate-authority':packages=['01'*32,'01'*32]
    elif kind=='bad-authority':packages=['AB'*32,'02'*32]
    elif kind=='foreign-index':struct.pack_into('<I',data,at+4,2)
    elif kind=='duplicate-key':struct.pack_into('<I',data,at+48,0)
    elif kind=='unsorted-keys':struct.pack_into('<I',data,at,70001)
    elif kind=='reserved-key':struct.pack_into('<I',data,at+48,2**32-1)
    elif kind=='zero-size':struct.pack_into('<Q',data,at+8,0)
    elif kind=='huge-size':struct.pack_into('<Q',data,at+8,2**64-1)
    elif kind=='aggregate':
        record=bytes(data[at:at+48]);records=[]
        for i in range(5):
            item=bytearray(record);struct.pack_into('<I',item,0,i);struct.pack_into('<Q',item,8,64*1024*1024);records.append(item)
        data=data[:at]+b''.join(records);struct.pack_into('<I',data,12,5)
    elif kind=='wrong-capsule-hash':digest='00'*32
    elif kind=='wrong-payload-hash':payload.write_bytes(b'changed')
    else:mode='capsule-cancel'
    path.write_bytes(data);before=payload.read_bytes()
    assert call(process_tools,path,root,packages,digest,mode)==dict(accepted=False,error_set=True)
    assert payload.read_bytes()==before and path.read_bytes()==data


@pytest.mark.parametrize('kind',['capsule-link','capsule-hardlink','cache-link','payload-link','payload-hardlink','relative-cache','missing-payload'])
def test_cache_and_capsule_cannot_redirect_verified_reads(process_tools,tmp_path,kind):
    path,root,payload=setup(tmp_path);digest=VECTOR['sha256'];alias=tmp_path/'alias'
    if kind.endswith('link') and not kind.endswith('hardlink'):
        target=path if kind=='capsule-link' else root if kind=='cache-link' else payload
        try:alias.symlink_to(target,target_is_directory=kind=='cache-link')
        except OSError as error:pytest.skip('Host cannot create links: '+str(error))
        if kind=='capsule-link':path=alias
        elif kind=='cache-link':root=alias
        else:
            alias.unlink();payload.rename(alias);payload.symlink_to(alias)
    elif kind=='capsule-hardlink':os.link(path,alias)
    elif kind=='payload-hardlink':os.link(payload,alias)
    elif kind=='relative-cache':root=Path('relative-cache')
    else:payload.unlink()
    assert call(process_tools,path,root,digest=digest)==dict(accepted=False,error_set=True)


def test_unicode_cache_and_capsule_paths(process_tools,tmp_path):
    directory=tmp_path/'audio-é-雪-😀';directory.mkdir(mode=0o700)
    path,root,_=setup(directory)
    assert call(process_tools,path,root)['accepted']


def test_maximum_owner_and_resource_counts_load_real_pcm(process_tools,tmp_path):
    path,root,payload=setup(tmp_path)
    packages=[f'{i:064x}' for i in range(32)]
    record=lambda key:struct.pack('<IIQ',key,key%32,payload.stat().st_size)+bytes.fromhex(payload.stem)
    data=b'TSFPAUD1'+struct.pack('<II',32,8192)+b''.join(bytes.fromhex(p) for p in packages)+b''.join(record(i) for i in range(8192))
    assert len(data)==394256
    path.write_bytes(data)
    assert call(process_tools,path,root,packages)==dict(accepted=True,count=8192,bytes=8192*60)
    path.write_bytes(data+b'x')
    assert not call(process_tools,path,root,packages)['accepted']
