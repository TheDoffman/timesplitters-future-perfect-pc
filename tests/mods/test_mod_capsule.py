"""Native source snapshots match the same vectors embedded in all launcher hosts."""
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
VECTOR=json.loads((ROOT/'tests/launcher/mod-launch-vectors.json').read_text())['vectors'][0]


def call(tools,path,digest=None,mode='read'):
    if digest is None:digest=hashlib.sha256(path.read_bytes()).hexdigest()
    worker=tools['worker'];driver=worker.with_name('tsfp-mod-capsule-driver'+worker.suffix)
    result=subprocess.run([str(driver),mode,str(path),digest],capture_output=True,timeout=10)
    assert result.returncode==0 and not result.stderr,(result.returncode,result.stdout,result.stderr)
    return json.loads(result.stdout)


def wire(source=b'return {api=1}',packages=None):
    packages=packages or ['01'*32]
    return b'TSFPMOD1'+struct.pack('<II',len(packages),len(source))+b''.join(bytes.fromhex(p) for p in packages)+source


def test_shared_launcher_vector_is_exact_and_source_snapshot_survives_replacement(process_tools,tmp_path):
    path=tmp_path/'launch.bin';path.write_bytes(base64.b64decode(VECTOR['wire']))
    result=call(process_tools,path,VECTOR['sha256'],mode='snapshot')
    assert result==dict(accepted=True,bytes=len(base64.b64decode(VECTOR['source'])),source_sha256=hashlib.sha256(base64.b64decode(VECTOR['source'])).hexdigest(),
                        profile=VECTOR['profile'],package=VECTOR['package'],unchanged=True)
    assert path.read_bytes()==b'replaced after capture'


@pytest.mark.parametrize('kind',['wrong-hash','bad-hash','uppercase-hash','magic','zero-count','large-count','zero-source','large-source',
                                  'trailing','truncated','duplicate-root','duplicate-dependency','unsorted'])
def test_invalid_policy_and_ambiguous_capsules_are_refused(process_tools,tmp_path,kind):
    data=wire();digest=None
    if kind=='wrong-hash':digest='0'*64
    if kind=='bad-hash':digest='xyz'
    if kind=='uppercase-hash':digest=hashlib.sha256(data).hexdigest().upper()
    if kind=='magic':data=b'badmagic'+data[8:]
    if kind=='zero-count':data=data[:8]+struct.pack('<I',0)+data[12:]
    if kind=='large-count':data=data[:8]+struct.pack('<I',33)+data[12:]
    if kind=='zero-source':data=data[:12]+struct.pack('<I',0)+data[16:]
    if kind=='large-source':data=data[:12]+struct.pack('<I',1048577)+data[16:]
    if kind=='trailing':data+=b'extra'
    if kind=='truncated':data=data[:-1]
    if kind=='duplicate-root':data=wire(packages=['01'*32,'01'*32])
    if kind=='duplicate-dependency':data=wire(packages=['01'*32,'02'*32,'02'*32])
    if kind=='unsorted':data=wire(packages=['01'*32,'03'*32,'02'*32])
    path=tmp_path/'launch.bin';path.write_bytes(data)
    assert call(process_tools,path,digest)=={'accepted':False}
    assert path.read_bytes()==data and len(list(tmp_path.iterdir()))==1


@pytest.mark.parametrize('source',[b'\0',b'-- a\0b',b'\x1bLua compiled',b'\xc0\x80',b'\xed\xa0\x80',b'\xf4\x90\x80\x80',b'\xe2\x82',b'\x80',b'\xff'])
def test_nontext_and_compiled_lua_are_refused(process_tools,tmp_path,source):
    path=tmp_path/'launch.bin';path.write_bytes(wire(source))
    assert not call(process_tools,path)['accepted']


@pytest.mark.parametrize('source',[b'--'+b'a'*(1024*1024-2),'-- Ω 😀\nreturn {api=1}'.encode()],ids=['maximum-source','unicode'])
def test_bounded_source_and_complete_unicode_are_accepted(process_tools,tmp_path,source):
    path=tmp_path/'launch.bin';path.write_bytes(wire(source));result=call(process_tools,path)
    assert result['accepted'] and result['source_sha256']==hashlib.sha256(source).hexdigest()


@pytest.mark.parametrize('kind',['file-link','parent-link','oversize','missing','directory','empty'])
def test_pinned_reader_cannot_follow_aliases_or_unbounded_inputs(process_tools,tmp_path,kind):
    directory=tmp_path/'source';directory.mkdir();path=directory/'launch.bin';data=wire();path.write_bytes(data);digest=hashlib.sha256(data).hexdigest()
    if kind.endswith('link'):
        alias=tmp_path/'alias'
        try:alias.symlink_to(path if kind=='file-link' else directory,target_is_directory=kind=='parent-link')
        except OSError as error:pytest.skip('Host cannot create links: '+str(error))
        path=alias if kind=='file-link' else alias/'launch.bin'
    if kind=='oversize':path.write_bytes(b'x'*(1048576+16+32*32+1))
    if kind=='missing':path=directory/'missing'
    if kind=='directory':path=directory
    if kind=='empty':path.write_bytes(b'')
    assert not call(process_tools,path,digest)['accepted']
