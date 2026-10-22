"""Offline texture tool checks; tiny synthetic images, no game-data copies."""
import importlib.util
import io
from pathlib import Path
import struct
import subprocess
import sys
import pytest
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
def module(name):
    spec=importlib.util.spec_from_file_location(name,ROOT/'tools'/f'{name}.py')
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
pack=module('texture-pack');catalog=module('texture-catalog')
worklist=module('texture-worklist')
inventory=module('texture-inventory')
formats=module('texture-formats')

def test_mip_chain_and_compressed_roundtrip():
    b,n=pack.encode(Image.new('RGBA',(8,4),(120,80,40,255)),'DXT1')
    assert n==4 and len(b)==128+16+8+8+8
    assert struct.unpack_from('<I',b,28)[0]==4
    color=Image.open(io.BytesIO(b)).convert('RGB').getpixel((0,0))
    assert all(abs(a-b)<9 for a,b in zip(color,(120,80,40)))

def test_alpha_and_bad_dimensions():
    b,n=pack.encode(Image.new('RGBA',(4,4),(80,80,80,0)),'DXT5')
    assert n==3 and Image.open(io.BytesIO(b)).getpixel((0,0))[3]==0
    with pytest.raises(ValueError):pack.encode(Image.new('RGB',(7,4)))

def test_content_identity():
    assert catalog.texture_key(12,4,4,bytes(range(8)))=='1fc92492b76218f1'
    assert catalog.texture_key(14,4,4,bytes(range(8)))!='1fc92492b76218f1'

def test_archive_index_bounds(tmp_path):
    p=tmp_path/'bad.pak';p.write_bytes(struct.pack('<4sII',b'P5CK',500,16))
    with pytest.raises(ValueError):list(catalog.entries(p))

def test_budget_rejection_writes_no_asset(tmp_path):
    result=subprocess.run([sys.executable,str(ROOT/'tools/texture-pack.py'),'nonexistent.png',
        '--key','0000000000000001','--width','4096','--height','4096','--budget-mib','1',
        '--output',str(tmp_path/'pack')],capture_output=True,text=True)
    assert result.returncode!=0 and 'budget exceeded' in result.stderr
    assert not (tmp_path/'pack').exists()


@pytest.mark.parametrize('size,alpha,allow,channels,accepted', [
    ((1025,512),255,False,'color',False),
    ((1025,512),255,True,'color',True),
    ((512,1025),255,True,'color',True),
    ((1026,512),255,True,'color',False),
    ((513,256),255,True,'color',False),
    ((1025,512),254,True,'color',False),
    ((1025,512),255,True,'luminance',False),
])
def test_explicit_source_rounding_is_bounded_and_recorded(tmp_path,size,alpha,allow,channels,accepted):
    import json
    source=tmp_path/'source.png';Image.new('RGBA',size,(70,90,110,alpha)).save(source)
    output=(256,512) if size[0]<size[1] else (512,256)
    args=[sys.executable,str(ROOT/'tools/texture-pack.py'),str(source),
          '--key','0000000000000001','--width',str(output[0]),'--height',str(output[1]),
          '--format','DXT5','--channels',channels,'--output',str(tmp_path/'pack')]
    if allow:args.append('--allow-one-pixel-aspect-rounding')
    result=subprocess.run(args,capture_output=True,text=True)
    assert (result.returncode==0)==accepted,result.stderr
    if not accepted:
        assert not (tmp_path/'pack').exists()
        return
    meta=json.loads((tmp_path/'pack/0000000000000001.json').read_text())
    assert meta['source_size']==list(size) and meta['output_size']==list(output)
    assert meta['source_aspect_rounding']==dict(source_long_axis_error_pixels=1.0,
                                               explicitly_allowed=True,resampling='LANCZOS')
    expected,mips=pack.encode(Image.open(source).convert('RGBA').resize(output,Image.Resampling.LANCZOS),'DXT5')
    assert (tmp_path/'pack/0000000000000001.dds').read_bytes()==expected
    assert meta['mips']==mips and meta['resampled']


