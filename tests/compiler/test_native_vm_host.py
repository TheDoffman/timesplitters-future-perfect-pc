from pathlib import Path
import os,subprocess,sys
import pytest
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile

def stage(tmp_path):
    for name in ('native_reservations.c','native_reservations.h','native_vm_host.c','native_vm_host.h',
                 'native_vm_host_contracts.c','native_vm_posix_model.c','host_pages.c','host_pages.h'):
        p=HERE/name
        if not p.exists():p=ROOT/'src'/('native' if name in ('host_pages.c','native_vm_host.c') else 'runtime')/name
        (tmp_path/name).write_bytes(p.read_bytes())

def execute(exe):
    p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=40)
    return p

def test_real_native_vm_host(tmp_path):
    stage(tmp_path)
    exe=_compile(tmp_path,tmp_path/'native_vm_host_contracts.c',extra=[
        tmp_path/'native_vm_host.c',tmp_path/'native_reservations.c',tmp_path/'host_pages.c',
        *(['/W4'] if os.name=='nt' else ['-Wall','-Wextra'])])
    p=execute(exe);assert p.returncode==0,p.stdout+p.stderr
    assert 'recommit zeroing, query and release passed' in p.stdout

def build_model(tmp_path,mutation=None):
    stage(tmp_path)
    if mutation:
        path=tmp_path/'native_vm_host.c';text=path.read_text(encoding='utf-8');old,new=mutation
        assert text.count(old)==1;path.write_text(text.replace(old,new),encoding='utf-8')
    headers={
        'pthread.h':'typedef int pthread_once_t;\n#define PTHREAD_ONCE_INIT 0\nint pthread_once(pthread_once_t *,void (*)(void));\n',
        'sys/mman.h':'''#include <stddef.h>
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 2
#define MAP_ANON 0x1000
#define MAP_FIXED 0x10
#define MAP_FAILED ((void *)-1)
#define MADV_FREE 5
void *mmap(void *,size_t,int,int,int,long long);
int munmap(void *,size_t);
int mprotect(void *,size_t,int);
int madvise(void *,size_t,int);
'''}
    for name,text in headers.items():
        p=tmp_path/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text,encoding='utf-8')
    flags=['/W4',f'/I{tmp_path}'] if os.name=='nt' else ['-Wall','-Wextra',f'-I{tmp_path}']
    return _compile(tmp_path,tmp_path/'native_vm_posix_model.c',extra=[tmp_path/'native_reservations.c',*flags])

def test_posix_vm_host_model(tmp_path):
    p=execute(build_model(tmp_path));assert p.returncode==0,p.stdout+p.stderr
    assert 'controlled POSIX reservation host contracts passed' in p.stdout

@pytest.mark.parametrize('mutation',[
    ('PROT_NONE,MAP_PRIVATE|MAP_ANON','PROT_NONE,MAP_PRIVATE|MAP_ANON|MAP_FIXED'),
    ('memset(base,0,bytes);','memset(base,0,bytes/2);'),
    ('if(zero && !mprotect(base,bytes,PROT_NONE))return 0;',
     'if(zero)return 0;'),
    ('(void)context;return munmap(base,bytes)==0;',
     '(void)context;(void)base;(void)bytes;return 1;'),
])
def test_posix_vm_host_negative_controls(tmp_path,mutation):
    p=execute(build_model(tmp_path,mutation))
    assert p.returncode!=0,'mutated POSIX VM host adapter passed'
