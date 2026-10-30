"""Original guest numeric conversions: whole RAM, typed native oracle and faults."""
from pathlib import Path
import ctypes,os,random,struct,subprocess,sys
import pytest
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(HERE))
from test_translation_product import _compile
BAD=0xc000000d
SMALL=0x80000005

def compile_fixture(tmp_path,mutation=None,name='numbers_cli.c'):
    for n in ('guest_numbers.c','guest_numbers.h','guest_kernel_numbers.inc','guest_virtual_memory.h','guest_allocation.h',name):
        p=HERE/n
        if not p.is_file():p=ROOT/'src/runtime'/n
        (tmp_path/n).write_bytes(p.read_bytes())
    if mutation:
        filename,a,b=mutation;p=tmp_path/filename;s=p.read_text(encoding='utf-8');assert s.count(a)==1
        p.write_bytes(s.replace(a,b).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    return _compile(tmp_path,tmp_path/name,extra=[tmp_path/'guest_numbers.c',*flags])

def span(address,size,write=False):
    if not address or not size or address+size>2**32:raise ValueError
    at=address&0x7fffffff
    if not at or at+size>512 or (at<480 and at+size>464) or (write and at<416 and at+size>400):raise ValueError
    return at

def model(args,original):
    ram=bytearray(original);op,a,base,c,d=args
    def u16(at):return int.from_bytes(ram[at:at+2],'little')
    def u32(at):return int.from_bytes(ram[at:at+4],'little')
    try:
        if op!=1 and base not in (0,2,8,10,16):raise ValueError
        if op<2:
            output=span(c,4,True)
            if op==1:
                header=span(a,8);length=u16(header);buffer=u32(header+4)
                if length%2:raise ValueError
                start=span(buffer,length) if length else 0
                units=[u16(start+i) for i in range(0,length,2)]
                if base not in (0,2,8,10,16):
                    ram[output:output+4]=b'\0'*4
                    return BAD,bytes(ram)
                def unit(index):return units[index] if index<len(units) else 0
            else:
                if not a:raise ValueError
                def unit(index):return ram[span(a+index,1)]
            index=0
            while 0<unit(index)<=32:index+=1
            negative=unit(index)==ord('-')
            if unit(index) in (ord('+'),ord('-')):index+=1
            if not base:
                base=10
                if unit(index)==ord('0') and unit(index+1) in b'xob':
                    base={ord('x'):16,ord('o'):8,ord('b'):2}[unit(index+1)];index+=2
            digits=''
            while True:
                char=chr(unit(index));candidate='0123456789abcdef'.find(char.lower())
                if len(char.lower())!=1 or candidate<0 or candidate>=base:break
                digits+=char;index+=1
            number=int(digits,base) if digits else 0
            value=(-number if negative else number)&0xffffffff
            ram[output:output+4]=value.to_bytes(4,'little')
            return (BAD if op==1 and not length else 0),bytes(ram)
        representation=format(a,{0:'d',2:'b',8:'o',10:'d',16:'X'}[base]).encode()
        if op==2:
            if not d:raise ValueError
            if c<len(representation):return SMALL,bytes(ram)
            text=representation+(b'\0' if c>len(representation) else b'')
            output=span(d,len(text),True);ram[output:output+len(text)]=text
        else:
            header=span(c,8,True);capacity=u16(header+2);buffer=u32(header+4)
            text=representation.decode().encode('utf-16le')+b'\0\0'
            if capacity<len(text):return SMALL,bytes(ram)
            output=span(buffer,len(text),True)
            if output<header+8 and header<output+len(text):raise ValueError
            ram[output:output+len(text)]=text
            ram[header:header+2]=(len(text)-2).to_bytes(2,'little')
        return 0,bytes(ram)
    except ValueError:return BAD,bytes(original)

def requests(seed=240,count=14000):
    rng=random.Random(seed)
    samples=['','012','0xFF','0XFF','0b101','0o17','4294967296','-4294967296','+678abc',' \t-42','\x01\x1f42','12\0'+'34','\xa042','9'*150,'f'*120]
    # A numeric unit immediately beyond Length must never extend a counted input.
    boundary=bytearray(b'\xa5'*512)
    struct.pack_into('<HHI',boundary,16,4,8,64)
    boundary[64:72]='123\0'.encode('utf-16le')
    yield (1,16,10,320,0),bytes(boundary)
    for i in range(count-1):
        ram=bytearray(rng.randbytes(512));op=i%4;base=rng.choice([0,2,8,10,16,3,17]);value=rng.getrandbits(32)
        capacity=rng.choice([0,1,2,4,8,10,11,12,16,20,32,33,34,0xffffffff]);body=64;record=16;output=320
        if op<2:
            text=rng.choice(samples) if i%3 else rng.choice(['',' ','\v','-','+'])+str(value)+rng.choice(['','stop','\0tail'])
            raw=text.encode('latin1') if op==0 else text.encode('utf-16le')
            ram[body:body+len(raw)+(1 if op==0 else 0)]=raw+(b'\0' if op==0 else b'')
            if op==1:struct.pack_into('<HHI',ram,record,len(raw),rng.randrange(256),body)
            source=body if op==0 else record
            if i%11==0:output=rng.choice([0,400,464,511,0xfffffffe,64,16,0x80000140])
            if i%17==0:source=rng.choice([0,511,0xfffffffe,0x80000040 if op==0 else 0x80000010])
            if op==1 and i%19==0:struct.pack_into('<H',ram,record,3)
            args=(op,source,base,output,0)
        else:
            if op==3:
                cap=capacity if capacity<65536 else 65535
                address=rng.choice([body,0x80000040,record,0x80000010,400,464,511,0xfffffffe,0]) if i%7==0 else body
                struct.pack_into('<HHI',ram,record,0x7777,cap,address)
                args=(op,value,base,record,0)
            else:
                destination=rng.choice([0,400,464,511,0xfffffffe,0x80000040]) if i%7==0 else body
                args=(op,value,base,capacity,destination)
        assert len(ram)==512
        yield args,bytes(ram)

def execute(exe,cases,expected,broken=False):
    data=b''.join(struct.pack('<5I',*a)+ram for a,ram in cases)
    run=subprocess.run([str(exe)],input=data,capture_output=True,timeout=40)
    assert run.returncode==0,run.stderr.decode(errors='replace')
    assert len(run.stdout)==len(cases)*516
    mismatch=[]
    for i,(status,ram) in enumerate(expected):
        part=run.stdout[i*516:(i+1)*516]
        if part!=struct.pack('<I',status)+ram:mismatch.append((i,cases[i][0],status,int.from_bytes(part[:4],'little')))
        if len(mismatch)>=3:break
    if broken:assert mismatch,'Broken numeric provider passed whole-RAM cases'
    else:assert not mismatch,mismatch

@pytest.mark.parametrize('mutation',[None,
    ('guest_numbers.c','if(!base) {\n        base=10;','if(!base) {\n        base=8;'),
    ('guest_numbers.c',"if(next=='x')base=16","if(next=='x' || next=='X')base=16"),
    ('guest_numbers.c','value=value*base+digit;','value=(uint64_t)value*base+digit>UINT32_MAX?UINT32_MAX:value*base+digit;'),
    ('guest_numbers.c','bytes=count+(capacity>count?1u:0u);','bytes=count+1;'),
    ('guest_numbers.c','count=number_digits(value,base,text);bytes=(count+1)*2;','count=number_digits(value,base,text);number_write(record,2,count*2);bytes=(count+1)*2;'),
    ('guest_numbers.c','if(!out || number_overlap(record,8,out,bytes))','if(!out || (number_overlap(record,8,out,bytes) && !bytes))'),
    ('guest_numbers.c','r.units=length/2;','r.units=length;'),
    ('guest_numbers.c','number_write(out+count*2,2,0);number_write(record,2,count*2);','number_write(out+count*2,2,0);number_write(record,2,bytes);'),
])
def test_numeric_full_ram_oracle_and_fault_controls(tmp_path,mutation):
    exe=compile_fixture(tmp_path,mutation);cases=list(requests())
    execute(exe,cases,[model(*case) for case in cases],bool(mutation))


@pytest.mark.skipif(os.name!='nt',reason='Actual ntdll reference requires Windows')
def test_real_windows_numeric_oracle(tmp_path):
    class NativeString(ctypes.Structure):
        _fields_=[('length',ctypes.c_uint16),('maximum',ctypes.c_uint16),('buffer',ctypes.c_void_p)]
    ntdll=ctypes.WinDLL('ntdll')
    ntdll.RtlCharToInteger.argtypes=[ctypes.c_char_p,ctypes.c_uint32,ctypes.POINTER(ctypes.c_uint32)]
    ntdll.RtlUnicodeStringToInteger.argtypes=[ctypes.POINTER(NativeString),ctypes.c_uint32,ctypes.POINTER(ctypes.c_uint32)]
    ntdll.RtlIntegerToChar.argtypes=[ctypes.c_uint32,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
    ntdll.RtlIntegerToUnicodeString.argtypes=[ctypes.c_uint32,ctypes.c_uint32,ctypes.POINTER(NativeString)]
    for name in ('RtlCharToInteger','RtlUnicodeStringToInteger','RtlIntegerToChar','RtlIntegerToUnicodeString'):
        getattr(ntdll,name).restype=ctypes.c_int32
    rng=random.Random(0x240);cases=[];expected=[]
    samples=['','012','0xFF','0XFF','0b101','0B101','0o17','0O17','4294967296','-4294967296','+678abc',' \t-42','\x01\x1f42','12\0'+'34','9'*150]
    for i in range(6000):
        op=i%4;base=rng.choice([0,2,8,10,16,3]);value=rng.getrandbits(32)
        ram=bytearray(rng.randbytes(512));out=bytearray(ram)
        if op<2:
            text=rng.choice(samples) if i%3 else rng.choice(['','-','+','\v'])+str(value)+rng.choice(['','tail'])
            raw=text.encode('ascii') if op==0 else text.encode('utf-16le')
            ram[64:64+len(raw)+(1 if op==0 else 0)]=raw+(b'\0' if op==0 else b'')
            if op:struct.pack_into('<HHI',ram,16,len(raw),len(raw)+2,64)
            out=bytearray(ram);native=ctypes.create_string_buffer(raw+(b'\0\0' if op else b'\0'))
            result=ctypes.c_uint32(int.from_bytes(ram[320:324],'little'))
            if op:
                record=NativeString(len(raw),len(raw)+2,ctypes.addressof(native))
                status=ntdll.RtlUnicodeStringToInteger(ctypes.byref(record),base,ctypes.byref(result))
            else:status=ntdll.RtlCharToInteger(native,base,ctypes.byref(result))
            out[320:324]=result.value.to_bytes(4,'little');args=(op,16 if op else 64,base,320,0)
        else:
            count=rng.choice([0,1,2,4,8,10,11,12,16,20,32,33,34])
            if op==3:struct.pack_into('<HHI',ram,16,0x7777,count*2,64)
            out=bytearray(ram);native=ctypes.create_string_buffer(bytes(ram[64:192]),128)
            if op==3:
                record=NativeString(0x7777,count*2,ctypes.addressof(native))
                status=ntdll.RtlIntegerToUnicodeString(value,base,ctypes.byref(record))
                out[16:18]=record.length.to_bytes(2,'little');args=(op,value,base,16,0)
            else:status=ntdll.RtlIntegerToChar(value,base,count,ctypes.addressof(native));args=(op,value,base,count,64)
            out[64:192]=native.raw
        cases.append((args,bytes(ram)));expected.append((status&0xffffffff,bytes(out)))
    execute(compile_fixture(tmp_path),cases,expected)


@pytest.mark.parametrize('mutation',[None,
    ('guest_kernel_numbers.inc','bytes=count*4','bytes=count*2'),
    ('guest_kernel_numbers.inc','tsfp_number_from_ansi(&vm,a[0],a[1],a[2])','tsfp_number_from_ansi(&vm,a[1],a[0],a[2])'),
    ('guest_kernel_numbers.inc','tsfp_number_to_ansi(&vm,a[0],a[1],a[2],a[3])','tsfp_number_to_ansi(&vm,a[0],a[1],a[2]+1,a[3])'),
    ('guest_kernel_numbers.inc','tsfp_number_to_unicode(&vm,a[0],a[1],a[2])','tsfp_number_to_unicode(&vm,a[1],a[0],a[2])'),
])
def test_actual_numeric_import_frames(tmp_path,mutation):
    exe=compile_fixture(tmp_path,mutation,'kernel_numbers_contract.c')
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert run.returncode!=0,'Broken import adapter passed'
    else:assert run.returncode==0,run.stdout+run.stderr


def test_literal_numeric_boundaries_and_footprints(tmp_path):
    cases=[];expected=[]
    def add(args,changes,updates,status=0):
        ram=bytearray(b'\xa5'*512)
        for at,data in changes:ram[at:at+len(data)]=data
        out=bytearray(ram)
        for at,data in updates:out[at:at+len(data)]=data
        cases.append((args,bytes(ram)));expected.append((status,bytes(out)))
    def record(length,capacity,buffer):return struct.pack('<HHI',length,capacity,buffer)
    add((0,64,0,320,0),[(64,b'012\0')],[(320,struct.pack('<I',12))])
    add((0,64,0,320,0),[(64,b'-4294967296\0')],[(320,b'\0'*4)])
    add((0,64,0,320,0),[(64,b'0xffffffff\0')],[(320,b'\xff'*4)])
    # Extended ANSI whitespace quirks from signed-char native implementations
    # are deliberately outside this provider's unsigned ASCII grammar.
    add((0,64,0,320,0),[(64,b'\xa042\0')],[(320,b'\0'*4)])
    add((0,510,10,320,0),[(510,b'12')],[],BAD)
    add((0,64,10,0x80000040,0),[(64,b'100\0')],[(64,struct.pack('<I',100))])
    add((1,16,0,320,0),[(16,record(10,0,64)),(64,'0b101'.encode('utf-16le'))],[(320,struct.pack('<I',5))])
    add((1,16,10,320,0),[(16,record(4,8,64)),(64,'123\0'.encode('utf-16le'))],[(320,struct.pack('<I',12))])
    add((1,16,0,320,0),[(16,record(0,0,0))],[(320,b'\0'*4)],BAD)
    add((1,16,3,320,0),[(16,record(2,2,64)),(64,b'1\0')],[(320,b'\0'*4)],BAD)
    add((1,16,10,320,0),[(16,record(3,4,64))],[],BAD)
    add((1,16,10,320,0),[(16,record(2,2,511))],[],BAD)
    add((1,16,10,400,0),[(16,record(0,0,0))],[],BAD)
    add((2,42,0,0xffffffff,64),[],[(64,b'42\0')])
    add((2,255,16,2,64),[],[(64,b'FF')])
    add((2,255,16,1,64),[],[],SMALL)
    add((3,0xffffffff,10,16,0),[(16,record(0x7777,20,64))],[],SMALL)
    add((3,0xffffffff,10,16,0),[(16,record(0x7777,22,64))],[(16,struct.pack('<H',20)),(64,'4294967295\0'.encode('utf-16le'))])
    add((3,42,10,16,0),[(16,record(0x7777,66,0x80000010))],[],BAD)
    execute(compile_fixture(tmp_path),cases,expected)


def test_selected_numeric_imports_and_cleanup():
    import re
    kernel=ROOT/'external/ts3FPrecomp/src/kernel'
    assert (kernel/'CMakeLists.txt').read_text(encoding='utf-8').count('${TSFP_SHARED_SOURCE}/runtime/guest_numbers.c')==1
    bridge=(kernel/'kernel_bridge.c').read_text(encoding='utf-8')
    assert bridge.count('#include "guest_kernel_numbers.inc"')==1
    for ordinal,name,size in ((267,'RtlCharToInteger',12),(292,'RtlIntegerToChar',16),(293,'RtlIntegerToUnicodeString',12),(309,'RtlUnicodeStringToInteger',12)):
        assert 'static void bridge_'+name+'(void)' not in bridge
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+'+str(size)+r';',bridge)
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+bridge_'+name+r';',bridge)
