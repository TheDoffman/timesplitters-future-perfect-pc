"""Original comparison and implicit extension contracts, with real consumers."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from types import SimpleNamespace as S
import pytest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/compiler'))
import translation_compare as own
from translation_decode import Operand
from test_translation_product import _compile
from test_translation_moves import _production_macros


def run_c(tmp_path, code):
    source=tmp_path/'compare.c';source.write_text(code,encoding='utf-8')
    exe=_compile(tmp_path,source)
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr+f' exit={result.returncode}'


def test_invalid_and_normalisation():
    a=Operand('reg',reg='eax');b=Operand('reg',reg='ax')
    for ops in ([],[a],[a,b],[Operand('imm',imm=0),a],[Operand('mem',mem_size=4)]*2,
                [Operand('reg',reg='xmm0'),a]):
        with pytest.raises(ValueError):own.cmp(S(needs_cf=True),S(address=0),ops)
    for name in ('lock cmp','xor'):
        with pytest.raises(ValueError):own.snapshot(None,None,[a,a],name)
    for name,ops in [('cdqe',[]),('cdq',[a]),('lock cdq',[])]:
        with pytest.raises(ValueError):own.extend(None,None,ops,name)
    for reg in ('al','ah','ax','eax'):
        a=Operand('reg',reg=reg);kind,ops=own.normalise_zero_test('test',[a,a])
        assert kind=='cmp' and ops[0] is a and ops[1].type=='imm' and ops[1].imm==0
    ops=[a,b];assert own.normalise_zero_test('test',ops)==('test',ops)


def test_compiled_snapshots_hooks_and_extensions(tmp_path):
    macros=_production_macros({'LO8','HI8','LO16','SET_LO8','SET_HI8','SET_LO16'})
    code='#include <stdint.h>\n#include <stdio.h>\n'+macros+r"""
static uint32_t eax,ebx,ecx,edx,_fa,_fb;static int32_t _fas,_fbs;static int _cf;
static uint32_t memory_value;static unsigned reads,hooks;static uint32_t address,site;
static uint32_t read_value(uint32_t p){++reads;address=p;return memory_value;}
static uint32_t hook(uint32_t s,uint32_t p,uint32_t v){++hooks;site=s;address=p;return v+3;}
#define MEM8(p) ((uint8_t)read_value(p))
#define MEM16(p) ((uint16_t)read_value(p))
#define MEM32(p) read_value(p)
#define RECOMP_LOAD_GLOBAL_U32(s,p,v) hook(s,p,v)
"""
    widths=(8,16,32)
    for bits in widths:
        reg={8:'al',16:'ax',32:'eax'}[bits];src={8:'bl',16:'bx',32:'ebx'}[bits]
        for kind in ('cmp','test'):
            for mode,ops in [('reg',[Operand('reg',reg=reg),Operand('reg',reg=src)]),
                             ('mem',[Operand('mem',mem_base='ecx',mem_size=bits//8),Operand('imm',imm=-1)]),
                             ('global',[Operand('mem',mem_disp=128,mem_size=bits//8),Operand('reg',reg=src)])]:
                body=getattr(own,kind)(S(needs_cf=True),S(address=0x1234),ops)
                code+=f'void {kind}_{mode}{bits}(void){{'+ '\n'.join(body)+'}\n'
    for name in ('cbw','cwd','cwde','cdq'):
        code+=f'void do_{name}(void){{'+ '\n'.join(own.extend(None,None,[],name))+'}\n'
    code+=r"""
