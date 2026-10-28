"""Native XBE loader: real bytes, independent metadata oracle and failure atomicity."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile
from test_xbe_image import fixture, KEYS
from xbe_image import XBEParser

CANDIDATE=Path(__file__).parent
PROVIDERS=CANDIDATE if (CANDIDATE/'guest_image.c').exists() else ROOT/'src/runtime'

def compile_loader(folder,mutation=None):
    source=(PROVIDERS/'guest_image.c').read_text()
    if mutation:
        before,after=mutation
        assert source.count(before)==1
        source=source.replace(before,after)
    (folder/'guest_image.c').write_text(source)
    (folder/'guest_image.h').write_bytes((PROVIDERS/'guest_image.h').read_bytes())
    harness=folder/'guest_image_runtime.c'
    harness.write_bytes((CANDIDATE/'guest_image_runtime.c').read_bytes())
    warnings=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    return _compile(folder,harness,extra=[folder/'guest_image.c',*warnings])

@pytest.fixture(scope='module')
def loader(tmp_path_factory):
    return compile_loader(tmp_path_factory.mktemp('native-image'))

def run(loader,tmp_path,data,limit=0x20000):
    path=tmp_path/'input.xbe'; path.write_bytes(data)
    output=tmp_path/'loaded.bin'
    if output.exists(): output.unlink()
    result=subprocess.run([str(loader),str(path),str(output),str(limit)],
                          capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr+f' rc={result.returncode}'
    info=json.loads(result.stdout)
    memory=output.read_bytes() if output.exists() else None
    return info,memory

def accepted(loader,tmp_path,data,limit=0x20000):
    info,memory=run(loader,tmp_path,data,limit)
    assert info['result']==0,info
    x=XBEParser(tmp_path/'input.xbe').parse(); h=x.header
    expected=bytearray(b'\xa5')*(h.base_address+h.image_size)
    expected[h.base_address:h.base_address+h.header_size]=data[:h.header_size]
    live=[s for s in x.sections if s.virtual_size]
    code=[s for s in live if s.flags&4]
    for s in live:
        n=min(s.virtual_size,s.raw_size)
        expected[s.virtual_addr:s.virtual_addr+s.virtual_size]=bytes(s.virtual_size)
        expected[s.virtual_addr:s.virtual_addr+n]=data[s.raw_addr:s.raw_addr+n]
    # CI enables pytest's full sequence diff. These large, repetitive buffers
    # can make that formatter quadratic even for an expected mutation failure.
    # Compare every byte, then raise a bounded diagnostic without assert rewrite.
    if memory is None or memory != expected:
        actual = memory or b''
        first = next((i for i, (a, b) in enumerate(zip(actual, expected)) if a != b),
                     min(len(actual), len(expected)))
        raise AssertionError(f'Guest memory differs at 0x{first:x}; '
                             f'actual length {len(actual)}, expected {len(expected)}')
    wanted=dict(base=h.base_address,header_size=h.header_size,image_end=h.base_address+h.image_size,
        section_count=len(x.sections),image_lo=min(s.virtual_addr for s in live),
        image_hi=max(s.virtual_addr+s.virtual_size for s in live),
        code_lo=min(s.virtual_addr for s in code),code_hi=max(s.virtual_addr+s.virtual_size for s in code),
        entry=h.entry_point,thunk=h.kernel_thunk_addr,thunk_count=len(x.kernel_imports),debug=int(h.is_debug),
        title_id=x.certificate.title_id,region=x.certificate.game_region,
        tls_directory=h.tls_addr,tls_start=x.tls.raw_data_start if x.tls else 0,
        tls_bytes=x.tls.raw_data_end-x.tls.raw_data_start if x.tls else 0,
        tls_zero_fill=x.tls.zero_fill_size if x.tls else 0,tls_index=x.tls.tls_index_addr if x.tls else 0,
        title_hex=x.certificate.title_name.rstrip(' \t').encode().hex())
    for name,value in wanted.items(): assert info[name]==value,(name,info[name],value)
    return info

def put(data,offset,*values): struct.pack_into('<'+'I'*len(values),data,offset,*values)

@pytest.mark.parametrize('kind',['retail','debug'])
def test_complete_bytes_bounds_and_metadata(loader,tmp_path,kind):
    data=fixture(kind)
    data[0x1100:0x1108]=bytes(range(8))
    put(data,0x2a0,7)
    accepted(loader,tmp_path,data)

def test_zero_directories_padding_holes_and_no_tls_template(loader,tmp_path):
    data=fixture(); put(data,0x12c,0); put(data,0x158,KEYS['retail'][1])
    put(data,0x408,0x100) # Raw alignment padding must not be copied past vsize.
    accepted(loader,tmp_path,data)
    data=fixture(); put(data,0x500,0,0)
    accepted(loader,tmp_path,data)

def test_multiple_sections_file_and_virtual_order_differ(loader,tmp_path):
    data=fixture(); data.extend(bytes(range(256)))
    put(data,0x11c,2)
    put(data,0x438,1,0x11800,0x100,0x1200,0x100,0x10480)
    accepted(loader,tmp_path,data)
    first=bytes(data[0x400:0x438]); data[0x400:0x438]=data[0x438:0x470]; data[0x438:0x470]=first
    accepted(loader,tmp_path,data)

@pytest.mark.parametrize('title',['a\u00a9\u4e2d\U0001f680  \t','\u4e2d'*40,''])
def test_bounded_utf16_title_conversion(loader,tmp_path,title):
    data=fixture(); raw=title.encode('utf-16-le'); data[0x20c:0x25c]=raw+bytes(80-len(raw))
    accepted(loader,tmp_path,data)

def test_invalid_surrogates_are_replaced_without_overrun(loader,tmp_path):
    data=fixture(); data[0x20c:0x25c]=bytes.fromhex('00d8')*40
    info,memory=run(loader,tmp_path,data)
    assert info['result']==0 and bytes.fromhex(info['title_hex']).decode()=='\ufffd'*40
    assert memory is not None

@pytest.mark.parametrize('offset,value',[
    (0,0),(0x104,0xfffff000),(0x104,0),(0x108,0x10000000),(0x108,0x100),(0x10c,0xffffffff),
    (0x110,1),(0x118,0x1ffff),(0x11c,0),(0x11c,4097),(0x120,0x10ff0),(0x120,0xfffffff0),
    (0x128,0),(0x158,0),(0x200,0x10),(0x200,4097),(0x400,0),(0x404,0x10000),
    (0x404,0xffffff00),(0x408,0xfffffff0),(0x40c,0xffffff00),(0x40c,0x100),
    (0x410,0x100000),(0x12c,0x11200),(0x500,0x11200),(0x504,0x11000),
    (0x508,0xfffffff0),(0x510,0xfffffff0),(0x1080,8),(0x1080,0x80000000),
])
def test_malformed_images_fail_without_any_writes(loader,tmp_path,offset,value):
    data=fixture(); put(data,offset,value)
    info,memory=run(loader,tmp_path,data)
    assert info['result']!=0 and memory is None,info

@pytest.mark.parametrize('length',[0,3,0x104,0x177,0xfff,0x11ff])
def test_truncated_images_leave_destination_and_metadata_unchanged(loader,tmp_path,length):
    info,memory=run(loader,tmp_path,fixture()[:length])
    assert info['result']!=0 and memory is None

@pytest.mark.parametrize('raw,va',[(0x1000,0x11800),(0x1100,0x11100)])
def test_overlapping_payload_or_guest_sections_are_rejected(loader,tmp_path,raw,va):
    data=fixture(); put(data,0x11c,2); put(data,0x438,1,va,0x100,raw,0x100,0x10480)
    info,memory=run(loader,tmp_path,data)
    assert info['reason']=='overlap' and memory is None

def test_bss_cannot_supply_entry_imports_or_tls_initializers(loader,tmp_path):
    for offset,value in [(0x128,0x11200^KEYS['retail'][0]),
                         (0x158,0x11200^KEYS['retail'][1]),(0x500,0x11200)]:
        data=fixture(); put(data,offset,value)
        if offset==0x500: put(data,0x504,0x11208)
        info,memory=run(loader,tmp_path,data)
        assert info['result']!=0 and memory is None

def test_import_terminator_required_in_backed_bytes(loader,tmp_path):
    data=fixture(); data[0x1080:]=struct.pack('<I',0x80000008)*((len(data)-0x1080)//4)
    info,memory=run(loader,tmp_path,data)
    assert info['reason']=='import' and memory is None

def test_guest_limit_is_checked_before_copy(loader,tmp_path):
    info,memory=run(loader,tmp_path,fixture(),0x11fff)
    assert info['reason']=='header' and memory is None
    accepted(loader,tmp_path,fixture(),0x12000)

def import_image(count):
    data=fixture(); data.extend(bytes(0x1000))
    put(data,0x408,0x1000); put(data,0x410,0x1000)
    put(data,0x12c,0)
    put(data,0x1080,*([0x80000008]*count),0)
    return data

def test_import_capacity_and_terminator_boundary(loader,tmp_path):
    accepted(loader,tmp_path,import_image(378))
    info,memory=run(loader,tmp_path,import_image(379))
    assert info['reason']=='import' and memory is None

@pytest.mark.parametrize('end,zero',[(0x700000,0),(0x700001,8),(0x710000,0x1f0),
    (0x710001,0x1fd),(0x10000,0xffffffff),(0xfffffff0,8),(0x4000000,8),(0x700001,'none')])
def test_bootstrap_alignment_capacity_and_overflow(loader,end,zero):
    result=subprocess.run([str(loader),'--bootstrap',str(end),str(zero)],
                          capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    actual=json.loads(result.stdout)
    shift=max(0,(end-0x700000+0xffff)//0x10000*0x10000)
    total=0 if zero=='none' else (zero+15)//16*16+4
    if shift+0xf80000>=64*1024*1024 or total>0x200:
        assert actual['result']!=0
    else:
        assert actual==dict(result=0,shift=shift,total=total)

def test_more_than_legacy_64_sections_and_late_failure(loader,tmp_path):
    original=fixture()
    data=original[:0x1000]+bytes(0x1000)+original[0x1000:]
    put(data,0x108,0x2000,0x3000)
    put(data,0x404,0x12000,0x300,0x2000)
    put(data,0x128,0x12000^KEYS['retail'][0],0)
    put(data,0x158,0x12080^KEYS['retail'][1]); put(data,0x160,0)
    put(data,0x11c,65)
    data[0x380:0x385]=b'name\0'
    put(data,0x414,0x10380)
    for i in range(1,65):
        put(data,0x400+i*56,1,0x12400+i*8,8,0,0,0x10380)
    accepted(loader,tmp_path,data)
    put(data,0x400+64*56+4,0xfffffff0)
    info,memory=run(loader,tmp_path,data)
    assert info['reason']=='section' and memory is None

@pytest.mark.parametrize('mutation',[
    ('memset(dest+va+copy,0,vn-copy)','memset(dest+va+copy,1,vn-copy)'),
    ('memcpy(dest+va,r.file+word(s+12),copy)','memcpy(dest+va+1,r.file+word(s+12),copy)'),
    ('r->info.region=word(p+0xa0)','r->info.region=word(p+0x9c)'),
])
def test_compiled_mutations_are_rejected(tmp_path,mutation):
    loader=compile_loader(tmp_path,mutation)
    data=fixture(); put(data,0x2a0,7)
    with pytest.raises(AssertionError): accepted(loader,tmp_path,data)

def test_private_supported_us_native_image(loader,tmp_path):
    path=ROOT/'private/game/default.xbe'
    if not path.exists(): pytest.skip('Private supported executable unavailable')
    data=path.read_bytes()
    assert hashlib.sha256(data).hexdigest()=='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    info=accepted(loader,tmp_path,data,64*1024*1024)
    assert info['section_count']==23 and info['thunk_count']==151
    assert info['title_id']==0x45410066 and info['entry']==0x38024d


def test_installed_loader_uses_project_preflight_and_materialization():
    kernel=ROOT/'external/ts3FPrecomp/src/kernel'
    text=(kernel/'xbox_memory_layout.c').read_text()
    init=text[text.index('BOOL xbox_MemoryLayoutInit('):]
    assert init.index('tsfp_image_inspect(')<init.index('CreateFileMappingW(')
    assert init.index('tsfp_image_bootstrap(')<init.index('CreateFileMappingW(')
    assert 'tsfp_image_load(xbe_data, xbe_size, g_memory_base, g_memory_size, &image)' in init
    assert 'xbe +' not in init and 'xbe_title_name_store' not in text
    assert 'tsfp_thread_memory_init(&g_guest_threads, &g_guest_allocations, &image)' in init
    assert 'g_guest_threads.tls_total != image_tls_total' in init
    assert '"${TSFP_SHARED_SOURCE}/runtime/guest_image.c"' in (kernel/'CMakeLists.txt').read_text()
