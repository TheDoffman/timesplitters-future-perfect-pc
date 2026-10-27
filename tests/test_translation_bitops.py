"""Independent bit-operation contracts with compiled and hardware references."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from types import SimpleNamespace

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import translation_bitops as bits
from translation_decode import Operand
from native_compiler import native_compiler
from test_translation_product import _compile
from test_translation_moves import _production_macros


def test_invalid_forms_and_defined_conditions():
    c = SimpleNamespace(needs_cf=True)
    for args in ([], [Operand('reg', reg='al'), Operand('reg', reg='cl')],
                 [Operand('reg', reg='ax'), Operand('reg', reg='ecx')],
                 [Operand('mem', mem_size=4), Operand('mem', mem_size=4)]):
        with pytest.raises(ValueError): bits.bit_test(c, None, args, 'bt')
    with pytest.raises(ValueError): bits.swap(c, None, [Operand('reg', reg='ax')])
    with pytest.raises(ValueError): bits.scan(c, None, [Operand('reg', reg='eax'), Operand('imm', imm=1)], 'bsf')
    with pytest.raises(ValueError): bits.bit_test(c, None, [Operand('reg', reg='eax'), Operand('imm', imm=0)], 'lock bts')
    for op in ('bt','bts','btr','btc'):
        assert bits.condition('jb',op)=='_cf'
        assert bits.condition('jae',op)=='!_cf'
        assert bits.condition('je',op) is None
    for op in ('bsf','bsr'):
        assert bits.condition('je',op)=='(_fa == 0)'
        assert bits.condition('jne',op)=='(_fa != 0)'
        assert bits.condition('jb',op) is None


def test_compiled_register_memory_alias_and_scan_contracts(tmp_path):
    context=SimpleNamespace(needs_cf=True)
    functions=[]; rows=[]; memrows=[]; scanrows=[]; hwrows=[]; hwscans=[]; hwmemory=[]
    asm=['.code']; declarations=[]
    def function(name, lines):
        functions.append('static void '+name+'(void) {\n'+'\n'.join(lines)+'\n}\n')
    def hardware(name, instruction):
        asm.extend([f'public {name}',f'{name} proc','mov r10,rcx','mov eax,edx','mov ecx,r8d',instruction,
                    'setc r8b','setz r9b','mov dword ptr [r10],eax','movzx r8d,r8b',
                    'mov dword ptr [r10+4],r8d','movzx r9d,r9b','mov dword ptr [r10+8],r9d','ret',f'{name} endp'])
        declarations.append(f'extern void {name}(State*,uint32_t,uint32_t);')
    for width in (16,32):
        a='ax' if width==16 else 'eax'; b='cx' if width==16 else 'ecx'
        row=[]; mrow=[]; srow=[]; hrow=[]; hsrow=[]; hmrow=[]
        for op in ('bt','bts','btr','btc'):
            name=f'{op}{width}';row.append(name);hrow.append('hw_'+name)
            function(name,bits.bit_test(context,None,[Operand('reg',reg=a),Operand('reg',reg=b)],op))
            hardware('hw_'+name,f'{op} {a},{b}')
            name=f'mem_{op}{width}';mrow.append(name)
            function(name,bits.bit_test(context,None,[Operand('mem',mem_base='ebx',mem_size=width//8),Operand('reg',reg=b)],op))
            hname='hw_'+name;hmrow.append(hname);size='word' if width==16 else 'dword'
            asm.extend([f'public {hname}',f'{hname} proc','mov r10,rcx','mov ecx,r8d',
                        f'{op} {size} ptr [rdx],{b}','setc al','movzx eax,al','mov dword ptr [r10],eax','ret',f'{hname} endp'])
            declarations.append(f'extern void {hname}(unsigned*,unsigned char*,uint32_t);')
            for immediate in (0,15,16,31,32,127,128,255):
                function(f'imm_{op}{width}_{immediate}',bits.bit_test(context,None,[Operand('mem',mem_base='ebx',mem_size=width//8),Operand('imm',imm=immediate)],op))
        for op in ('bsf','bsr'):
            name=f'{op}{width}';srow.append(name);hsrow.append('hw_'+name)
            function(name,bits.scan(context,None,[Operand('reg',reg=a),Operand('reg',reg=b)],op))
            hardware('hw_'+name,f'{op} {a},{b}')
            function('alias_'+name,bits.scan(context,None,[Operand('reg',reg=a),Operand('reg',reg=a)],op))
            function('mem_'+name,bits.scan(context,None,[Operand('reg',reg=a),Operand('mem',mem_base='eax',mem_size=width//8)],op))
        function(f'alias_btc{width}',bits.bit_test(context,None,[Operand('reg',reg=a),Operand('reg',reg=a)],'btc'))
        rows.append('{'+','.join(row)+'}');memrows.append('{'+','.join(mrow)+'}');scanrows.append('{'+','.join(srow)+'}')
        hwrows.append('{'+','.join(hrow)+'}');hwscans.append('{'+','.join(hsrow)+'}')
        hwmemory.append('{'+','.join(hmrow)+'}')
    function('swap32',bits.swap(context,None,[Operand('reg',reg='eax')]))
    hardware('hw_swap32','bswap eax');asm.append('end');objects=[]
    if os.name=='nt':
        compiler,env=native_compiler(tmp_path);assembler=Path(compiler).with_name('ml64.exe')
        src=tmp_path/'oracle.asm';src.write_text('\n'.join(asm),encoding='utf-8');obj=tmp_path/'oracle.obj'
        r=subprocess.run([str(assembler),'/nologo','/c',f'/Fo{obj}',str(src)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
        assert r.returncode==0,r.stdout+r.stderr
        objects=[str(obj)]
    macros=_production_macros({'LO16','SET_LO16'})
    code='''#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct {uint32_t a,cf,zf;} State;
static uint32_t eax,ecx,ebx,_fa,_fb;static int32_t _fas,_fbs;static int _cf;
static union {uint32_t alignment;unsigned char bytes[256];} memory,oracle;
static unsigned reads;static uint32_t last_address;
static unsigned address_only;
static void *mapped(uint32_t address,unsigned width) {last_address=address;++reads;if(address_only)return memory.bytes;if(address+width>256)abort();return memory.bytes+address;}
#define MEM16(a) (*(uint16_t*)mapped((uint32_t)(a),2))
#define MEM32(a) (*(uint32_t*)mapped((uint32_t)(a),4))
'''+macros+'\n'+'\n'.join(functions)+'\n'
    code+='typedef void (*Fn)(void);\nstatic Fn ops[2][4]={'+','.join(rows)+'};\n'
    code+='static Fn memops[2][4]={'+','.join(memrows)+'};\nstatic Fn scans[2][2]={'+','.join(scanrows)+'};\n'
    immediate_rows=[]
    for width in (16,32):
        immediate_rows.append('{'+','.join('{'+','.join(f'imm_{op}{width}_{i}' for i in (0,15,16,31,32,127,128,255))+'}' for op in ('bt','bts','btr','btc'))+'}')
    code+='static Fn immediates[2][4][8]={'+','.join(immediate_rows)+'};\n'
    if os.name=='nt':
        code+='\n'.join(declarations)+'\ntypedef void (*HW)(State*,uint32_t,uint32_t);\n'
        code+='static HW hw[2][4]={'+','.join(hwrows)+'};\nstatic HW hs[2][2]={'+','.join(hwscans)+'};\n'
        code+='typedef void (*HM)(unsigned*,unsigned char*,uint32_t);\nstatic HM hm[2][4]={'+','.join(hwmemory)+'};\n'
    code+='''static uint32_t random_value=1;
static uint32_t next_value(void){random_value=random_value*1664525u+1013904223u;return random_value;}
static uint32_t change(uint32_t v,unsigned bit,unsigned op){uint32_t m=1u<<bit;return op==0?v:op==1?v|m:op==2?v&~m:v^m;}
static uint32_t load(unsigned at,unsigned width){uint32_t v=0;memcpy(&v,memory.bytes+at,width);return v;}
int main(void){
for(unsigned w=0;w<2;++w){unsigned width=w?32:16;uint32_t mask=w?0xffffffffu:0xffffu;
 for(unsigned n=0;n<65536;++n){uint32_t a=w?next_value():0xbeef0000u|n,b=w?next_value():n;
  for(unsigned op=0;op<4;++op){eax=a;ecx=b;_cf=9;_fa=123;_fb=456;_fas=12;_fbs=45;ops[w][op]();
   uint32_t result=(a&~mask)|(change(a&mask,b&(width-1),op)&mask);
   if(eax!=result||_cf!=((a>>(b&(width-1)))&1u)||_fa!=123||_fb!=456||_fas!=12||_fbs!=45)return 1;
'''
    if os.name=='nt':code+='   State ref;hw[w][op](&ref,a,b);if(ref.a!=eax||ref.cf!=(unsigned)_cf)return 2;\n'
    code+='''  }
  for(unsigned reverse=0;reverse<2;++reverse){eax=a;ecx=b;_cf=7;scans[w][reverse]();uint32_t v=b&mask;unsigned index=0;
   if(v){if(reverse){for(unsigned k=0;k<width;++k)if(v&(1u<<k))index=k;}else{while(!(v&(1u<<index)))++index;}}
   uint32_t result=v?((a&~mask)|index):a;
   if(eax!=result||_fa!=v||_fb||_cf!=7)return 3;
'''
    if os.name=='nt':code+='   State ref;hs[w][reverse](&ref,a,b);if(ref.zf!=(unsigned)(v==0)||(v&&ref.a!=eax))return 4;\n'
    code+='''  }
  eax=a;ecx=a;if(w)alias_btc32();else alias_btc16();if(eax!=((a&~mask)|(change(a&mask,a&(width-1),3)&mask)))return 5;
 }
 for(int offset=-192;offset<=192;++offset)for(unsigned op=0;op<4;++op){
  memset(memory.bytes,0xa5,sizeof(memory.bytes));unsigned char expected[256];memcpy(expected,memory.bytes,256);memcpy(oracle.bytes,memory.bytes,256);
  int word=offset>=0?offset/(int)width:-((-offset+(int)width-1)/(int)width);
  unsigned address=128+word*(width/8),bit=(unsigned)offset&(width-1);uint32_t old=load(address,width/8),value=change(old,bit,op);
  memcpy(expected+address,&value,width/8);ebx=128;ecx=(uint32_t)offset;reads=0;memops[w][op]();
  if(memcmp(expected,memory.bytes,256)||last_address!=address||reads!=(op?2u:1u)||_cf!=((old>>bit)&1u))return 6;
'''
    if os.name=='nt':code+='  unsigned carry=9;hm[w][op](&carry,oracle.bytes+128,(uint32_t)offset);if(memcmp(oracle.bytes,memory.bytes,256)||carry!=(unsigned)_cf)return 11;\n'
    code+=''' }
 const uint32_t extremes[]={0,15,16,31,32,0x7fff,0x8000,0xffff,0x7fffffff,0x80000000,0xffffffff};
 for(unsigned i=0;i<11;++i){uint32_t off=extremes[i]&mask;int64_t signedoff=(int64_t)off-((off&(1u<<(width-1)))?(1ll<<width):0);
  int64_t word=signedoff>=0?signedoff/width:-((-signedoff+width-1)/width);uint32_t base=0xfffffff0u;
  ebx=base;ecx=extremes[i];address_only=1;memops[w][0]();address_only=0;
  if(last_address!=(uint32_t)((uint64_t)base+(uint64_t)(word*(width/8))))return 12;
 }
 const unsigned imm[]={0,15,16,31,32,127,128,255};
 for(unsigned op=0;op<4;++op)for(unsigned i=0;i<8;++i){memset(memory.bytes,0xa5,256);uint32_t old=load(128,width/8);ebx=128;reads=0;immediates[w][op][i]();
  if(last_address!=128||load(128,width/8)!=(change(old,imm[i]&(width-1),op)&mask))return 7;}
 for(unsigned n=0;n<1000;++n){uint32_t source=next_value();memcpy(memory.bytes+128,&source,width/8);eax=128;reads=0;if(w)mem_bsf32();else mem_bsf16();
  if(reads!=1||last_address!=128||_fa!=(source&mask))return 8;}
}
for(unsigned n=0;n<10000;++n){uint32_t a=next_value();eax=a;_cf=7;_fa=12;swap32();uint32_t result=0;for(unsigned b=0;b<4;++b)result|=((a>>(b*8))&255u)<<((3-b)*8);
 if(eax!=result||_cf!=7||_fa!=12)return 9;
'''
    if os.name=='nt':code+=' State ref;hw_swap32(&ref,a,0);if(ref.a!=eax)return 10;\n'
    code+='}puts("PASS: bit widths, signed memory offsets, immediates, aliases, scan zero policy, swaps and native references");return 0;}\n'
    src=tmp_path/'bitops.c';src.write_text(code,encoding='utf-8');exe=_compile(tmp_path,src,objects=objects)
    r=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=45)
    assert r.returncode==0,r.stdout+r.stderr+f' exit={r.returncode}'


def test_copied_bitops_module_runs_without_runtime(tmp_path):
    for name in ('translation_bitops.py','translation_integer.py','translation_operands.py'):
        shutil.copyfile(ROOT/'tools'/name,tmp_path/name)
    script="""import sys
