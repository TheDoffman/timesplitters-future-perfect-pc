"""Original creation/case: complete guest RAM, ownership and native ABI contracts."""
from pathlib import Path
import ctypes,os,random,re,struct,subprocess,sys
import pytest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
from test_translation_product import _compile
BAD=0xc000000d;SMALL=0x80000005;MEMORY=0xc0000017

def fixture(tmp_path,mutation=None,native=False):
    for name in ('guest_string_transform.c','guest_string_transform.h','host_string_case.c','host_string_case.h','guest_kernel_string_transform.inc','guest_allocation.c','guest_allocation.h','guest_virtual_memory.h','string_transform_cli.c'):
        p=ROOT/('tests/compiler' if name=='string_transform_cli.c' else 'src/runtime')/name;(tmp_path/name).write_bytes(p.read_bytes())
    if mutation:
        name,a,b=mutation;p=tmp_path/name;s=p.read_text(encoding='utf-8');assert s.count(a)==1;p.write_bytes(s.replace(a,b).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    if not native:flags+=['/DTSFP_TEST_ASCII_CASE' if os.name=='nt' else '-DTSFP_TEST_ASCII_CASE']
    return _compile(tmp_path,tmp_path/'string_transform_cli.c',extra=[tmp_path/n for n in ('guest_string_transform.c','host_string_case.c','guest_allocation.c')]+flags)

def span(address,n,write=False,mode=0):
    if not address or not n or address+n>2**32:raise ValueError
    at=address&0x7fffffff
    if not at or at+n>1024 or at<496 and at+n>480:raise ValueError
    if write and (at<416 and at+n>400 or mode&3==2 and at+n>512):raise ValueError
    return at

def unit(value,width,upper):
    value&=255 if width==1 else 65535
    return value-32 if upper and 97<=value<=122 else value+32 if not upper and 65<=value<=90 else value

def model(args,original):
    op,dest,source,flag,mode=args;ram=bytearray(original);owned=[(512,512 if mode&3==1 else 48)] if mode&3 in (1,3) else []
    if op>=3:return unit(dest,1 if op<5 else 2,op in (3,5)),owned,bytes(ram)
    try:
        d=span(dest,8,True,mode);oldlen,cap,buf=struct.unpack_from('<HHI',ram,d)
        if op==0:
            data=bytearray();offset=0
            while True:
                at=span(source+offset,2,False,mode);item=bytes(ram[at:at+2]);data+=item
                if item==b'\0\0':break
                if offset==65532:return 0,owned,bytes(ram)
                offset+=2
            span(source,len(data),False,mode);length=len(data)-2;allocate=True
        else:
            s=span(source,8,False,mode);length,maximum,sbuf=struct.unpack_from('<HHI',ram,s)
            if length%2:raise ValueError
            allocate=bool(flag&255)
            if not allocate and length>cap:return SMALL,owned,bytes(ram)
            at=span(sbuf,length,False,mode) if length else 0
            values=struct.unpack('<'+'H'*(length//2),ram[at:at+length]) if length else ()
            data=struct.pack('<'+'H'*len(values),*(unit(v,2,op==1) for v in values))
        fresh=False
        if allocate:
            if owned and owned[0][1]==512:return (0 if not op else MEMORY),owned,bytes(ram)
            size=max(16,len(data));buf=560 if owned else 512
            if buf+size>1024:return (0 if not op else MEMORY),owned,bytes(ram)
            owned=owned+[(buf,size)];ram[buf:buf+size]=b'\0'*size;fresh=True
        try:
            if data:
                at=span(buf,len(data),True,mode)
                if at<d+8 and d<at+len(data):raise ValueError
                ram[at:at+len(data)]=data
        except ValueError:
            if fresh:ram[d:d+8]=original[d:d+8]
            return (0 if not op else BAD),owned[:-1] if fresh else owned,bytes(ram)
        struct.pack_into('<H',ram,d,length)
        if allocate:struct.pack_into('<HI',ram,d+2,len(data),buf)
        return (1 if not op else 0),owned,bytes(ram)
    except (ValueError,struct.error):return (0 if not op else BAD),owned,bytes(original)

def make(args,ram):
    op,a,b,flag,mode=args;ram=bytearray(ram);struct.pack_into('<I',ram,64,a)
    if op<3:struct.pack_into('<I',ram,68,b)
    if op in (1,2):struct.pack_into('<I',ram,72,flag)
    return args,bytes(ram)

def cases(count=10000):
    rng=random.Random(245)
    for op in range(3):
        for data in (b'',b'a\0B\0',b'a\0\0\0B\0',b'\xff\xff\x00\xd8'):
            for cap in range(9):
                for flag in (0,1,0x100,0x101):
                    ram=bytearray(b'\xa5'*1024);ram[128:128+len(data)+2]=data+b'\0\0'
                    struct.pack_into('<HHI',ram,32,len(data),0,128 if data else 0);struct.pack_into('<HHI',ram,16,123,cap,320)
                    yield make((op,16,128 if op==0 else 32,flag,0),ram)
    for i in range(count):
        op=i%7;mode=rng.choice((0,0,0,1,2,3));ram=bytearray(rng.randbytes(1024));dest=rng.choice((16,16,32,64,400,480,512,0,0xfffffffc,0x80000010));source=32;flag=rng.choice((0,1,128,256,257,0xffffffff))
        if op<3:
            length=rng.randrange(0,81)//2*2;data=rng.randbytes(length);ram[128:130+length]=data+b'\0\0'
            if op==0:source=rng.choice((128,128,128,0,400,480,0xfffffff0,0x80000080))
            else:struct.pack_into('<HHI',ram,32,length+(i%31==0),rng.randrange(128),rng.choice((128,128,128,0,400,480,0xfffffff0,0x80000080)))
            at=dest&0x7fffffff
            if at and at+8<=1024:struct.pack_into('<HHI',ram,at,123,rng.choice((0,1,2,3,8,32,64,128,65535)),rng.choice((320,320,321,128,130,16,400,480,512,0x80000140,0x80000010,0xfffffffe)))
        else:dest=rng.getrandbits(32)
        yield make((op,dest,source,flag,mode),ram)

def execute(exe,vectors,bridge):
    return subprocess.run([str(exe),*(['bridge'] if bridge else [])],input=b''.join(struct.pack('<5I',*a)+ram for a,ram in vectors),capture_output=True,timeout=60)

def agrees(exe):
    vectors=list(cases())
    for bridge in (False,True):
        run=execute(exe,vectors,bridge)
        if run.returncode or len(run.stdout)!=1040*len(vectors):return False
        for i,(args,ram) in enumerate(vectors):
            status,owned,out=model(args,ram);want=struct.pack('<4I',status,len(owned),owned[0][0] if owned else 0,owned[0][1] if owned else 0)+out
            if run.stdout[i*1040:(i+1)*1040]!=want:return False
    return True

def test_original_creation_and_case_ram_ownership(tmp_path):assert agrees(fixture(tmp_path))

@pytest.mark.parametrize('mutation',[
 ('guest_string_transform.c','length=transform_read(src,2);','length=transform_read(src+2,2);'),
 ('guest_string_transform.c','length,length,allocate','length,length+2,allocate'),
 ('guest_string_transform.c','transform_write(record,2,length);','transform_write(record,2,length+2);'),
 ('guest_string_transform.c','return TRANSFORM_SMALL;','return TRANSFORM_MEMORY;'),
 ('guest_string_transform.c','(void)tsfp_heap_release(vm->allocations,owned);','(void)tsfp_allocation_remaining(vm->allocations,owned);'),
 ('guest_string_transform.c','transform_overlap(record,8,body,bytes)','transform_overlap(record,0,body,bytes)'),
 ('guest_kernel_string_transform.inc','(uint8_t)a[2],upper','(uint8_t)(a[2]>>8),upper'),
 ('host_string_case.c',"return value-32;","return value-31;"),
])
def test_compiled_bad_creation_case_or_ownership_rejected(tmp_path,mutation):assert not agrees(fixture(tmp_path,mutation))

@pytest.mark.skipif(os.name!='nt',reason='Actual Windows ntdll counted ASCII case and creation references')
def test_native_creation_and_ascii_case_records(tmp_path):
    class Record(ctypes.Structure):_fields_=[('length',ctypes.c_ushort),('maximum',ctypes.c_ushort),('buffer',ctypes.c_void_p)]
    dll=ctypes.WinDLL('ntdll');create=dll.RtlCreateUnicodeString;create.argtypes=[ctypes.POINTER(Record),ctypes.c_void_p];create.restype=ctypes.c_ubyte
    functions=[dll.RtlUpcaseUnicodeString,dll.RtlDowncaseUnicodeString]
    for f in functions:f.argtypes=[ctypes.POINTER(Record),ctypes.POINTER(Record),ctypes.c_ubyte];f.restype=ctypes.c_uint32
    release=dll.RtlFreeUnicodeString;release.argtypes=[ctypes.POINTER(Record)];release.restype=None
    rng=random.Random(245);vectors=[];want=[]
    for i in range(6000):
        op=i%3;text=bytes(rng.choice(b'aBcD09-\0') for _ in range(rng.randrange(40)));data=b''.join(bytes((x,0)) for x in text)
        flag=rng.choice((0,1,0x100,0x101));cap=rng.randrange(84);ram=bytearray(b'\xa5'*1024);ram[128:130+len(data)]=data+b'\0\0';struct.pack_into('<HHI',ram,32,len(data),0,128);struct.pack_into('<HHI',ram,16,123,cap,320)
        a,ram=make((op,16,128 if op==0 else 32,flag,0),ram);expected=bytearray(ram)
        native_source=ctypes.create_string_buffer(data+b'\0\0');source=Record(len(data),0,ctypes.addressof(native_source));body=ctypes.create_string_buffer(b'\xa5'*100);dest=Record(123,cap,ctypes.addressof(body))
        allocated=op==0 or bool(flag&255);status=create(ctypes.byref(dest),native_source) if op==0 else functions[op-1](ctypes.byref(dest),ctypes.byref(source),flag&255)
        owner=[]
        if status==(1 if op==0 else 0):
            n=dest.maximum if op==0 else dest.length;buf=512 if allocated else 320
            if allocated:
                size=max(16,n);owner=[(512,size)];expected[buf:buf+size]=b'\0'*size
            expected[buf:buf+n]=ctypes.string_at(dest.buffer,n);struct.pack_into('<H',expected,16,dest.length)
            if allocated:struct.pack_into('<HI',expected,18,dest.maximum,512)
        if allocated:release(ctypes.byref(dest))
        want.append(struct.pack('<4I',int(status),len(owner),owner[0][0] if owner else 0,owner[0][1] if owner else 0)+expected);vectors.append((a,ram))
    exe=fixture(tmp_path,native=True)
    for bridge in (False,True):
        run=execute(exe,vectors,bridge);assert run.returncode==0,run.stderr.decode(errors='replace');assert run.stdout==b''.join(want)


def test_selected_transform_imports_and_dispatch(tmp_path):
    kernel=ROOT/'external/ts3FPrecomp/src/kernel';bridge=(kernel/'kernel_bridge.c').read_text(encoding='utf-8');cmake=(kernel/'CMakeLists.txt').read_text(encoding='utf-8')
    for name in ('guest_string_transform','host_string_case'):assert cmake.count('${TSFP_SHARED_SOURCE}/runtime/'+name+'.c')==1
    assert bridge.count('#include "guest_kernel_string_transform.inc"')==1;assert 'bridge_guest_wcslen_bytes' not in bridge
    assertions=''
    for ordinal,name,size in ((274,'RtlCreateUnicodeString',8),(275,'RtlDowncaseUnicodeChar',4),(276,'RtlDowncaseUnicodeString',12),(296,'RtlLowerChar',4),(313,'RtlUpcaseUnicodeChar',4),(314,'RtlUpcaseUnicodeString',12),(316,'RtlUpperChar',4)):
        assert 'static void bridge_'+name+'(void)' not in bridge
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+bridge_'+name+r';',bridge)
        assertions+='assert(stdcall_args_for_ordinal('+str(ordinal)+')=='+str(size)+');'
    start=bridge.index('static int stdcall_args_for_ordinal(');end=bridge.index('static bridge_func_t bridge_for_ordinal(',start);source=tmp_path/'sizes.c'
    source.write_bytes(('#include <stdint.h>\n#include <assert.h>\ntypedef uint32_t ULONG;\n'+bridge[start:end]+'int main(void){'+assertions+'return 0;}\n').encode())
    run=subprocess.run([str(_compile(tmp_path,source))],capture_output=True,text=True,timeout=15);assert run.returncode==0,run.stdout+run.stderr


@pytest.mark.parametrize('mutation',[None,('if(length==65532)return 0;','if(length==65534)return 0;')])
def test_maximum_lengths_real_free_and_standard_leaf(tmp_path,mutation):
    for name in ('guest_string_transform.c','guest_string_transform.h','host_string_case.c','host_string_case.h','guest_text_codec.c','guest_text_codec.h','guest_allocation.c','guest_allocation.h','guest_virtual_memory.h','string_transform_limits.c'):
        p=ROOT/('tests/compiler' if name=='string_transform_limits.c' else 'src/runtime')/name;(tmp_path/name).write_bytes(p.read_bytes())
    if mutation:
        p=tmp_path/'guest_string_transform.c';s=p.read_text(encoding='utf-8');assert s.count(mutation[0])==1;p.write_bytes(s.replace(*mutation).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'string_transform_limits.c',extra=[tmp_path/n for n in ('guest_string_transform.c','host_string_case.c','guest_text_codec.c','guest_allocation.c')]+flags)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert (run.returncode==0)==(mutation is None),run.stdout+run.stderr
