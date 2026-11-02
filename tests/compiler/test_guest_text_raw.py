"""Raw charset policy: independent RAM model, real imports and native references."""
from pathlib import Path
import ctypes,os,random,re,struct,subprocess,sys
import pytest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
from test_translation_product import _compile
BAD=0xc000000d;FAILED=0xc0000001;OVERFLOW=0x80000005

def fixture(tmp_path,mutation=None,native=False):
    for n in ('guest_text_raw.c','guest_text_raw.h','guest_text_codec.h','guest_string_transform.h','host_text_codec.c','host_text_codec.h','host_string_case.c','host_string_case.h','guest_kernel_text_raw.inc','guest_allocation.h','guest_virtual_memory.h','text_raw_cli.c'):
        p=ROOT/('tests/compiler' if n=='text_raw_cli.c' else 'src/runtime')/n;(tmp_path/n).write_bytes(p.read_bytes())
    if mutation:
        name,a,b=mutation;p=tmp_path/name;s=p.read_text(encoding='utf-8');assert s.count(a)==1;p.write_bytes(s.replace(a,b).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    flags+=['/DTSFP_TEST_ASCII_CASE' if os.name=='nt' else '-DTSFP_TEST_ASCII_CASE']
    if not native:flags+=['/DTSFP_TEST_LATIN1_CODEC' if os.name=='nt' else '-DTSFP_TEST_LATIN1_CODEC']
    return _compile(tmp_path,tmp_path/'text_raw_cli.c',extra=[tmp_path/n for n in ('guest_text_raw.c','host_text_codec.c','host_string_case.c')]+flags)

def span(a,n,write=False):
    if not a or not n or a+n>2**32:raise ValueError
    at=a&0x7fffffff
    if not at or at+n>1024 or at<496 and at+n>480:raise ValueError
    if write and at<416 and at+n>400:raise ValueError
    return at

def model(args,original):
    op,out,cap,count,source,length,mode=args;ram=bytearray(original);query=op in (1,3);wide=op>=2
    try:
        if query and not count:raise ValueError
        atcount=span(count,4,True) if count else None
        length=length//2*2 if wide else length
        atsource=span(source,length) if length else 0;data=bytes(ram[atsource:atsource+length]) if length else b''
        if mode&1 or not wide and mode&16:return FAILED,original
        if wide:
            values=struct.unpack('<'+'H'*(length//2),data)
            if op==4:values=[v-32 if 97<=v<=122 else v for v in values]
            result=bytes(v if v<=255 else 63 for v in values)
        else:result=b''.join(bytes((v,0)) for v in data)
        if query:struct.pack_into('<I',ram,atcount,len(result));return 0,bytes(ram)
        used=min(len(result),cap)
        if not wide:used=used//2*2
        elif used<len(result) and mode&4:return FAILED,original
        if used:
            atout=span(out,used,True)
            if atcount is not None and atout<atcount+4 and atcount<atout+used:raise ValueError
            if mode&(2|8|32):return FAILED,original
            ram[atout:atout+used]=result[:used]
        if atcount is not None:struct.pack_into('<I',ram,atcount,used)
        return (OVERFLOW if op==4 and used<len(result) else 0),bytes(ram)
    except ValueError:return BAD,original

def make(args,ram):
    op,out,cap,count,source,length,mode=args;ram=bytearray(ram)
    values=(count,source,length) if op in (1,3) else (out,cap,count,source,length)
    struct.pack_into('<'+'I'*len(values),ram,64,*values);return args,bytes(ram)

def cases(count=12000):
    rng=random.Random(246)
    for op in range(5):
        for data in (b'',b'a\0b',b'a\0b\0',b'\x80\xff',bytes(range(24))):
            for cap in range(9):
                for counter in (0,32):
                    ram=bytearray(b'\xa5'*1024);ram[128:128+len(data)]=data
                    yield make((op,224,cap,counter,128 if data else 0,len(data),0),ram)
    for i in range(count):
        op=i%5;ram=bytearray(rng.randbytes(1024));n=rng.randrange(81);data=rng.randbytes(n);ram[128:128+n]=data
        a=(op,rng.choice((320,320,321,128,16,32,64,400,480,0,0xfffffff0,0x80000140)),rng.choice((0,1,2,3,8,32,64,128,0xffffffff)),rng.choice((32,32,0,64,128,320,321,400,480,0xfffffffe,0x80000020)),rng.choice((128,128,128,64,0,400,480,0xfffffff0,0x80000080)),n,rng.choice((0,0,0,1,2,4,8,16,32,64)))
        yield make(a,ram)

def execute(exe,vectors,bridge,codepage=0):
    return subprocess.run([str(exe),'bridge' if bridge else 'direct',str(codepage)],input=b''.join(struct.pack('<7I',*a)+ram for a,ram in vectors),capture_output=True,timeout=60)

def agrees(exe):
    vectors=list(cases());want=b''.join(struct.pack('<I',model(a,ram)[0])+model(a,ram)[1] for a,ram in vectors)
    for bridge in (False,True):
        r=execute(exe,vectors,bridge)
        if r.returncode or r.stdout!=want:return False
    return True

def test_original_raw_services_whole_ram_and_imports(tmp_path):assert agrees(fixture(tmp_path))

@pytest.mark.parametrize('mutation',[
 ('guest_text_raw.c','bytes&=~UINT32_C(1);','bytes|=UINT32_C(0);'),
 ('guest_text_raw.c','used&=~UINT32_C(1);','used|=UINT32_C(0);'),
 ('guest_text_raw.c','raw_count(result,used);','raw_count(result,needed);'),
 ('guest_text_raw.c','raw_overlap(body,used,result,4)','raw_overlap(body,0,result,4)'),
 ('guest_text_raw.c','input,bytes,capacity);','input,bytes,capacity+1);'),
 ('guest_text_raw.c','map(value,2,1);','map(value,2,0);'),
 ('guest_text_raw.c','if(upper && used<needed)status=RAW_OVERFLOW;','if(upper && used<needed)status=0;'),
 ('guest_text_raw.c','written!=needed','written>needed'),
 ('guest_kernel_text_raw.inc','a[0],a[1],a[2],operation','a[1],a[0],a[2],operation'),
 ('guest_kernel_text_raw.inc','bridge_raw_text(TSFP_RAW_ANSI_SIZE,1);','bridge_raw_text(TSFP_RAW_UNICODE_SIZE,1);'),
])
def test_compiled_bad_raw_services_rejected(tmp_path,mutation):assert not agrees(fixture(tmp_path,mutation))

@pytest.mark.skipif(os.name!='nt',reason='Actual Windows ACP raw RTL references')
def test_actual_ntdll_raw_references(tmp_path):
    nt=ctypes.WinDLL('ntdll');functions=[nt.RtlMultiByteToUnicodeN,nt.RtlMultiByteToUnicodeSize,nt.RtlUnicodeToMultiByteN,nt.RtlUnicodeToMultiByteSize,nt.RtlUpcaseUnicodeToMultiByteN]
    for i,f in enumerate(functions):
        f.argtypes=[ctypes.POINTER(ctypes.c_uint32),ctypes.c_void_p,ctypes.c_uint32] if i in (1,3) else [ctypes.c_void_p,ctypes.c_uint32,ctypes.POINTER(ctypes.c_uint32),ctypes.c_void_p,ctypes.c_uint32];f.restype=ctypes.c_uint32
    rng=random.Random(246);vectors=[];want=[]
    for i in range(7500):
        op=i%5;n=rng.randrange(81);data=rng.randbytes(n)
        if op==4:data=b''.join(bytes((rng.choice(b'aBcD09-\0'),0)) for _ in range(n//2))+data[:n%2]
        cap=rng.randrange(161);counter=32 if op in (1,3) or i%7 else 0;ram=bytearray(b'\xa5'*1024);ram[128:128+len(data)]=data
        args,ram=make((op,224,cap,counter,128 if data else 0,len(data),0),ram);expected=bytearray(ram)
        source=ctypes.create_string_buffer(data or b'\0');out=ctypes.create_string_buffer(b'\xa5'*160);written=ctypes.c_uint32(0xa5a5a5a5)
        status=functions[op](ctypes.byref(written),source,len(data)) if op in (1,3) else functions[op](out,cap,ctypes.byref(written) if counter else None,source,len(data))
        if op not in (1,3):expected[224:384]=out.raw[:160]
        if counter:struct.pack_into('<I',expected,32,written.value)
        want.append(struct.pack('<I',status)+expected);vectors.append((args,ram))
    exe=fixture(tmp_path,native=True)
    for bridge in (False,True):
        r=execute(exe,vectors,bridge);assert r.returncode==0,r.stderr.decode(errors='replace');assert r.stdout==b''.join(want)

@pytest.mark.skipif(os.name!='nt',reason='Windows standard encoding code-page boundary')
@pytest.mark.parametrize('codepage,text',[(1252,'Aa\u0100\xe9\u20acB\0z'),(932,'Aa\u6f22\u5b57\u3042\u30abB\0z'),(65001,'Aa\u20ac\u6f22\U0001f600B\0z')])
def test_windows_raw_prefixes_and_sizes(tmp_path,codepage,text):
    kernel=ctypes.WinDLL('kernel32');mb=kernel.MultiByteToWideChar;wc=kernel.WideCharToMultiByte
    mb.argtypes=[ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_int,ctypes.c_void_p,ctypes.c_int];mb.restype=ctypes.c_int
    wc.argtypes=[ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_int,ctypes.c_void_p,ctypes.c_int,ctypes.c_void_p,ctypes.c_void_p];wc.restype=ctypes.c_int
    def convert(data,wide):
        if not data:return b''
        src=ctypes.create_string_buffer(data)
        if wide:
            n=wc(codepage,0,src,len(data)//2,None,0,None,None);assert n>0;out=ctypes.create_string_buffer(n);assert wc(codepage,0,src,len(data)//2,out,n,None,None)==n
        else:
            n=mb(codepage,0,src,len(data),None,0);assert n>0;out=ctypes.create_string_buffer(n*2);assert mb(codepage,0,src,len(data),out,n)==n
        return out.raw
    wide=text.encode('utf-16le');narrow=convert(wide,True);vectors=[];want=[]
    for op in range(5):
        data=narrow if op<2 else wide
        if op==4:data=b''.join(struct.pack('<H',v-32 if 97<=v<=122 else v) for v in struct.unpack('<'+'H'*(len(wide)//2),wide))
        result=convert(data,op>=2)
        for cap in range(len(result)+3):
            ram=bytearray(b'\xa5'*1024);original=narrow if op<2 else wide;ram[128:128+len(original)]=original
            args,ram=make((op,320,cap,32,128,len(original),0),ram);expected=bytearray(ram);used=len(result)
            if op not in (1,3):
                if op==0:used=min(used,cap)//2*2
                else:
                    # Enumerate complete Unicode scalars, independent of the binary-search adapter.
                    transformed=''.join(chr(ord(c)-32) if 'a'<=c<='z' else c for c in text) if op==4 else text
                    prefixes=[convert(transformed[:i].encode('utf-16le'),True) for i in range(len(transformed)+1)]
                    used=max(len(p) for p in prefixes if len(p)<=cap)
                expected[320:320+used]=result[:used]
            struct.pack_into('<I',expected,32,used);status=OVERFLOW if op==4 and used<len(result) else 0
            want.append(struct.pack('<I',status)+expected);vectors.append((args,ram))
    exe=fixture(tmp_path,native=True)
    for bridge in (False,True):
        r=execute(exe,vectors,bridge,codepage);assert r.returncode==0,r.stderr.decode(errors='replace');assert r.stdout==b''.join(want)


def test_raw_provider_selection_and_actual_dispatch(tmp_path):
    kernel=ROOT/'external/ts3FPrecomp/src/kernel';bridge=(kernel/'kernel_bridge.c').read_text(encoding='utf-8');cmake=(kernel/'CMakeLists.txt').read_text(encoding='utf-8')
    assert cmake.count('${TSFP_SHARED_SOURCE}/runtime/guest_text_raw.c')==1;assert bridge.count('#include "guest_kernel_text_raw.inc"')==1
    checks=''
    for ordinal,name,size in ((299,'RtlMultiByteToUnicodeN',20),(300,'RtlMultiByteToUnicodeSize',12),(310,'RtlUnicodeToMultiByteN',20),(311,'RtlUnicodeToMultiByteSize',12),(315,'RtlUpcaseUnicodeToMultiByteN',20)):
        assert 'static void bridge_'+name+'(void)' not in bridge;assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+bridge_'+name+r';',bridge);checks+='assert(stdcall_args_for_ordinal('+str(ordinal)+')=='+str(size)+');'
    start=bridge.index('static int stdcall_args_for_ordinal(');end=bridge.index('static bridge_func_t bridge_for_ordinal(',start);p=tmp_path/'sizes.c'
    p.write_bytes(('#include <assert.h>\n#include <stdint.h>\ntypedef uint32_t ULONG;\n'+bridge[start:end]+'int main(void){'+checks+'return 0;}\n').encode())
    r=subprocess.run([str(_compile(tmp_path,p))],capture_output=True,text=True,timeout=15);assert r.returncode==0,r.stdout+r.stderr


def test_raw_32bit_counts_and_null_contracts(tmp_path):
    for n in ('guest_text_raw.c','guest_text_raw.h','guest_text_codec.h','guest_string_transform.h','host_text_codec.c','host_text_codec.h','host_string_case.c','host_string_case.h','guest_allocation.h','guest_virtual_memory.h','text_raw_limits.c'):
        p=ROOT/('tests/compiler' if n=='text_raw_limits.c' else 'src/runtime')/n;(tmp_path/n).write_bytes(p.read_bytes())
    flags=['/W4','/DTSFP_TEST_LATIN1_CODEC'] if os.name=='nt' else ['-Wall','-Wextra','-DTSFP_TEST_LATIN1_CODEC']
    exe=_compile(tmp_path,tmp_path/'text_raw_limits.c',extra=[tmp_path/n for n in ('guest_text_raw.c','host_text_codec.c','host_string_case.c')]+flags)
    r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30);assert r.returncode==0,r.stdout+r.stderr
