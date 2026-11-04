"""Original extended division: independent math, whole RAM and real x86 APIs."""
from pathlib import Path
import os,random,re,struct,subprocess,sys
import pytest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
from native_compiler import native_compiler
from test_translation_product import _compile
MASK=2**64-1

def compile_fixture(tmp_path,mutation=None):
    for name in ('guest_integer_division.c','guest_integer_division.h','guest_kernel_division.inc','guest_virtual_memory.h','guest_allocation.h','integer_division_contract.c'):
        source=ROOT/('tests/compiler' if name=='integer_division_contract.c' else 'src/runtime')/name
        (tmp_path/name).write_bytes(source.read_bytes())
    if mutation:
        filename,a,b=mutation;p=tmp_path/filename;s=p.read_text(encoding='utf-8');assert s.count(a)==1
        p.write_bytes(s.replace(a,b).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    return _compile(tmp_path,tmp_path/'integer_division_contract.c',extra=[tmp_path/'guest_integer_division.c',*flags])

def cases(count=12000):
    for a in (0,1,2**32-1,2**32,2**63-1,2**63,2**64-1,0x123456789abcdef0):
        for b in (0,1,3,0xaaaaaaaaaaaaaaab,2**64-1):
            yield 0,a,b,0,16
            for shift in (0,1,31,32,63,64,128,255,256,0xffffff01):yield 1,a,b,shift,0
    rng=random.Random(242)
    for i in range(count):
        yield i%2,rng.getrandbits(64),rng.getrandbits(64) if i%17 else 0,rng.getrandbits(32),rng.choice([0,16,64,0x80000010,0x80000040,232,254,0xfffffffe])

def arithmetic(op,a,b,shift):
    if not op:return (a//(b&0xffffffff),a%(b&0xffffffff)) if b&0xffffffff else (0xa5a5a5a5a5a5a5a5,0x5a5a5a5a)
    signed=a if a<2**63 else a-2**64
    q=(abs(signed)*b//2**64)//2**(shift&255)
    return ((-q if signed<0 else q)&MASK),0x5a5a5a5a

def model(case):
    op,a,b,shift,out=case;ram=bytearray(b'\xa5'*256)
    struct.pack_into('<IIII',ram,64,a&0xffffffff,a>>32,b&0xffffffff,b>>32 if op else out)
    if op:struct.pack_into('<I',ram,80,shift)
    q,rem=arithmetic(op,a,b,shift);returned=0x8765432112345678;fault=int(not op and not b&0xffffffff)
    if not fault:
        at=out&0x7fffffff
        valid=not out or (out<=0xfffffffc and at and at+4<=256 and not (at<240 and at+4>232))
        if op or valid:
            returned=q
            if not op and out:struct.pack_into('<I',ram,at,rem)
    return q,rem,returned,fault,bytes(ram)

def agrees(exe):
    vectors=list(cases());run=subprocess.run([str(exe)],input=''.join(f'{o} {a:x} {b:x} {s:x} {p:x}\n' for o,a,b,s,p in vectors),text=True,capture_output=True,timeout=45)
    if run.returncode:return False
    if len(run.stdout.splitlines())!=len(vectors):return False
    for case,line in zip(vectors,run.stdout.splitlines(),strict=True):
        q,rem,result,fault,ram=model(case)
        if line.split()!=[f'{q:016x}',f'{rem:08x}',f'{result:016x}',str(fault),ram.hex()]:return False
    return True

def test_original_math_and_checked_imports(tmp_path):
    assert agrees(compile_fixture(tmp_path))

@pytest.mark.parametrize('mutation',[
 ('guest_integer_division.c','next.quotient=dividend/divisor;','next.quotient=(uint32_t)dividend/divisor;'),
 ('guest_integer_division.c','+(carry>>32);','+(carry>>31);'),
 ('guest_integer_division.c','magnitude=negative?UINT64_C(0)-dividend:dividend;','magnitude=dividend;'),
 ('guest_integer_division.c','if(shift>=64)return 0;','shift&=63;'),
 ('guest_kernel_division.inc','((uint64_t)words[3]<<32)|words[2]','((uint64_t)words[2]<<32)|words[3]'),
 ('guest_kernel_division.inc','if(!words[2]){tsfp_guest_divide_error(g_xbox_kernel_caller,0);return;}','if(!words[2])return;'),
 ('guest_kernel_division.inc','bridge_division_arguments(&vm,words,5)','bridge_division_arguments(&vm,words,4)'),
 ('guest_kernel_division.inc','out[i]=(unsigned char)(result.remainder>>(i*8))','out[i]=(unsigned char)(result.quotient>>(i*8))'),
])
def test_compiled_bad_dividers_and_adapters_are_rejected(tmp_path,mutation):
    assert not agrees(compile_fixture(tmp_path,mutation))

@pytest.mark.skipif(os.name!='nt',reason='Actual x86 ntdll/WoW64 reference needs Windows')
def test_actual_native_x86_arithmetic(tmp_path):
    compiler,env=native_compiler(tmp_path);x86=Path(compiler).parent.parent/'x86/cl.exe';assert x86.is_file()
    env=dict(env);env['PATH']=str(x86.parent)+os.pathsep+env['PATH'];env['LIB']=re.sub(r'(?i)([\\/])x64(?=;|$)',r'\1x86',env['LIB']);env['VSCMD_ARG_TGT_ARCH']='x86'
    source=tmp_path/'integer_division_x86.c';source.write_bytes((ROOT/'tests/compiler'/source.name).read_bytes());exe=tmp_path/'native-x86.exe'
    result=subprocess.run([str(x86),'/nologo','/std:c11','/W4','/WX','/O2',str(source),f'/Fe:{exe}'],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=90)
    assert result.returncode==0,result.stdout+result.stderr
    vectors=list(cases(6000));result=subprocess.run([str(exe)],input=''.join(f'{o} {a:x} {b:x} {s}\n' for o,a,b,s,p in vectors),capture_output=True,text=True,timeout=45)
    assert result.returncode==0,result.stderr
    lines=result.stdout.splitlines();assert len(lines)==len(vectors)
    for case,line in zip(vectors,lines,strict=True):
        op,a,b,shift,_=case;q,rem=arithmetic(op,a,b,shift);fault=not op and not b&0xffffffff
        assert line.split()==[f'{q:016x}',f'{rem:08x}',f'{0xc0000094 if fault else 0:08x}'],case

def test_division_selection_and_actual_dispatch_cleanup():
    kernel=ROOT/'external/ts3FPrecomp/src/kernel';bridge=(kernel/'kernel_bridge.c').read_text(encoding='utf-8')
    assert (kernel/'CMakeLists.txt').read_text(encoding='utf-8').count('${TSFP_SHARED_SOURCE}/runtime/guest_integer_division.c')==1
    assert bridge.count('#include "guest_kernel_division.inc"')==1
    for ordinal,name,size in ((282,'RtlExtendedLargeIntegerDivide',16),(283,'RtlExtendedMagicDivide',20)):
        assert 'static void bridge_'+name+'(void)' not in bridge
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+'+str(size)+r';',bridge)
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+bridge_'+name+r';',bridge)


def test_real_dispatch_argument_sizes_execute(tmp_path):
    bridge=(ROOT/'external/ts3FPrecomp/src/kernel/kernel_bridge.c').read_text(encoding='utf-8')
    start=bridge.index('static int stdcall_args_for_ordinal(')
    end=bridge.index('static bridge_func_t bridge_for_ordinal(',start)
    source=tmp_path/'argument_sizes.c'
    source.write_bytes(('#include <assert.h>\n#include <stdint.h>\ntypedef uint32_t ULONG;\n'+bridge[start:end]+\
      'int main(void){assert(stdcall_args_for_ordinal(281)==12);assert(stdcall_args_for_ordinal(282)==16);assert(stdcall_args_for_ordinal(283)==20);return 0;}\n').encode())
    exe=_compile(tmp_path,source)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
    assert run.returncode==0,run.stdout+run.stderr