static int32_t signed_value(uint32_t value,unsigned bits){int64_t v=value; if(v&(INT64_C(1)<<(bits-1)))v-=(INT64_C(1)<<bits);return (int32_t)v;}
static void (*regops[3][2])(void)={{cmp_reg8,test_reg8},{cmp_reg16,test_reg16},{cmp_reg32,test_reg32}};
static void (*memops[3][2])(void)={{cmp_mem8,test_mem8},{cmp_mem16,test_mem16},{cmp_mem32,test_mem32}};
static void (*globalops[3][2])(void)={{cmp_global8,test_global8},{cmp_global16,test_global16},{cmp_global32,test_global32}};
static int verify(unsigned w,uint32_t a,uint32_t b){unsigned bits=8u<<w;uint32_t mask=bits==32?0xffffffffu:(1u<<bits)-1;
for(unsigned op=0;op<2;++op){eax=a;ebx=b;ecx=128;edx=0x13572468;_cf=1;
regops[w][op]();if(eax!=a||ebx!=b||edx!=0x13572468||_fa!=(a&mask)||_fb!=(b&mask)||_fas!=signed_value(a&mask,bits)||_fbs!=signed_value(b&mask,bits)||_cf!=(op?0:((a&mask)<(b&mask))))return 1;
reads=hooks=0;memory_value=a;memops[w][op]();if(reads!=1||hooks||address!=128||_fa!=(a&mask)||_fb!=mask||_cf!=(op?0:((a&mask)<mask)))return 2;
reads=hooks=0;globalops[w][op]();uint32_t expected=(a+((w==2&&op==0)?3u:0u))&mask;
if(reads!=1||hooks!=(unsigned)(w==2&&op==0)||address!=128||_fa!=expected||_fb!=(b&mask)||_cf!=(op?0:(expected<(b&mask))))return 3;
if(hooks&&site!=0x1234)return 4;
}return 0;}
static int extension(uint32_t a,uint32_t d){_fa=17;_fb=29;_fas=-3;_fbs=-7;_cf=1;
eax=a;edx=d;do_cbw();uint32_t byte=a&255,word=a&65535;
if(eax!=((a&0xffff0000u)|(byte<128?byte:byte+65280))||edx!=d)return 5;
eax=a;edx=d;do_cwd();if(eax!=a||edx!=((d&0xffff0000u)|(word<32768?0u:65535u)))return 6;
eax=a;edx=d;do_cwde();if(eax!=(uint32_t)signed_value(word,16)||edx!=d)return 7;
eax=a;edx=d;do_cdq();if(eax!=a||edx!=(a<0x80000000u?0u:0xffffffffu))return 8;
if(_fa!=17||_fb!=29||_fas!=-3||_fbs!=-7||_cf!=1)return 9;return 0;}
int main(void){for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b){int r=verify(0,0xabcd0000u|a,0x12340000u|b);if(r)return r;}
for(unsigned a=0;a<65536;++a){int r=extension(0x89ab0000u|a,0xcdef1234);if(r)return r;r=extension(a,0x1234abcd);if(r)return r;}
uint32_t seed=1234567;for(unsigned i=0;i<10000;++i){seed=seed*1664525u+1013904223u;uint32_t a=seed;seed=seed*1664525u+1013904223u;
for(unsigned w=1;w<3;++w){int r=verify(w,a,seed);if(r)return r;}int r=extension(a,seed);if(r)return r;}
puts("PASS snapshots, widths, carry, hooks, single reads, implicit extension and preserved state");return 0;}
"""
    run_c(tmp_path,code)


def test_independent_copy(tmp_path):
    for name in ('translation_compare.py','translation_decode.py','translation_integer.py','translation_operands.py','x86_decode.py'):
        shutil.copyfile(ROOT/'tools/compiler'/name,tmp_path/name)
    result=subprocess.run([sys.executable,'-I','-c',"import sys;sys.path.insert(0,sys.argv[1]);import translation_compare as c;assert c.extend(None,None,[],'cwd');assert not any(n.startswith('tools.recomp') for n in sys.modules)",str(tmp_path)],capture_output=True,text=True)
    assert result.returncode==0,result.stderr


def test_production_consumers(tmp_path):
    script="""import json
from tools.recomp.lifter import Lifter,normalise_zero_test
from tools.recomp.translator import FunctionTranslator
import translation_compare as c
assert Lifter._lift_cmp is c.cmp and Lifter._lift_test is c.test and Lifter._snapshot_flags is c.snapshot
assert normalise_zero_test is c.normalise_zero_test
cases={'cmp_after':'6639d8b800000000bb000000000f94c10f9cc50f92c20f97c6c3',
       'test_after':'85d8b800000000bb000000000f94c10f98c50f92c2c3',
       'borrow':'39d819c9c3','clear':'85d819c9c3',
       'cbw_probe':'6698c3','cwd_probe':'6699c3','cwde_probe':'98c3','cdq_probe':'99c3',
       'join_probe':'85f6740485c0eb053d000000000f94c3c3'}
out={}
for name,raw in cases.items():
 data=bytes.fromhex(raw);info={'name':name,'size':len(data)};t=FunctionTranslator(b'',{0x1000:info});t._read_func_bytes=lambda a,b:data
 out[name]=t.translate_function(0x1000,info)
print(json.dumps(out))
"""
    r=subprocess.run([sys.executable,'-c',script],cwd=ROOT/'external/ts3FPrecomp',env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=30)
    assert r.returncode==0,r.stdout+r.stderr
    macros=_production_macros({'LO8','HI8','LO16','SET_LO8','SET_HI8','SET_LO16',})
    code='#include <stdint.h>\n#include <stdio.h>\n'+macros+r"""
