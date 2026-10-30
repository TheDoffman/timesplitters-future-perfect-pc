"""Exercise real host protection, mapping and ownership, without game assets."""
from pathlib import Path
import os
import subprocess
import sys
import pytest

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile


def source_file(name):
    direct=HERE/name
    if direct.exists(): return direct
    return ROOT/'src'/('native' if name=='host_pages.c' else 'runtime')/name


def test_real_host_pages(tmp_path):
    for name in ('host_pages.c','host_pages.h','host_blocks.c','host_blocks.h','host_pages_contracts.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'host_pages_contracts.c',
        extra=[tmp_path/'host_pages.c',tmp_path/'host_blocks.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'contracts passed' in run.stdout


def posix_model(tmp_path,mutation=None):
    for name in ('host_pages.c','host_pages.h','host_pages_posix_contracts.c'):
        source=source_file(name).read_text(encoding='utf-8')
        if name=='host_pages.c' and mutation:
            old,new=mutation
            assert source.count(old)==1
            source=source.replace(old,new)
        (tmp_path/name).write_text(source,encoding='utf-8')
    headers={
        'unistd.h': '#define _SC_PAGESIZE 1\nlong sysconf(int);\n',
        'sys/mman.h': '''#include <stddef.h>
#include <stdint.h>
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 2
#define MAP_ANON 4096
#define MAP_FAILED ((void *)(intptr_t)-1)
void *mmap(void *,size_t,int,int,int,int64_t);
int munmap(void *,size_t);
int mprotect(void *,size_t,int);
''',
        'mach/mach.h': '''#include <stdint.h>
typedef unsigned mach_port_t,mach_msg_type_number_t;
typedef int kern_return_t;
typedef uint64_t mach_vm_address_t,mach_vm_size_t;
typedef int *vm_region_info_t;
typedef struct { int protection; } vm_region_basic_info_data_64_t;
#define VM_PROT_READ 1
#define VM_PROT_WRITE 2
#define VM_PROT_EXECUTE 4
#define VM_REGION_BASIC_INFO_COUNT_64 9
#define VM_REGION_BASIC_INFO_64 10
#define MACH_PORT_NULL 0
#define KERN_SUCCESS 0
mach_port_t mach_task_self(void);
kern_return_t mach_port_deallocate(mach_port_t,mach_port_t);
''',
        'mach/mach_vm.h': '''kern_return_t mach_vm_region(mach_port_t,mach_vm_address_t *,
mach_vm_size_t *,int,vm_region_info_t,mach_msg_type_number_t *,mach_port_t *);
''',
    }
    for name,source in headers.items():
        path=tmp_path/name
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(source,encoding='utf-8')
    flags=['/W4',f'/I{tmp_path}'] if os.name=='nt' else ['-Wall','-Wextra',f'-I{tmp_path}']
    return _compile(tmp_path,tmp_path/'host_pages_posix_contracts.c',extra=flags)


def test_posix_host_page_adapter(tmp_path):
    exe=posix_model(tmp_path)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'contracts passed' in run.stdout


@pytest.mark.parametrize('mutation',[
    ('offset=start%page;','offset=0;'),
    ('pointer<result.base || pointer-result.base>=result.bytes ||',''),
    ('if(object!=MACH_PORT_NULL)mach_port_deallocate(mach_task_self(),object);',
     'if(object==UINT32_MAX)mach_port_deallocate(mach_task_self(),object);'),
    ('return p==MAP_FAILED?NULL:p;','return p;'),
])
def test_compiled_host_page_negative_controls(tmp_path,mutation):
    exe=posix_model(tmp_path,mutation)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60)
    assert run.returncode!=0,'Mutated host adapter incorrectly passed'
