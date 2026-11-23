"""The source installer verifies pinned bytes and never extracts archive paths."""
import hashlib
import io
import json
from pathlib import Path
import tarfile
import pytest
import spirv_cross as cross

def fixture(root,extra=None):
    content={name:b'fixture' for name in (*cross.SOURCES,'LICENSE','spirv_glsl.hpp','spirv_cross.hpp')}
    content.update(extra or {})
    buffer=io.BytesIO()
    with tarfile.open(fileobj=buffer,mode='w:gz') as archive:
        for name,data in content.items():
            member=tarfile.TarInfo('source/'+name);member.size=len(data)
            archive.addfile(member,io.BytesIO(data))
    data=buffer.getvalue()
    pin=dict(commit='12345678',url='https://invalid.example/source.tar.gz',bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
    config=root/'config/build';config.mkdir(parents=True)
    (config/'spirv-cross.json').write_text(json.dumps(pin))
    download=root/'private/downloads/spirv-cross-12345678.tar.gz';download.parent.mkdir(parents=True);download.write_bytes(data)
    return download

def test_verified_minimal_sources_and_preserved_license(tmp_path):
    fixture(tmp_path,{'../outside.hpp':b'outside','unrelated.cpp':b'not part of the library'})
    source=cross.setup(tmp_path)
    assert cross.verify(tmp_path)==source
    assert (source/'LICENSE').read_bytes()==b'fixture'
    assert not (source/'unrelated.cpp').exists()
    assert not (tmp_path/'external/outside.hpp').exists()
    (source/'spirv_glsl.cpp').write_text('altered')
    with pytest.raises(ValueError,match='changed'):cross.setup(tmp_path)

def test_corrupt_archive_cannot_create_dependency(tmp_path):
    archive=fixture(tmp_path);archive.write_bytes(b'corrupt')
    with pytest.raises(ValueError,match='pinned bytes/hash'):cross.setup(tmp_path)
    assert not (tmp_path/'external/spirv-cross').exists()
