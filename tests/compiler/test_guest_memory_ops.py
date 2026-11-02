"""Original memory/mask imports: whole-RAM models, actual native x86 and faults."""
from pathlib import Path
import os,random,re,struct,subprocess,sys
import pytest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
from native_compiler import native_compiler
from test_translation_product import _compile

def fixture(tmp_path,mutation=None):
    for name in ('guest_memory_ops.c','guest_memory_ops.h','guest_kernel_memory_ops.inc','guest_virtual_memory.h','guest_allocation.h','memory_ops_cli.c'):
        source=ROOT/('tests/compiler' if name=='memory_ops_cli.c' else 'src/runtime')/name
        (tmp_path/name).write_bytes(source.read_bytes())
    if mutation:
        name,a,b=mutation;p=tmp_path/name;s=p.read_text(encoding='utf-8');assert s.count(a)==1
        p.write_bytes(s.replace(a,b).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    return _compile(tmp_path,tmp_path/'memory_ops_cli.c',extra=[tmp_path/'guest_memory_ops.c',*flags])

def span(a,size,write=False):
    if not a or not size or a+size>2**32:raise ValueError
    at=a&0x7fffffff
    if not at or at+size>512 or (at<480 and at+size>464) or (write and at<416 and at+size>400):raise ValueError
    return at

def frame(op,a,b,c,ram):
    data=bytearray(ram);words=0 if op in (4,5) else 2 if op in (1,6) else 3
    for i,x in enumerate((a,b,c)[:words]):struct.pack_into('<I',data,32+i*4,x)
    return bytes(data)

def model(args,ram):
    op,a,b,c=args;out=bytearray(ram);value=1
    try:
        if op in (4,5):
            size=4 if op==4 else 2
            value=int.from_bytes(a.to_bytes(4,'little')[:size],'big')
        elif op==6:
            dest=span(a,4,True);mapping=span(b,16);value32=int.from_bytes(ram[dest:dest+4],'little')
            values=struct.unpack_from('<4I',ram,mapping)
            # The selected flags are evaluated against the accumulated rights.
            for bit,mapped in zip((31,30,29,28),values):
                if value32&(1<<bit):value32|=mapped
            struct.pack_into('<I',out,dest,value32&0x0fffffff)
        elif op==3:
            if c:
                source=span(b,c);dest=span(a,c,True);out[dest:dest+c]=ram[source:source+c]
        else:
            size=b if op!=2 else b//4*4
            if size:
                dest=span(a,size,True);pattern=(b'\0' if op==1 else c.to_bytes(4,'little') if op==2 else bytes([c&255]))
                out[dest:dest+size]=(pattern*((size+len(pattern)-1)//len(pattern)))[:size]
    except ValueError:value=0
    return value,bytes(out)

def vectors(count=14000):
    rng=random.Random(243)
    # Literal ordered mapping, output/table alias and partial-word tail cases.
    literals=[(6,64,96,0),(6,96,96,0),(6,0x80000064,96,0),(2,64,3,0x12345678),(2,65,7,0x12345678),(3,66,64,32),(3,64,66,32)]
    for args in literals:
        ram=bytearray(range(256))*2;struct.pack_into('<I',ram,64,0x80000001);struct.pack_into('<4I',ram,96,0x40000002,0x20000004,0x10000008,0x10)
        yield args,frame(*args,ram)
    for i in range(count):
        op=i%7;ram=rng.randbytes(512)
        a=rng.choice([0,16,32,64,65,96,100,320,400,464,508,510,0xfffffffe,0x80000040,0x80000020]);b=rng.choice([0,1,2,3,4,7,16,32,64,96,200,320,400,464,508,0xffffffff,0x80000040]);c=rng.getrandbits(32)
        if op==3:c=rng.choice([0,1,4,7,16,32,128,512,0xffffffff])
        if op in (4,5):a=rng.getrandbits(32)
        yield (op,a,b,c),frame(op,a,b,c,ram)

def execute(exe,cases):
    payload=b''.join(struct.pack('<4I',*a)+ram for a,ram in cases)
    return subprocess.run([str(exe)],input=payload,capture_output=True,timeout=45)

def agrees(exe):
    cases=list(vectors());result=execute(exe,cases)
    if result.returncode or len(result.stdout)!=1032*len(cases):return False
    for i,(args,ram) in enumerate(cases):
        direct,expected=model(args,ram);value=direct if args[0] in (4,5) else 0
        want=struct.pack('<I',direct)+expected+struct.pack('<I',value)+expected
        if result.stdout[i*1032:(i+1)*1032]!=want:return False
    return True

def test_original_memory_and_import_contracts(tmp_path):
    assert agrees(fixture(tmp_path))

@pytest.mark.parametrize('mutation',[
 ('guest_memory_ops.c','if(width==4)bytes-=bytes%4;','if(width==4)bytes+=0;'),
 ('guest_memory_ops.c','pattern>>((i%4)*8)','pattern>>((3-i%4)*8)'),
 ('guest_memory_ops.c','memmove(out,in,bytes);','for(uint32_t i=0;i<bytes;++i)out[i]=in[i];'),
 ('guest_memory_ops.c','value&=UINT32_C(0x0fffffff);','value|=UINT32_C(0xf0000000);'),
 ('guest_memory_ops.c','for(unsigned i=0;i<4;++i)if(value&flags[i])value|=table[i];','for(unsigned i=0;i<4;++i)if(value&flags[i])value^=table[i];'),
 ('guest_memory_ops.c','value=memory_word(out);','value=0;'),
 ('guest_kernel_memory_ops.inc','tsfp_memory_move(&vm,a[0],a[1],a[2])','tsfp_memory_move(&vm,a[1],a[0],a[2])'),
 ('guest_kernel_memory_ops.inc','tsfp_memory_byte_swap(g_ecx,2)','tsfp_memory_byte_swap(g_ecx,4)'),
 ('guest_kernel_memory_ops.inc','bridge_memory_arguments(&vm,a,3))(void)tsfp_memory_fill(&vm,a[0],a[1],a[2],4)','bridge_memory_arguments(&vm,a,2))(void)tsfp_memory_fill(&vm,a[0],a[1],0,4)'),
])
def test_compiled_wrong_providers_and_adapters_are_rejected(tmp_path,mutation):
    assert not agrees(fixture(tmp_path,mutation))

@pytest.mark.skipif(os.name!='nt',reason='Actual x86 ntdll/WoW64 oracle needs Windows')
def test_actual_x86_native_memory_reference(tmp_path):
    compiler,env=native_compiler(tmp_path);x86=Path(compiler).parent.parent/'x86/cl.exe';assert x86.is_file()
    env=dict(env);env['PATH']=str(x86.parent)+os.pathsep+env['PATH'];env['LIB']=re.sub(r'(?i)([\\/])x64(?=;|$)',r'\1x86',env['LIB']);env['VSCMD_ARG_TGT_ARCH']='x86'
    source=tmp_path/'memory_ops_x86.c';source.write_bytes((ROOT/'tests/compiler'/source.name).read_bytes());exe=tmp_path/'native-x86.exe'
    run=subprocess.run([str(x86),'/nologo','/std:c11','/O2','/W4','/WX',str(source),f'/Fe:{exe}'],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=90);assert run.returncode==0,run.stdout+run.stderr
    cases=[]
    for args,ram in vectors(7000):
        result,expected=model(args,ram)
        if args[0] not in (4,5) and not result:continue
        if args[0]==6:
            a,b=(x&0x7fffffff for x in args[1:3])
            if a<b+16 and b<a+4:continue # Native overlapping mapping reads are not this provider's snapshot contract.
        cases.append((args,ram))
    assert len(cases)>2500
    run=execute(exe,cases);assert run.returncode==0,run.stderr.decode(errors='replace');assert len(run.stdout)==516*len(cases)
    for i,(args,ram) in enumerate(cases):
        value,expected=model(args,ram);returned=value if args[0] in (4,5) else 0
        assert run.stdout[i*516:(i+1)*516]==struct.pack('<I',returned)+expected,args

def test_memory_selection_and_dispatch_cleanup():
    kernel=ROOT/'external/ts3FPrecomp/src/kernel';bridge=(kernel/'kernel_bridge.c').read_text(encoding='utf-8')
    assert (kernel/'CMakeLists.txt').read_text(encoding='utf-8').count('${TSFP_SHARED_SOURCE}/runtime/guest_memory_ops.c')==1
    assert bridge.count('#include "guest_kernel_memory_ops.inc"')==1
    for ordinal,name,size in ((284,'RtlFillMemory',12),(285,'RtlFillMemoryUlong',12),(298,'RtlMoveMemory',12),(320,'RtlZeroMemory',8),(307,'RtlUlongByteSwap',0),(318,'RtlUshortByteSwap',0),(297,'RtlMapGenericMask',8)):
        assert 'static void bridge_'+name+'(void)' not in bridge
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+'+str(size)+r';',bridge)
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+bridge_'+name+r';',bridge)


def test_actual_dispatch_sizes_for_memory_and_fastcall_swaps(tmp_path):
    bridge=(ROOT/'external/ts3FPrecomp/src/kernel/kernel_bridge.c').read_text(encoding='utf-8')
    start=bridge.index('static int stdcall_args_for_ordinal(');end=bridge.index('static bridge_func_t bridge_for_ordinal(',start)
    assertions=''.join('assert(stdcall_args_for_ordinal('+str(o)+')=='+str(n)+');' for o,n in ((284,12),(285,12),(298,12),(320,8),(297,8),(307,0),(318,0)))
    source=tmp_path/'sizes.c';source.write_bytes(('#include <stdint.h>\n#include <assert.h>\ntypedef uint32_t ULONG;\n'+bridge[start:end]+'int main(void){'+assertions+'return 0;}\n').encode())
    run=subprocess.run([str(_compile(tmp_path,source))],capture_output=True,text=True,timeout=15);assert run.returncode==0,run.stdout+run.stderr
