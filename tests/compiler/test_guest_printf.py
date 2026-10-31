"""Original mapped printf, independent host atom oracles and true x86 ABI probe."""
from pathlib import Path
import os,re,struct,subprocess,sys
import pytest
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(HERE))
from native_compiler import native_compiler
from test_translation_product import _compile

def fixture(tmp_path,name,mutation):
    for n in ('guest_printf.c','guest_printf.h','guest_kernel_printf.inc','guest_virtual_memory.h','guest_allocation.h',name):
        p=HERE/n
        if not p.is_file():p=ROOT/'src/runtime'/n
        (tmp_path/n).write_bytes(p.read_bytes())
    if mutation:
        filename,a,b=mutation;p=tmp_path/filename;s=p.read_text(encoding='utf-8');assert s.count(a)==1
        p.write_bytes(s.replace(a,b).encode('utf-8'))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/name,extra=[tmp_path/'guest_printf.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=40)
    if mutation:assert run.returncode!=0,'Broken guest printf passed'
    else:assert run.returncode==0,run.stdout+run.stderr

@pytest.mark.parametrize('mutation',[None,
    ('guest_printf.c','if(!pf_read(s,s->cursor,bytes,value))return 0;','if(bytes==8)s->cursor=(s->cursor+7)&~UINT64_C(7);if(!pf_read(s,s->cursor,bytes,value))return 0;'),
    ('guest_printf.c','memcpy(&value.d,&raw,8)','value.d=(double)raw'),
    ('guest_printf.c',"else if(c=='l') {++s->format;","else if(c=='l') {t->bits=64;++s->format;"),
    ('guest_printf.c','copied=(uint32_t)length;','copied=(uint32_t)length;if(copied>511)copied=511;'),
    ('guest_printf.c','if(precision<0)t->have_precision=0','if(precision<0)t->precision=0'),
    ('guest_printf.c','t->bits/8','4'),
    ('guest_printf.c','pointer=pf_span(s,address,bytes,1);if(!pointer)return 0;','pointer=pf_span(s,address,bytes,1);if(!pointer)return 0;pointer[0]=(unsigned char)s->used;'),
    ('guest_printf.c','if(pf_overlap(output,(size_t)state.used+1,state.counts[n].pointer,state.counts[n].bytes))goto done;','if(pf_overlap(output,(size_t)state.used+1,state.counts[n].pointer,state.counts[n].bytes) && !state.count)goto done;'),
    ('guest_printf.c','while(state.used<capacity-1)','while(state.used<capacity)'),
])
def test_mapped_printf_oracles_and_guards(tmp_path,mutation):
    fixture(tmp_path,'printf_contract.c',mutation)

@pytest.mark.parametrize('mutation',[None,
    ('guest_kernel_printf.inc','words[0],words[1],words[2],(uint32_t)tail,NULL','words[0],words[1],words[2],(uint32_t)tail+4,NULL'),
    ('guest_kernel_printf.inc','words[0],65536,words[1],(uint32_t)tail,NULL','words[0],65536,words[1],(uint32_t)tail-4,NULL'),
    ('guest_kernel_printf.inc','words[0],words[1],words[2],words[3],NULL','words[0],words[1],words[2],words[2],NULL'),
    ('guest_kernel_printf.inc','bytes=count*4','bytes=count*2'),
])
def test_actual_printf_import_abi(tmp_path,mutation):
    fixture(tmp_path,'kernel_printf_contract.c',mutation)

@pytest.mark.skipif(os.name!='nt',reason='Microsoft x86 stack oracle requires native Windows/WoW64')
def test_native_x86_variadic_layout(tmp_path):
    compiler,environment=native_compiler(tmp_path)
    x86=Path(compiler).parent.parent/'x86/cl.exe';assert x86.is_file(),'MSVC x86 toolchain required alongside x64'
    environment=dict(environment);environment['PATH']=str(x86.parent)+os.pathsep+environment['PATH']
    environment['LIB']=re.sub(r'(?i)([\\/])x64(?=;|$)',r'\1x86',environment['LIB']);environment['VSCMD_ARG_TGT_ARCH']='x86'
    source=tmp_path/'printf_x86_probe.c';source.write_bytes((HERE/source.name).read_bytes());exe=tmp_path/'x86-oracle.exe'
    compile_=subprocess.run([str(x86),'/nologo','/std:c11','/W4','/WX','/O2',str(source),f'/Fe:{exe}'],cwd=tmp_path,env=environment,capture_output=True,text=True,timeout=90)
    assert compile_.returncode==0,compile_.stdout+compile_.stderr
    run=subprocess.run([str(exe)],capture_output=True,timeout=20)
    assert run.returncode==0,run.stderr.decode(errors='replace')
    assert run.stdout==struct.pack('<IQdI',0x11223344,0x8877665544332211,3.25,0xa1b2c3d4),run.stdout.hex()

def test_selected_printf_provider_and_cdecl_cleanup():
    kernel=ROOT/'external/ts3FPrecomp/src/kernel'
    cmake=(kernel/'CMakeLists.txt').read_text(encoding='utf-8')
    assert cmake.count('${TSFP_SHARED_SOURCE}/runtime/guest_printf.c')==1
    bridge=(kernel/'kernel_bridge.c').read_text(encoding='utf-8')
    assert bridge.count('#include "guest_kernel_printf.inc"')==1
    for name in ('bridge_format_printf','bridge_va_next32','bridge_va_next64'):
        assert name not in bridge
    for ordinal,name in ((361,'RtlSnprintf'),(362,'RtlSprintf'),(363,'RtlVsnprintf'),(364,'RtlVsprintf')):
        assert 'static void bridge_'+name+'(void)' not in bridge
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+0;\s*/\*\s*'+name+r'\b',bridge)
        assert 'case '+str(ordinal)+': return bridge_'+name+';' in bridge
