"""Project continuation provider contracts, separate from translator stubs."""
import os
from pathlib import Path
import subprocess
import sys

import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile


def build(folder,mutation=None):
    source=(ROOT/'src/runtime/guest_nonlocal_jump.c').read_text()
    if mutation:
        before,after=mutation
        assert source.count(before)==1
        source=source.replace(before,after)
    provider=folder/'provider.c';provider.write_text(source)
    harness=folder/'contracts.c'
    harness.write_bytes((ROOT/'tests/compiler/nonlocal_jump_runtime.c').read_bytes())
    return _compile(folder,harness,extra=[provider,*([] if os.name=='nt' else ['-pthread'])])


@pytest.fixture(scope='module')
def provider(tmp_path_factory):
    return build(tmp_path_factory.mktemp('continuations'))


def run(executable,mode):
    return subprocess.run([str(executable),mode],cwd=executable.parent,
                          capture_output=True,text=True,timeout=30)


def test_installed_kernel_uses_project_continuation_provider():
    kernel=ROOT/'external/ts3FPrecomp/src/kernel'
    memory=(kernel/'xbox_memory_layout.c').read_text()
    assert 'RECOMP_JMPBUF_SLOTS' not in memory
    assert 'jmp_buf *recomp_setjmp_slot' not in memory
    assert 'int recomp_guest_longjmp' not in memory
    cmake=(kernel/'CMakeLists.txt').read_text()
    assert 'target_sources(xbox_kernel PRIVATE "${TSFP_SHARED_SOURCE}/runtime/guest_nonlocal_jump.c")' in cmake


@pytest.mark.parametrize('mode',[
    'basic','zero','high-bit','all-bits','rearm','nested','capacity','threads','missing'])
def test_native_continuation_contract(provider,mode):
    result=run(provider,mode)
    assert result.returncode==0,result.stdout+result.stderr+f' return={result.returncode}'


@pytest.mark.skipif(os.name!='nt',reason='Real Windows fiber stack ownership adapter')
def test_foreign_stack_transfers_to_owning_fiber(provider):
    result=run(provider,'fibers')
    assert result.returncode==0,result.stdout+result.stderr


@pytest.mark.skipif(os.name!='nt',reason='Windows foreign-stack rejection')
@pytest.mark.parametrize('mode',['returning-adapter','missing-adapter'])
def test_foreign_stack_without_transfer_stops_before_native_unwind(provider,mode):
    result=run(provider,mode)
    assert result.returncode!=0
    assert '[tsfp-jump] foreign stack for guest buffer 00000081' in result.stderr
    assert 'adapter did not transfer control' in result.stderr


@pytest.mark.parametrize('mode,mutation,expected',[
    ('basic',('g_ebx=MEM32(guest_buffer+4u);','g_ebx=0;'),1),
    ('basic',('g_esp=MEM32(guest_buffer+16u)+4u;','g_esp=MEM32(guest_buffer+16u)+8u;'),1),
    ('zero',('const uint32_t bits=result ? result : 1u;','const uint32_t bits=result ? result : 7u;'),4),
    ('nested',('active=index+1u;','(void)index;'),6),
    ('rearm',('\n    unsigned index=continuation_for(guest_buffer);','\n    unsigned index=TSFP_CONTINUATIONS;'),1),
    ('capacity',('TSFP_CONTINUATIONS-1u;','0u;'),3),
])
def test_compiled_negative_controls(tmp_path,mode,mutation,expected):
    result=run(build(tmp_path,mutation),mode)
    assert result.returncode==expected,result.stdout+result.stderr+f' return={result.returncode}'


def test_project_emitted_calls_with_real_provider_and_canonical_header(tmp_path):
    from test_translation_control import context,instruction,function
    import translation_control
    code=r'''
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE 1
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ebx,g_ecx,g_edx,g_esi,g_edi,g_esp,g_ebp,g_seh_ebp;
static unsigned char memory[1024];
static unsigned escaped;
static uint32_t returned_ebp,resume_value;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"check line %d\n",__LINE__);return 1; } } while(0)
static void target_4000(void) {
    const uint32_t buffer=MEM32(esp+4);
    MEM32(buffer)=g_ebp;MEM32(buffer+4)=ebx;MEM32(buffer+8)=edi;
    MEM32(buffer+12)=esi;MEM32(buffer+16)=esp;
    esp+=4;eax=0;
}
static void target_5000(void) { ++escaped;esp+=4; }
'''
    call_context=context(SETJMP_FN=0x4000,LONGJMP_FN=0x5000,publishes_ebp=True)
    code+=function('nonlocal_jump',['uint32_t ebp=0x333;']+
        translation_control.call(call_context,instruction('call',0x5000),[])+['++escaped;'])
    code+=function('resume_frame',['uint32_t ebp=0x1234;']+
        translation_control.call(call_context,instruction('call',0x4000),[])+[
        'if(!eax) { ebp=0;ebx=edi=esi=0;MEM32(esp)=0x80;MEM32(esp+4)=resume_value;nonlocal_jump();++escaped; }',
        'returned_ebp=ebp;'])
    code+=r'''
int main(int argc,char **argv) {
    if(argc!=2)return 99;
    resume_value=(uint32_t)strtoul(argv[1],NULL,10);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    esp=512;ebx=123;edi=456;esi=789;MEM32(esp)=0x80;resume_frame();
    CHECK(eax==(resume_value ? resume_value : 1u));
    CHECK(returned_ebp==0x1234 && g_ebp==0x1234 && g_seh_ebp==0x1234);
    CHECK(ebx==123 && edi==456 && esi==789 && esp==512 && escaped==0);
    MEM32(esp)=0x99;nonlocal_jump();CHECK(escaped==2 && esp==512);
    return 0;
}
'''
    harness=tmp_path/'emitted.c';harness.write_text(code)
    executable=_compile(tmp_path,harness,extra=[ROOT/'src/runtime/guest_nonlocal_jump.c'])
    for value in (0,9,0x80000000,0xffffffff):
        result=run(executable,str(value))
        assert result.returncode==0,result.stdout+result.stderr+f' value={value}'