def test_worklist_deduplicates_keys_but_keeps_unsupported(tmp_path):
    import json
    entries=[]
    for digest, key in [('a', '1'*16), ('b', '1'*16), ('c', None)]:
        entry=dict(sha256=digest, width=4, height=4, archive_format=0 if key else 5,
                   sources=[dict(archive=f'pak/{digest}.pak')])
        if key: entry.update(key=key, xbox_format=12)
        entries.append(entry)
    (tmp_path/'catalog.json').write_text(json.dumps(dict(scope='test',textures=entries)))
    report=worklist.build(tmp_path)
    assert report['counts']==dict(needs_art=1,unsupported_format=1)
    assert not report['complete']
    row=next(t for t in report['textures'] if t['id']=='1'*16)
    assert row['catalog_payloads']==2 and row['occurrences']==2
    assert row['archives']==['pak/a.pak','pak/b.pak']


def test_retained_original_skips_existing_draft_and_rejects_reinstallation(tmp_path):
    import json
    key = '1'*16
    entry = dict(key=key, sha256='a', width=64, height=64, archive_format=1,
                 xbox_format=14, sources=[dict(archive='pak/icons.pak')])
    (tmp_path/'catalog.json').write_text(json.dumps(dict(scope='test', textures=[entry])))
    policy = tmp_path/'originals.json'
    policy.write_text(json.dumps(dict(keys=[key], reason='Keep the original icon')))
    authored = tmp_path/'authored'
    authored.mkdir()
    (authored/'draft.png').write_bytes(b'old generated source')
    (authored/f'{key}-prompt.json').write_text(json.dumps(dict(source='draft.png')))
    report = worklist.build(tmp_path, originals_path=policy)
    assert report['counts'] == {'retained_original': 1}
    assert report['planned_supported_dds_bytes'] == 0
    assert report['textures'][0]['target_size'] is None
    (tmp_path/'pack').mkdir()
    (tmp_path/'pack'/f'{key}.dds').write_bytes(b'accidental reinstallation')
    with pytest.raises(ValueError, match='Retained-original texture has a replacement'):
        worklist.build(tmp_path, originals_path=policy)


def test_temporary_storage_includes_comparison_and_unknown_output_directories(tmp_path, monkeypatch):
    monkeypatch.setattr(worklist, 'TEMPORARY_BUDGET_BYTES', 32)
    contents = {'review/evidence.json': 5, 'comparison/packs/test/file.dds': 11,
                'new-staging/output.bin': 16, 'authored/source.png': 40,
                'pack/active.dds': 40, 'catalog.json': 40}
    for name, size in contents.items():
        path = tmp_path/name; path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'x'*size)
    files = [p for p in tmp_path.rglob('*') if p.is_file()]
    report = worklist.temporary_storage(tmp_path, files)
    assert report['temporary_logical_bytes'] == 32
    assert report['temporary_headroom_bytes'] == 0
    assert not report['temporary_budget_compliant']
    assert set(report['temporary_directories']) == {'review', 'comparison', 'new-staging'}
    assert report['temporary_allocated_bytes'] == sum(worklist.allocation(tmp_path/n)
        for n in contents if n.startswith(('review/', 'comparison/', 'new-staging/')))
    (tmp_path/'new-staging/output.bin').write_bytes(b'x'*15)
    assert worklist.temporary_storage(tmp_path, files)['temporary_budget_compliant']


