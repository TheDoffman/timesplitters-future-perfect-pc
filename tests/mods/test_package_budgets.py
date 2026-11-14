"""Reject allocation/CPU amplification before invoking general ZIP/JSON decoders."""
import json
from pathlib import Path
import struct
import sys
from types import SimpleNamespace
import zipfile

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
import mod_package as package
from test_creator_service import package as creator_package


@pytest.mark.parametrize('field,value', [('count',65535),('count',1),('size',20000000),('disk',1)])
def test_forged_zip_directory_rejected_before_zipfile(tmp_path, monkeypatch, field, value):
    raw = bytearray(creator_package(tmp_path))
    end = raw.rfind(b'PK\x05\x06')
    if field == 'count':
        struct.pack_into('<HH',raw,end+8,value,value)
    elif field == 'size':
        struct.pack_into('<I',raw,end+12,value)
    else:
        struct.pack_into('<H',raw,end+4,value)
    path = tmp_path / 'forged.zip'
    path.write_bytes(raw)
    monkeypatch.setattr(zipfile,'ZipFile',lambda *_: pytest.fail('Must reject before ZIP allocation'))
    with pytest.raises(package.ModError):
        package.inspect_package(path)


def test_central_directory_count_is_verified_not_trusted(tmp_path, monkeypatch):
    raw = bytearray(creator_package(tmp_path))
    end = raw.rfind(b'PK\x05\x06')
    # Advertise3 entries but retain only2; the library must not be opened first.
    struct.pack_into('<HH',raw,end+8,3,3)
    path = tmp_path / 'forged-count.zip'
    path.write_bytes(raw)
    monkeypatch.setattr(zipfile,'ZipFile',lambda *_: pytest.fail('Must reject before ZIP allocation'))
    with pytest.raises(package.ModError):
        package.inspect_package(path)


@pytest.mark.parametrize('text', [
    '{"nested":' + '['*32 + '0' + ']'*32 + '}',
    '{"items":[' + ','.join('0' for _ in range(4097)) + ']}',
    json.dumps({'items': [[0]*4096 for _ in range(16)]}),
], ids=['depth','members','nodes'])
def test_json_budgets_apply_before_object_decoder(monkeypatch, text):
    monkeypatch.setattr(package,'json',SimpleNamespace(
        loads=lambda *a,**k: pytest.fail('Must reject before JSON allocation'), JSONDecodeError=json.JSONDecodeError))
    with pytest.raises(package.ModError):
        package.read_json(text)


@pytest.mark.parametrize('value', ['1e400','9'*129,'79228162514264337593543950336'])
def test_json_numbers_must_fit_native_decoder(value):
    with pytest.raises(package.ModError):
        package.read_json('{"number":' + value + '}')


def test_text_literals_do_not_consume_structural_or_numeric_budget():
    value = {'source':'['*5000 + ' 1e400 ' + ']'*5000, 'escaped':'"\\,{}'}
    assert package.read_json(json.dumps(value)) == value


@pytest.mark.parametrize('name,data', [('scripts/main.lua',b'a'*(1024*1024+1)),
                                     ('maps/map.json',b' '*(4*1024*1024+1))], ids=['script','map'])
def test_source_and_map_size_rejected_before_decoding(name,data,monkeypatch):
    monkeypatch.setattr(package,'read_json',lambda *_: pytest.fail('Must reject before decoder'))
    with pytest.raises(package.ModError):
        package.validate_payload(name,data)


@pytest.mark.parametrize('text', ['bad\x7fname','bad\x85name','\ud800','\U0001f600'*61],
                         ids=['del','c1','surrogate','utf16-length'])
def test_manifest_text_matches_native_control_and_utf16_limits(tmp_path,text):
    path = tmp_path / 'manifest.zip'
    path.write_bytes(creator_package(tmp_path))
    with zipfile.ZipFile(path) as archive:
        manifest = json.loads(archive.read('mod.json'))
    manifest['name'] = text
    with pytest.raises(package.ModError):
        package.validate_manifest(manifest)
