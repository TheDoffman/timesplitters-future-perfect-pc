"""Independent date vectors and actual import adapters, with compiled fault controls."""
from pathlib import Path
import datetime as dt
import os,random,struct,subprocess,sys,re
import pytest
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(HERE))
from test_translation_product import _compile

def row(decode,success,input_,expected=b''):
    return bytes((decode,success))+bytes(6)+input_.ljust(16,b'\0')+expected.ljust(16,b'\0')

@pytest.fixture(scope='module')
def calendar_vectors():
    epoch=dt.datetime(1601,1,1);rng=random.Random(304305);rows=[]
    for day in range(146097):
        value=epoch+dt.timedelta(days=day,hours=day%24,minutes=day%60,seconds=day%59,milliseconds=day%1000)
        fields=struct.pack('<8H',value.year,value.month,value.day,value.hour,value.minute,value.second,value.microsecond//1000,(value.weekday()+1)%7)
        ticks=struct.pack('<Q',(day*86400000+value.hour*3600000+value.minute*60000+value.second*1000+value.microsecond//1000)*10000)
        rows.extend((row(0,1,fields,ticks),row(1,1,ticks,fields)))
    for _ in range(3000):
        value=epoch+dt.timedelta(days=rng.randrange((dt.datetime(9999,12,31)-epoch).days),milliseconds=rng.randrange(86400000))
        delta=value-epoch;ticks=(delta.days*86400000+delta.seconds*1000+delta.microseconds//1000)*10000
        fields=struct.pack('<8H',value.year,value.month,value.day,value.hour,value.minute,value.second,value.microsecond//1000,(value.weekday()+1)%7)
        rows.extend((row(0,1,fields,struct.pack('<Q',ticks)),row(1,1,struct.pack('<Q',ticks+9999),fields)))
    for fields in ((1600,1,1,0,0,0,0,0),(30828,1,1,0,0,0,0,0),(1900,2,29,0,0,0,0,0),(2000,2,30,0,0,0,0,0),
                   (2024,0,1,0,0,0,0,0),(2024,13,1,0,0,0,0,0),(2024,1,0,0,0,0,0,0),(2024,1,1,24,0,0,0,0),
                   (2024,1,1,0,60,0,0,0),(2024,1,1,0,0,60,0,0),(2024,1,1,0,0,0,1000,0),(65535,1,1,0,0,0,0,0)):
        rows.append(row(0,0,struct.pack('<8H',*fields)))
    rows.append(row(0,1,struct.pack('<8H',1601,1,1,0,0,0,0,65535),bytes(8)))
    rows.append(row(1,0,struct.pack('<Q',1<<63)))
    if os.name=='nt':rows.extend(windows_vectors(rng))
    return b''.join(rows)

def windows_vectors(rng):
    # API oracle only: no Windows implementation source is used.
    import ctypes as c
    class ST(c.Structure):_fields_=[(n,c.c_uint16) for n in ('year','month','weekday','day','hour','minute','second','ms')]
    kernel=c.WinDLL('kernel32',use_last_error=True)
    kernel.SystemTimeToFileTime.argtypes=[c.POINTER(ST),c.POINTER(c.c_uint64)]
    kernel.FileTimeToSystemTime.argtypes=[c.POINTER(c.c_uint64),c.POINTER(ST)]
    for _ in range(12000):
        fields=(rng.randrange(1598,30831),rng.randrange(0,14),rng.randrange(0,33),rng.randrange(0,25),rng.randrange(0,61),rng.randrange(0,61),rng.randrange(0,1001),rng.randrange(65536))
        y,m,d,h,mi,s,ms,w=fields;st=ST(y,m,w,d,h,mi,s,ms);ticks=c.c_uint64()
        ok=bool(kernel.SystemTimeToFileTime(c.byref(st),c.byref(ticks)))
        yield row(0,ok,struct.pack('<8H',*fields),struct.pack('<Q',ticks.value) if ok else b'')
        ticks=c.c_uint64(rng.getrandbits(64));st=ST()
        ok=bool(kernel.FileTimeToSystemTime(c.byref(ticks),c.byref(st)))
        yield row(1,ok,struct.pack('<Q',ticks.value),struct.pack('<8H',st.year,st.month,st.day,st.hour,st.minute,st.second,st.ms,st.weekday) if ok else b'')

def run_fixture(tmp_path,fixture,mutation,vectors=b''):
    for name in ('guest_time.c','guest_time.h','guest_virtual_memory.h','guest_allocation.h','guest_kernel_time.inc',fixture):
        p=HERE/name
        if not p.is_file():p=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    if mutation:
        name,a,b=mutation;p=tmp_path/name;s=p.read_text(encoding='utf-8');assert s.count(a)==1
        p.write_bytes(s.replace(a,b).encode('utf-8'))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/fixture,extra=[tmp_path/'guest_time.c',*flags])
    run=subprocess.run([str(exe)],input=vectors,capture_output=True,timeout=30)
    if mutation:assert run.returncode!=0,'Broken time implementation passed'
    else:assert run.returncode==0,run.stdout.decode()+run.stderr.decode()

@pytest.mark.parametrize('mutation',[None,
    ('guest_time.c','year%400==0','year%400!=0'),
    ('guest_time.c','day>time_month_days(year,month)','day>=time_month_days(year,month)'),
    ('guest_time.c','ticks/10000','ticks/1000'),
    ('guest_time.c','(days+1)%7','days%7'),
    ('guest_time.c','if(ticks>>63)','if(ticks==0)'),
])
def test_calendar_vectors_and_boundaries(tmp_path,mutation,calendar_vectors):
    run_fixture(tmp_path,'time_contract.c',mutation,calendar_vectors)

@pytest.mark.parametrize('mutation',[None,
    ('guest_kernel_time.inc','fields,16,0','fields,2,0'),
    ('guest_kernel_time.inc','time,8,1','time,8,0'),
    ('guest_kernel_time.inc','address>UINT32_MAX-(bytes-1)','address==UINT32_MAX'),
    ('guest_kernel_time.inc','result[16]={0}','result[16]={1}'),
    ('guest_kernel_time.inc','memcpy(output,result,sizeof(result));g_eax=1','memcpy(output,result,sizeof(result));g_eax=0'),
])
def test_calendar_guest_abi(tmp_path,mutation):
    run_fixture(tmp_path,'kernel_time_contract.c',mutation)

def test_retired_calendar_callers():
    runtime=ROOT/'external/ts3FPrecomp';bridge=(runtime/'src/kernel/kernel_bridge.c').read_text(encoding='utf-8')
    assert bridge.count('#include "guest_kernel_time.inc"')==1
    cmake=(runtime/'src/kernel/CMakeLists.txt').read_text(encoding='utf-8')
    assert cmake.count('${TSFP_SHARED_SOURCE}/runtime/guest_time.c')==1
    assert 'kernel_thunks.c' not in re.search(r'set\(KERNEL_SOURCES(.*?)\)',cmake,re.S).group(1)
    for ordinal,name in ((304,'RtlTimeFieldsToTime'),(305,'RtlTimeToTimeFields')):
        assert 'static void bridge_'+name+'(void)' not in bridge
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+8;\s*/\*\s*'+name+r'\b',bridge)
    retired=re.compile(r'\b(xbox_RtlTimeFieldsToTime|xbox_RtlTimeToTimeFields|SystemTimeToFileTime|FileTimeToSystemTime)\b')
    for base in (runtime/'src',runtime/'titles/futureperfect/src',ROOT/'src'):
        for p in base.rglob('*'):
            if p.suffix not in ('.c','.h','.inc') or p.name=='kernel_thunks.c':continue
            assert not retired.search(p.read_text(encoding='utf-8',errors='replace')),p
