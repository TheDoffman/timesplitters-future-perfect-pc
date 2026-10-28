import os
from pathlib import Path
import shutil,subprocess,sys
import pytest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile
from native_compiler import native_compiler
from branch_feedback import parse_dump,parse_sites_dump,sites_dump_beside

def build(tmp_path,variant='production',enabled=True):
    header=(ROOT/'src/runtime/tsfp_branch_feedback.h').read_text()
    source=(ROOT/'src/runtime/guest_branch_feedback.c').read_text()
    if variant=='lost_flags':
        source=source.replace('_InterlockedOr8(', '_InterlockedExchange8(').replace('__atomic_fetch_or(', '__atomic_exchange_n(')
    elif variant=='lost_saturation':
        assert 'else record->saturated=1;' in source
        source=source.replace('else record->saturated=1;','else record->saturated=0;')
    elif variant=='lost_targets':
        source=source.replace('record->target[record->count++]=target','record->target[(record->count++,0)]=target')
    elif variant=='lost_counter':
        source=source.replace('increment64(matched ? &g_icall_guard_hits : &g_icall_guard_misses);','(void)matched;')
    elif variant=='lost_endpoint':
        header=header.replace('address < RECOMP_ICALL_FB_BASE + RECOMP_ICALL_FB_SIZE;','address < RECOMP_ICALL_FB_BASE + RECOMP_ICALL_FB_SIZE - 1u;')
    elif variant=='double_address':
        header=header.replace('(uint32_t)(address);','(uint32_t)((address),(address));')
    (tmp_path/'tsfp_branch_feedback.h').write_text(header)
    implementation=tmp_path/'guest_branch_feedback.c';implementation.write_text(source)
    harness=tmp_path/'recorder.c';harness.write_bytes((ROOT/'tests/compiler/branch_feedback_runtime.c').read_bytes())
    options=(['/DRECOMP_ICALL_FEEDBACK'] if enabled else []) if os.name=='nt' else (['-DRECOMP_ICALL_FEEDBACK','-pthread'] if enabled else ['-pthread'])
    return _compile(tmp_path,harness,extra=[*options,implementation])

