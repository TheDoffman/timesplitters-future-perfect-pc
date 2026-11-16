"""Actual native immutable WAV store, authority/budget/type/link and snapshot tests."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
from concurrent.futures import ThreadPoolExecutor

import pytest
from test_script_process import process_tools

PACKAGE='01'*32
OTHER='02'*32


def wave(channels=1,rate=22050,pcm=None):
    pcm=pcm if pcm is not None else struct.pack('<8h',-32768,-50,-1,0,1,50,100,32767)
    fmt=struct.pack('<HHIIHH',1,channels,rate,rate*channels*2,channels*2,16)
    body=b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(pcm))+pcm
    return b'RIFF'+struct.pack('<I',len(body))+body


def asset(tmp_path,key=1,package=PACKAGE,data=None):
    path=tmp_path/f'clip-{key}.wav'
    data=wave(pcm=struct.pack('<8h',*range(key%100,key%100+8))) if data is None else data
    path.write_bytes(data)
    return dict(key=key,package=package,path=str(path.resolve()),sha256=hashlib.sha256(data).hexdigest(),bytes=len(data))


def command(tools,pins,packages=None,mode='read'):
    worker=tools['worker'];exe=worker.with_name('tsfp-mod-audio-assets-driver'+worker.suffix)
    packages=[PACKAGE] if packages is None else packages
    args=[str(exe),mode,str(len(packages)),*packages,str(len(pins))]
    for pin in pins:args.extend(str(pin[name]) for name in ('key','package','path','sha256','bytes'))
    return args


def load(tools,pins,packages=None,mode='read'):
    result=subprocess.run(command(tools,pins,packages,mode),capture_output=True,text=True,timeout=20)
    assert result.returncode==0 and not result.stderr,(result.returncode,result.stdout,result.stderr)
    return json.loads(result.stdout)


def test_unsorted_resources_have_exact_owned_pcm_and_allocation_free_lookup(process_tools,tmp_path):
    pins=[asset(tmp_path,70000,OTHER,wave(2,48000)),asset(tmp_path,0),asset(tmp_path,2)]
    result=load(process_tools,pins,[PACKAGE,OTHER])
    assert result['accepted'] and result['count']==3 and result['bytes']==sum(p['bytes'] for p in pins)
    assert [v['key'] for v in result['views']]==[70000,0,2]
    for row,pin in zip(result['views'],pins):
        assert row['pcm_sha256']==hashlib.sha256(Path(pin['path']).read_bytes()[44:]).hexdigest()
    assert (result['views'][0]['rate'],result['views'][0]['channels'],result['views'][0]['frames'])==(48000,2,4)


def test_creator_audio_template_waves_load_through_the_actual_native_store(process_tools,tmp_path):
    from mod_creator import create_project
    project=tmp_path/'Creator audio';create_project(project,mod_id='audio-lab',name='Audio lab',author='creator',template='audio')
    pins=[]
    for key in (1,2):
        path=project/'audio'/f'{key:08x}.wav';data=path.read_bytes()
        pins.append(dict(key=key,package=PACKAGE,path=str(path),sha256=hashlib.sha256(data).hexdigest(),bytes=len(data)))
    result=load(process_tools,pins)
    assert result['accepted'] and result['count']==2
    assert all((v['rate'],v['channels'],v['frames'])==(22050,1,22050) for v in result['views'])


@pytest.mark.parametrize('kind',['untrusted-owner','bad-package','duplicate-package','no-packages','many-packages',
    'duplicate-key','unknown-key','bad-hash','wrong-hash','zero-size','wrong-size','oversize','empty','malformed','cancel'])
def test_invalid_handoff_never_publishes_partial_assets(process_tools,tmp_path,kind):
    first=asset(tmp_path,1);second=asset(tmp_path,2);pins=[first,second];packages=[PACKAGE];mode='read'
    if kind=='untrusted-owner':second['package']=OTHER
    elif kind=='bad-package':packages=['AB'*32]
    elif kind=='duplicate-package':packages=[PACKAGE,PACKAGE]
    elif kind=='no-packages':packages=[]
    elif kind=='many-packages':packages=[f'{v:064x}' for v in range(1,34)]+[PACKAGE]
    elif kind=='duplicate-key':second['key']=first['key']
    elif kind=='unknown-key':second['key']=2**32-1
    elif kind=='bad-hash':second['sha256']='x'*64
    elif kind=='wrong-hash':second['sha256']='f'*64
    elif kind=='zero-size':second['bytes']=0
    elif kind=='wrong-size':second['bytes']+=1
    elif kind=='oversize':second['bytes']=64*1024*1024+1
    elif kind=='empty':pins=[]
    elif kind=='malformed':pins[1]=asset(tmp_path,2,data=b'RIFF'+b'\0'*40)
    else:mode='cancel'
    assert load(process_tools,pins,packages,mode)==dict(accepted=False,error_set=True)


@pytest.mark.parametrize('kind',['file-link','parent-link','hard-link','directory','missing'])
def test_unsafe_file_identity_is_refused(process_tools,tmp_path,kind):
    pin=asset(tmp_path);source=Path(pin['path']);alias=tmp_path/'alias'
    if kind in ('file-link','parent-link'):
        try:alias.symlink_to(source if kind=='file-link' else tmp_path,target_is_directory=kind=='parent-link')
        except OSError as error:pytest.skip('Host cannot create symlink: '+str(error))
        pin['path']=str(alias if kind=='file-link' else alias/source.name)
    elif kind=='hard-link':os.link(source,alias);pin['path']=str(alias)
    elif kind=='directory':pin['path']=str(tmp_path)
    else:pin['path']=str(tmp_path/'missing')
    assert not load(process_tools,[pin])['accepted']


def test_snapshot_survives_file_replacement_and_owns_all_views(process_tools,tmp_path):
    pins=[asset(tmp_path,2,data=wave(2)),asset(tmp_path,0)]
    expected=[hashlib.sha256(Path(p['path']).read_bytes()[44:]).hexdigest() for p in pins]
    child=subprocess.Popen(command(process_tools,pins,mode='snapshot'),stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    reader=ThreadPoolExecutor(max_workers=1)
    try:
        assert reader.submit(child.stdout.readline).result(timeout=10)=='ready\n'
        for pin in pins:Path(pin['path']).write_bytes(b'replaced after capture')
        output,error=child.communicate('x',timeout=15)
        assert child.returncode==0 and not error,(output,error)
        result=json.loads(output);assert [v['pcm_sha256'] for v in result['views']]==expected
    finally:
        if child.poll() is None:child.kill();child.wait(timeout=5)
        reader.shutdown(wait=True)


def test_unicode_asset_path_uses_native_utf8_contract(process_tools,tmp_path):
    folder=tmp_path/'authored-é-雪-🎵';folder.mkdir()
    pin=asset(folder)
    assert load(process_tools,[pin])['accepted']


def test_complete_64mib_input_and_256mib_aggregate_boundary(process_tools,tmp_path):
    size=64*1024*1024;path=tmp_path/'large.wav'
    header=bytearray(wave(2,48000,pcm=b'\0'*4)[:44])
    struct.pack_into('<I',header,4,size-8);struct.pack_into('<I',header,40,size-44)
    with path.open('wb') as stream:stream.write(header);stream.truncate(size)
    with path.open('rb') as stream:digest=hashlib.file_digest(stream,'sha256').hexdigest()
    pins=[dict(key=i,package=PACKAGE,path=str(path.resolve()),sha256=digest,bytes=size) for i in range(5)]
    assert load(process_tools,pins[:4])['accepted']
    assert load(process_tools,pins)==dict(accepted=False,error_set=True)
