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
    assert events[-1]==(20.3,'-E\n')
    path=tmp_path/'script.json';path.write_text(json.dumps({'schema':1,'events':[
        {'at':2,'commands':['+E']},{'at':1,'commands':['-E']}]}))
    with pytest.raises(ValueError):module.input_script(path,150)


@pytest.mark.parametrize('command', ['button 1 1','button 5 0','pointer 640 480','motion -4096 4096','wheel -8'])
def test_script_accepts_bounded_mouse_input(command):
    assert runner().script_command(command)


@pytest.mark.parametrize('command', ['button 6 1','button 1 2','motion inf 0','pointer nan 3',
                                    'motion 4097 0','wheel 9','wheel 1\nquit','pointer 0 1 2'])
def test_script_rejects_unbounded_mouse_input(command):
    assert not runner().script_command(command)


def test_story_observer_distinguishes_intro_pause_and_active_gameplay():
    module=runner()
    words={0x79094C:2,0x7DE458:0,0x761C48:0}
    assert module.story_snapshot(words.__getitem__)['active']
    words[0x7DE458]=0x20000000
    assert not module.story_snapshot(words.__getitem__)['active']
    words[0x79094C]=24
    assert not module.story_snapshot(words.__getitem__)['active']
    words[0x79094C]=2
    words[0x7DE458]=0;words[0x761C48]=0x10000;words[0x10000]=1
    assert not module.story_snapshot(words.__getitem__)['active']
    words[0x761C48]=0xfffffff0
    with pytest.raises(ValueError):module.story_snapshot(words.__getitem__)


def test_story_window_uses_complete_host_intervals_after_active_warmup(tmp_path):
    import csv
    module=runner();origin=10**12
    path=tmp_path/'frames.csv'
    with path.open('w',newline='') as out:
        writer=csv.DictWriter(out,fieldnames=['frame','elapsed_ms','frame_ms','swap_ms','kind','guest_frame','clock_ns'])
        writer.writeheader()
        for frame in range(2,402):
            writer.writerow(dict(frame=frame,elapsed_ms=(frame-1)*100,frame_ms=100,
                                 swap_ms=1,kind='guest',guest_frame=frame,clock_ns=origin+(frame-1)*100_000_000))
    samples=[dict(active=3<=second<=35,clock_ns=origin+second*10**9) for second in range(40)]
    result=module.story_window(samples,path)
    assert result['guest_fps']==10 and result['start_ns']>=origin+8*10**9
    with pytest.raises(ValueError):module.story_window([dict(active=False,clock_ns=origin)],path)


def test_packaged_runtime_uses_manifest_hash_without_checkout_sdk(tmp_path,monkeypatch):
    import hashlib,json,importlib.util
    spec=importlib.util.spec_from_file_location('package_linux_runner',ROOT/'tools/testing/test-native-linux.py')
    runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
    exe=tmp_path/'Bin/futureperfect_recomp';exe.parent.mkdir();exe.write_bytes(b'packaged executable')
    (tmp_path/'BUILD.json').write_text(json.dumps({'source_commit':'fixture-revision','files':{'Bin/futureperfect_recomp':hashlib.sha256(exe.read_bytes()).hexdigest()}}))
    _,actual,manifest=runner.runtime_target(tmp_path)
    assert actual==exe and manifest['source_revision']=='fixture-revision'
    assert manifest['executable_sha256']==hashlib.sha256(exe.read_bytes()).hexdigest()
    monkeypatch.setattr(runner.linux_vulkan_sdk,'verify_install',lambda:pytest.fail('Packaged game used checkout SDK'))
    monkeypatch.setenv('LD_LIBRARY_PATH','checkout-sdk');monkeypatch.setenv('VK_DRIVER_FILES','checkout-driver')
    env=runner.runtime_environment(tmp_path)
    with pytest.raises(ValueError,match='Packaged acceptance'):
        runner.runtime_environment(tmp_path,tmp_path/'checkout-sdk')
    assert 'LD_LIBRARY_PATH' not in env and 'VK_DRIVER_FILES' not in env
    assert env['RECOMP_FFMPEG_DIR']==str(tmp_path/'Bin')
    exe.write_bytes(b'changed')
    with pytest.raises(ValueError,match='differs'):runner.runtime_target(tmp_path)


@pytest.mark.parametrize('text,requested,active',[
    ('[RHI] vulkan: llvmpipe, Vulkan 1.4, depth D24S8, validation on',True,True),
    ('[RHI] vulkan: validation asked for, but the layer is not installed',True,False),
    ('[RHI] vulkan: llvmpipe, validation on',False,False),
    ('',True,False)])
def test_validation_receipt_reports_actual_layer(text,requested,active):
    import importlib.util
    spec=importlib.util.spec_from_file_location('linux_validation_runner',ROOT/'tools/testing/test-native-linux.py')
    runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
    assert runner.validation_active(text,requested)==active


def test_owned_resource_snapshot_uses_only_scalar_child_fields(tmp_path):
    import importlib.util
    spec=importlib.util.spec_from_file_location('linux_resources_runner',ROOT/'tools/testing/test-native-linux.py')
    runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
    base=tmp_path/'123';(base/'fd').mkdir(parents=True)
    (base/'status').write_text('Name: game\nVmSize: 8192 kB\nVmRSS: 1024 kB\nThreads: 4\n')
    for name in ('0','1','2'):(base/'fd'/name).touch()
    assert runner.owned_resource_snapshot(123,5.1234,tmp_path)==dict(seconds=5.123,rss_bytes=1048576,virtual_bytes=8388608,threads=4,open_fds=3)


def test_startup_movie_receipt_requires_complete_decode_not_only_open():
    text = r"[XMV] playing d:\xmv\eag_e.xmv: 640x480, PCM sound"+"\n"
    assert runner().movie_results(text)==[]
    partial=text+"[XMV] movie closed after 2 pictures\n"
    assert runner().movie_results(partial)==[dict(name='eag_e.xmv',pictures=2,eof=False)]
    full=text+"[XMV] update #176: the movie is over\n[XMV] movie closed after 88 pictures\n"
    assert runner().movie_results(full)==[dict(name='eag_e.xmv',pictures=88,eof=True)]