static uint32_t eax,ebx,ecx,edx,esi,edi,esp,g_ebp,g_seh_ebp;
#define RECOMP_FAST_ENTRY(site) 0
"""+'\n'.join(json.loads(r.stdout).values())+r"""
int main(void){const uint32_t v[]={0,1,127,128,255,32767,32768,65535,0x7fffffffu,0x80000000u,0xffffffffu};
for(unsigned i=0;i<11;++i)for(unsigned j=0;j<11;++j){uint32_t a=v[i],b=v[j];
eax=a;ebx=b;ecx=edx=0;cmp_after();if((ecx&255u)!=((uint16_t)a==(uint16_t)b)||((ecx>>8)&255u)!=((int16_t)a<(int16_t)b)||(edx&255u)!=((uint16_t)a<(uint16_t)b)||((edx>>8)&255u)!=((uint16_t)a>(uint16_t)b))return 1;
eax=a;ebx=b;ecx=edx=0;test_after();if((ecx&255u)!=((a&b)==0)||((ecx>>8)&255u)!=((a&b)>>31)||edx)return 2;
eax=a;ebx=b;ecx=17;borrow();if(ecx!=(a<b?0xffffffffu:0))return 3;
eax=a;ebx=b;ecx=17;clear();if(ecx)return 4;
eax=a;edx=b;cwd_probe();if(eax!=a||edx!=((b&0xffff0000u)|((a&32768u)?65535u:0)))return 5;
eax=a;edx=b;cdq_probe();if(eax!=a||edx!=(a>>31?0xffffffffu:0))return 6;
eax=a;cbw_probe();if(eax!=((a&0xffff0000u)|(uint16_t)(int16_t)(int8_t)a))return 7;
eax=a;cwde_probe();if(eax!=(uint32_t)(int32_t)(int16_t)a)return 8;
for(esi=0;esi<2;++esi){eax=a;ebx=123;join_probe();if((ebx&255u)!=(a==0))return 9;}
}puts("PASS decoded compare/test consumers and implicit extensions");return 0;}
"""
    run_c(tmp_path,code)


@pytest.mark.skipif(os.name!='nt',reason='Supplemental Windows x64 instruction oracle')
def test_extensions_against_x86(tmp_path):
    from native_compiler import native_compiler
    compiler,env=native_compiler(tmp_path)
    assembly=['.code'];code='#include <stdint.h>\n#include <stdio.h>\n'
    code+='typedef struct {uint32_t a,d,before,after;} State;\n'
    for name in ('cbw','cwd','cwde','cdq'):
        assembly += [f'public hw_{name}',f'hw_{name} proc','mov r10,rcx','mov eax,edx','mov edx,r8d',
                     'cmp r9d,1','pushfq','pop r11','mov dword ptr [r10+8],r11d',name,
                     'pushfq','pop r11','mov dword ptr [r10],eax','mov dword ptr [r10+4],edx',
                     'mov dword ptr [r10+12],r11d','ret',f'hw_{name} endp']
        code+=f'extern void hw_{name}(State*,uint32_t,uint32_t,uint32_t);\n'
        code+=f'static State own_{name}(uint32_t eax,uint32_t edx){{'+'\n'.join(own.extend(None,None,[],name))+'State s={eax,edx,0,0};return s;}\n'
    assembly+=['end'];asm=tmp_path/'reference.asm';asm.write_text('\n'.join(assembly))
    obj=tmp_path/'reference.obj'
    r=subprocess.run([str(Path(compiler).with_name('ml64.exe')),'/nologo','/c',f'/Fo{obj}',str(asm)],env=env,capture_output=True,text=True,timeout=60)
    assert r.returncode==0,r.stdout+r.stderr
    code+='static void (*hw[])(State*,uint32_t,uint32_t,uint32_t)={hw_cbw,hw_cwd,hw_cwde,hw_cdq};\n'
    code+='static State (*fn[])(uint32_t,uint32_t)={own_cbw,own_cwd,own_cwde,own_cdq};\n'
    code+=r"""
int main(void){for(unsigned n=0;n<65536;++n)for(unsigned sign=0;sign<2;++sign){uint32_t a=(sign?0xabcd0000u:0x12340000u)|n,d=0x55aa1234u;
for(unsigned op=0;op<4;++op){State ref,got=fn[op](a,d);hw[op](&ref,a,d,n%3);if(got.a!=ref.a||got.d!=ref.d||((ref.before^ref.after)&0x8d5u))return (int)op+1;}}
puts("PASS actual x86 implicit-extension values and preserved arithmetic flags");return 0;}
"""
    src=tmp_path/'oracle.c';src.write_text(code,encoding='utf-8');exe=_compile(tmp_path,src,objects=[str(obj)])
    r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert r.returncode==0,r.stdout+r.stderr
