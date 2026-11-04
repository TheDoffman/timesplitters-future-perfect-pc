"""Verify lossless span packing and manifest validation without licensed artwork."""
import importlib.util
from pathlib import Path
import json
import hashlib
from PIL import Image
import pytest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('input_icon_import',ROOT/'tools/graphics/import-input-icons.py')
icons=importlib.util.module_from_spec(spec);spec.loader.exec_module(icons)

def test_rgba_runs_preserve_transparency_and_antialiasing():
    source=Image.new('RGBA',(8,8))
    for y in range(2,6):
        for x in range(1,7):source.putpixel((x,y),(240,50,80,127 if x==1 else 255))
    spans=icons.rectangles(source);result=Image.new('RGBA',source.size)
    for x,y,w,h,*color in spans:
        for cy in range(y,y+h):
            for cx in range(x,x+w):result.putpixel((cx,cy),tuple(color))
    assert result.tobytes()==source.tobytes()
    assert len(spans)==2

def test_manifest_maps_keys_and_reuses_idle_for_missing_pressed_state(tmp_path):
    Image.new('RGBA',(128,128),(255,255,255,255)).save(tmp_path/'e.png')
    manifest=tmp_path/'manifest.json';manifest.write_text(json.dumps({'keys':[{'key':69,'idle':'e.png'}]}))
    output=tmp_path/'data.inc'
    assert icons.compile_pack(manifest,output)==1
    data=output.read_text()
    assert '#define TSFP_INPUT_ICONS 1' in data
    table=data.split('input_icon_lookup[256]={\n')[1].split('\n')[0].split(',')
    assert table[69]=='0' and table[70]=='-1'
    manifest.write_text(json.dumps({'keys':[{'key':69,'idle':'e.png'},{'key':69,'idle':'e.png'}]}))
    with pytest.raises(ValueError,match='duplicated'):icons.compile_pack(manifest,output)

def test_rejects_paths_outside_asset_folder(tmp_path):
    manifest=tmp_path/'manifest.json';manifest.write_text(json.dumps({'keys':[{'key':69,'idle':'../e.png'}]}))
    with pytest.raises(ValueError,match='escapes'):icons.compile_pack(manifest,tmp_path/'data.inc')

def test_source_directory_and_integrity(tmp_path):
    source=tmp_path/'source';source.mkdir()
    png=source/'e.png';Image.new('RGBA',(128,128),(255,255,255,255)).save(png)
    manifest=tmp_path/'manifest.json'
    manifest.write_text(json.dumps({'keys':[{'key':69,'idle':'e.png','sha256':hashlib.sha256(png.read_bytes()).hexdigest()}]}))
    assert icons.compile_pack(manifest,tmp_path/'data.inc',source)==1
    Image.new('RGBA',(128,128),(0,0,0,255)).save(png)
    with pytest.raises(ValueError,match='SHA-256'):icons.compile_pack(manifest,tmp_path/'data.inc',source)
