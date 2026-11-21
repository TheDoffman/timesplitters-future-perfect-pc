"""Pinned shader compiler delivery must reject corrupt archives and installed bytes."""
import hashlib
import io
import json
from pathlib import Path
import sys
import zipfile
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools/build'))
import windows_vulkan as dxc


def fixture(tmp_path):
    stream=io.BytesIO()
    with zipfile.ZipFile(stream,'w') as archive:
        for name in dxc.FILES:
            archive.writestr(name, 'fixture '+name)
        archive.writestr('../../escaped.txt','must never extract')
    data=stream.getvalue()
    config=tmp_path/'config/build'
    config.mkdir(parents=True)
    pin=dict(version='test',url='https://invalid.example/dxc.zip',bytes=len(data),
             sha256=hashlib.sha256(data).hexdigest())
    (config/'dxc-windows.json').write_text(json.dumps(pin))
    download=tmp_path/'private/downloads'
    download.mkdir(parents=True)
    (download/'dxc-test.zip').write_bytes(data)
    (tmp_path/'external/ts3FPrecomp/third_party').mkdir(parents=True)
    return download/'dxc-test.zip'


def test_install_verify_and_modified_binary(tmp_path):
    fixture(tmp_path)
    directory=dxc.setup(tmp_path)
    assert dxc.verify(tmp_path)==directory
    assert not (tmp_path/'external/ts3FPrecomp/escaped.txt').exists()
    (directory/'bin/x64/dxcompiler.dll').write_bytes(b'modified')
    with pytest.raises(ValueError,match='file changed'):
        dxc.verify(tmp_path)
    with pytest.raises(ValueError,match='file changed'):
        dxc.setup(tmp_path)


def test_corrupt_download_never_installed(tmp_path):
    archive=fixture(tmp_path)
    archive.write_bytes(b'corrupt')
    with pytest.raises(ValueError,match='publisher digest'):
        dxc.setup(tmp_path)
    assert not (tmp_path/'external/ts3FPrecomp/third_party/dxc').exists()


def test_gameplay_cannot_accept_fallback_or_absent_validation():
    from native_refresh_game import renderer_checks
    log='[RHI] vulkan: hardware, validation on\n[PRESENT] backend=vulkan total=300 guest=300'
    assert all(renderer_checks(log,'vulkan',True).values())
    assert not all(renderer_checks(log.replace('validation on','validation off'),'vulkan',True).values())
    assert not all(renderer_checks(log.replace('backend=vulkan','backend=d3d11'),'vulkan',True).values())
    assert not all(renderer_checks(log+'\n[VKVAL] invalid image barrier','vulkan',True).values())
    assert not all(renderer_checks(log+'\n[RHI] ERROR: present failed','vulkan',True).values())
    assert all(renderer_checks(log+'\n[HLE-D3D8] skipped 0 unknown shader, 0 stride, 0 failed','vulkan',True).values())
    assert not all(renderer_checks(log+'\nD3D8 VSH: Failed to compile shader','vulkan',True).values())
    assert not all(renderer_checks('','vulkan').values())
