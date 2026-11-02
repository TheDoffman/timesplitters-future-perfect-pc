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


def runner():
    spec=importlib.util.spec_from_file_location('linux_game_runner',ROOT/'tools/testing/test-native-linux.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module


@pytest.mark.skipif(os.name=='nt',reason='POSIX no-follow file creation')
def test_runner_resets_only_disposable_high_refresh_preference(tmp_path):
    rate=tmp_path/'pc-frame-rate.ini';rate.write_text('TSFP_FRAME_RATE_V1 240\n')
    unrelated=tmp_path/'pc-display.ini';unrelated.write_text('retain resolution and FOV')
    runner().test_frame_rate(tmp_path,60)
    assert rate.read_text()=='TSFP_FRAME_RATE_V1 0\n'
    assert unrelated.read_text()=='retain resolution and FOV'


@pytest.mark.skipif(os.name=='nt',reason='POSIX no-follow file creation')
def test_runner_rejects_linked_frame_rate_without_changing_its_target(tmp_path):
    personal=tmp_path/'personal.ini';personal.write_text('TSFP_FRAME_RATE_V1 240\n')
    (tmp_path/'pc-frame-rate.ini').symlink_to(personal)
    with pytest.raises(OSError):runner().test_frame_rate(tmp_path,60)
    assert personal.read_text()=='TSFP_FRAME_RATE_V1 240\n'


@pytest.mark.skipif(os.name=='nt',reason='POSIX no-follow file creation')
@pytest.mark.parametrize('fps',[60,120,180,240])
def test_runner_selects_each_supported_preset_over_stale_preferences(tmp_path,fps):
    path=tmp_path/'pc-frame-rate.ini';path.write_text('TSFP_FRAME_RATE_V1 180\n')
    runner().test_frame_rate(tmp_path,fps)
    assert path.read_text()==f'TSFP_FRAME_RATE_V1 {0 if fps==60 else fps}\n'


def test_runner_rejects_unknown_preset_without_creating_files(tmp_path):
    with pytest.raises(ValueError):runner().test_frame_rate(tmp_path,200)
    assert not list(tmp_path.iterdir())


@pytest.mark.parametrize('bad',[
    {'at':float('nan'),'commands':['+E']},
    {'at':1,'commands':['quit']},
    {'at':1,'commands':['+E\ncrash']},
    {'at':200,'commands':['+E']},
    {'at':True,'commands':['+E']},
])
def test_input_schedule_rejects_unsafe_or_unbounded_commands(tmp_path,bad):
    import json
    path=tmp_path/'script.json';path.write_text(json.dumps({'schema':1,'events':[bad]}))
    with pytest.raises(ValueError):runner().input_script(path,150)


def test_input_schedule_validates_real_benchmark_and_order(tmp_path):
    import json
    module=runner();events=module.input_script(ROOT/'tests/fixtures/linux-story-benchmark.json',150)
    assert events[0]==(6,'focus 1\n+E\n')
    assert events[-1]==(93.3,'-E\n')
    path=tmp_path/'script.json';path.write_text(json.dumps({'schema':1,'events':[
        {'at':2,'commands':['+E']},{'at':1,'commands':['-E']}]}))
    with pytest.raises(ValueError):module.input_script(path,150)
