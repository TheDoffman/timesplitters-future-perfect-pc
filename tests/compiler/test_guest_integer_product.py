"""Original ordinal281: Python integer oracle, full frames and bad controls."""
from pathlib import Path
import os,random,re,subprocess,sys
import pytest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
from test_translation_product import _compile

def compile_fixture(tmp_path,mutation=None):
    for name in ('guest_integer_product.c','guest_integer_product.h','guest_kernel_product.inc','guest_virtual_memory.h','guest_allocation.h','integer_product_contract.c'):
        source=ROOT/('tests/compiler' if name=='integer_product_contract.c' else 'src/runtime')/name
        (tmp_path/name).write_bytes(source.read_bytes())
    if mutation:
        filename,a,b=mutation;p=tmp_path/filename;s=p.read_text(encoding='utf-8');assert s.count(a)==1
        p.write_bytes(s.replace(a,b).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    return _compile(tmp_path,tmp_path/'integer_product_contract.c',extra=[tmp_path/'guest_integer_product.c',*flags])

def vectors():
    # Cross every signed endpoint and non-palindromic word boundary explicitly.
    for a in (0,1,2**31-1,2**31,2**32-1,2**32,2**63-1,2**63,2**64-1,0x123456789abcdef0):
        for b in (0,1,2,2**31-1,2**31,2**32-2,2**32-1,0x12345678):yield a,b
    rng=random.Random(241)
    for _ in range(20000):yield rng.getrandbits(64),rng.getrandbits(32)

def agrees(exe):
    cases=list(vectors())
    run=subprocess.run([str(exe)],input=''.join(f'{a:x} {b:x}\n' for a,b in cases),capture_output=True,text=True,timeout=30)
    if run.returncode:return False
    lines=run.stdout.splitlines()
    if len(lines)!=len(cases):return False
    for (a,b),line in zip(cases,lines):
        signed_a=a if a<2**63 else a-2**64
        signed_b=b if b<2**31 else b-2**32
        expected=(signed_a*signed_b)%(2**64)
        if line.split()!=[f'{expected:016x}']*2:return False
    return True

def test_independent_products_and_complete_import_frames(tmp_path):
    assert agrees(compile_fixture(tmp_path))

@pytest.mark.parametrize('mutation',[
 ('guest_integer_product.c','if(multiplier&UINT32_C(0x80000000))','if(0)'),
 ('guest_integer_product.c','return multiplicand*factor;','return (uint32_t)multiplicand*factor;'),
 ('guest_kernel_product.inc','((uint64_t)words[1]<<32)|words[0]','((uint64_t)words[0]<<32)|words[1]'),
 ('guest_kernel_product.inc','g_edx=(uint32_t)(result>>32);','g_edx=(uint32_t)result;'),
 ('guest_kernel_product.inc','frame=vm.resolve(vm.host,stack,12,0);','frame=vm.resolve(vm.host,stack,8,0);'),
])
def test_compiled_bad_products_and_adapters_are_rejected(tmp_path,mutation):
    assert not agrees(compile_fixture(tmp_path,mutation))

def test_product_selection_and_cleanup():
    kernel=ROOT/'external/ts3FPrecomp/src/kernel'
    assert (kernel/'CMakeLists.txt').read_text(encoding='utf-8').count('${TSFP_SHARED_SOURCE}/runtime/guest_integer_product.c')==1
    bridge=(kernel/'kernel_bridge.c').read_text(encoding='utf-8')
    assert bridge.count('#include "guest_kernel_product.inc"')==1
    assert 'static void bridge_RtlExtendedIntegerMultiply(void)' not in bridge
    assert re.search(r'case\s+281:\s+return\s+12;',bridge)
    assert re.search(r'case\s+281:\s+return\s+bridge_RtlExtendedIntegerMultiply;',bridge)
