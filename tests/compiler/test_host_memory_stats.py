"""Real Windows/Darwin snapshot and controlled failures/ABI record revisions."""
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


def stage(tmp_path):
    for name in ('host_memory_stats.c','host_memory_stats.h','host_memory_stats_model.c'):
        path=HERE/name
        if not path.exists():path=ROOT/'src'/('native' if name=='host_memory_stats.c' else 'runtime')/name
        (tmp_path/name).write_bytes(path.read_bytes())
    (tmp_path/'host_pages.h').write_bytes((ROOT/'src/runtime/host_pages.h').read_bytes())


def model(tmp_path,host,mutation=None):
    stage(tmp_path)
    if mutation:
        p=tmp_path/'host_memory_stats.c';s=p.read_text(encoding='utf-8')
        old,new=mutation
        assert s.count(old)==1
        p.write_text(s.replace(old,new),encoding='utf-8')
    headers={
        'windows.h': '''#include <stdint.h>
typedef int BOOL;
typedef struct { uint32_t dwLength,dwMemoryLoad;
uint64_t ullTotalPhys,ullAvailPhys,ullTotalPageFile,ullAvailPageFile;
uint64_t ullTotalVirtual,ullAvailVirtual,ullAvailExtendedVirtual; } MEMORYSTATUSEX;
BOOL GlobalMemoryStatusEx(MEMORYSTATUSEX *);
''',
        'mach/mach.h': '''#include <stdint.h>
typedef uint32_t mach_port_t,mach_msg_type_number_t;
typedef int kern_return_t,integer_t;
typedef integer_t *host_info64_t;
typedef struct { uint32_t free_count,active_count,inactive_count; uint64_t other[4]; } vm_statistics64_data_t;
#define HOST_VM_INFO64_COUNT 12
#define HOST_VM_INFO64 4
#define MACH_PORT_NULL 0
#define KERN_SUCCESS 0
mach_port_t mach_task_self(void);
mach_port_t mach_host_self(void);
kern_return_t mach_port_deallocate(mach_port_t,mach_port_t);
kern_return_t host_statistics64(mach_port_t,int,host_info64_t,mach_msg_type_number_t *);
''',
        'mach/mach_host.h':'', 'mach/host_info.h':'', 'mach/vm_statistics.h':'',
    }
    for name,source in headers.items():
        p=tmp_path/name;p.parent.mkdir(parents=True,exist_ok=True)
        p.write_text(source,encoding='utf-8')
    define=f'STATS_MODEL_WINDOWS={int(host=="windows")}'
    flags=['/W4',f'/I{tmp_path}',f'/D{define}'] if os.name=='nt' else ['-Wall','-Wextra',f'-I{tmp_path}',f'-D{define}']
    return _compile(tmp_path,tmp_path/'host_memory_stats_model.c',extra=flags)


@pytest.mark.parametrize('host',['windows','darwin'])
def test_host_stats_models(tmp_path,host):
    exe=model(tmp_path,host)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'contracts passed' in run.stdout


@pytest.mark.parametrize('host,mutation',[
    ('windows',('MEMORYSTATUSEX info={0};','MEMORYSTATUSEX info; memset(&info,0xa5,sizeof(info));')),
    ('darwin',('(void)mach_port_deallocate(mach_task_self(),host);',
        'if(host==UINT32_MAX)(void)mach_port_deallocate(mach_task_self(),host);')),
    ('darwin',('count<needed','count<HOST_VM_INFO64_COUNT || needed>HOST_VM_INFO64_COUNT')),
    ('darwin',('result=(uint64_t)info.free_count+(uint64_t)info.inactive_count;',
        'result=(uint32_t)(info.free_count+info.inactive_count);')),
])
def test_host_stats_negative_controls(tmp_path,host,mutation):
    exe=model(tmp_path,host,mutation)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert run.returncode!=0,'Mutated host statistics adapter incorrectly passed'


def test_real_host_available_memory(tmp_path):
    stage(tmp_path)
    (tmp_path/'host_pages.c').write_bytes((ROOT/'src/native/host_pages.c').read_bytes())
    harness=r'''
#include "host_memory_stats.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    uint64_t bytes=UINT64_MAX;
    assert(!tsfp_host_available_memory(NULL));
    assert(tsfp_host_available_memory(&bytes) && bytes!=UINT64_MAX);
    printf("real host available-memory snapshot: %llu bytes\n",(unsigned long long)bytes);
    return 0;
}
'''
    p=tmp_path/'host_stats.c';p.write_text(harness,encoding='utf-8')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,p,extra=[tmp_path/'host_memory_stats.c',tmp_path/'host_pages.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'real host available-memory snapshot:' in run.stdout