def test_worklist_cli_reports_overbudget_before_returning_failure(tmp_path):
    import json
    (tmp_path/'catalog.json').write_text(json.dumps(dict(scope='test', textures=[])))
    (tmp_path/'comparison').mkdir()
    # Tiny fixture: override only the threshold in the child, not its accounting.
    (tmp_path/'comparison/sample.bin').write_bytes(b'x'*32)
    code = ("import importlib.util; from pathlib import Path; "
            f"s=importlib.util.spec_from_file_location('worklist', {str(ROOT/'tools/texture-worklist.py')!r}); "
            "m=importlib.util.module_from_spec(s); s.loader.exec_module(m); "
            "m.TEMPORARY_BUDGET_BYTES=32; m.main()")
    result = subprocess.run([sys.executable, '-c', code, '--root', str(tmp_path)],
                            capture_output=True, text=True)
    assert result.returncode == 2 and 'before production' in result.stderr
    report = json.loads((tmp_path/'worklist.json').read_text())
    assert report['temporary_logical_bytes'] == 32
    assert not report['temporary_budget_compliant']


def test_worklist_rejects_stale_metadata_and_alpha_loss(tmp_path):
    import hashlib
    data,mips=pack.encode(Image.new('RGBA',(8,8),(80,90,100,255)), 'DXT1')
    path=tmp_path/('1'*16+'.dds');path.write_bytes(data)
    meta=dict(key=path.stem,sha256=hashlib.sha256(data).hexdigest(),output_size=[8,8],bytes=len(data))
    original=dict(width=4,height=4,xbox_format=12)
    assert worklist.check_asset(path,meta,original)==len(data)
    with pytest.raises(ValueError,match='Alpha format lost'):
        worklist.check_asset(path,meta,dict(original,xbox_format=15))
    with pytest.raises(ValueError,match='Metadata mismatch'):
        worklist.check_asset(path,dict(meta,sha256='0'*64),original)
    path.write_bytes(data[:-8])
    with pytest.raises(ValueError,match='Incomplete mip chain'):
        worklist.check_asset(path,meta,original)


def test_budget_counts_metadata_before_writing(tmp_path):
    image=tmp_path/'source.png';Image.new('RGB',(4,4),(90,90,90)).save(image)
    output=tmp_path/'pack';output.mkdir()
    encoded,_=pack.encode(Image.open(image).convert('RGBA'),'DXT1')
    # The DDS alone fits exactly; its required metadata must trigger rejection.
    (output/'existing.bin').write_bytes(bytes(1024*1024-len(encoded)))
    result=subprocess.run([sys.executable,str(ROOT/'tools/texture-pack.py'),str(image),
        '--key','0000000000000001','--width','4','--height','4','--format','DXT1',
        '--budget-mib','1','--output',str(output)],capture_output=True,text=True)
    assert result.returncode!=0 and 'including texture metadata' in result.stderr
    assert len(list(output.iterdir()))==1


def test_inventory_keeps_uncataloged_entries_and_deduplicates_shared_assets(tmp_path):
    import json
    game=tmp_path/'game'; (game/'pak').mkdir(parents=True)
    root=tmp_path/'art';root.mkdir()
    # Two shared texture occurrences and an entry not proven texture-free.
    data=bytearray(128)
    struct.pack_into('<4sII',data,0,b'P5CK',12,48)
    for index, crc in enumerate((1,2,3)):
        struct.pack_into('<4I',data,12+index*16,crc,64+index*16,16,0)
    (game/'pak/gun.pak').write_bytes(data)
    textures=[dict(sha256=str(i),width=4,height=4,archive_format=0,
                   xbox_format=12,key='1'*16,
                   sources=[dict(archive='pak/gun.pak',crc=f'{i:08x}',offset=64+(i-1)*16,length=16)])
              for i in (1,2)]
    (root/'catalog.json').write_text(json.dumps(dict(scope='fixture',textures=textures)))
    report=inventory.build(game,root)
    assert report['totals']['cataloged_texture_entries']==2
    assert report['totals']['unclassified_entries']==1
    assert report['unique_supported_keys']==1
    assert report['groups']['weapons']['texture_ids']==['1'*16]
    assert report['budget_mib']==6144
    assert not report['complete'] and not report['playtest_gate']['ready']
    textures[0]['sources'][0]['offset']=65
    (root/'catalog.json').write_text(json.dumps(dict(scope='fixture',textures=textures)))
    with pytest.raises(ValueError,match='Stale catalog'):
        inventory.build(game,root)


