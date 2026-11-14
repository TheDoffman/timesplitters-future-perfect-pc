"""Production helper bundles, compilation pins and provenance refusal."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import subprocess

import pytest
import bundle_mod_runtime as packaging
import mod_script_protocol as protocol
import linux_dependency_sources as linux_sources

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('runtime_image_producer',ROOT/'tools/build/mod_runtime_images.py')
producer=importlib.util.module_from_spec(spec);spec.loader.exec_module(producer)


@pytest.fixture(scope='module')
def production(tmp_path_factory):
    root=tmp_path_factory.mktemp('production-helpers')
    cmake=Path(sys.executable).with_name('cmake.exe' if os.name=='nt' else 'cmake')
    builder=producer.worker_builder();builder.build(root,str(cmake))
    return root


def clone_build(source,destination):
    _,_,files=producer.validate_build(source)
    for file in [source/'BUILD-worker.json',source/'cmake/CMakeCache.txt',*files.values()]:
        target=destination/file.relative_to(source);target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(file,target)
    return destination


def test_production_bundle_contains_only_helpers_and_compiles_exact_pins(production,tmp_path):
    runtime=tmp_path/'Game/ModRuntime';header=tmp_path/'generated/mod-runtime-images.h'
    descriptor=producer.stage(production,runtime,header);bundle=runtime/descriptor['identity']
    producer.verify_bundle(bundle,descriptor)
    target,_,files=producer.validate_build(production)
    assert set(p.name for p in bundle.iterdir())==set(files)|{'runtime.json'}
    assert 'tsfp-mod-runner' not in descriptor['files'] and not any('test' in n for n in descriptor['files'])
    worker='tsfp-mod-worker.exe' if target=='windows-x64' else 'tsfp-mod-worker'
    assert descriptor['files'][worker]['sha256'] in header.read_text()
    assert 'ModRuntime/'+descriptor['identity'] in header.read_text()
    assert str(ROOT) not in (bundle/'runtime.json').read_text()
    stamps={p.name:p.stat().st_mtime_ns for p in bundle.iterdir()};stamp=header.stat().st_mtime_ns
    assert producer.stage(production,runtime,header)==descriptor
    assert stamp==header.stat().st_mtime_ns and stamps=={p.name:p.stat().st_mtime_ns for p in bundle.iterdir()}


@pytest.mark.parametrize('kind',['testing','sanitize','host_sanitize','source','image','cache','license'])
def test_nonproduction_or_changed_build_cannot_publish(production,tmp_path,kind):
    output=clone_build(production,tmp_path/'build');receipt_path=output/'BUILD-worker.json';receipt=json.loads(receipt_path.read_text())
    if kind in ('testing','sanitize','host_sanitize'):receipt[kind]=True
    if kind=='source':receipt['sources']['src/mods/mod_worker.c']='0'*64
    if kind=='image':
        _,_,files=producer.validate_build(output);image=next(p for name,p in files.items() if name.startswith('tsfp-mod-worker'))
        data=image.read_bytes();image.write_bytes(data[:-1]+bytes([data[-1]^1]))
    if kind=='cache':
        cache=output/'cmake/CMakeCache.txt';text=cache.read_text();assert 'TSFP_MOD_TESTING:UNINITIALIZED=OFF' in text or 'TSFP_MOD_TESTING:BOOL=OFF' in text
        cache.write_text(text.replace('TSFP_MOD_TESTING:UNINITIALIZED=OFF','TSFP_MOD_TESTING:UNINITIALIZED=ON').replace('TSFP_MOD_TESTING:BOOL=OFF','TSFP_MOD_TESTING:BOOL=ON'))
    if kind=='license':(output/'Licenses/Lua.txt').write_text('changed license')
    receipt_path.write_text(json.dumps(receipt))
    runtime=tmp_path/'Game/ModRuntime';header=tmp_path/'generated/pins.h'
    with pytest.raises(ValueError):producer.stage(output,runtime,header)
    assert not runtime.exists() and not header.exists()


def test_immutable_bundle_refuses_unknown_files_and_preserves_them(production,tmp_path):
    runtime=tmp_path/'Game/ModRuntime';header=tmp_path/'generated/pins.h'
    descriptor=producer.stage(production,runtime,header);keep=runtime/descriptor['identity']/'keep.txt';keep.write_text('keep')
    before=header.read_bytes()
    with pytest.raises(ValueError,match='unrecognized'):producer.stage(production,runtime,header)
    assert keep.read_text()=='keep' and header.read_bytes()==before


def test_unrecognized_header_is_never_overwritten(production,tmp_path):
    runtime=tmp_path/'Game/ModRuntime';header=tmp_path/'pins.h';header.write_text('preserve this file')
    with pytest.raises(ValueError,match='unrecognized helper header'):producer.stage(production,runtime,header)
    assert header.read_text()=='preserve this file'


def test_wrong_native_format_is_refused(tmp_path):
    path=tmp_path/'image';path.write_bytes(b'not a native executable')
    with pytest.raises(ValueError,match='native target'):producer.native_image(path,producer.host_platform())


@pytest.mark.skipif(sys.platform!='darwin',reason='Mach-O deployment target is a native Mac check')
def test_mac_helpers_preserve_apple_silicon_deployment_floor(production,tmp_path):
    for name in ('tsfp-mod-worker','tsfp-mod-guardian'):
        data=subprocess.check_output(['otool','-l',str(production/'cmake'/name)],text=True)
        assert '\n    minos 11.0\n' in data
    output=clone_build(production,tmp_path/'build');cache=output/'cmake/CMakeCache.txt'
    text=cache.read_text();assert '=11.0' in text
    cache.write_text(text.replace('=11.0','=27.0'))
    with pytest.raises(ValueError,match='deployment target'):producer.validate_build(output)


def test_receipt_cannot_attest_to_sources_changed_during_build(tmp_path,monkeypatch):
    builder=producer.worker_builder();output=tmp_path/'build';target=producer.host_platform()
    binary=output/'cmake'/('Release' if target=='windows-x64' else '')/('tsfp-mod-worker.exe' if target=='windows-x64' else 'tsfp-mod-worker')
    binary.parent.mkdir(parents=True);binary.write_bytes(b'fixture output')
    receipt=output/'BUILD-worker.json';receipt.write_text('preserve prior receipt')
    samples=iter([{'source':'before'},{'source':'after'}])
    monkeypatch.setattr(builder,'source_hashes',lambda:next(samples))
    monkeypatch.setattr(builder,'prepare_lua',lambda _:tmp_path/'lua')
    monkeypatch.setattr(builder.subprocess,'run',lambda *a,**k:None)
    with pytest.raises(ValueError,match='changed during compilation'):builder.build(output)
    assert receipt.read_text()=='preserve prior receipt'


def package_fixture(production,tmp_path):
    root=tmp_path/'source';output=clone_build(production,root/'build/mod-worker')
    target=producer.host_platform();build=root/'build/game'
    parent=build/'Release' if target=='windows-x64' else build
    descriptor=producer.stage(output,parent/'ModRuntime',build/'generated/mod-runtime-images.h')
    return root,build,descriptor


def test_relocated_production_helper_preserves_compiled_hash_and_runs(production,tmp_path):
    root,build,descriptor=package_fixture(production,tmp_path)
    stage=tmp_path/'relocated package'
    assert packaging.copy(root,build,stage)==descriptor
    bundle=stage/'Bin/ModRuntime'/descriptor['identity'];producer.verify_bundle(bundle,descriptor)
    target=producer.host_platform();suffix='.exe' if target=='windows-x64' else ''
    worker=bundle/('tsfp-mod-worker'+suffix)
    runner=production/'cmake'/('Release' if suffix else '')/('tsfp-mod-runner'+suffix)
    cache=tmp_path/'cache';cache.mkdir()
    players=[(1,0,0,100,0.,1.,2.),(2,1,0,100,2.,3.,4.)]
    wire=protocol.load((ROOT/'examples/mods/first-to-three/scripts/main.lua').read_bytes())
    wire+=protocol.event(1,1,players=players)
    wire+=b''.join(protocol.event(i+2,3,tick=i+1,players=players,actor=1,target=2) for i in range(3))
    wire+=protocol.event(5,4,tick=4,players=players)+protocol.quit_worker()
    result=subprocess.run([str(runner),str(worker),str(cache)],input=wire,
                          capture_output=True,timeout=8)
    assert result.returncode==0 and result.stderr==b'',(result.returncode,result.stdout,result.stderr)
    replies=protocol.replies(result.stdout)
    assert len(replies)==6 and all(reply['status']==0 for reply in replies) and not list(cache.iterdir())
    for sequence,reply in enumerate(replies[1:],1):protocol.validate_commands(reply,sequence,[1,2],stopping=sequence==5)
    assert [reply['commands'][0]['value'] for reply in replies[2:5]]==[1,2,3]
    assert replies[4]['commands'][-1]['kind']==2
    # Package assembly cannot merge over even its own prior immutable output.
    with pytest.raises(FileExistsError):packaging.copy(root,build,stage)


@pytest.mark.parametrize('kind',['header','metadata','image','unknown'])
def test_packaging_refuses_tampered_bundle_before_creating_output(production,tmp_path,kind):
    root,build,descriptor=package_fixture(production,tmp_path)
    bundle,_=packaging.inspect(root,build)
    if kind=='header':
        header=build/'generated/mod-runtime-images.h';header.write_text(header.read_text().replace('ModRuntime/','ModRuntime/../'))
    if kind=='metadata':
        path=bundle/'runtime.json';data=json.loads(path.read_text());data['producer_sha256']='0'*64;path.write_text(json.dumps(data))
    if kind=='image':
        name=next(n for n in descriptor['files'] if n.startswith('tsfp-mod-worker'))
        path=bundle/name;path.chmod(0o600);path.write_bytes(path.read_bytes()+b'changed')
    if kind=='unknown':(bundle/'keep.txt').write_text('keep this')
    stage=tmp_path/'package'
    with pytest.raises(ValueError):packaging.copy(root,build,stage)
    assert not stage.exists()
    if kind=='unknown':assert (bundle/'keep.txt').read_text()=='keep this'


def test_linux_helper_retains_source_and_lua_notice_in_complete_audit(tmp_path,monkeypatch):
    root=tmp_path/'source';stage=tmp_path/'package';(stage/'Support/Runtime').mkdir(parents=True)
    name='Bin/ModRuntime/'+'a'*64+'/tsfp-mod-worker'
    record={name:dict(source=str(root/'build/mod-worker/cmake/tsfp-mod-worker'),source_sha256='b'*64)}
    original=Path.iterdir
    monkeypatch.setattr(Path,'iterdir',lambda path:iter(()) if path==Path('/usr/share/common-licenses') else original(path))
    report=linux_sources.collect(root,stage,record,tmp_path/'launcher',tmp_path/'media',tmp_path/'sdk',{})
    entry=report['components'][name]
    assert entry['provider']=='project mod runtime' and entry['source']=='Source/pc-port-source.zip'
    assert entry['notices']==[name.rsplit('/',1)[0]+'/Lua.txt']
    assert entry['sha256']=='b'*64 and not report['source_packages']