sys.path.insert(0,sys.argv[1])
from types import SimpleNamespace as S
import translation_bitops as b
assert b.swap(None,None,[S(type='reg',reg='eax')])
assert not any(n.startswith('tools.recomp') for n in sys.modules)
"""
    r=subprocess.run([sys.executable,'-I','-c',script,str(tmp_path)],cwd=tmp_path,capture_output=True,text=True,timeout=15)
    assert r.returncode==0,r.stdout+r.stderr


def test_decoded_consumers_keep_carry_and_zero_snapshots(tmp_path):
    runtime=ROOT/'external/ts3FPrecomp'
    script='''import json
from tools.recomp.lifter import Lifter
from tools.recomp.translator import FunctionTranslator
import translation_bitops as b
assert Lifter._lift_bit_test is b.bit_test and Lifter._lift_scan is b.scan and Lifter._lift_swap is b.swap
cases={
 'bt_overwrite':'0fa3c8b800000000b91f0000000f92c30f93c7c3',
 'bts_branch':'0fabc87206bb00000000c3bb01000000c3',
 'btc_cmov':'0fbbc80f42dac3',
 'scan_word':'660fbcc1b9000000000f94c30f95c7c3',
 'scan_reverse':'0fbdc1b9000000000f94c30f95c7c3',
 'memory_bit':'660fa30bb900000000c703000000000f92c2c3',
 'swap_probe':'0fc8c3'}
out={}
for name,raw in cases.items():
 data=bytes.fromhex(raw);info={'name':name,'size':len(data)}
 t=FunctionTranslator(b'',{0x1000:info});t._read_func_bytes=lambda a,b:data
 out[name]=t.translate_function(0x1000,info)
print(json.dumps(out))
'''
    r=subprocess.run([sys.executable,'-c',script],cwd=runtime,env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=30)
    assert r.returncode==0,r.stdout+r.stderr
    bodies=json.loads(r.stdout)
    macros=_production_macros({'LO8','HI8','LO16','SET_LO8','SET_HI8','SET_LO16'})
    code='''#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t eax,ebx,ecx,edx,esp,g_ebp,g_seh_ebp;
static union{uint32_t align;unsigned char bytes[128];} memory;
#define MEM16(a) (*(uint16_t*)(memory.bytes+(uint32_t)(a)))
#define MEM32(a) (*(uint32_t*)(memory.bytes+(uint32_t)(a)))
#define RECOMP_FAST_ENTRY(site) 0
#define RECOMP_AFTER_STORE32(address,value) ((void)0)
'''+macros+'\n'+'\n'.join(bodies.values())+'''
int main(void){
for(unsigned bit=0;bit<32;++bit)for(unsigned set=0;set<2;++set){uint32_t a=set?(1u<<bit):0;
 eax=a;ecx=bit;ebx=0xabcd0000;bt_overwrite();if((ebx&255u)!=set||((ebx>>8)&255u)!=(1-set))return 1;
 eax=a;ecx=bit;ebx=99;bts_branch();if(ebx!=set||eax!=(1u<<bit))return 2;
 eax=a;ecx=bit;edx=73;ebx=11;btc_cmov();if(ebx!=(set?73u:11u)||eax!=(a^(1u<<bit)))return 3;
}
const uint32_t values[]={0,1,2,0x8000u,0x10000u,0x80000000u,0xffffffffu};
for(unsigned i=0;i<7;++i){unsigned v=values[i]&0xffffu,k=0;while(k<16&&!(v&(1u<<k)))++k;
 eax=0xabcd1234;ecx=values[i];ebx=0;scan_word();if(eax!=(v?(0xabcd0000u|k):0xabcd1234u)||(ebx&255u)!=(v==0)||((ebx>>8)&255u)!=(v!=0))return 4;
 eax=0xabcd1234;ecx=values[i];ebx=0;scan_reverse();k=0;for(unsigned b=0;b<32;++b)if(values[i]&(1u<<b))k=b;
 if(eax!=(values[i]?k:0xabcd1234u)||(ebx&255u)!=(values[i]==0))return 5;
}
for(unsigned set=0;set<2;++set){memset(memory.bytes,0,128);memory.bytes[62]=set?0x80:0;ebx=64;ecx=0xfffffff7u;edx=0;memory_bit();if((edx&255u)!=set)return 6;}
eax=0x01234567;swap_probe();if(eax!=0x67452301)return 7;
puts("PASS: decoded carry branches, SET/CMOV, operand overwrites, scan zero and swap");return 0;}
'''
    source=tmp_path/'consumers.c';source.write_text(code,encoding='utf-8');exe=_compile(tmp_path,source)
    r=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=30)
    assert r.returncode==0,r.stdout+r.stderr+f' exit={r.returncode}'
