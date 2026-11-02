"""Original guest EBP diagnostics: independent complete-memory/ABI contracts."""
from pathlib import Path
import os,random,re,struct,subprocess,sys
import pytest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
from test_translation_product import _compile
from native_compiler import native_compiler

def fixture(tmp_path,mutation=None):
    for name in ('guest_stack_trace.c','guest_stack_trace.h','guest_kernel_stack_trace.inc','guest_kernel_rtl.h','guest_critical.h','guest_rtl_services.h','guest_virtual_memory.h','guest_allocation.h','stack_trace_cli.c'):
        p=ROOT/('tests/compiler' if name=='stack_trace_cli.c' else 'src/runtime')/name;(tmp_path/name).write_bytes(p.read_bytes())
    if mutation:
        name,a,b=mutation;p=tmp_path/name;s=p.read_text(encoding='utf-8');assert s.count(a)==1;p.write_bytes(s.replace(a,b).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    return _compile(tmp_path,tmp_path/'stack_trace_cli.c',extra=[tmp_path/'guest_stack_trace.c',*flags])

def span(a,n,write=False):
    if not a or not n or a+n>2**32:raise ValueError
    at=a&0x7fffffff
    if not at or at+n>4096 or at<2816 and at+n>2800:raise ValueError
    if write and at<3520 and at+n>3500:raise ValueError
    return at

def model(op,v,args,original):
    ram=bytearray(original);sp,bp,caller,low,high,code_low,code_high=v
    if op==3:return (0,1,*args),bytes(ram)
    empty=(0,0,0,0,0,0)
    if not low<high or not low<=sp<high or not code_low<code_high:return empty,original
    if op==1 and args[2]&255:return empty,original
    # Decode all well-shaped physical frame pairs first; selection/hash/output
    # are independent list operations, not interleaved with the C walk.
    chain=[caller];cursor=bp;visited=set();previous=None
    while cursor and cursor!=0xffffffff:
        if cursor in visited or cursor%4 or cursor<max(low,sp) or cursor+8>high or previous is not None and cursor<previous+8:break
        try:at=span(cursor,8)
        except ValueError:break
        visited.add(cursor);next_,pc=struct.unpack_from('<II',ram,at);chain.append(pc);previous,cursor=cursor,next_
    skip=args[0] if op==0 else args[2]>>8 if op==1 else 1
    cap=min(args[1],65535) if op==0 else args[1] if op==1 else 2
    selected=[]
    for pc in chain[skip:]:
        if len(selected)>=cap or not code_low<=pc<code_high:break
        selected.append(pc)
    try:
        if op==2:
            a=span(args[0],4,True) if args[0] else None;b=span(args[1],4,True) if args[1] else None
            if a is not None and b is not None and a!=b and abs(a-b)<4:raise ValueError
            selected+=(2-len(selected))*[0]
            if a is not None:struct.pack_into('<I',ram,a,selected[0])
            if b is not None:struct.pack_into('<I',ram,b,selected[1])
            return empty,bytes(ram)
        output=args[2] if op==0 else args[0];hash_=args[3] if op==0 else 0
        h=span(hash_,4,True) if hash_ else None
        a=span(output,len(selected)*4,True) if selected else None
        if a is not None and h is not None and a<h+4 and h<a+len(selected)*4:raise ValueError
        if selected:struct.pack_into('<'+'I'*len(selected),ram,a,*selected)
        if h is not None:struct.pack_into('<I',ram,h,sum(selected)&0xffffffff)
        return (len(selected),0,0,0,0,0),bytes(ram)
    except ValueError:return empty,original

def make(op,v,args,ram):
    ram=bytearray(ram);n=4 if op in (0,3) else 3 if op==1 else 2;at=v[0]&0x7fffffff
    if at+n*4<=4096:struct.pack_into('<'+'I'*n,ram,at,*args[:n])
    return op,tuple(v),tuple(args),bytes(ram)

def base(depth=4):
    ram=bytearray(b'\xa5'*4096)
    for i in range(depth):struct.pack_into('<II',ram,512+i*16,512+(i+1)*16 if i+1<depth else 0,0x1100+i*4)
    return ram

def cases(count=6000):
    v=(64,512,0x1000,32,3072,0x1000,0x3000)
    for op in range(3):
        for skip in (0,1,2,8,0xffffffff):
            for n in (0,1,2,4,8,65536,0xffffffff):
                args=(skip,n,128,32) if op==0 else (128,n,skip<<8&0xffffffff,0) if op==1 else (128,132,0,0)
                yield make(op,v,args,base())
    for bp in (0,0xffffffff,511,514,3068,3072,3500):
        ram=base();struct.pack_into('<II',ram,3068,0,0x2200)
        if bp==514:struct.pack_into('<II',ram,514,0,0x2200)
        for op in range(3):yield make(op,(64,bp,0x1000,32,3072,0x1000,0x3000),(0,8,128,32) if op==0 else (128,8,0,0) if op==1 else (128,132,0,0),ram)
    ram=base();struct.pack_into('<III',ram,512,516,0x1100,0x1200)
    yield make(0,v,(0,8,128,32),ram)
    ram=base()
    for i in range(4):struct.pack_into('<I',ram,512+i*16+4,0xf0000001+i)
    yield make(0,(64,512,0xf0000000,32,3072,0x80000000,0xfffffffe),(0,8,128,32),ram)
    rng=random.Random(247)
    for i in range(count):
        op=i%4;ram=base(rng.randrange(0,120));a=rng.choice((128,128,129,64,512,3500,2800,0,0xfffffffc,0x80000080));h=rng.choice((32,32,0,64,128,129,3500,2800,0xfffffffe,0x80000020));n=rng.choice((0,1,2,8,32,128,65536,0xffffffff));skip=rng.choice((0,0,1,2,80,0xffffffff))
        bp=rng.choice((512,512,512,0,0xffffffff,514,3068,3500));sp=rng.choice((64,64,128,3000));caller=rng.choice((0x1000,0x1234,0x2fff,0,0x3000));lo,hi=rng.choice(((32,3072),(32,3072),(64,512),(3072,32),(0,32)))
        if i%3==0:
            pos=512+rng.randrange(100)*16;struct.pack_into('<II',ram,pos,rng.choice((0,512,pos,pos-8,pos+4,pos+8,2800,0xffffffff)),rng.choice((0x1100,0,0x3000,0xffffffff)))
        args=(skip,n,a,h) if op==0 else (a,n,rng.choice((0,0,1,2,0x100,0x200,0xffffff00)),0) if op==1 else (a,h,0,0) if op==2 else tuple(rng.getrandbits(32) for _ in range(4))
        yield make(op,(sp,bp,caller,lo,hi,0x1000,0x3000),args,ram)

def execute(exe,vectors,bridge):
    return subprocess.run([str(exe),'bridge' if bridge else 'direct'],input=b''.join(struct.pack('<12I',op,*v,*a)+ram for op,v,a,ram in vectors),capture_output=True,timeout=60)

def agrees(exe):
    vectors=list(cases());want=b''.join(struct.pack('<6I',*model(op,v,a,ram)[0])+model(op,v,a,ram)[1] for op,v,a,ram in vectors)
    for bridge in (False,True):
        r=execute(exe,vectors,bridge)
        if r.returncode or r.stdout!=want:return False
    return True

def test_original_guest_stack_diagnostics_whole_ram(tmp_path):assert agrees(fixture(tmp_path))

@pytest.mark.parametrize('mutation',[
 ('guest_stack_trace.c','trace_collect(vm,&v,1,2,values)','trace_collect(vm,&v,0,2,values)'),
 ('guest_stack_trace.c','(frame&3)','(frame&1)'),
 ('guest_stack_trace.c','v->high-frame<8','v->high-frame<4'),
 ('guest_stack_trace.c','if(next<frame || next-frame<8)break;\n        floor=frame+8;','if(next<frame || next-frame<4)break;\n        floor=frame+4;'),
 ('guest_stack_trace.c','sum+=values[i];','sum^=values[i];'),
 ('guest_stack_trace.c','trace_overlap(body,used*4,hash_word,4)','trace_overlap(body,0,hash_word,4)'),
 ('guest_stack_trace.c','trace_write(b,values[1])','trace_write(b,values[0])'),
 ('guest_kernel_stack_trace.inc','a[1]>65535?65535:a[1]','a[1]&65535'),
 ('guest_kernel_stack_trace.inc','a[2]>>8','a[2]>>9'),
 ('guest_kernel_stack_trace.inc','tsfp_kernel_rtl_assert(&vm,a[0],a[1],a[2],a[3])','tsfp_kernel_rtl_assert(&vm,a[0],a[1],a[2],0)'),
])
def test_compiled_bad_walkers_and_imports_rejected(tmp_path,mutation):assert not agrees(fixture(tmp_path,mutation))


def test_stack_import_selection_and_dispatch_sizes(tmp_path):
    kernel=ROOT/'external/ts3FPrecomp/src/kernel';bridge=(kernel/'kernel_bridge.c').read_text(encoding='utf-8');cmake=(kernel/'CMakeLists.txt').read_text(encoding='utf-8')
    assert cmake.count('${TSFP_SHARED_SOURCE}/runtime/guest_stack_trace.c')==1;assert bridge.count('#include "guest_kernel_stack_trace.inc"')==1
    assertions=''
    for ordinal,name,size in ((264,'RtlAssert',16),(266,'RtlCaptureStackBackTrace',16),(288,'RtlGetCallersAddress',8),(319,'RtlWalkFrameChain',12)):
        assert 'static void bridge_'+name+'(void)' not in bridge
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+bridge_'+name+r';',bridge);assertions+='assert(stdcall_args_for_ordinal('+str(ordinal)+')=='+str(size)+');'
    assert 'tsfp_thread_stack_bounds(&g_guest_threads,esp,low,high)' in (kernel/'xbox_memory_layout.c').read_text(encoding='utf-8')
    start=bridge.index('static int stdcall_args_for_ordinal(');end=bridge.index('static bridge_func_t bridge_for_ordinal(',start);p=tmp_path/'sizes.c';p.write_bytes(('#include <assert.h>\n#include <stdint.h>\ntypedef uint32_t ULONG;\n'+bridge[start:end]+'int main(void){'+assertions+'return 0;}\n').encode())
    r=subprocess.run([str(_compile(tmp_path,p))],capture_output=True,text=True,timeout=15);assert r.returncode==0,r.stdout+r.stderr


@pytest.mark.skipif(os.name!='nt',reason='Actual x86 ntdll EBP walks and stack cleanup')
def test_actual_x86_native_ebp_chains_and_hashes(tmp_path):
    compiler,env=native_compiler(tmp_path);x86=Path(compiler).parent.parent/'x86/cl.exe';assert x86.is_file()
    env=dict(env);env['PATH']=str(x86.parent)+os.pathsep+env['PATH'];env['LIB']=re.sub(r'(?i)([\\/])x64(?=;|$)',r'\1x86',env['LIB']);env['VSCMD_ARG_TGT_ARCH']='x86'
    source=tmp_path/'stack_trace_x86.c';source.write_bytes((ROOT/'tests/compiler'/source.name).read_bytes());native=tmp_path/'native.exe'
    r=subprocess.run([str(x86),'/nologo','/std:c11','/O2','/W4','/WX',str(source),'/Fe:'+str(native)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=90);assert r.returncode==0,r.stdout+r.stderr
    requests=[(0,skip,n,depth) for depth in range(17) for skip in (0,1,2,16,0xffffffff) for n in (0,1,2,8,24)]
    requests += [(1,0,n,depth) for depth in range(17) for n in (0,1,2,8,24)]+[(2,0,2,depth) for depth in range(17)]
    r=subprocess.run([str(native)],input=b''.join(struct.pack('<4I',*a) for a in requests),capture_output=True,timeout=60);assert r.returncode==0,r.stderr.decode(errors='replace');assert len(r.stdout)==144*len(requests)
    vectors=[];want=[]
    for i,(op,skip,n,depth) in enumerate(requests):
        result=struct.unpack_from('<36I',r.stdout,i*144);ram=base(depth)
        for j in range(depth):struct.pack_into('<I',ram,512+j*16+4,0x1100+(j%4)*4)
        args=(skip,n,128,32) if op==0 else (128,n,0,0) if op==1 else (128,132,0,0)
        vector=make(op,(64,512 if depth else 0,0x1000,32,3072,0x1000,0x3000),args,ram);expected=bytearray(vector[3])
        if op<2:
            if result[0]:struct.pack_into('<'+'I'*result[0],expected,128,*result[4:4+result[0]])
            if op==0:struct.pack_into('<I',expected,32,result[1])
        else:struct.pack_into('<II',expected,128,*result[2:4])
        vectors.append(vector);want.append(struct.pack('<6I',result[0] if op<2 else 0,0,0,0,0,0)+expected)
    exe=fixture(tmp_path)
    for bridge in (False,True):
        r=execute(exe,vectors,bridge);assert r.returncode==0,r.stderr.decode(errors='replace');assert r.stdout==b''.join(want)
