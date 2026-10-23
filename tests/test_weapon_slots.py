"""Shared slot input/guest selection policy, vocabulary and host integration."""
import json
import re
from pathlib import Path
import subprocess
import os
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[1]

def test_weapon_slots(tmp_path):
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('weapon-slots.exe' if os.name=='nt' else 'weapon-slots')
    source=str(ROOT/'tests/weapon_slots.c')
    args=([compiler,'/nologo','/std:c11','/W4','/WX','/I'+str(ROOT/'src'),source,'/Fe:'+str(exe)]
          if os.name=='nt' else [compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),source,'-o',str(exe)])
    p=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert p.returncode==0,p.stdout+p.stderr
    p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert p.returncode==0,p.stdout+p.stderr

def test_slot_defaults_and_runtime_hook():
    slots=re.findall(r'PC_WEAPON_SLOT\((\d+), (\d+)\)',(ROOT/'src/pc_weapon_slots.def').read_text())
    bindings=json.loads((ROOT/'config/pc-controls.default.json').read_text())['controllers'][0]['bindings']
    assert len(slots)==10
    assert [int(n) for n,_ in slots]==list(range(1,11))
    assert len([k for k in bindings if k.startswith('weapon_slot_')])==10
    for n,key in slots:
        assert bindings['weapon_slot_'+n]==('key:'+chr(int(key)) if int(key) else 'none')
    assert bindings['black']=='key:X' and bindings['white']=='key:Z'
    patch=next((ROOT/'patches').glob('*-keyboard-weapon-slots.patch')).read_text()
    assert 'sub_001BF140_gen();' in patch and 'pc_display_weapon_slots();' in patch
    assert 'tsfp_native_key_down' in patch and 'pc_controls_key_down' in patch
    assert 'K_WEAPON_SLOT' in patch
