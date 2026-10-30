from pathlib import Path
import os
import sys
import subprocess
import pytest
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile
from test_io_packet_abi import function

def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/"src/runtime"/name

@pytest.mark.skipif(os.name!='nt',reason='Real Windows bridge locks; actual kernel-header acceptance separate')
@pytest.mark.parametrize('mutation',[None,
    ('stacks=(uint8_t)STACK_ARG(0)','stacks=(uint8_t)STACK_ARG(1)'),
    ('bytes<=owner->span','bytes<=UINT32_MAX'),
    ('owner->phase==TSFP_IO_READY &&','owner->phase!=TSFP_IO_DEAD &&'),
    ('tsfp_io_registry_remove(&bridge_requests.registry,address)','bridge_request_deallocate(NULL,address)'),
])
def test_original_kernel_request_adapters(tmp_path,mutation):
    providers=('guest_io_pool','guest_io_registry','guest_io_owner','guest_io_request')
    for name in [f'{p}.{ext}' for p in providers for ext in ('c','h')]+[
        'guest_io_stack.h','guest_kernel_requests.inc','kernel_request_contract.c']:
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('guest_allocation.c','guest_allocation.h'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    bridge=(ROOT/'external/ts3FPrecomp/src/kernel/kernel_bridge.c').read_text(encoding='utf-8')
    cleanup=function(bridge,'static int stdcall_args_for_ordinal(ULONG ordinal)')
    fixture=tmp_path/'kernel_request_contract.c'
    fixture.write_text(fixture.read_text(encoding='utf-8').replace('/*ACTUAL_CLEANUP*/',cleanup),encoding='utf-8')
    if mutation:
        old,new=mutation;p=tmp_path/'guest_kernel_requests.inc';s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    exe=_compile(tmp_path,tmp_path/'kernel_request_contract.c',extra=[
        *[tmp_path/f'{p}.c' for p in providers],tmp_path/'guest_allocation.c','/W4'])
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:assert result.returncode!=0,'Broken kernel request adapter passed'
    else:
        assert result.returncode==0,result.stdout+result.stderr
        assert 'PASS:' in result.stdout

def test_request_adapter_compiles_with_actual_kernel_headers(tmp_path):
    from native_compiler import native_compiler
    compiler,env=native_compiler(tmp_path)
    for name in ('guest_io_pool.h','guest_io_registry.h','guest_io_owner.h',
                 'guest_io_request.h','guest_io_stack.h','guest_kernel_requests.inc','kernel_request_header_probe.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    source=tmp_path/'kernel_request_header_probe.c';obj=tmp_path/('probe.obj' if os.name=='nt' else 'probe.o')
    runtime=ROOT/'external/ts3FPrecomp'
    if os.name=='nt':
        args=[compiler,'/nologo','/std:c11','/W4','/WX','/D_CRT_SECURE_NO_WARNINGS','/c',str(source),
              '/I'+str(ROOT/'src/runtime'),'/I'+str(runtime/'src'),'/I'+str(runtime/'src/kernel'),'/Fo:'+str(obj)]
    else:
        args=[compiler,'-std=c11','-Wall','-Wextra','-Werror','-c',str(source),
              '-I',str(ROOT/'src/runtime'),'-I',str(runtime/'src'),'-isystem',str(runtime/'src/kernel'),'-o',str(obj)]
    run=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
    assert obj.is_file()
