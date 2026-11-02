"""Real production comparison functions against IEEE relations and raw x86 flags."""
from pathlib import Path
import importlib.util,math,os,random,struct,subprocess
import pytest
from test_guest_status_flags import ROOT,STATUS
from test_translation_product import _compile
from test_translation_function import translator
from test_status_flags_translation import execute
from native_compiler import native_compiler

CASES=[(name,kind) for name in ('comiss','ucomiss','comisd','ucomisd') for kind in ('reg','mem')]
CASES += [(name,'x87') for name in ('fcomi','fcomip','fucomi','fucomip','fcom','fcomp','fcompp','fucom','fucomp','fucompp','fxam')]


def raw(name,kind):
    if kind!='x87':return ('66' if name.endswith('sd') else '')+'0f'+('2e' if name.startswith('u') else '2f')+('c1' if kind=='reg' else '03')+'c3'
    return dict(fcomi='dbf1',fcomip='dff1',fucomi='dbe9',fucomip='dfe9',fcom='d8d1',fcomp='d8d9',fcompp='ded9',fucom='dde1',fucomp='dde9',fucompp='dae9',fxam='d9e5')[name]+'c3'


def build(tmp_path,mutation=None):
    if mutation:
        import translation_compare as original
        old,new=mutation;text=Path(original.__file__).read_text(encoding='utf-8');assert text.count(old)==1
        path=tmp_path/'mutated_compare.py';path.write_bytes(text.replace(old,new).encode());spec=importlib.util.spec_from_file_location('mutated_compare',path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        saved=original.floating_status;original.floating_status=module.floating_status
    try:
        functions=[]
        for i,(name,kind) in enumerate(CASES):
            t,info=translator(raw(name,kind));info['name']='op'+str(i);functions.append(t.translate_function(0x1000,info))
    finally:
        if mutation:original.floating_status=saved
    for name in ('recomp_types.h','guest_status_flags.h','guest_state.c'):(tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    source=tmp_path/'float_compare.c';source.write_bytes(('''#define RECOMP_GENERATED_CODE 1
#define RECOMP_FAST_ENTRY(site) 0
#include "recomp_types.h"
#include <stdio.h>
#include <assert.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
ptrdiff_t g_xbox_mem_offset;
static union {uint64_t align;unsigned char data[1024];} memory;
'''+ '\n'.join(functions)+'''
int main(void) {
 uint32_t p[7],out[13];unsigned char before[64];void(*ops[])(void)={'''+','.join('op'+str(i) for i in range(len(CASES)))+'''};
#ifdef _WIN32
 assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
#endif
 g_xbox_mem_offset=(ptrdiff_t)memory.data;
 while(fread(p,1,sizeof(p),stdin)==sizeof(p)) {
  assert(p[0]<sizeof(ops)/sizeof(ops[0]));g_esp=100;g_ebx=256;memcpy(memory.data+256,p+3,8);
  g_status_flags.value=p[5];g_status_flags.known=p[6];g_fp_top=0;g_fp_cc=0x4500;g_fp_cmp=-1;
  for(unsigned i=0;i<8;++i)g_fp_stack[i]=100.125+i;
  memcpy(g_fp_stack,p+1,8);memcpy(g_fp_stack+1,p+3,8);memcpy(before,g_fp_stack,64);
  memcpy(g_xmm0.u,p+1,8);memcpy(g_xmm1.u,p+3,8);g_xmm0.q[1]=UINT64_C(0x1122334455667788);g_xmm1.q[1]=UINT64_C(0x8877665544332211);
  ops[p[0]]();assert(g_esp==104);out[0]=g_status_flags.value;out[1]=g_status_flags.known;out[2]=(uint32_t)g_fp_top;out[3]=g_fp_cc;
  memcpy(out+4,g_xmm0.u,16);memcpy(out+8,g_xmm1.u,16);out[12]=!memcmp(before,g_fp_stack,64);
  assert(fwrite(out,1,sizeof(out),stdout)==sizeof(out));
 }
 return 0;
}
''').encode());flag='/DTSFP_RECORD_STATUS_FLAGS=1' if os.name=='nt' else '-DTSFP_RECORD_STATUS_FLAGS=1'
    return _compile(tmp_path,source,extra=[tmp_path/'guest_state.c',flag])


def model(p):
    index,al,ah,bl,bh,value,known=p;name,kind=CASES[index]
    single=name.endswith('ss');a=struct.unpack('<f' if single else '<d',struct.pack('<I',al) if single else struct.pack('<2I',al,ah))[0];b=struct.unpack('<f' if single else '<d',struct.pack('<I',bl) if single else struct.pack('<2I',bl,bh))[0]
    unordered=math.isnan(a) or math.isnan(b);less=a<b;equal=a==b
    cc=0x4500;top=2 if name in ('fcompp','fucompp') else int(name in ('fcomip','fucomip','fcomp','fucomp'))
    if kind!='x87' or name in ('fcomi','fcomip','fucomi','fucomip'):
        flags=0x45 if unordered else 1 if less else 0x40 if equal else 0
        value=value&~STATUS|flags|2;known|=STATUS|2
    elif name=='fxam':
        cc=(0x100 if math.isnan(a) else 0x500 if math.isinf(a) else 0x4000 if a==0 else 0x400)|(0x200 if ah>>31 else 0)
    else:cc=0x4500 if unordered else 0x100 if less else 0x4000 if equal else 0
    return (value&0xffffffff,known&0xffffffff,top,cc,al,ah,0x55667788,0x11223344,bl,bh,0x44332211,0x88776655,1)


def vectors(random_count=7000):
    f=(0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x3f800000,0xbf800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f800001,0xffc12345,0xff800001)
    d=(0,0x8000000000000000,1,0x8000000000000001,0x000fffffffffffff,0x0010000000000000,0x3ff0000000000000,0xbff0000000000000,0x7fefffffffffffff,0xffefffffffffffff,0x7ff0000000000000,0xfff0000000000000,0x7ff8123456789abc,0x7ff0000000000001,0xfff8123456789abc,0xfff0000000000001)
    rng=random.Random(251)
    for i,(name,_) in enumerate(CASES):
        for a in f if name.endswith('ss') else d:
            for b in f if name.endswith('ss') else d:
                for value in (2,STATUS|2):yield i,a&0xffffffff,a>>32,b&0xffffffff,b>>32,value,STATUS|2
    for _ in range(random_count):
        i=rng.randrange(len(CASES));a=rng.getrandbits(64);b=rng.getrandbits(64)
        yield i,a&0xffffffff,a>>32,b&0xffffffff,b>>32,rng.getrandbits(32)|2,rng.getrandbits(32)|2


def run(exe,data):return subprocess.run([str(exe)],input=b''.join(struct.pack('<7I',*p) for p in data),capture_output=True,timeout=60)


def test_production_ieee_comparison_status_and_preserved_state(tmp_path):
    data=list(vectors());r=run(build(tmp_path),data);assert r.returncode==0,r.stderr.decode(errors='replace');assert len(r.stdout)==52*len(data)
    for i,p in enumerate(data):assert struct.unpack_from('<13I',r.stdout,i*52)==model(p),(i,p,CASES[p[0]])


@pytest.mark.parametrize('mutation',[
    ('TSFP_FLAG_CF | TSFP_FLAG_PF | TSFP_FLAG_ZF','TSFP_FLAG_CF | TSFP_FLAG_ZF'),
    ("? TSFP_FLAG_CF : (({left}) == ({right}))", "? TSFP_FLAG_ZF : (({left}) == ({right}))"),
    ('TSFP_STATUS_FLAGS,{value},0','TSFP_FLAG_CF | TSFP_FLAG_PF | TSFP_FLAG_ZF,{value},0'),
])
def test_compiled_wrong_float_flags_fail(tmp_path,mutation):
    data=list(vectors(0));r=run(build(tmp_path,mutation),data);assert r.returncode==0
    assert any(struct.unpack_from('<13I',r.stdout,i*52)!=model(p) for i,p in enumerate(data))


def test_unordered_status_survives_register_overwrite_and_reaches_branch(tmp_path):
    execute(tmp_path,'0f2ec10f57c07a06b801000000c3b802000000c3',
            'g_esp=100;g_xmm0.u[0]=0x7fc00001;g_xmm1.u[0]=0x3f800000;g_status_flags.value=0x8d7;g_status_flags.known=0x8d7;',
            'CHECK(g_eax==2);CHECK(g_status_flags.value==0x47);CHECK(g_status_flags.known==0x8d7);')


@pytest.mark.skipif(os.name!='nt',reason='Actual SSE/x87 flags with masked exceptions')
def test_native_float_comparison_flags(tmp_path):
    asm=['.data','quiet_nan dq 07ff8000000000000h','.code'];decl=[]
    for i,(name,kind) in enumerate(CASES):
        body=['fninit','fld qword ptr [quiet_nan]','fucom st(0)','fstp st(0)','fld qword ptr [rcx+12]','fld qword ptr [rcx+4]','movq xmm0,qword ptr [rcx+4]','movq xmm1,qword ptr [rcx+12]',
              'pushfq','pop r10','and r10,-2262','mov r11d,[rcx+20]','and r11d,2261','or r10,r11','push r10','popfq']
        if kind=='x87':instruction=f'{name} st(0),st(1)' if name in ('fcomi','fcomip','fucomi','fucomip') else name if name in ('fxam','fcompp','fucompp') else f'{name} st(1)'
        else:instruction=f'{name} xmm0,'+('xmm1' if kind=='reg' else ('qword' if name.endswith('sd') else 'dword')+' ptr [rcx+12]')
        pops=2 if name in ('fcompp','fucompp') else int(name in ('fcomip','fucomip','fcomp','fucomp'))
        body += [instruction,'pushfq','pop r11','fnstsw ax','movzx eax,ax','shl rax,32','or rax,r11',*(['fstp st(0)']*(2-pops)),'fnclex','ret']
        asm += [f'public hw{i}',f'hw{i} proc',*body,f'hw{i} endp'];decl.append(f'extern uint64_t hw{i}(const uint32_t*);')
    asm+=['end'];compiler,env=native_compiler(tmp_path);source=tmp_path/'hardware.asm';obj=tmp_path/'hardware.obj';source.write_bytes(('\n'.join(asm)+'\n').encode())
    r=subprocess.run([str(Path(compiler).with_name('ml64.exe')),'/nologo','/c',f'/Fo{obj}',str(source)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60);assert r.returncode==0,r.stdout+r.stderr
    source=tmp_path/'oracle.c';source.write_bytes(('''#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <io.h>
#include <fcntl.h>
#include <xmmintrin.h>
'''+ '\n'.join(decl)+'''
int main(void) {uint32_t p[7];uint64_t out;uint64_t(*ops[])(const uint32_t*)={'''+','.join('hw'+str(i) for i in range(len(CASES)))+'''};
assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);_mm_setcsr(0x1f80);
while(fread(p,1,sizeof(p),stdin)==sizeof(p)) {out=ops[p[0]](p);assert(fwrite(&out,1,sizeof(out),stdout)==sizeof(out));}return 0;}
''').encode());exe=_compile(tmp_path,source,objects=[str(obj)])
    data=list(vectors());hw=run(exe,data);actual=run(build(tmp_path),data);assert hw.returncode==actual.returncode==0;assert len(hw.stdout)==8*len(data) and len(actual.stdout)==52*len(data)
    for i,p in enumerate(data):
        value,known,top,cc=struct.unpack_from('<4I',actual.stdout,i*52);flags,status=struct.unpack_from('<2I',hw.stdout,i*8)
        assert value&known&STATUS==flags&known&STATUS,(i,p,value,known,flags)
        assert top==(((status>>11)&7)-6)%8,(i,p,top,status)
        # The scalar SSE forms do not touch the x87 stack/status either.
        assert cc&0x4700==status&0x4700,(i,p,cc,status)
