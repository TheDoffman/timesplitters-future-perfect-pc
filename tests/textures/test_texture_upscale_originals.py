"""Asset-free checks for the original-based local upscale pipeline."""
import importlib.util
import io
from pathlib import Path
import sys

import numpy as np
from PIL import Image
import pytest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('original_upscale',ROOT/'tools/textures/texture-upscale-originals.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)


def test_alpha_survives_ai_and_dds(tmp_path):
    source=Image.new('RGBA',(8,4),(90,120,20,255))
    source.putalpha(Image.fromarray(np.arange(32,dtype=np.uint8).reshape(4,8)*8))
    enhanced=Image.new('RGB',(32,16),(100,110,40))
    row={'key':'0123456789abcdef','output_size':[16,8],'route':'realesrgan-x4plus'}
    entry={'width':8,'height':4,'xbox_format':15,'sha256':'a'*64}
    expected=source.getchannel('A').resize((16,8),Image.Resampling.BILINEAR)
    m.compile_one(source,enhanced,row,entry,tmp_path,{},1)
    dds=tmp_path/'0123456789abcdef.dds'
    decoded=Image.open(dds).convert('RGBA')
    assert decoded.getchannel('A').tobytes()==expected.tobytes()
    assert decoded.getpixel((0,0))[:3]==(100,110,40)
    with pytest.raises(ValueError,match='overwrite'):
        m.compile_one(source,enhanced,row,entry,tmp_path,{},1)
    broken=bytearray(dds.read_bytes());broken[-1]^=1;dds.write_bytes(broken)
    import json
    with pytest.raises(ValueError,match='Metadata mismatch'):
        m.audit.check_asset(dds,json.loads(dds.with_suffix('.json').read_text()),entry)


def test_original_source_rejects_bad_hash_and_path(tmp_path):
    payload=b'x'*136;(tmp_path/'game.pak').write_bytes(payload)
    entry={'key':'0'*16,'sha256':'f'*64,'sources':[{'archive':'game.pak','offset':0,'length':136}]}
    with pytest.raises(ValueError,match='source changed'):m.original(tmp_path,entry)
    entry['sources'][0]['archive']='../outside.pak'
    with pytest.raises(ValueError,match='escapes'):m.original(tmp_path,entry)


@pytest.mark.parametrize('fmt', [0,25,26])
def test_channel_data_never_uses_color_ai(fmt):
    assert m.choose_route({'xbox_format':fmt},{'visual_category':'environment'})=='original-channel-resample'


def test_unknown_and_ui_use_conservative_route():
    for category in ['interface','reflection','effect','material_uncertain','unknown']:
        assert m.choose_route({'xbox_format':12},{'visual_category':category})=='original-specialist-resample'
    assert m.choose_route({'xbox_format':12},{'visual_category':'environment'})=='realesrgan-x4plus'


def test_explicit_icon_approval_controls_generation(tmp_path):
    import json
    key='0123456789abcdef'
    entry=dict(key=key,width=64,height=64,xbox_format=12,sha256='a'*64,sources=[])
    (tmp_path/'catalog.json').write_text(json.dumps({'textures':[entry]}))
    (tmp_path/'visual-classification.json').write_text(json.dumps({'records':[
        dict(key=key,visual_category='interface')]}))
    policy=tmp_path/'policy.json'
    def plan(keys, approved):
        policy.write_text(json.dumps(dict(keys=keys,approved_upscaled_character_icon_keys=approved)))
        return m.build_plan(tmp_path,tmp_path,policy)[0]
    assert not plan([key],[])['records']
    assert plan([],[])['records'][0]['route']=='original-specialist-resample'
    assert plan([],[key])['records'][0]['route']=='realesrgan-x4plus'
    with pytest.raises(ValueError,match='both retained'):plan([key],[key])
    with pytest.raises(ValueError,match='Unknown approved'):plan([],['f'*16])
    with pytest.raises(ValueError,match='Invalid approved'):plan([],[key,key])
    entry['xbox_format']=25
    (tmp_path/'catalog.json').write_text(json.dumps({'textures':[entry]}))
    with pytest.raises(ValueError,match='64x64 color'):plan([],[key])


def test_scratch_removes_only_its_own_pngs(tmp_path):
    with m.scratch(tmp_path) as folder:
        saved=folder
        (folder/'in/a.png').write_bytes(b'not an actual image')
        (folder/'out/a.png').write_bytes(b'not an actual image')
    assert not saved.exists()


def test_budget_and_dimensions(tmp_path):
    assert m.dimensions({'width':1024,'height':512})==(2048,1024)
    with pytest.raises(ValueError):m.dimensions({'width':100,'height':64})
    source=Image.new('RGBA',(8,8),'white')
    with pytest.raises(ValueError,match='budget'):
        m.compile_one(source,source,{'key':'0'*16,'output_size':[16,16],'route':'original-specialist-resample'},
                      {'width':8,'height':8,'xbox_format':12,'sha256':'a'*64},tmp_path,{},0,m.BUDGET)
    assert not list(tmp_path.iterdir())


def test_large_overlap_keeps_pixel_positions(monkeypatch,tmp_path):
    source=Image.fromarray(np.random.default_rng(5).integers(0,256,(128,600,3),dtype=np.uint8))
    def fake_infer(args,inp,out):
        with Image.open(inp) as image:image.resize((image.width*4,image.height*4),Image.Resampling.NEAREST).save(out)
    monkeypatch.setattr(m,'infer',fake_infer)
    with m.scratch(tmp_path) as folder: result=m.large_image(None,source,folder)
    assert result.tobytes()==source.resize((2400,512),Image.Resampling.NEAREST).tobytes()

def test_candidate_lock_prevents_concurrent_writers(tmp_path):
    with m.candidate_lock(tmp_path/'pack'):
        with pytest.raises(ValueError,match='lock exists'):
            with m.candidate_lock(tmp_path/'pack'):pass
    assert not (tmp_path/'production.lock').exists()


def test_full_small_run_resumes_and_rejects_tampering(monkeypatch,tmp_path):
    import argparse,json
    root=tmp_path/'workspace';root.mkdir();game=tmp_path/'game';game.mkdir()
    (root/'review').mkdir()
    keys=['0'*16,'1'*16,'2'*16]
    entries=[dict(key=k,width=64,height=64,xbox_format=12,sha256=str(i)*64,sources=[])
             for i,k in enumerate(keys)]
    (root/'catalog.json').write_text(json.dumps({'textures':entries}))
    (root/'visual-classification.json').write_text(json.dumps({'records':[
        {'key':keys[0],'visual_category':'environment'},
        {'key':keys[1],'visual_category':'interface'},
        {'key':keys[2],'visual_category':'character'}]}))
    policy=tmp_path/'policy.json';policy.write_text(json.dumps({'keys':[keys[2]]}))
    modeldir=tmp_path/'models';modeldir.mkdir()
    for suffix in ('bin','param'):(modeldir/f'realesrgan-x4plus.{suffix}').write_bytes(b'model')
    exe=tmp_path/'engine';exe.write_bytes(b'engine')
    monkeypatch.setattr(m.variants.catalog.format_tool,'verify_mapping',lambda _:None)
    monkeypatch.setattr(m,'original',lambda game,entry:Image.new('RGBA',(64,64),(30,70,150,255)))
    calls=[]
    def fake_infer(args,inp,out):
        calls.append(True)
        for p in inp.glob('*.png'):
            with Image.open(p) as im:im.resize((im.width*4,im.height*4),Image.Resampling.NEAREST).save(out/p.name)
    monkeypatch.setattr(m,'infer',fake_infer)
    args=argparse.Namespace(root=root,game=game,output=root/'authored/test/pack',exe=exe,
         models=modeldir,policy=policy,plan_only=False,keys=None,limit=None,compare=None,gpu=0)
    m.run(args)
    assert len(list(args.output.glob('*.dds')))==2 and len(calls)==1
    result=json.loads((args.output.parent/'audit.json').read_text())
    assert result['full_scope_complete'] and result['candidate_count']==2
    hashes={p.name:m.sha(p.read_bytes()) for p in args.output.iterdir()}
    m.run(args)
    assert len(calls)==1 and hashes=={p.name:m.sha(p.read_bytes()) for p in args.output.iterdir()}
    target=args.output/f'{keys[0]}.dds';raw=bytearray(target.read_bytes());raw[-1]^=1;target.write_bytes(raw)
    with pytest.raises(ValueError,match='Metadata mismatch'):m.run(args)
    assert not (args.output.parent/'production.lock').exists()
    (args.output.parent/'activation.json').write_text('{}')
    with pytest.raises(ValueError,match='already activated'):m.run(args)

def test_activation_preserves_prior_pack_and_installs_hardlinks(monkeypatch,tmp_path):
    import json
    spec=importlib.util.spec_from_file_location('activate_originals',ROOT/'tools/textures/texture-activate-originals.py')
    a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)
    root=tmp_path/'workspace'; candidate=root/'authored/new/pack';candidate.mkdir(parents=True)
    (candidate/'0123456789abcdef.dds').write_bytes(b'candidate')
    (candidate/'0123456789abcdef.json').write_bytes(b'{}')
    old=root/'pack';old.mkdir();(old/'old.dds').write_bytes(b'prior artwork')
    install=tmp_path/'installation';(install/'Settings').mkdir(parents=True)
    (install/'TimeSplitters Launcher.exe').write_bytes(b'launcher');(install/'BUILD.json').write_text('{}')
    controls={'mouse':{'sensitivity':0.2},'launcher':{'language_pack':'fr','skip_startup_splashes':True}}
    (install/'Settings/pc-controls.json').write_text(json.dumps(controls))
    monkeypatch.setattr(a,'validate',lambda root,candidate:(list(candidate.iterdir()),{'keys':1,'logical_bytes':11}))
    if a.os.name=='nt':
        import subprocess
        monkeypatch.setattr(subprocess,'check_output',lambda *args,**kwargs:'')
    result=a.activate(root,candidate,install)
    assert (candidate.parent/'previous-enhanced-pack/old.dds').read_bytes()==b'prior artwork'
    assert (root/'pack/0123456789abcdef.dds').read_bytes()==b'candidate'
    linked=install/'Textures/0123456789abcdef.dds'
    assert linked.stat().st_ino==(root/'pack'/linked.name).stat().st_ino
    after=json.loads((install/'Settings/pc-controls.json').read_text())
    expected=json.loads(json.dumps(controls));expected['launcher']['enhanced_textures']=True
    assert after==expected and result['enhanced_enabled']


