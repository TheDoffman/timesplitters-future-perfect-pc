"""Exercise actual lifting and runtime acknowledgement, without game assets."""
import os
from pathlib import Path
import re
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[1]

def lift(raw,site=0x3D66C3):
    sys.path.insert(0,str(ROOT/'external/ts3FPrecomp'))
    from tools.recomp.disasm import Disassembler
    from tools.recomp.lifter import Lifter
    data=bytes.fromhex(raw)
    instruction=Disassembler().disassemble_function(data,site,site+len(data))[0]
    return '\n'.join(Lifter().lift_instruction(instruction))

@pytest.mark.parametrize('raw,observed',[
    ('898810041000',True), # supported XBE's mov [eax+0x100410],ecx
    ('a318e37d00',True),   # absolute 32-bit store
    ('894c9004',True),     # indexed 32-bit store
    ('c70001000000',True), # immediate 32-bit store
    ('8b8810041000',False),# load
    ('888810041000',False),# byte store
    ('66898810041000',False),# word store
    ('64898810041000',False),# segmented store
    ('89c8',False),        # register move
])
def test_after_store_scope(raw,observed):
    code=lift(raw)
    assert ('RECOMP_AFTER_STORE32' in code)==observed
    if observed:
        assert code.index('=') < code.index('RECOMP_AFTER_STORE32')

@pytest.mark.parametrize('mutation,setting',[
    (None,None),(None,'0'),(None,'1'),
    ('omit',None),('before-store',None),('other-bits',None)])
def test_actual_pfb_ack(tmp_path,mutation,setting):
    runtime=(ROOT/'external/ts3FPrecomp/src/kernel/xbox_memory_layout.c').read_text()
    body=re.search(r'void xbox_Nv2aAcknowledgePfbFlush\(uint32_t guest_address\)\n\{.*?\n\}',runtime,re.S)
    assert body,'Apply complete patch stack first'
    store=lift('898810041000')
    assert 'MEM32(eax + 0x100410) = ecx;' in store
    if mutation=='omit':store=store.split('\n')[0]
    if mutation=='before-store':store='\n'.join(reversed(store.splitlines()))
    body=body.group()
    if mutation=='other-bits':
        assert 'before & ~0x00010000L' in body
        body=body.replace('before & ~0x00010000L','0')
    fixture=(ROOT/'tests/gpu_flush_ack.c').read_text()
    fixture=fixture.replace('/* ACTUAL_ACK_FUNCTION */',body).replace('/* ACTUAL_GUEST_STORE */',store)
    fixture=fixture.replace('/* NEIGHBOR_GUEST_STORE */',lift('898810041000',0x3D66C4))
    source=tmp_path/'ack.c';source.write_text(fixture)
    compiler,env=native_compiler(tmp_path)
    output=tmp_path/('ack.exe' if os.name=='nt' else 'ack')
    args=([compiler,'/nologo','/std:c11','/W4','/I'+str(ROOT/'src'),str(source),'/Fe:'+str(output)]
          if os.name=='nt' else [compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),str(source),'-o',str(output)])
    subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,check=True,timeout=60)
    environment={k:v for k,v in os.environ.items() if k!='RECOMP_PFB_SYNC_ACK'}
    if setting is not None:environment['RECOMP_PFB_SYNC_ACK']=setting
    run=subprocess.run([str(output)],env=environment,capture_output=True,text=True,timeout=10)
    if mutation:
        assert run.returncode!=0 and 'FAIL:' in run.stderr,run.stdout+run.stderr
    else:assert run.returncode==0,run.stdout+run.stderr
