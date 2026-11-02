"""Linux game host contracts that also run on Windows/Mac without game assets."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import pytest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile

@pytest.mark.parametrize('mutation',[False,True],ids=['scalar-movs','reject-high-byte-regression'])
def test_x64_mmio_decoder(tmp_path,mutation):
    header=(ROOT/'src/native/x64_access.h').read_text()
    if mutation:
        old='if(a.result_bytes==1&&!rex&&reg>=4&&reg<8)'
        assert old in header;header=header.replace(old,'if(a.bytes==1&&!rex&&reg>=4&&reg<8)')
    (tmp_path/'x64_access.h').write_text(header)
    (tmp_path/'x64_access_cases.h').write_bytes((ROOT/'tests/gameplay/x64_access_cases.h').read_bytes())
    source=tmp_path/'decoder.c';source.write_text('#include "x64_access_cases.h"\nint main(void){return tsfp_x64_decoder_cases()?0:1;}\n')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,source,extra=flags)
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
    assert (result.returncode!=0)==mutation,result.stdout+result.stderr

def builder():
    spec=importlib.util.spec_from_file_location('linux_game_builder',ROOT/'tools/build/build-native-linux.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

def test_build_rejects_wrong_host_before_runtime_changes(tmp_path,monkeypatch):
    module=builder();monkeypatch.setattr(module.platform,'system',lambda:'Windows')
    monkeypatch.setattr(sys,'argv',['build-native-linux.py','--game-dir',str(tmp_path),'--setup','--generate'])
    monkeypatch.setattr(module,'run',lambda *a,**k:pytest.fail('Wrong-host runtime mutation'))
    with pytest.raises(SystemExit) as error:module.main()
    assert error.value.code==2

def test_build_rejects_wrong_xbe_before_dependencies_or_patches(tmp_path,monkeypatch):
    module=builder();monkeypatch.setattr(module.platform,'system',lambda:'Linux')
    monkeypatch.setattr(module.platform,'machine',lambda:'x86_64')
    (tmp_path/'default.xbe').write_bytes(b'wrong game')
    monkeypatch.setattr(sys,'argv',['build-native-linux.py','--game-dir',str(tmp_path),'--setup','--generate'])
    monkeypatch.setattr(module,'run',lambda *a,**k:pytest.fail('Invalid-XBE runtime mutation'))
    monkeypatch.setattr(module.linux_vulkan_sdk,'verify_install',lambda *a:pytest.fail('Invalid-XBE dependency access'))
    with pytest.raises(SystemExit) as error:module.main()
    assert error.value.code==2
