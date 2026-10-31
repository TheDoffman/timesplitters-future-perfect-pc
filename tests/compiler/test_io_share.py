from pathlib import Path
import os,sys,subprocess
import pytest
HERE=Path(__file__).resolve().parent
ROOT=next(parent for parent in HERE.parents if (parent/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile
@pytest.mark.parametrize('mutation',[None,
 ('s[4+i]!=s[0]','s[4+i]==s[0]'),
 ('!allow[i] && s[1+i]','allow[i] && s[1+i]'),
 ('if(update) {++s[0];','if(1) {++s[0];'),
 ('s[0]==255','s[0]==254'),
 ('f[2]&0x81','f[2]&0'),
])
def test_original_byte_sharing(tmp_path,mutation):
    for name in ('guest_io_share.c','guest_io_share.h','io_share_contract.c'):(tmp_path/name).write_bytes(source_file(name).read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_io_share.c';s=p.read_text(encoding='utf-8');assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_share_contract.c',extra=[tmp_path/'guest_io_share.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert run.returncode!=0,'Broken sharing policy passed'
    else:assert run.returncode==0,run.stdout+run.stderr

@pytest.mark.parametrize('mutation',[None,
 ('group=STACK_ARG(3);','group=STACK_ARG(4);'),
 ('(uint8_t)STACK_ARG(4)!=0','STACK_ARG(4)!=0'),
 ('group,7','group,3'),
 ('case  80: return 16;','case  80: return 20;'),
])
def test_original_sharing_bridge(tmp_path,mutation):
    for name in ('guest_io_share.c','guest_io_share.h','guest_kernel_share.inc','kernel_share_contract.c'):(tmp_path/name).write_bytes(source_file(name).read_bytes())
    if mutation and not mutation[0].startswith('case '):
        old,new=mutation;p=tmp_path/'guest_kernel_share.inc';s=p.read_text(encoding='utf-8');assert old in s;p.write_bytes(s.replace(old,new).encode())
    from test_io_packet_abi import function
    bridge=(ROOT/'external/ts3FPrecomp/src/kernel/kernel_bridge.c').read_text(encoding='utf-8')
    cleanup=function(bridge,'static int stdcall_args_for_ordinal(ULONG ordinal)')
    if mutation and mutation[0].startswith('case '):
        old,new=mutation;assert cleanup.count(old)==1;cleanup=cleanup.replace(old,new)
    f=tmp_path/'kernel_share_contract.c';f.write_bytes(f.read_bytes().replace(b'/*ACTUAL_CLEANUP*/',cleanup.encode()))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'kernel_share_contract.c',extra=[tmp_path/'guest_io_share.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert run.returncode!=0,'Broken share bridge passed'
    else:assert run.returncode==0,run.stdout+run.stderr


def test_share_adapter_actual_headers(tmp_path):
    from native_compiler import native_compiler
    compiler,env=native_compiler(tmp_path)
    for name in ('guest_io_share.h','guest_kernel_share.inc','kernel_share_header_probe.c'):(tmp_path/name).write_bytes(source_file(name).read_bytes())
    source=tmp_path/'kernel_share_header_probe.c';obj=tmp_path/('probe.obj' if os.name=='nt' else 'probe.o');runtime=ROOT/'external/ts3FPrecomp'
    if os.name=='nt':
        args=[compiler,'/nologo','/std:c11','/W4','/WX','/D_CRT_SECURE_NO_WARNINGS','/c',str(source),'/I'+str(ROOT/'src/runtime'),'/I'+str(runtime/'src'),'/I'+str(runtime/'src/kernel'),'/Fo:'+str(obj)]
    else:
        args=[compiler,'-std=c11','-Wall','-Wextra','-Werror','-c',str(source),'-I'+str(ROOT/'src/runtime'),'-isystem',str(runtime/'src'),'-isystem',str(runtime/'src/kernel'),'-o',str(obj)]
    run=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=90)
    assert run.returncode==0,run.stdout+run.stderr


def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/'src/runtime'/name