def run(executable,tmp_path,mode='basic',filename='icall_targets.dump'):
    path=tmp_path/filename
    env=dict(os.environ,RECOMP_ICALL_FEEDBACK_PATH=str(path))
    result=subprocess.run([str(executable),mode,str(path)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    return result,path

def expected_basic():
    sites={0xabcdef:({0xfedcba},False),0x4444:(set(range(1,7)),True),0xfffffff0:({0,0xffffffff},False)}
    sites.update({0x1001*i:({0x9000+i},False) for i in range(1,129)})
    return {0x10000:3,0x10001:1,0x80ffff:2},sites

@pytest.mark.parametrize('variant',['production','lost_flags','lost_saturation','lost_targets','lost_counter','lost_endpoint','double_address'])
def test_observation_contract_and_compiled_negative_controls(tmp_path,variant):
    result,path=run(build(tmp_path,variant),tmp_path)
    if variant in ('lost_counter','double_address'):
        assert result.returncode=={'lost_counter':16,'double_address':14}[variant],result.stdout+result.stderr
        return
    assert result.returncode==0,result.stdout+result.stderr
    actual=(parse_dump(path),parse_sites_dump(sites_dump_beside(path)))
    if variant=='production':assert actual==expected_basic()
    else:assert actual!=expected_basic(),variant

@pytest.mark.parametrize('mode',['threads','capacity','exit'])
def test_shared_recording_capacity_and_idempotent_exit(tmp_path,mode):
    result,path=run(build(tmp_path),tmp_path,mode,'space in name.targets.dump')
    assert result.returncode==0,result.stdout+result.stderr
    targets=parse_dump(path);sites=parse_sites_dump(sites_dump_beside(path))
    if mode=='threads':
        assert targets=={0x10000+i:3 for i in range(4096)}
        assert set(sites)==set(range(0x800000,0x800080))
        assert all(len(values)==6 and values<=set(range(1,9)) and full for values,full in sites.values())
    elif mode=='capacity':
        assert targets=={}
        assert sites=={i:({i^0x80000000},False) for i in range(1,32769)}
        assert '# dropped site observations: 2' in Path(sites_dump_beside(path)).read_text()
    else:
        assert targets=={0x10007:3} and sites=={}
        assert result.stderr.count('resolved targets')==1

def test_alternate_path_failed_open_and_disabled_hooks(tmp_path):
    exe=build(tmp_path)
    result,path=run(exe,tmp_path,filename='custom.log');assert result.returncode==0
    assert (parse_dump(path),parse_sites_dump(sites_dump_beside(path)))==expected_basic()
    result,path=run(exe,tmp_path,filename='missing/targets.dump')
    assert result.returncode==0 and 'cannot open target dump' in result.stderr and not path.exists()
    other=tmp_path/'disabled';other.mkdir()
    result,path=run(build(other,enabled=False),other)
    assert result.returncode==0 and not path.exists() and not Path(sites_dump_beside(path)).exists()


@pytest.mark.parametrize('stream', ['target', 'site'])
@pytest.mark.parametrize('variant', ['production', 'lost_error'])
def test_prior_write_error_is_reported_even_when_close_succeeds(tmp_path, stream, variant):
    (tmp_path/'tsfp_branch_feedback.h').write_bytes((ROOT/'src/runtime/tsfp_branch_feedback.h').read_bytes())
    source=(ROOT/'src/runtime/guest_branch_feedback.c').read_text()
    if variant=='lost_error':
        assert source.count('int failed=ferror(output);')==2
        source=source.replace('int failed=ferror(output);','int failed=0;')
    (tmp_path/'guest_branch_feedback.c').write_text(source)
    harness=tmp_path/'write-error.c'
    harness.write_text(r'''
#include <stdio.h>
#include <string.h>
static FILE *faulty;
static unsigned closes;
static const char *selected;
static FILE *open_stream(const char *path, const char *mode) {
    FILE *result=fopen(path,mode);
    if (strstr(path,selected)) faulty=result;
    return result;
}
int stream_error(FILE *stream) { return stream==faulty ? 1 : ferror(stream); }
static int close_stream(FILE *stream) {
    ++closes;
    if (stream==faulty) faulty=NULL;
    return fclose(stream);
}
#define fopen open_stream
#define ferror stream_error
#define fclose close_stream
#include "guest_branch_feedback.c"
int main(int argc, char **argv) {
    if (argc!=2) return 1;
    selected=argv[1];
    RECOMP_ICALL_OBSERVE(0x10000,RECOMP_ICALL_SEEN_RESOLVED);
    RECOMP_ICALL_OBSERVE_SITE(0x10001,0x10002);
    recomp_icall_feedback_dump("targets.dump");
    return closes!=2;
}
''')
    flags=['/DRECOMP_ICALL_FEEDBACK'] if os.name=='nt' else ['-DRECOMP_ICALL_FEEDBACK']
    executable=_compile(tmp_path,harness,extra=flags)
    result=subprocess.run([str(executable),stream],cwd=tmp_path,capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    assert (f'incomplete {stream} dump' in result.stderr)==(variant=='production')
    other='site' if stream=='target' else 'target'
    assert f'incomplete {other} dump' not in result.stderr


@pytest.mark.parametrize('mode', ['busy', 'snapshot-allocation', 'path-allocation'])
def test_site_snapshot_failure_preserves_prior_dump_and_recovers(tmp_path, mode):
    (tmp_path/'tsfp_branch_feedback.h').write_bytes((ROOT/'src/runtime/tsfp_branch_feedback.h').read_bytes())
    (tmp_path/'guest_branch_feedback.c').write_bytes((ROOT/'src/runtime/guest_branch_feedback.c').read_bytes())
    harness=tmp_path/'snapshot-failure.c'
    harness.write_text(r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned allocation, fail_allocation;
static void *allocate(size_t size) {
    ++allocation;
    return allocation==fail_allocation ? NULL : malloc(size);
}
#define malloc allocate
#include "guest_branch_feedback.c"
int main(int argc, char **argv) {
    if (argc!=2) return 1;
    FILE *prior=fopen("sites.dump","w");
    if (!prior) return 2;
    fputs("previous-complete-dump",prior);fclose(prior);
    RECOMP_ICALL_OBSERVE(0x10000,RECOMP_ICALL_SEEN_RESOLVED);
    RECOMP_ICALL_OBSERVE_SITE(0x10001,0x10002);
    if (!strcmp(argv[1],"busy")) site_lock=1;
    else fail_allocation=!strcmp(argv[1],"snapshot-allocation") ? 1u : 2u;
    recomp_icall_feedback_dump("targets.dump");
    char buffer[64]={0};
    prior=fopen("sites.dump","r");if (!prior) return 3;
    (void)fread(buffer,1,sizeof(buffer)-1,prior);fclose(prior);
    if (strcmp(buffer,"previous-complete-dump")) return 4;
    site_lock=0;fail_allocation=0;
    recomp_icall_feedback_dump("targets.dump");
    return 0;
}
''')
    flags=['/DRECOMP_ICALL_FEEDBACK'] if os.name=='nt' else ['-DRECOMP_ICALL_FEEDBACK']
    executable=_compile(tmp_path,harness,extra=flags)
    result=subprocess.run([str(executable),mode],cwd=tmp_path,capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    assert parse_dump(tmp_path/'targets.dump')=={0x10000:1}
    assert parse_sites_dump(tmp_path/'sites.dump')=={0x10001:({0x10002},False)}
    assert ('site snapshot busy' in result.stderr)==(mode=='busy')


def test_runtime_bridge_retains_inherited_recorder_protocol(tmp_path):
    compiler,environment=native_compiler(tmp_path)
    environment.update(TSFP_PROJECT_ROOT=str(ROOT),TSFP_TEST_CC=compiler)
    code="""import os, subprocess
from pathlib import Path
from tools.recomp import test_icall_guarded_runtime as test
test._cc=lambda:os.environ['TSFP_TEST_CC']
def run(directory,sources,extra=()):
    exe=Path(directory)/'recording.exe'
    if os.name=='nt':
        flags=[('/'+arg[1:] if arg.startswith(('-D','-I')) else arg) for arg in extra]
        command=[test._cc(),'/nologo','/std:c11','/O2','/WX',*flags,*sources,'/Fe:'+str(exe)]
    else:
        command=[test._cc(),'-std=c11','-O2','-Werror',*extra,*sources,'-o',str(exe)]
    built=subprocess.run(command,cwd=directory,capture_output=True,text=True)
    assert built.returncode==0,built.stdout+built.stderr
    return subprocess.run([str(exe)],cwd=directory,capture_output=True,text=True)
test._run=run
test.test_runtime_site_dump_round_trips_through_the_merge_parser()
"""
    result=subprocess.run([sys.executable,'-c',code],cwd=ROOT/'external/ts3FPrecomp',
        env=environment,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
