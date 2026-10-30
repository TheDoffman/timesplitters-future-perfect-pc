"""Counted guest strings: independent byte models, native oracles and bad-code controls."""
from pathlib import Path
import os,random,struct,subprocess,sys,re
import pytest
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(HERE))
from test_translation_product import _compile

def u16(ram,at):return struct.unpack_from('<H',ram,at)[0]
def record(ram,at):return struct.unpack_from('<HHI',ram,at)
def put_record(ram,at,length,maximum,buffer):struct.pack_into('<HHI',ram,at,length,maximum,buffer)
def upper(value):return value-32 if 97<=value<=122 else value
def units(data,width):return list(data) if width==1 else list(struct.unpack('<'+'H'*(len(data)//2),data))

def expected_model(op,width,a,b,c,initial):
    out=bytearray(initial);result=0
    dl,dm,db=record(out,a) if op<6 else (0,0,0)
    if op==0:
        length=0
        if b:
            while any(out[b+length:b+length+width]):length+=width
        put_record(out,a,length,length+width if b else 0,b)
    elif op==1:
        sl,_,sb=record(out,b);left=units(out[db:db+dl],width);right=units(out[sb:sb+sl],width)
        result=dl//width-sl//width
        for x,y in zip(left,right):
            if c:x,y=upper(x),upper(y)
            if x!=y:result=x-y;break
    elif op in (2,3,4,5):
        if op==2 and not b:struct.pack_into('<H',out,a,0);return 0,out
        if op==5:
            if not b:return 0,out
            sb=b;sl=0
            while any(out[sb+sl:sb+sl+2]):sl+=2
        else:sl,_,sb=record(out,b)
        offset=dl if op in (3,5) else 0
        if offset+sl>dm and op in (3,5):return 0xc0000023,out
        if not sl and op==3:return 0,out
        count=sl if op in (3,5) else min(sl,dm)
        data=bytes(out[sb:sb+count])
        if op==4:data=bytes(upper(x) for x in data)
        out[db+offset:db+offset+count]=data
        total=offset+count
        if width==2 and dm-total>=2:out[db+total:db+total+2]=b'\0\0'
        struct.pack_into('<H',out,a,total)
    elif op==6:
        while result<c and out[a+result]==out[b+result]:result+=1
    elif op==7:
        while result<(b//4)*4 and struct.unpack_from('<I',out,a+result)[0]==c:result+=4
    return result&0xffffffff,out

def native_model(op,width,a,b,c,initial):
    import ctypes as ct
    class S(ct.Structure):_fields_=[('length',ct.c_uint16),('maximum',ct.c_uint16),('buffer',ct.c_void_p)]
    ram=(ct.c_ubyte*512).from_buffer_copy(initial);base=ct.addressof(ram);n=ct.WinDLL('ntdll')
    def string(at):
        length,maximum,buffer=record(initial,at);return S(length,maximum,base+buffer if buffer else None)
    result=0;d=string(a) if op<6 else None;s=string(b) if op in (1,2,3,4) and b else None
    if op==0:
        f=n.RtlInitUnicodeString if width==2 else n.RtlInitAnsiString;f.argtypes=[ct.POINTER(S),ct.c_void_p];f.restype=None
        f(ct.byref(d),base+b if b else None)
        out=bytearray(ram);put_record(out,a,d.length,d.maximum,(d.buffer-base) if d.buffer else 0);return 0,out
    if op==1:
        f=n.RtlCompareUnicodeString if width==2 else n.RtlCompareString;f.argtypes=[ct.POINTER(S),ct.POINTER(S),ct.c_ubyte];f.restype=ct.c_int32
        result=f(ct.byref(d),ct.byref(s),c)
    elif op in (2,3,4,5):
        if op==2:f=n.RtlCopyUnicodeString if width==2 else n.RtlCopyString
        elif op==3:f=n.RtlAppendUnicodeStringToString if width==2 else n.RtlAppendStringToString
        elif op==4:f=n.RtlUpperString
        else:f=n.RtlAppendUnicodeToString
        f.argtypes=[ct.POINTER(S),ct.c_void_p if op==5 else ct.POINTER(S)]
        f.restype=ct.c_uint32 if op in (3,5) else None
        argument=(base+b if b else None) if op==5 else (ct.byref(s) if s else None)
        value=f(ct.byref(d),argument);result=value or 0
        out=bytearray(ram);struct.pack_into('<H',out,a,d.length);return result,out
    elif op==6:
        f=n.RtlCompareMemory;f.argtypes=[ct.c_void_p,ct.c_void_p,ct.c_size_t];f.restype=ct.c_size_t
        result=f(base+a,base+b,c)
    else:
        f=n.RtlCompareMemoryUlong;f.argtypes=[ct.c_void_p,ct.c_size_t,ct.c_uint32];f.restype=ct.c_size_t
        result=f(base+a,b,c)
    return result&0xffffffff,bytearray(ram)

@pytest.fixture(scope='module')
def strings_vectors():
    rng=random.Random(237);rows=[]
    for case in range(20000 if os.name=='nt' else 14000):
        native=case>=14000;op=case%8;width=1+((case//8)%2)
        if op==4:width=1
        if op==5:width=2
        ram=bytearray(rng.randbytes(512));a,b,c=17,33,rng.randrange(2)
        dl=rng.randrange(41)*width;dm=max(dl,rng.randrange(41)*width);sl=rng.randrange(41)*width
        put_record(ram,17,dl,dm,65);put_record(ram,33,sl,sl if case%3 else 0,257)
        # Case-insensitive and upper vectors are ASCII. Sensitive Unicode vectors
        # cover the full code-unit range, including NUL and isolated surrogates.
        alphabet=128 if c or op==4 else (256 if width==1 else 65536)
        for start,length in ((65,dl),(257,sl)):
            data=[rng.randrange(alphabet) for _ in range(length//width)]
            payload=bytes(data) if width==1 else struct.pack('<'+'H'*len(data),*data)
            ram[start:start+length]=payload
        if op in (0,5):
            b=257 if case%11 else 0
            ram[257+sl:257+sl+width]=bytes(width)
        if op==2 and case%13==0:b=0
        if op==6:
            a,b,c=65,257,rng.randrange(100);ram[a:a+c]=ram[b:b+c]
            if c and case%3:ram[a+rng.randrange(c)]^=1
        if op==7:
            a,b,c=65,rng.randrange(101),rng.getrandbits(32)
            ram[a:a+100]=struct.pack('<I',c)*25
            if b>=4 and case%3:ram[a+4*rng.randrange(b//4)]^=1
        expected,out=(native_model if native else expected_model)(op,width,a,b,c,ram)
        rows.append(struct.pack('<8I',op,width,a,b,c,expected,1,0)+ram+out)
    return b''.join(rows)

def fixture(tmp_path,name,mutation,vectors=b''):
    for n in ('guest_strings.c','guest_strings.h','guest_virtual_memory.h','guest_allocation.h','guest_kernel_strings.inc',name):
        p=HERE/n
        if not p.is_file():p=ROOT/'src/runtime'/n
        (tmp_path/n).write_bytes(p.read_bytes())
    if mutation:
        filename,a,b=mutation;p=tmp_path/filename;s=p.read_text(encoding='utf-8');assert s.count(a)==1
        p.write_bytes(s.replace(a,b).encode('utf-8'))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/name,extra=[tmp_path/'guest_strings.c',*flags])
    run=subprocess.run([str(exe)],input=vectors,capture_output=True,timeout=40)
    if mutation:assert run.returncode!=0,'Broken counted-string implementation passed'
    else:assert run.returncode==0,run.stdout.decode()+run.stderr.decode()

@pytest.mark.parametrize('mutation',[None,
    ('guest_strings.c','if(x!=y)','if(!x || x!=y)'),
    ('guest_strings.c','for(i=0;i<common;i+=width)','for(i=0;i<common;++i)'),
    ('guest_strings.c','if(s.length>d.maximum-offset)return STRING_SMALL;','if(s.length>d.maximum-offset)return 0;'),
    ('guest_strings.c','memmove(body,input,copy)','memmove(body,input+1,copy)'),
    ('guest_strings.c','if(width==2 && d.maximum-total>=2)','if(width==2 && d.maximum-total>2)'),
    ('guest_strings.c','if(a[i]!=b[i])break;','if(a[i]==b[i])break;'),
])
def test_original_counted_strings(tmp_path,mutation,strings_vectors):
    fixture(tmp_path,'strings_contract.c',mutation,strings_vectors)

@pytest.mark.parametrize('mutation',[None,
    ('guest_kernel_strings.inc','bridge_string_initialize(2)','bridge_string_initialize(1)'),
    ('guest_kernel_strings.inc','bridge_string_compare(1,1)','bridge_string_compare(1,0)'),
    ('guest_kernel_strings.inc','bridge_string_update(2,TSFP_STRING_APPEND,1)','bridge_string_update(2,TSFP_STRING_COPY,1)'),
    ('guest_kernel_strings.inc','STACK_ARG(2);tsfp_guest_vm vm;int32_t','STACK_ARG(1);tsfp_guest_vm vm;int32_t'),
    ('guest_kernel_strings.inc','g_eax=status?result:0','g_eax=status?0:result'),
])
def test_original_string_imports(tmp_path,mutation):
    fixture(tmp_path,'kernel_strings_contract.c',mutation)

def test_selected_counted_string_retirement():
    rt=ROOT/'external/ts3FPrecomp';bridge=(rt/'src/kernel/kernel_bridge.c').read_text(encoding='utf-8')
    assert bridge.count('#include "guest_kernel_strings.inc"')==1
    assert (rt/'src/kernel/CMakeLists.txt').read_text(encoding='utf-8').count('${TSFP_SHARED_SOURCE}/runtime/guest_strings.c')==1
    imports=((268,12,'CompareMemory'),(269,12,'CompareMemoryUlong'),(270,12,'CompareString'),(271,12,'CompareUnicodeString'),
             (279,12,'EqualString'),(280,12,'EqualUnicodeString'),(289,8,'InitAnsiString'),(290,8,'InitUnicodeString'),
             (261,8,'AppendStringToString'),(262,8,'AppendUnicodeStringToString'),(263,8,'AppendUnicodeToString'),
             (272,8,'CopyString'),(273,8,'CopyUnicodeString'),(317,8,'UpperString'))
    for ordinal,bytes_,name in imports:
        assert 'static void bridge_Rtl'+name+'(void)' not in bridge
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+'+str(bytes_)+r';\s*/\*\s*Rtl'+name+r'\b',bridge)
    retired=re.compile(r'\bxbox_Rtl(?:InitAnsiString|InitUnicodeString|AnsiStringToUnicodeString|UnicodeStringToAnsiString|EqualString|CompareMemoryUlong)\b')
    for base in (rt/'src',rt/'titles/futureperfect/src',ROOT/'src'):
        for p in base.rglob('*'):
            if p.suffix not in ('.c','.h','.inc','.cpp') or p.name=='kernel_thunks.c':continue
            assert not retired.search(p.read_text(encoding='utf-8',errors='replace')),p
