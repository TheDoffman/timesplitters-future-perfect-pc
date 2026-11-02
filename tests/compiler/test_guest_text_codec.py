"""Original counted conversions: independent RAM/ownership and native API evidence."""
from pathlib import Path
import ctypes,os,random,re,struct,subprocess,sys
import pytest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
from test_translation_product import _compile
BAD=0xc000000d;FAILED=0xc0000001;SMALL=0x80000005;MEMORY=0xc0000017;LONG=0xc00000f0

def fixture(tmp_path,mutation=None,native=False):
    for name in ('guest_text_codec.c','guest_text_codec.h','host_text_codec.c','host_text_codec.h','guest_kernel_text_codec.inc','guest_allocation.c','guest_allocation.h','guest_virtual_memory.h','text_codec_cli.c'):
        source=ROOT/('tests/compiler' if name=='text_codec_cli.c' else 'src/runtime')/name
        (tmp_path/name).write_bytes(source.read_bytes())
    if mutation:
        name,a,b=mutation;p=tmp_path/name;s=p.read_text(encoding='utf-8');assert s.count(a)==1
        p.write_bytes(s.replace(a,b).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    if not native:flags+=['/DTSFP_TEST_LATIN1_CODEC' if os.name=='nt' else '-DTSFP_TEST_LATIN1_CODEC']
    return _compile(tmp_path,tmp_path/'text_codec_cli.c',extra=[tmp_path/n for n in ('guest_text_codec.c','host_text_codec.c','guest_allocation.c')]+flags)

def span(address,bytes_,write=False,mode=0):
    if not address or not bytes_ or address+bytes_>2**32:raise ValueError
    at=address&0x7fffffff
    if not at or at+bytes_>1024 or (at<496 and at+bytes_>480):raise ValueError
    if write and ((at<416 and at+bytes_>400) or (mode&3==2 and at+bytes_>512)):raise ValueError
    return at

def preload(op,mode):
    if op<2:return [(512,512 if mode&3==1 else 48)] if mode&3 in (1,3) else []
    return [] if mode&3==1 else [(512,48)]

def encoded(source,direction):
    if not direction:return b''.join(bytes([x,0]) for x in source)
    return bytes(x if x<=255 else 63 for x in struct.unpack('<'+'H'*(len(source)//2),source))

def model(args,original):
    op,dest,src,flag,mode=args;ram=bytearray(original);owned=preload(op,mode)
    try:
        if op>=2:
            if not dest:return 1,owned,bytes(ram)
            d=span(dest,8,True,mode);buffer=struct.unpack_from('<I',ram,d+4)[0]
            if not buffer:return 1,owned,bytes(ram)
            remaining=next((a+n-buffer for a,n in owned if a<=buffer<a+n),0)
            if not remaining:return 0,owned,bytes(ram)
            body=span(buffer,remaining,False,mode)
            if d<body+remaining and body<d+8:return 0,owned,bytes(ram)
            if not any(a==buffer for a,n in owned):return 0,owned,bytes(ram)
            owned=[(a,n) for a,n in owned if a!=buffer];ram[d:d+8]=b'\0'*8;return 1,owned,bytes(ram)
        d=span(dest,8,True,mode);s=span(src,8,False,mode);length,maximum,source=struct.unpack_from('<HHI',ram,s)
        oldlen,capacity,buffer=struct.unpack_from('<HHI',ram,d)
        if op and length%2:raise ValueError
        body=bytes(ram[span(source,length,False,mode):][:length]) if length else b''
        if mode&4:return FAILED,owned,bytes(ram)
        if mode&64:return LONG,owned,bytes(ram)
        result=encoded(body,op);term=1 if op else 2
        if len(result)>65535-term:return LONG,owned,bytes(ram)
        used=len(result);status=0
        if not flag&255 and used+term>capacity:
            if not op or not capacity:return SMALL,owned,bytes(ram)
            if mode&16:return FAILED,owned,bytes(ram)
            used=min(used,capacity-1);status=SMALL
        if mode&(8|32):return FAILED,owned,bytes(ram)
        fresh=False
        if flag&255:
            if owned and owned[0][1]==512:return MEMORY,owned,bytes(ram)
            size=max(16,len(result)+term);buffer=560 if owned else 512
            if buffer+size>1024:return MEMORY,owned,bytes(ram)
            capacity=len(result)+term;owned=owned+[(buffer,size)];ram[buffer:buffer+size]=b'\0'*size;fresh=True
        try:
            out=span(buffer,used+term,True,mode)
            if out<d+8 and d<out+used+term:raise ValueError
        except ValueError:
            return BAD,(owned[:-1] if fresh else owned),bytes(ram)
        ram[out:out+used+term]=result[:used]+b'\0'*term;struct.pack_into('<H',ram,d,used)
        if fresh:struct.pack_into('<HI',ram,d+2,capacity,buffer)
        return status,owned,bytes(ram)
    except ValueError:return (BAD if op<2 else 0),owned,bytes(original)

def make(args,ram):
    op,a,b,flag,mode=args;out=bytearray(ram);struct.pack_into('<I',out,64,a)
    if op<2:struct.pack_into('<II',out,68,b,flag)
    return args,bytes(out)

def cases(count=10000):
    rng=random.Random(244)
    # Empty and exact/partial-capacity boundaries; BOOLEAN uses only its byte.
    for op in (0,1):
        for text in (b'',b'A',b'A\0B',bytes([0x80,0xff])):
            source=text if not op else b''.join(bytes([x,0]) for x in text)
            for cap in (0,1,2,3,4,6,8):
                for flag in (0,1,0x100,0x101):
                    ram=bytearray(b'\xa5'*1024);ram[128:128+len(source)]=source
                    struct.pack_into('<HHI',ram,32,len(source),0,128 if source else 0);struct.pack_into('<HHI',ram,16,0x7777,cap,320)
                    yield make((op,16,32,flag,0),ram)
    for i in range(count):
        op=i%4;mode=rng.choice([0,0,0,1,2,3,4,8,16,32,64]);ram=bytearray(rng.randbytes(1024));a=rng.choice([16,16,32,64,400,480,0,0xfffffffc,0x80000010]);src=32;flag=rng.choice([0,1,128,256,257,0xffffffff])
        if op<2:
            length=rng.randrange(81);data=rng.randbytes(length) if not op else rng.randbytes(length//2*2)
            if op:length=len(data)+(1 if i%31==0 else 0)
            ram[128:128+len(data)]=data
            struct.pack_into('<HHI',ram,32,length,rng.randrange(96),rng.choice([128,128,0,400,480,0xfffffff0,0x80000080]))
            at=a&0x7fffffff
            if at and at+8<=1024:struct.pack_into('<HHI',ram,at,0x7777,rng.choice([0,1,2,3,8,32,64,128,65535]),rng.choice([320,320,321,128,130,16,400,480,512,0x80000140,0x80000010,0xfffffffe]))
        else:
            a=rng.choice([16,16,32,400,480,512,520,0,0xfffffffc,0x80000010]);at=a&0x7fffffff
            if at and at+8<=1024:struct.pack_into('<HHI',ram,at,123,456,rng.choice([0,512,512,513,1023,320,0x80000200]))
        yield make((op,a,src,flag,mode),ram)

def execute(exe,vectors,bridge):
    return subprocess.run([str(exe),*(['bridge'] if bridge else [])],input=b''.join(struct.pack('<5I',*a)+ram for a,ram in vectors),capture_output=True,timeout=60)

def agrees(exe):
    vectors=list(cases())
    for bridge in (False,True):
        run=execute(exe,vectors,bridge)
        if run.returncode or len(run.stdout)!=1040*len(vectors):return False
        for i,(args,ram) in enumerate(vectors):
            value,owned,out=model(args,ram)
            if bridge and args[0]>=2:value=0
            expected=struct.pack('<4I',value,len(owned),owned[0][0] if owned else 0,owned[0][1] if owned else 0)+out
            if run.stdout[i*1040:(i+1)*1040]!=expected:return False
    return True

def test_original_counted_services_and_ownership(tmp_path):
    assert agrees(fixture(tmp_path))

@pytest.mark.parametrize('mutation',[
 ('guest_text_codec.c','length=text_read(src,2);','length=text_read(src+2,2);'),
 ('guest_text_codec.c','needed+terminator>capacity','needed>capacity'),
 ('guest_text_codec.c','memset(output+used,0,terminator);','memset(output+used,0x7f,terminator);'),
 ('guest_text_codec.c','text_write(dst,2,used);','text_write(dst,2,used+terminator);'),
 ('guest_text_codec.c','text_write(dst+4,4,buffer);owned=0;','text_write(dst+4,4,buffer);owned=buffer;'),
 ('guest_text_codec.c','(void)tsfp_heap_release(vm->allocations,owned);','(void)tsfp_allocation_remaining(vm->allocations,owned);'),
 ('guest_text_codec.c','if(!body || text_overlap(dst,8,body,actual))','if(!body)'),
 ('guest_kernel_text_codec.inc','(uint8_t)a[2],direction','(uint8_t)(a[2]>>8),direction'),
 ('guest_text_codec.c','if(!buffer)return 1;','if(!buffer){memset(record,0,8);return 1;}'),
 ('guest_text_codec.c','if(!tsfp_heap_release(vm->allocations,buffer))','if(!tsfp_allocation_remaining(vm->allocations,buffer))'),
])
def test_compiled_bad_converters_and_ownership_are_rejected(tmp_path,mutation):
    assert not agrees(fixture(tmp_path,mutation))


@pytest.mark.skipif(os.name!='nt',reason='Actual Windows ntdll locale/record oracle')
def test_actual_windows_counted_string_oracle(tmp_path):
    class Record(ctypes.Structure):_fields_=[('length',ctypes.c_ushort),('maximum',ctypes.c_ushort),('buffer',ctypes.c_void_p)]
    dll=ctypes.WinDLL('ntdll');convert=[dll.RtlAnsiStringToUnicodeString,dll.RtlUnicodeStringToAnsiString];release=[dll.RtlFreeUnicodeString,dll.RtlFreeAnsiString]
    for f in convert:f.argtypes=[ctypes.POINTER(Record),ctypes.POINTER(Record),ctypes.c_ubyte];f.restype=ctypes.c_uint32
    for f in release:f.argtypes=[ctypes.POINTER(Record)];f.restype=None
    rng=random.Random(24401);vectors=[];expected=[]
    for i in range(6000):
        op=i%2;count=rng.randrange(33);data=rng.randbytes(count) if not op else rng.randbytes(count*2)
        flag=rng.choice([0,1,0x100,0x101]);cap=rng.choice([0,1,2,3,4,8,16,32,64,128,65535]);term=1 if op else 2
        ram=bytearray(rng.randbytes(1024));ram[128:128+len(data)]=data
        struct.pack_into('<HHI',ram,32,len(data),rng.randrange(96),128 if data else 0);struct.pack_into('<HHI',ram,16,0x7777,cap,320)
        args,ram=make((op,16,32,flag,0),ram);out=bytearray(ram)
        native_source=ctypes.create_string_buffer(data or b'\0');native_body=ctypes.create_string_buffer(ram[320:])
        source=Record(len(data),struct.unpack_from('<H',ram,34)[0],ctypes.addressof(native_source) if data else None)
        dest=Record(0x7777,cap,ctypes.addressof(native_body));status=convert[op](ctypes.byref(dest),ctypes.byref(source),flag&255)
        owner=[];buffer=320
        if flag&255:
            assert status==0 and dest.buffer
            size=max(16,dest.maximum);owner=[(512,size)];buffer=512
            out[512:512+size]=b'\0'*size;out[512:512+dest.maximum]=ctypes.string_at(dest.buffer,dest.maximum)
        else:
            assert dest.buffer==ctypes.addressof(native_body);out[320:]=native_body.raw[:704]
        struct.pack_into('<HHI',out,16,dest.length,dest.maximum,buffer)
        expected.append(struct.pack('<4I',status,len(owner),owner[0][0] if owner else 0,owner[0][1] if owner else 0)+out)
        vectors.append((args,ram))
        if flag&255:release[op](ctypes.byref(dest))
    exe=fixture(tmp_path,native=True);want=b''.join(expected)
    for bridge in (False,True):
        run=execute(exe,vectors,bridge);assert run.returncode==0,run.stderr.decode(errors='replace');assert len(run.stdout)==len(want)
        for i in range(len(vectors)):assert run.stdout[i*1040:(i+1)*1040]==want[i*1040:(i+1)*1040],(bridge,i,vectors[i][0])


def test_counted_limits_and_null_contracts(tmp_path):
    for name in ('guest_text_codec.c','guest_text_codec.h','host_text_codec.c','host_text_codec.h','guest_allocation.c','guest_allocation.h','guest_virtual_memory.h','text_codec_limits.c'):
        source=ROOT/('tests/compiler' if name=='text_codec_limits.c' else 'src/runtime')/name
        (tmp_path/name).write_bytes(source.read_bytes())
    flags=['/W4','/DTSFP_TEST_LATIN1_CODEC'] if os.name=='nt' else ['-Wall','-Wextra','-DTSFP_TEST_LATIN1_CODEC']
    exe=_compile(tmp_path,tmp_path/'text_codec_limits.c',extra=[tmp_path/n for n in ('guest_text_codec.c','host_text_codec.c','guest_allocation.c')]+flags)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30);assert run.returncode==0,run.stdout+run.stderr


@pytest.mark.skipif(os.name!='nt',reason='Windows standard NLS code-page adapters')
@pytest.mark.parametrize('codepage,text',[(1252,'AĀé€B\0z'),(932,'A漢字あカB\0z'),(65001,'A€漢😀B\0z')])
def test_windows_codec_multibyte_prefix_boundaries(tmp_path,codepage,text):
    kernel=ctypes.WinDLL('kernel32');to_wide=kernel.MultiByteToWideChar;to_bytes=kernel.WideCharToMultiByte
    to_wide.argtypes=[ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_int,ctypes.c_void_p,ctypes.c_int];to_wide.restype=ctypes.c_int
    to_bytes.argtypes=[ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_int,ctypes.c_void_p,ctypes.c_int,ctypes.c_void_p,ctypes.c_void_p];to_bytes.restype=ctypes.c_int
    def convert(data,op):
        if not data:return b''
        source=ctypes.create_string_buffer(data)
        if not op:
            n=to_wide(codepage,0,source,len(data),None,0);assert n>0;out=ctypes.create_string_buffer(n*2);assert to_wide(codepage,0,source,len(data),out,n)==n;return out.raw
        n=to_bytes(codepage,0,source,len(data)//2,None,0,None,None);assert n>0;out=ctypes.create_string_buffer(n);assert to_bytes(codepage,0,source,len(data)//2,out,n,None,None)==n;return out.raw
    wide=text.encode('utf-16le');narrow=convert(wide,1);vectors=[];outputs=[]
    for op,data in ((0,narrow),(1,wide),(0,b''),(1,b'')):
        converted=convert(data,op);term=1 if op else 2
        for cap in range(len(converted)+3):
            for flag in (0,1,0x100,0x101):
                ram=bytearray(b'\xa5'*1024);ram[128:128+len(data)]=data;struct.pack_into('<HHI',ram,32,len(data),0,128 if data else 0);struct.pack_into('<HHI',ram,16,0x7777,cap,320)
                args,ram=make((op,16,32,flag,0),ram);expected=bytearray(ram);status=0;used=len(converted);owner=[]
                if not flag&255 and used+term>cap:
                    status=SMALL
                    if not op or not cap:
                        outputs.append(struct.pack('<4I',status,0,0,0)+expected);vectors.append((args,ram));continue
                    # Enumerate logical characters rather than binary-searching units.
                    # All source samples here contain valid complete Unicode scalars.
                    candidates=[convert(text[:i].encode('utf-16le'),1) for i in range(len(text)+1)] if data else [b'']
                    fits=[candidate for candidate in candidates if len(candidate)<=cap-1]
                    used=max(map(len,fits))
                at=512 if flag&255 else 320
                if flag&255:
                    size=max(16,len(converted)+term);owner=[(512,size)];expected[at:at+size]=b'\0'*size
                expected[at:at+used+term]=converted[:used]+b'\0'*term;struct.pack_into('<H',expected,16,used)
                if flag&255:struct.pack_into('<HI',expected,18,len(converted)+term,512)
                outputs.append(struct.pack('<4I',status,len(owner),owner[0][0] if owner else 0,owner[0][1] if owner else 0)+expected);vectors.append((args,ram))
    exe=fixture(tmp_path,native=True);payload=b''.join(struct.pack('<5I',*a)+ram for a,ram in vectors);want=b''.join(outputs)
    for bridge in (False,True):
        run=subprocess.run([str(exe),'bridge' if bridge else 'direct',str(codepage)],input=payload,capture_output=True,timeout=45)
        assert run.returncode==0,run.stderr.decode(errors='replace');assert len(run.stdout)==len(want)
        for i in range(len(vectors)):assert run.stdout[i*1040:(i+1)*1040]==want[i*1040:(i+1)*1040],(codepage,bridge,i,vectors[i][0])


def test_selected_counted_imports_and_dispatch_sizes(tmp_path):
    kernel=ROOT/'external/ts3FPrecomp/src/kernel';bridge=(kernel/'kernel_bridge.c').read_text(encoding='utf-8');cmake=(kernel/'CMakeLists.txt').read_text(encoding='utf-8')
    for name in ('guest_text_codec','host_text_codec'):assert cmake.count('${TSFP_SHARED_SOURCE}/runtime/'+name+'.c')==1
    assert bridge.count('#include "guest_kernel_text_codec.inc"')==1
    assertions=''
    for ordinal,name,size in ((260,'RtlAnsiStringToUnicodeString',12),(308,'RtlUnicodeStringToAnsiString',12),(286,'RtlFreeAnsiString',4),(287,'RtlFreeUnicodeString',4)):
        assert 'static void bridge_'+name+'(void)' not in bridge
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+bridge_'+name+r';',bridge)
        assertions+='assert(stdcall_args_for_ordinal('+str(ordinal)+')=='+str(size)+');'
    start=bridge.index('static int stdcall_args_for_ordinal(');end=bridge.index('static bridge_func_t bridge_for_ordinal(',start)
    source=tmp_path/'sizes.c';source.write_bytes(('#include <stdint.h>\n#include <assert.h>\ntypedef uint32_t ULONG;\n'+bridge[start:end]+'int main(void){'+assertions+'return 0;}\n').encode())
    run=subprocess.run([str(_compile(tmp_path,source))],capture_output=True,text=True,timeout=15);assert run.returncode==0,run.stdout+run.stderr