def test_activation_failure_rolls_back_pack_moves(monkeypatch,tmp_path):
    spec=importlib.util.spec_from_file_location('activate_originals_failure',ROOT/'tools/textures/texture-activate-originals.py')
    a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)
    root=tmp_path/'workspace'; candidate=root/'authored/new/pack';candidate.mkdir(parents=True)
    (candidate/'new.dds').write_bytes(b'new');(root/'pack').mkdir();(root/'pack/old.dds').write_bytes(b'old')
    monkeypatch.setattr(a,'validate',lambda root,candidate:(list(candidate.iterdir()),{}))
    real_rename=Path.rename
    def fail_move(self,target):
        if self==candidate:raise OSError('simulated move failure')
        return real_rename(self,target)
    monkeypatch.setattr(Path,'rename',fail_move)
    with pytest.raises(OSError,match='simulated'):a.activate(root,candidate)
    assert (root/'pack/old.dds').read_bytes()==b'old' and (candidate/'new.dds').read_bytes()==b'new'


def test_activation_rejects_incomplete_scope(tmp_path):
    import json
    spec=importlib.util.spec_from_file_location('activate_originals_incomplete',ROOT/'tools/textures/texture-activate-originals.py')
    a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)
    root=tmp_path/'workspace';candidate=root/'authored/new/pack';candidate.mkdir(parents=True)
    raw=json.dumps({'textures':[]}).encode();(root/'catalog.json').write_bytes(raw)
    (candidate.parent/'plan.json').write_text(json.dumps({'records':[{'key':'0'*16}],'retained_original_keys':[],'catalog_sha256':m.sha(raw)}))
    with pytest.raises(ValueError,match='incomplete'):a.activate(root,candidate)
    assert not (root/'pack').exists()

