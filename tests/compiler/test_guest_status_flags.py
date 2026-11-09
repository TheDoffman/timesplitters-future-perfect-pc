"""Original persistent arithmetic-status model and producer integration."""
from pathlib import Path
import os,random,re,struct,subprocess,sys
import pytest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
from test_translation_product import _compile
STATUS=0x8d5

def fixture(tmp_path,mutation=None):
    for n in ('guest_status_flags.h','status_flags_cli.c'):(tmp_path/n).write_bytes((ROOT/('src/runtime' if n.endswith('.h') else 'tests/compiler')/n).read_bytes())
    if mutation:
        name,a,b=mutation;p=tmp_path/name;s=p.read_text(encoding='utf-8');assert s.count(a)==1;p.write_bytes(s.replace(a,b).encode())
    return _compile(tmp_path,tmp_path/'status_flags_cli.c',extra=['/W4'] if os.name=='nt' else ['-Wall','-Wextra'])

def model(p):
    op,bits,a,b,fallback,value,known=p;mask=2**bits-1;a&=mask;b&=mask;carry=(value&1) if known&1 else int(bool(fallback));r=a
    def publish(defined,v,undefined=0):
        nonlocal value,known
        value=(value&~(defined|undefined))|(v&defined)|2;known=(known&~undefined)|defined|2
    if op<=6 or op==10:
        sub=op in (1,3,5,6,10);c=carry if op in (2,3) else 0;x=0 if op==6 else a;y=1 if op in (4,5) else a if op==6 else b
        full=x-y-c if sub else x+y+c;r=full&mask
        if op in (2,3) and not known&1:known=known&~STATUS|2;value|=2
        else:
            v=(int(full<0 if sub else full>mask))|((r&255).bit_count()%2==0)*4|((x^y^r)&16)|(r==0)*64|((r>>(bits-1))&1)*128
            signed=lambda x:x-2**bits if x&2**(bits-1) else x
            mathematical=signed(x)-signed(y)-c if sub else signed(x)+signed(y)+c
            v|=int(not -2**(bits-1)<=mathematical<2**(bits-1))*2048
            publish(STATUS&~(1 if op in (4,5) else 0),v)
        if op==10:r=a
    elif op<=9 or op==11:
        r=a&b if op in (7,11) else a|b if op==8 else a^b
        v=((r&255).bit_count()%2==0)*4|(r==0)*64|((r>>(bits-1))&1)*128;publish(STATUS&~16,v,16)
        if op==11:r=a
    elif op==12:known=known&~(fallback&STATUS)|2;value|=2
    elif op in (13,14):publish(1,int(op==14))
    elif op==15:value=value^1|2;known|=2
    elif op==16:publish(STATUS&~2048,a)
    elif op==17:
        k=known&STATUS&255;r=((a&~k)|(value&k)|2)&0xd7
    elif 18<=op<34:
        code=op-18
        required=(2048,1,64,65,128,4,2176,2240)[code//2]
        if known&required!=required:r=int(bool(fallback))
        else:
            cf,pf,zf,sf,of=(bool(value&bit) for bit in (1,4,64,128,2048))
            r=int((of,cf,zf,cf or zf,sf,pf,sf!=of,zf or sf!=of)[code//2])^(code&1)
    return r,value&0xffffffff,known&0xffffffff,(value&1) if known&1 else int(bool(fallback))

def vectors(count=18000):
    rng=random.Random(248)
    edge=(0,1,2,0x7f,0x80,0xff,0x7fff,0x8000,0xffff,0x7fffffff,0x80000000,0xffffffff)
    for op in range(18):
        for bits in (8,16,32):
            for a,b in zip(edge,edge[::-1]):
                for c in (0,1):yield op,bits,a,b,c,0xffffffff if c else 2,STATUS if c else 2
    for i in range(count):yield i%18,rng.choice((8,16,32)),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32)

def agrees(exe):
    data=list(vectors());r=subprocess.run([str(exe)],input=b''.join(struct.pack('<7I',*p) for p in data),capture_output=True,timeout=30)
    return r.returncode==0 and r.stdout==b''.join(struct.pack('<4I',*model(p)) for p in data)

def test_original_status_values_validity_and_carry(tmp_path):assert agrees(fixture(tmp_path))


def test_all_conditions_values_and_partial_knowledge(tmp_path):
    bits=(1,4,16,64,128,2048)
    states=[sum(bit for i,bit in enumerate(bits) if n&(1<<i)) for n in range(64)]
    data=[(op,32,0,0,fallback,value,known)
          for op in range(18,34) for value in states for known in states for fallback in (0,1)]
    result=subprocess.run([str(fixture(tmp_path))],input=b''.join(struct.pack('<7I',*p) for p in data),capture_output=True,timeout=30)
    assert result.returncode==0
    assert result.stdout==b''.join(struct.pack('<4I',*model(p)) for p in data)

@pytest.mark.parametrize('mutation',[
 ('guest_status_flags.h','left+right+carry>mask','left+right+carry>=mask'),
 ('guest_status_flags.h','left<(uint64_t)right+carry','left<=(uint64_t)right+carry'),
 ('guest_status_flags.h','UINT32_C(0x9669)','UINT32_C(0x6996)'),
 ('guest_status_flags.h','(left^right^result)&TSFP_FLAG_AF','(left^right^result)&TSFP_FLAG_CF'),
 ('guest_status_flags.h','~(left^right)&(left^result)&sign','(left^right)&(left^result)&sign'),
 ('guest_status_flags.h','tsfp_status_szp(result&mask,bits),TSFP_FLAG_AF);','tsfp_status_szp(result&mask,bits),0);'),
 ('guest_status_flags.h','s->known&=~(bits&TSFP_STATUS_FLAGS);','s->known|=bits&TSFP_STATUS_FLAGS;'),
 ('guest_status_flags.h','s->value^=TSFP_FLAG_CF;','s->value|=TSFP_FLAG_CF;'),
])
def test_compiled_bad_status_helpers_are_rejected(tmp_path,mutation):assert not agrees(fixture(tmp_path,mutation))


@pytest.mark.skipif(os.name!='nt',reason='Actual x86 arithmetic EFLAGS oracle')
def test_actual_x86_alu_status_flags(tmp_path):
    from native_compiler import native_compiler
    compiler,env=native_compiler(tmp_path);cc=Path(compiler).parent.parent/'x86/cl.exe';assert cc.is_file()
    env=dict(env);env['PATH']=str(cc.parent)+os.pathsep+env['PATH'];env['LIB']=re.sub(r'(?i)([\\/])x64(?=;|$)',r'\1x86',env['LIB']);env['VSCMD_ARG_TGT_ARCH']='x86'
    source=tmp_path/'status_flags_x86.c';source.write_bytes((ROOT/'tests/compiler'/source.name).read_bytes());exe=tmp_path/'native.exe'
    r=subprocess.run([str(cc),'/nologo','/std:c11','/O2','/W4','/WX',str(source),'/Fe:'+str(exe)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=90);assert r.returncode==0,r.stdout+r.stderr
    requests=[p for p in vectors(36000) if p[0]<12 or p[0]==17]
    # Exhaust every8-bit pair for ADD/SUB and both carry inputs for ADC/SBB.
    for op in range(4):
        for c in ((0,1) if op in (2,3) else (0,)):
            requests.extend((op,8,a,b,c,2|c,STATUS) for a in range(256) for b in range(256))
    payload=b''.join(struct.pack('<7I',*p) for p in requests)
    native=subprocess.run([str(exe)],input=payload,capture_output=True,timeout=60);assert native.returncode==0,native.stderr.decode(errors='replace');assert len(native.stdout)==len(requests)*8
    original=subprocess.run([str(fixture(tmp_path))],input=payload,capture_output=True,timeout=60);assert original.returncode==0,original.stderr.decode(errors='replace');assert len(original.stdout)==len(requests)*16
    for i,p in enumerate(requests):
        actual=struct.unpack_from('<4I',original.stdout,i*16);hw=struct.unpack_from('<2I',native.stdout,i*8);valid=actual[2]&STATUS
        assert actual[0]==hw[0],(i,p,actual,hw)
        assert actual[1]&valid==hw[1]&valid,(i,p,actual,hw)
