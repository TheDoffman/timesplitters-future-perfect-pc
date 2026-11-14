"""Production helper bundles, compilation pins and provenance refusal."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys

import pytest

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