@pytest.mark.parametrize('pixels', ['black','noise'])
def test_neural_fidelity_guard_rejects_bad_gpu_output(tmp_path,pixels):
    source=Image.new('RGBA',(64,64),(100,90,30,255))
    enhanced=(Image.new('RGB',(256,256),'black') if pixels=='black' else
              Image.fromarray(np.random.default_rng(12).integers(0,256,(256,256,3),dtype=np.uint8)))
    row={'key':'a'*16,'output_size':[128,128],'route':'realesrgan-x4plus'}
    entry={'width':64,'height':64,'xbox_format':12,'sha256':'b'*64}
    with pytest.raises(ValueError,match='fidelity guard'):
        m.compile_one(source,enhanced,row,entry,tmp_path,{},0)
    assert not list(tmp_path.iterdir())


def test_channel_texels_preserved_losslessly(tmp_path):
    source=Image.fromarray(np.random.default_rng(9).integers(0,256,(4,8,4),dtype=np.uint8))
    row={'key':'c'*16,'output_size':[16,8],'route':'original-channel-resample'}
    entry={'width':8,'height':4,'xbox_format':26,'sha256':'b'*64}
    result=m.compile_one(source,source,row,entry,tmp_path,{},0)
    expected=source.resize((16,8),Image.Resampling.NEAREST)
    assert result.tobytes()==expected.tobytes()
    for y in range(4):
        for x in range(8): assert result.getpixel((x*2,y*2))==source.getpixel((x,y))


@pytest.mark.parametrize('size',[(256,8),(256,32),(16,16)])
def test_small_or_thin_color_does_not_use_neural_model(size):
    assert m.choose_route({'xbox_format':12,'width':size[0],'height':size[1]},
                          {'visual_category':'environment'})=='original-thin-resample'
