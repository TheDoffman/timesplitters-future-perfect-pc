
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
import hashlib
import json
import pytest


spec=importlib.util.spec_from_file_location('embedded',_layout_root/'tools/textures/texture-embedded-scan.py')
scan=importlib.util.module_from_spec(spec)
spec.loader.exec_module(scan)
spec=importlib.util.spec_from_file_location('embedded_verify',_layout_root/'tools/textures/texture-embedded-verify.py')
verify=importlib.util.module_from_spec(spec);spec.loader.exec_module(verify)


def texture(w=8,h=4,kind=0):
    header=struct.pack('<7I',w,h,w,h,0,kind,0xffffffff)+bytes(100)
    size=((w+3)//4)*((h+3)//4)*8 if kind==0 else w*h*4
    return header+bytes(size)


def test_unaligned_nested_and_multiple_headers():
    data=b'abc'+texture()+b'xyz12'+texture(16,16)
    rows=list(scan.find_headers(data,0,len(data)))
    assert [p for p,r in rows]==[3,3+len(texture())+5]
    assert all(r['status']=='candidate' for p,r in rows)


def test_entry_bounds_prevent_borrowing_next_payload():
    data=texture()+texture()
    assert scan.inspect_header(data,0,128)['status']=='truncated_base'
    assert list(scan.find_headers(data,0,127))==[]
    assert scan.inspect_header(data,-1,len(data)) is None


def test_nonpower_and_mismatched_dimensions_rejected():
    data=bytearray(texture());struct.pack_into('<I',data,8,16)
    assert list(scan.find_headers(data,0,len(data)))==[]
    data=texture(7,4)
    assert list(scan.find_headers(data,0,len(data)))==[]


def test_unknown_format_remains_candidate_not_silently_ignored():
    data=texture(kind=99)
    rows=list(scan.find_headers(data,0,len(data)))
    assert len(rows)==1 and rows[0][1]['status']=='unknown_format'


def test_4096_dimension_and_partial_signature():
    data=texture(4096,1)
    assert list(scan.find_headers(data,0,len(data)))[0][1]['width']==4096
    assert list(scan.find_headers(data[:27],0,27))==[]


def test_table_flag_stride_terminator_and_bounds():
    data=bytearray(512);struct.pack_into('<I',data,0,16)
    struct.pack_into('<4I',data,16,128,0,0,0x10000000)
    struct.pack_into('<4I',data,32,256,0,0,0) # no embedded flag
    struct.pack_into('<I',data,48,0xffffffff)
    assert verify.table_references(data)=={128:16}
    struct.pack_into('<I',data,16,500)
    assert verify.table_references(data)=={}
    struct.pack_into('<I',data,16,128)
    struct.pack_into('<I',data,48,0)
    assert verify.table_references(data)=={} # missing sentinel
    struct.pack_into('<I',data,0,513)
    assert verify.table_references(data)=={}


def test_header_and_payload_fully_inside_nonzero_entry():
    data=b'xx'+texture()+b'yy'
    assert len(list(scan.find_headers(data,2,len(data)-2)))==1
    assert list(scan.find_headers(data,3,len(data)-2))==[]


def candidate_fixture():
    data=bytearray(128)+texture()
    struct.pack_into('<I',data,0,16)
    struct.pack_into('<4I',data,16,128,0,0,0x10000000)
    struct.pack_into('<I',data,32,0xffffffff)
    row=dict(relative_offset=128,base_bytes=16,width=8,height=4,archive_format=0,
             header_sha256=hashlib.sha256(data[128:256]).hexdigest(),
             base_sha256=hashlib.sha256(data[256:]).hexdigest(),
             key=scan.catalog_tool.texture_key(12,8,4,data[256:]))
    return data,row


def test_candidate_rechecks_bytes_metadata_and_reference():
    data,row=candidate_fixture()
    assert verify.verified_payload(data,row)[2]==16
    data[-1]=1
    with pytest.raises(ValueError,match='changed since scan'):verify.verified_payload(data,row)
    data,row=candidate_fixture();row['width']=16
    with pytest.raises(ValueError,match='metadata changed'):verify.verified_payload(data,row)
    data,row=candidate_fixture();struct.pack_into('<I',data,28,0)
    with pytest.raises(ValueError,match='table reference'):verify.verified_payload(data,row)


def test_candidate_rejects_wrong_identity_and_truncation():
    data,row=candidate_fixture();row['key']='0'*16
    with pytest.raises(ValueError,match='identity changed'):verify.verified_payload(data,row)
    data,row=candidate_fixture()
    with pytest.raises(ValueError,match='truncated'):verify.verified_payload(data[:-1],row)


def test_inventory_tracks_embedded_without_calling_parent_texture_free(tmp_path):
    spec=importlib.util.spec_from_file_location('inventory',_layout_root/'tools/textures/texture-inventory.py')
    inventory=importlib.util.module_from_spec(spec);spec.loader.exec_module(inventory)
    game=tmp_path/'game';(game/'pak').mkdir(parents=True);root=tmp_path/'art';root.mkdir()
    data,row=candidate_fixture();archive=bytearray(64)+data
    struct.pack_into('<4sII',archive,0,b'P5CK',12,16)
    struct.pack_into('<4I',archive,12,1,64,len(data),0)
    path=game/'pak/model.pak';path.write_bytes(archive)
    source=dict(archive='pak/model.pak',crc='00000001',offset=192,length=144,
                embedded=True,container_offset=64,container_length=len(data),relative_offset=128,table_reference=16)
    entry=dict(sha256=hashlib.sha256(data[128:]).hexdigest(),width=8,height=4,
               archive_format=0,xbox_format=12,key=row['key'],sources=[source])
    (root/'catalog.json').write_text(json.dumps(dict(scope='fixture',textures=[entry])))
    result=inventory.build(game,root)
    assert result['totals']['cataloged_embedded_texture_occurrences']==1
    assert result['totals']['unclassified_entries']==1
    assert result['unique_supported_keys']==1
    # A rescan after catalog integration must still discover nested sources,
    # rather than treating them as missing top-level headers.
    rescan=scan.scan(game,root)
    assert len(rescan['candidates'])==1
    assert rescan['totals'].get('cataloged_headers_found',0)==0
    struct.pack_into('<I',archive,80,129);path.write_bytes(archive)
    with pytest.raises(ValueError,match='table reference changed'):inventory.build(game,root)