def test_unknown_format_byte_counts_remain_hypotheses():
    assert inventory.size_hypotheses(256,256,349504)==['32-bit 7 mip levels']
    assert inventory.size_hypotheses(64,64,4096)==['8-bit base level']
    assert inventory.size_hypotheses(64,64,4095)==[]


def test_rectangular_morton_decode_uses_both_axes():
    # Known 4x2 Morton ordering: rows take indices 0,1,4,5 and 2,3,6,7.
    assert formats.unswizzle(bytes(range(8)),4,2,1)==bytes([0,1,4,5,2,3,6,7])
    assert formats.unswizzle(bytes(range(8)),2,4,1)==bytes(range(8))
    with pytest.raises(ValueError,match='truncated'):
        formats.unswizzle(bytes(7),4,2,1)
    with pytest.raises(ValueError,match='power-of-two'):
        formats.unswizzle(bytes(8),3,2,1)


def test_uncompressed_decode_preserves_channel_semantics():
    assert formats.decode(2,1,1,bytes([10,20,30,40])).getpixel((0,0))==(30,20,10,40)
    assert formats.decode(5,1,1,bytes([64])).getpixel((0,0))==(255,255,255,64)
    assert formats.decode(6,1,1,bytes([64])).getpixel((0,0))==(64,64,64,255)
    assert formats.decode(7,1,1,bytes([64,128])).getpixel((0,0))==(64,64,64,128)
    with pytest.raises(ValueError,match='verified uncompressed'):
        formats.decode(3,1,1,bytes(2))


def test_format_proof_rejects_wrong_executable(tmp_path):
    path=tmp_path/'wrong.xbe';path.write_bytes(bytes(512))
    with pytest.raises(ValueError,match='pinned US'):
        formats.verify_mapping(path)


def test_uncompressed_catalog_keys_use_original_swizzled_base_level():
    for kind,fmt,bpp in [(2,6,4),(5,25,1),(6,0,1),(7,26,2)]:
        raw=bytes(range(8*bpp))
        entry=catalog.classify(kind,4,2,raw+b'mip data')
        assert entry['key']==catalog.texture_key(fmt,4,2,raw)
        assert entry['xbox_format']==fmt
        assert 'scene check pending' in entry['runtime_validation']
        assert catalog.classify(kind,4,2,raw[:-1])=={}
    assert catalog.classify(3,4,2,bytes(64))=={}


def test_alpha_only_replacement_survives_bc3_encoding():
    source=formats.decode(5,4,4,bytes([0,255]*8))
    encoded,_=pack.encode(source,'DXT5')
    decoded=Image.open(io.BytesIO(encoded)).convert('RGBA')
    assert decoded.getchannel('A').getextrema()==(0,255)
    assert decoded.convert('RGB').getextrema()==((255,255),(255,255),(255,255))


def test_source_channel_conversion_preserves_original_semantics():
    source=Image.new('RGBA',(2,2),(120,80,40,72))
    lum=pack.channel_layout(source,'luminance').getpixel((0,0))
    assert lum[0]==lum[1]==lum[2] and lum[3]==255
    assert pack.channel_layout(source,'alpha').getpixel((0,0))==(255,255,255,72)
    assert pack.channel_layout(source,'color').getpixel((0,0))==(120,80,40,72)


def test_lossless_dds_preserves_gradient_and_alpha():
    source=Image.new('RGBA',(8,4));source.putdata([(i*7,i*7,i*7,i*5) for i in range(32)])
    data,mips=pack.encode(source,'RGBA8')
    assert Image.open(io.BytesIO(data)).convert('RGBA').tobytes()==source.tobytes()
    assert len(data)==128+128+32+8+4 and mips==4
    assert struct.unpack_from('<I',data,20)[0]==32


