"""Shared SPIR-V interface adapter with no GPU or game assets."""
import os
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile

def test_vulkan_interface(tmp_path):
    source=tmp_path/'interface.c'
    source.write_bytes((ROOT/'tests/graphics/vulkan_interface.c').read_bytes())
    (tmp_path/'vulkan_interface.h').write_bytes((ROOT/'src/graphics/vulkan_interface.h').read_bytes())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-Werror']
    exe=_compile(tmp_path,source,extra=flags)
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
    assert result.returncode==0,result.stdout+result.stderr
