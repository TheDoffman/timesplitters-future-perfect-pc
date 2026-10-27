
# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[2] / 'tools'
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import importlib.util
from pathlib import Path
import struct
import pytest


def load(name):
    spec=importlib.util.spec_from_file_location(name,_layout_root/'tools/textures'/f'{name}.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


scan=load('texture-nonpak-scan')
xbe=load('texture-xbe-sections')


def test_nonpak_scan_retains_unknown_files_empty_files_and_bounds(tmp_path):
    header=struct.pack('<7I',8,4,8,4,0,0,0xffffffff)+bytes(100)
    (tmp_path/'mystery.bin').write_bytes(b'odd'+header+bytes(16))
    (tmp_path/'empty').write_bytes(b'')
    (tmp_path/'other.pak').write_bytes(header+bytes(16))
    result=scan.scan(tmp_path)
    assert result['totals']['files_scanned']==2
    assert result['totals']['verified_header_shape_hits']==1
    row=next(r for r in result['files'] if r['path']=='mystery.bin')
    assert row['verified_header_shape_hits'][0]['offset']==3
    assert result['complete'] is False and result['catalog_changes']==0


def test_magic_is_not_accepted_as_a_valid_texture():
    result=scan.scan_bytes(b'code DDS '+bytes(200)+b'XPR0')
    assert result['signatures']['dds']['samples'][0]['status']=='invalid_header'
    assert result['signatures']['xpr0']['samples'][0]['status']=='signature_only_not_verified'
    assert result['verified_header_shape_hits']==[]


def test_dds_base_cannot_borrow_past_file_end():
    header=bytearray(128);header[:4]=b'DDS '
    struct.pack_into('<4I',header,4,124,0,8,8)
    struct.pack_into('<I',header,76,32);header[84:88]=b'DXT1'
    assert scan.dds_candidate(header+bytes(31),0)['status']=='truncated_base'
    assert scan.dds_candidate(header+bytes(32),0)['status']=='bounded_base_candidate'
    assert scan.dds_candidate(b'DDS ',0)['status']=='truncated_header'


def test_signature_count_is_complete_when_samples_are_bounded():
    result=scan.scan_bytes(b'XPR0 '*50)['signatures']['xpr0']
    assert result['count']==50 and len(result['samples'])==32
    assert result['samples_truncated']


def fake_xbe():
    data=bytearray(0x500);data[:4]=b'XBEH'
    struct.pack_into('<I',data,0x104,0x10000)
    struct.pack_into('<II',data,0x11c,1,0x10200)
    struct.pack_into('<6I',data,0x200,0,0x12000,32,0x400,32,0x10300)
    data[0x300:0x305]=b'test\0'
    return data


def test_xbe_parser_rejects_out_of_bounds_table_payload_and_name():
    data=fake_xbe();assert xbe.sections(data)[0]['name']=='test'
    struct.pack_into('<I',data,0x120,0xfffffff0)
    with pytest.raises(ValueError,match='table out of bounds'):xbe.sections(data)
    data=fake_xbe();struct.pack_into('<I',data,0x210,0x200)
    with pytest.raises(ValueError,match='data or name'):xbe.sections(data)
    data=fake_xbe();data[0x300:0x400]=b'x'*256
    with pytest.raises(ValueError,match='Unterminated'):xbe.sections(data)


def test_section_payload_requires_exact_shape_and_reserved_header():
    data=bytearray(struct.pack('<7I',512,512,512,512,0,0,0xffffffff)+bytes(100+131072))
    section={'offset':0,'length':len(data)}
    assert len(xbe.section_payload(data,section))==131200
    with pytest.raises(ValueError,match='length'):xbe.section_payload(data,{'offset':1,'length':len(data)})
    data[30]=1
    with pytest.raises(ValueError,match='reserved'):xbe.section_payload(data,section)
    data[30]=0;struct.pack_into('<I',data,20,3)
    with pytest.raises(ValueError,match='header changed'):xbe.section_payload(data,section)


def test_section_proof_rejects_unpinned_executable(tmp_path):
    path=tmp_path/'wrong.xbe';path.write_bytes(fake_xbe())
    with pytest.raises(ValueError,match='pinned US'):xbe.verified_sections(path)


def test_pak_rescan_does_not_expect_xbe_sections_inside_archives(tmp_path):
    import json
    game=tmp_path/'game';(game/'pak').mkdir(parents=True)
    root=tmp_path/'art';root.mkdir()
    (root/'catalog.json').write_text(json.dumps({'textures':[{'sources':[
        {'archive':'default.xbe','offset':1234,'source_kind':'xbe_section'}]}]}))
    report=scan.embedded.scan(game,root)
    assert report['candidates']==[]
    assert report['totals'].get('cataloged_headers_found',0)==0
