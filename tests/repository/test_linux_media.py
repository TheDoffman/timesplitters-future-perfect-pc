"""Release media must keep exact source/binary provenance and decoder license policy."""
import hashlib,json
from pathlib import Path
import pytest
import linux_ffmpeg as media

def fixture(root):
    config=root/'config/build';config.mkdir(parents=True)
    spec=dict(version='fixture',archive='source.tar.gz',sha256=hashlib.sha256(b'source').hexdigest(),libraries=['libavcodec.so.60','libavutil.so.58'])
    (config/'ffmpeg-linux.json').write_text(json.dumps(spec))
    base=media.location(root);(base/'install/lib').mkdir(parents=True)
    (base/spec['archive']).write_bytes(b'source')
    libraries={}
    for name in spec['libraries']:(base/'install/lib'/name).write_bytes(name.encode());libraries[name]=media.digest(base/'install/lib'/name)
    receipt=dict(pin=spec,libraries=libraries,license='LGPL version 2.1 or later',configuration='--enable-decoder=wmv2')
    (base/'BUILD.json').write_text(json.dumps(receipt));return base,receipt

def test_source_and_binary_integrity(tmp_path):
    base,receipt=fixture(tmp_path);assert media.verify_install(tmp_path)==(base,receipt)
    (base/'install/lib/libavcodec.so.60').write_bytes(b'changed')
    with pytest.raises(ValueError,match='library changed'):media.verify_install(tmp_path)

@pytest.mark.parametrize('change',['source','pin','missing','gpl','nonfree'])
def test_reject_incomplete_or_different_media(tmp_path,change):
    base,receipt=fixture(tmp_path)
    if change=='source':(base/'source.tar.gz').write_bytes(b'changed')
    if change=='pin':receipt['pin']['version']='changed'
    if change=='missing':receipt['libraries'].pop('libavutil.so.58')
    if change in ('gpl','nonfree'):receipt['configuration']='--enable-'+change
    (base/'BUILD.json').write_text(json.dumps(receipt))
    with pytest.raises(ValueError):media.verify_install(tmp_path)
