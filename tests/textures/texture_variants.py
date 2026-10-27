
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
import numpy as np
from PIL import Image

spec=importlib.util.spec_from_file_location('variants',_layout_root/'tools/textures/texture-variants.py')
variants=importlib.util.module_from_spec(spec);spec.loader.exec_module(variants)


def test_same_pixels_and_small_compression_error_are_candidates():
    a=variants.fingerprint(Image.new('RGBA',(32,32),(80,90,100,255)),128)
    assert variants.plausible(variants.compare(a,a))
    b=a.copy();b[:,:,:3]+=2
    assert variants.plausible(variants.compare(a,b))


def test_palette_and_alpha_changes_rejected():
    a=np.full((128,128,4),80,dtype=np.int16);b=a.copy();b[:,:,0]+=25
    assert not variants.plausible(variants.compare(a,b))
    b=a.copy();b[5,5,3]=255
    assert not variants.plausible(variants.compare(a,b))


def test_shifted_island_rejected_even_on_mostly_flat_background():
    a=np.zeros((128,128,4),dtype=np.int16);a[:,:,3]=255;a[24:64,24:64,:3]=150
    b=np.roll(a,8,axis=1)
    assert not variants.plausible(variants.compare(a,b))