@pytest.mark.parametrize('pixel_format', ['RGBA8', 'DXT5'])
def test_original_alpha_compile_preserves_color_and_channel(tmp_path, pixel_format):
    import hashlib
    import json
    authored=tmp_path/'art.png'
    Image.new('RGBA',(8,4),(120,80,40,0)).save(authored)
    original=tmp_path/'original.png'
    mask=Image.new('RGBA',(4,2),(0,0,0,0))
    mask.putalpha(Image.frombytes('L',(4,2),bytes([0,64,128,255,255,128,64,0])))
    mask.save(original)
    digest=hashlib.sha256(original.read_bytes()).hexdigest()
    output=tmp_path/'pack'
    result=subprocess.run([sys.executable,str(ROOT/'tools/texture-pack.py'),str(authored),
        '--key','1'*16,'--width','16','--height','8','--format',pixel_format,
        '--output',str(output),'--alpha-source',str(original),
        '--alpha-source-sha256',digest],capture_output=True,text=True)
    assert result.returncode==0,result.stderr
    dds=output/('1'*16+'.dds')
    decoded=Image.open(dds).convert('RGBA')
    expected=mask.getchannel('A').resize((16,8),Image.Resampling.BILINEAR)
    alpha_error=max(abs(a-b)for a,b in zip(decoded.getchannel('A').tobytes(),expected.tobytes()))
    # BC3 is lossy (this gradient has up to 23/255 error with the pinned
    # encoder). Confirm the channel also matches an independently assembled
    # reference rather than treating the bound as proof of correct routing.
    assert alpha_error <= (0 if pixel_format=='RGBA8' else 32)
    reference=Image.new('RGBA',(16,8),(120,80,40,255));reference.putalpha(expected)
    reference_bytes,_=pack.encode(reference,pixel_format)
    assert decoded.getchannel('A').tobytes()==Image.open(io.BytesIO(reference_bytes)).getchannel('A').tobytes()
    assert all(abs(a-b)<=8 for a,b in zip(decoded.convert('RGB').getpixel((8,4)),(120,80,40)))
    meta=json.loads(dds.with_suffix('.json').read_text())
    assert meta['original_alpha']['sha256']==digest
    assert meta['original_alpha']['source_size']==[4,2]
    assert meta['original_alpha']['resampled']
    assert worklist.check_asset(dds,meta,dict(width=4,height=2,xbox_format=15))==dds.stat().st_size


@pytest.mark.parametrize('case', ['missing_hash','bad_hash','wrong_digest','wrong_aspect','no_alpha','dxt1','alpha_channels'])
def test_invalid_original_alpha_never_writes_pack(tmp_path, case):
    import hashlib
    source=tmp_path/'source.png';Image.new('RGBA',(8,4),(50,60,70,255)).save(source)
    original=tmp_path/'original.png'
    Image.new('RGB' if case=='no_alpha' else 'RGBA',(4,4) if case=='wrong_aspect' else (4,2)).save(original)
    digest=hashlib.sha256(original.read_bytes()).hexdigest()
    args=[sys.executable,str(ROOT/'tools/texture-pack.py'),str(source),'--key','2'*16,
          '--width','8','--height','4','--output',str(tmp_path/'pack'),'--alpha-source',str(original)]
    if case!='missing_hash':args+=['--alpha-source-sha256','bad' if case=='bad_hash' else '0'*64 if case=='wrong_digest' else digest]
    if case=='dxt1':args+=['--format','DXT1']
    if case=='alpha_channels':args+=['--channels','alpha']
    result=subprocess.run(args,capture_output=True,text=True)
    assert result.returncode!=0
    assert not (tmp_path/'pack').exists()
