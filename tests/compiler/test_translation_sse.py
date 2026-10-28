"""Independent lane/bit references and hardware comparisons for SSE emission."""
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
import translation_sse as own
from translation_decode import Operand
from test_translation_product import _compile


def reg(name):return Operand('reg',reg=name)
def mem(size):return Operand('mem',mem_base='eax',mem_size=size)
def emit(name,operands):
    return own.emit(S(_unimplemented=lambda *_:['UNSUPPORTED;']),S(address=0x1234),name,operands)


_PRELUDE=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
#include <limits.h>
typedef union {float f[4];double d[2];uint32_t u[4];int32_t i[4];uint64_t q[2];} RecompXmm;
static RecompXmm xmm0,xmm1;
static uint32_t eax,ebx,g_ebp;static double _fca,_fcb;
static uint32_t memory[16];static unsigned reads[16],writes[16],hooks;
static uint32_t read32(uint32_t p){if(p%4||p>=64)abort();reads[p/4]++;return memory[p/4];}
static uint32_t *write32(uint32_t p){if(p%4||p>=64)abort();writes[p/4]++;return &memory[p/4];}
#define MEM32(p) read32(p)
#define RECOMP_F32_ADD_AT(site,a,b) (hooks++,(a)+(b))
#define RECOMP_F32_SUB_AT(site,a,b) (hooks++,(a)-(b))
#define RECOMP_F32_MUL_AT(site,a,b) (hooks++,(a)*(b))
#define RECOMP_F32_DIV_AT(site,a,b) (hooks++,(a)/(b))
#define RECOMP_I32_TO_F32_AT(site,p,v) (hooks++,(float)(int32_t)(v))
#define RECOMP_I32_TO_F64(p,v) (hooks++,(double)(int32_t)(v))
'''.replace('#include <limits.h>','#include <limits.h>\n#include <stdlib.h>')


def function(name,instruction,operands,store=False):
    lines=emit(instruction,operands)
    if store:
        # Preserve read-count instrumentation while providing the write lvalue.
        import re
        lines=[re.sub(r'^MEM32\((.*)\) =',r'*write32(\1) =',line) for line in lines]
    return f'static void {name}(void){{\n'+'\n'.join(lines)+'\n}\n'


def execute(tmp_path,name,source):
    path=tmp_path/(name+'.c');path.write_text(source,encoding='utf-8')
    run=subprocess.run([str(_compile(tmp_path,path))],capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr+f' exit={run.returncode}'


def test_standalone_and_invalid_sse(tmp_path):
    for name,operands in [('addps',[]),('addss',[reg('eax'),reg('xmm1')]),
                          ('movd',[reg('xmm0'),reg('ax')]),('shufps',[reg('xmm0'),reg('xmm1')])]:
        with pytest.raises(ValueError):emit(name,operands)
    for name in ('translation_sse.py','translation_operands.py'):
        shutil.copyfile(ROOT/'tools/compiler'/name,tmp_path/name)
    run=subprocess.run([sys.executable,'-I','-c',
        'import sys;sys.path.insert(0,sys.argv[1]);import translation_sse;'
        'assert "sqrtsd" in translation_sse.MNEMONICS;'
        'assert not any(n.startswith("tools.recomp") for n in sys.modules)',str(tmp_path)],capture_output=True,text=True)
    assert run.returncode==0,run.stderr


def test_moves_shuffles_masks_and_memory_execute(tmp_path):
    code=_PRELUDE
    names=['movss','movsd','movq','movd','movaps','movups','movapd','movupd','movdqa','movdqu','movlps','movhps','movlpd','movhpd']
    for i,name in enumerate(names):
        width=4 if name in ('movss','movd') else 8 if name in ('movsd','movq','movlps','movhps','movlpd','movhpd') else 16
        code+=function(f'load{i}',name,[reg('xmm0'),mem(width)])
        code+=function(f'store{i}',name,[mem(width),reg('xmm1')],True)
    for name in ('movss','movsd','movq','movlhps','movhlps'):
        code+=function(name,name,[reg('xmm0'),reg('xmm1')])
        code+=function(name+'_alias',name,[reg('xmm0'),reg('xmm0')])
    for name in ('unpcklps','unpckhps','unpcklpd','unpckhpd'):
        code+=function(name,name,[reg('xmm0'),reg('xmm1')])
    for kind in ('ps','pd'):
        for control in range(256):
            code+=function(f'shuf{kind}{control}','shuf'+kind,[reg('xmm0'),reg('xmm1'),Operand('imm',imm=control)])
        code+=f'static void(*shuffle_{kind}[256])(void)={{'+','.join(f'shuf{kind}{i}' for i in range(256))+'};\n'
        code+=function('mask_'+kind,'movmsk'+kind,[reg('eax'),reg('xmm1')])
    code+='static void(*loads[])(void)={'+','.join(f'load{i}' for i in range(len(names)))+'};\n'
    code+='static void(*stores[])(void)={'+','.join(f'store{i}' for i in range(len(names)))+'};\n'
    code+=r'''
int main(void){RecompXmm a={{0}},b={{0}},want;
for(unsigned i=0;i<4;++i){a.u[i]=0x12345678u+i*0x11111111u;b.u[i]=0xffc00001u+i;}
unsigned widths[]={1,2,2,1,4,4,4,4,4,4,2,2,2,2};
for(unsigned op=0;op<14;++op){unsigned width=widths[op],high=op==11||op==13;
memset(reads,0,sizeof reads);memset(writes,0,sizeof writes);for(unsigned i=0;i<16;++i)memory[i]=0xdeadbeefu;
for(unsigned i=0;i<4;++i)memory[4+i]=b.u[i];eax=16;xmm0=a;loads[op]();
want=a;if(op<4)want.q[0]=want.q[1]=0;
for(unsigned i=0;i<width;++i)want.u[i+2*high]=b.u[i];
if(memcmp(&want,&xmm0,16))return 1;
for(unsigned i=0;i<16;++i)if(reads[i]!=(i>=4&&i<4+width)||writes[i])return 2;
memset(reads,0,sizeof reads);xmm1=b;stores[op]();
for(unsigned i=0;i<16;++i){if(reads[i]||writes[i]!=(i>=4&&i<4+width))return 3;
if(i>=4&&i<4+width&&memory[i]!=b.u[i-4+2*high])return 4;}}
xmm0=a;xmm1=b;movss();want=a;want.u[0]=b.u[0];if(memcmp(&want,&xmm0,16))return 5;
xmm0=a;movsd();want=a;want.q[0]=b.q[0];if(memcmp(&want,&xmm0,16))return 6;
xmm0=a;movq();want.q[0]=b.q[0];want.q[1]=0;if(memcmp(&want,&xmm0,16))return 7;
xmm0=a;movlhps_alias();if(xmm0.q[1]!=a.q[0]||xmm0.q[0]!=a.q[0])return 8;
xmm0=a;movhlps_alias();if(xmm0.q[0]!=a.q[1]||xmm0.q[1]!=a.q[1])return 9;
for(unsigned control=0;control<256;++control){xmm0=a;xmm1=b;shuffle_ps[control]();
for(unsigned i=0;i<4;++i){RecompXmm input=i<2?a:b;if(xmm0.u[i]!=input.u[(control/(1u<<(i*2)))%4])return 10;}
xmm0=a;shuffle_pd[control]();if(xmm0.q[0]!=a.q[control&1]||xmm0.q[1]!=b.q[(control>>1)&1])return 11;}
for(unsigned mask=0;mask<16;++mask){for(unsigned i=0;i<4;++i)xmm1.u[i]=((mask>>i)&1u)<<31;mask_ps();if(eax!=mask)return 12;
mask_pd();if(eax!=(((mask>>1)&1)|((mask>>2)&2)))return 13;}
xmm0=a;xmm1=b;unpcklps();if(xmm0.u[0]!=a.u[0]||xmm0.u[1]!=b.u[0]||xmm0.u[2]!=a.u[1]||xmm0.u[3]!=b.u[1])return 14;
xmm0=a;unpckhps();if(xmm0.u[0]!=a.u[2]||xmm0.u[1]!=b.u[2]||xmm0.u[2]!=a.u[3]||xmm0.u[3]!=b.u[3])return 15;
xmm0=a;unpcklpd();if(xmm0.q[0]!=a.q[0]||xmm0.q[1]!=b.q[0])return 16;
xmm0=a;unpckhpd();if(xmm0.q[0]!=a.q[1]||xmm0.q[1]!=b.q[1])return 17;
puts("PASS SIMD payload moves, preservation, read/write widths, masks and all shuffle controls");return 0;}
'''
    execute(tmp_path,'moves',code)


def test_scalar_and_packed_numeric_contracts_execute(tmp_path):
    code=_PRELUDE
    operations=('add','sub','mul','div','min','max','sqrt')
    for suffix in own._SUFFIXES:
        for op in operations:code+=function(op+suffix,op+suffix,[reg('xmm0'),reg('xmm1')])
    for name in ('rcpss','rcpps','rsqrtss','rsqrtps'):
        code+=function(name,name,[reg('xmm0'),reg('xmm1')])
    for name in ('cvtsi2ss','cvtsi2sd'):
        code+=function(name,name,[reg('xmm0'),reg('eax')])
        code+=function(name+'_mem',name,[reg('xmm0'),mem(4)])
    for name in ('cvtss2si','cvttss2si','cvtsd2si','cvttsd2si'):
        code+=function(name,name,[reg('eax'),reg('xmm1')])
    for name in ('cvtss2sd','cvtsd2ss','comiss','ucomiss','comisd','ucomisd'):
        code+=function(name,name,[reg('xmm0'),reg('xmm1')])
    code+='static void(*ops[4][7])(void)={'+','.join('{'+','.join(op+suffix for op in operations)+'}' for suffix in own._SUFFIXES)+'};\n'
    code+=r'''
static double reference(double a,double b,unsigned op){switch(op){case 0:return a+b;case 1:return a-b;case 2:return a*b;case 3:return a/b;case 4:return a<b?a:b;case 5:return a>b?a:b;default:return sqrt(b);}}
int main(void){double values[]={-INFINITY,-9.,-0.,0.,1.,4.,9.,INFINITY,NAN};
for(unsigned shape=0;shape<4;++shape)for(unsigned op=0;op<7;++op)for(unsigned a=0;a<9;++a)for(unsigned b=0;b<9;++b){
unsigned wide=shape%2,lanes=shape<2?1:wide?2:4;RecompXmm original,expected;
for(unsigned i=0;i<(wide?2:4);++i){if(wide){xmm0.d[i]=values[(a+i)%9];xmm1.d[i]=values[(b+i)%9];}else{xmm0.f[i]=(float)values[(a+i)%9];xmm1.f[i]=(float)values[(b+i)%9];}}
original=xmm0;expected=xmm0;for(unsigned i=0;i<lanes;++i){if(wide)expected.d[i]=reference(xmm0.d[i],xmm1.d[i],op);else expected.f[i]=(float)reference(xmm0.f[i],xmm1.f[i],op);}
hooks=0;ops[shape][op]();if(hooks!=(shape==0&&op<4))return 1;
for(unsigned i=0;i<(wide?2:4);++i){if(wide){if(i>=lanes&&xmm0.q[i]!=original.q[i])return 2;if(isnan(expected.d[i])?!isnan(xmm0.d[i]):xmm0.q[i]!=expected.q[i])return 3;}
else{if(i>=lanes&&xmm0.u[i]!=original.u[i])return 4;if(isnan(expected.f[i])?!isnan(xmm0.f[i]):xmm0.u[i]!=expected.u[i])return 5;}}}
for(unsigned i=0;i<4;++i)xmm1.f[i]=(float)((i+1)*(i+1));rcpps();for(unsigned i=0;i<4;++i)if(xmm0.f[i]!=1.f/xmm1.f[i])return 6;
rsqrtps();for(unsigned i=0;i<4;++i)if(xmm0.f[i]!=1.f/(i+1))return 7;
int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
double conversions[]={-INFINITY,-2147483649.,-2147483648.,-3.5,-2.5,-1.9,-0.,0.,1.9,2.5,3.5,2147483647.,2147483648.,INFINITY,NAN};
for(unsigned m=0;m<4;++m){if(fesetround(modes[m]))return 8;for(unsigned i=0;i<15;++i){
xmm1.d[0]=conversions[i];double rounded=nearbyint(conversions[i]);uint32_t want=rounded>=-2147483648.&&rounded<2147483648.?(uint32_t)(int32_t)rounded:0x80000000u;
cvtsd2si();if(eax!=want)return 9;rounded=trunc(conversions[i]);want=rounded>=-2147483648.&&rounded<2147483648.?(uint32_t)(int32_t)rounded:0x80000000u;
cvttsd2si();if(eax!=want)return 10;}}
fesetround(FE_TONEAREST);xmm0.q[1]=0x123456789abcdef0ULL;eax=0xffffffffu;cvtsi2sd();if(xmm0.d[0]!=-1.||xmm0.q[1]!=0x123456789abcdef0ULL)return 11;
xmm1.d[0]=3.141592653589793;cvtsd2ss();if(xmm0.f[0]!=(float)3.141592653589793)return 12;
xmm1.f[0]=1.25f;cvtss2sd();if(xmm0.d[0]!=1.25)return 13;
xmm0.d[0]=1.25;xmm1.d[0]=2.5;comisd();if(_fca!=1.25||_fcb!=2.5)return 14;
eax=16;memory[4]=0xfffffffeu;memset(reads,0,sizeof reads);hooks=0;cvtsi2ss_mem();if(xmm0.f[0]!=-2.f||reads[4]!=1||hooks!=1)return 15;
hooks=0;cvtsi2sd_mem();if(xmm0.d[0]!=-2.||reads[4]!=2||hooks!=1)return 16;
puts("PASS scalar/packed numerics, double lanes, special values, conversion bounds/modes and hooks");return 0;}
'''
    execute(tmp_path,'numeric',code)


@pytest.mark.skipif(os.name!='nt',reason='Supplemental native x86 SSE/SSE2 oracle')
def test_against_native_sse_intrinsics(tmp_path):
    code=_PRELUDE+'\n#include <emmintrin.h>\n'
    cases=[]
    for suffix in own._SUFFIXES:
        for op in ('add','sub','mul','div','min','max','sqrt'):
            name=op+suffix;wide=suffix.endswith('d')
            code+=function('own_'+name,name,[reg('xmm0'),reg('xmm1')])
            kind='__m128d' if wide else '__m128';load='_mm_loadu_pd' if wide else '_mm_loadu_ps'
            store='_mm_storeu_pd' if wide else '_mm_storeu_ps';field='d' if wide else 'f'
            call=f'_mm_{op}_{suffix}(a,b)'
            if op=='sqrt':
                call=f'_mm_sqrt_{suffix}(b)' if suffix in ('ps','pd') else ('_mm_move_ss(a,_mm_sqrt_ss(b))' if suffix=='ss' else '_mm_sqrt_sd(a,b)')
            code+=f'static void hw_{name}(RecompXmm left,RecompXmm right,RecompXmm *result){{{kind} a={load}(left.{field}),b={load}(right.{field});{store}(result->{field},{call});}}\n'
            cases.append((name,int(wide),int(op in ('min','max'))))
    for kind in ('ps','pd','ss','sd'):
        for index,predicate in enumerate(own._PREDICATES):
            name='cmp'+predicate+kind;wide=kind.endswith('d');field='d' if wide else 'f'
            code+=function('own_'+name,name,[reg('xmm0'),reg('xmm1')])
            ctype='__m128d' if wide else '__m128';load='_mm_loadu_pd' if wide else '_mm_loadu_ps';store='_mm_storeu_pd' if wide else '_mm_storeu_ps'
            code+=f'static void hw_{name}(RecompXmm left,RecompXmm right,RecompXmm *result){{{ctype} a={load}(left.{field}),b={load}(right.{field});{store}(result->{field},_mm_cmp{predicate}_{kind}(a,b));}}\n'
            cases.append((name,int(wide),1))
    code+='struct Case{void(*own)(void);void(*native)(RecompXmm,RecompXmm,RecompXmm*);int wide,exact;};static struct Case cases[]={'+','.join(f'{{own_{name},hw_{name},{wide},{exact}}}' for name,wide,exact in cases)+'};\n'
    for name in ('cvtss2si','cvttss2si','cvtsd2si','cvttsd2si'):
        code+=function('own_'+name,name,[reg('eax'),reg('xmm1')])
    code+=r'''
__declspec(noinline) static uint32_t hardware_convert(unsigned op,RecompXmm source){
switch(op){case 0:return _mm_cvtss_si32(_mm_loadu_ps(source.f));case 1:return _mm_cvttss_si32(_mm_loadu_ps(source.f));
case 2:return _mm_cvtsd_si32(_mm_loadu_pd(source.d));default:return _mm_cvttsd_si32(_mm_loadu_pd(source.d));}}
static void(*convert[])(void)={own_cvtss2si,own_cvttss2si,own_cvtsd2si,own_cvttsd2si};
int main(void){uint64_t seed=1234567;RecompXmm left,right,want;
uint32_t floats[]={0,0x80000000u,0x3f800000u,0xbf800000u,0x7f800000u,0xff800000u,0x7fc12345u,0xffc54321u,0x00800000u,0x7f7fffffu};
uint64_t doubles[]={0,UINT64_C(0x8000000000000000),UINT64_C(0x3ff0000000000000),UINT64_C(0xbff0000000000000),UINT64_C(0x7ff0000000000000),UINT64_C(0xfff0000000000000),UINT64_C(0x7ff8123456789abc),UINT64_C(0xfff8543212345678),UINT64_C(0x0010000000000000),UINT64_C(0x7fefffffffffffff)};
for(unsigned t=0;t<1100;++t){for(unsigned k=0;k<sizeof cases/sizeof cases[0];++k){struct Case *c=&cases[k];
for(unsigned lane=0;lane<(c->wide?2:4);++lane){seed=seed*UINT64_C(6364136223846793005)+1;
if(c->wide){left.q[lane]=t<100?doubles[(t/10+lane)%10]:seed;seed=seed*UINT64_C(6364136223846793005)+1;right.q[lane]=t<100?doubles[(t%10+lane)%10]:seed;}
else{left.u[lane]=t<100?floats[(t/10+lane)%10]:(uint32_t)seed;seed=seed*UINT64_C(6364136223846793005)+1;right.u[lane]=t<100?floats[(t%10+lane)%10]:(uint32_t)seed;}}
xmm0=left;xmm1=right;c->native(left,right,&want);c->own();
for(unsigned lane=0;lane<(c->wide?2:4);++lane){int same=c->wide?xmm0.q[lane]==want.q[lane]:xmm0.u[lane]==want.u[lane];
if(!same&&!c->exact)same=c->wide?(isnan(xmm0.d[lane])&&isnan(want.d[lane])):(isnan(xmm0.f[lane])&&isnan(want.f[lane]));
if(!same){fprintf(stderr,"native case=%u trial=%u lane=%u\n",k,t,lane);return 1;}}}}
int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};double values[]={-INFINITY,-2147483649.,-2147483648.,-2.5,-1.9,-0.,0.,1.9,2.5,2147483647.,2147483648.,INFINITY,NAN};
for(unsigned m=0;m<4;++m){fesetround(modes[m]);for(unsigned op=0;op<4;++op)for(unsigned i=0;i<13;++i){
if(op<2)xmm1.f[0]=(float)values[i];else xmm1.d[0]=values[i];uint32_t expected=hardware_convert(op,xmm1);convert[op]();
if(eax!=expected){fprintf(stderr,"convert op=%u mode=%u input=%u got=%x want=%x\n",op,m,i,eax,expected);return 2;}}}
fesetround(FE_TONEAREST);puts("PASS native SSE scalar/packed/double arithmetic, comparisons and conversions");return 0;}
'''
    execute(tmp_path,'hardware',code)


def test_decoded_production_sse_consumers_execute(tmp_path):
    script=r'''
import json
from tools.recomp.lifter import Lifter
from tools.recomp.translator import FunctionTranslator
import translation_sse as own
assert Lifter._lift_sse is own.emit
out={}
cases={'double_math':'f20f10c1f20f58c1660f2fc10f97c0c3',
       'packed_root':'0f51c10f50c0c3','double_truncate':'f20f2cc1c3',
       'packed_integer':'660fdbc1c3','double_mask_compare':'660fc2c100c3',
       'scalar_memory':'f30f1000c3','double_memory':'f20f1000c3',
       'preserve_flags':'39d8660f51c10f94c2f20fc2c1000f94c1c3'}
for name,raw in cases.items():
 data=bytes.fromhex(raw);info={'name':name,'size':len(data)}
 t=FunctionTranslator(b'',{0x1000:info});t._read_func_bytes=lambda a,b:data
 out[name]=t.translate_function(0x1000,info)
print(json.dumps(out))
'''
    run=subprocess.run([sys.executable,'-c',script],cwd=ROOT/'external/ts3FPrecomp',
        env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    from test_translation_moves import _production_macros
    code=_PRELUDE+'\n'+_production_macros({'LO8','SET_LO8'})+'\n'
    code+='static uint32_t ecx,edx,esi,edi,esp,g_seh_ebp;\n#define RECOMP_FAST_ENTRY(site) 0\n'
    code+='\n'.join(json.loads(run.stdout).values())
    code+=r'''
int main(void){double values[]={-9.,-0.,0.,1.25,3.5,INFINITY,NAN};
for(unsigned i=0;i<7;++i){xmm0.q[1]=UINT64_C(0x123456789abcdef0);xmm1.d[0]=values[i];eax=0;double_math();
if(xmm0.q[1]!=UINT64_C(0x123456789abcdef0))return 1;
if(isnan(values[i])?!isnan(xmm0.d[0]):xmm0.d[0]!=2*values[i])return 2;
if(eax!=(2*values[i]>values[i]))return 3;}
for(unsigned i=0;i<4;++i)xmm1.f[i]=(float)((i+1)*(i+1));packed_root();
for(unsigned i=0;i<4;++i)if(xmm0.f[i]!=(float)(i+1))return 4;if(eax)return 5;
xmm1.d[0]=-3.75;double_truncate();if(eax!=0xfffffffdu)return 6;
for(unsigned i=0;i<4;++i){xmm0.u[i]=0xff00ff00u+i;xmm1.u[i]=0x12345678u+i;}RecompXmm a=xmm0,b=xmm1;packed_integer();
for(unsigned i=0;i<4;++i)if(xmm0.u[i]!=(a.u[i]&b.u[i]))return 7;
xmm0.d[0]=1.25;xmm1.d[0]=1.25;xmm0.d[1]=NAN;xmm1.d[1]=1.;double_mask_compare();if(xmm0.q[0]!=UINT64_MAX||xmm0.q[1])return 8;
eax=16;memory[4]=0xffc12345u;memory[5]=0x12345678u;xmm0.q[0]=xmm0.q[1]=UINT64_MAX;memset(reads,0,sizeof reads);scalar_memory();
if(xmm0.u[0]!=memory[4]||xmm0.u[1]||xmm0.q[1]||reads[4]!=1||reads[5])return 9;
xmm0.q[1]=UINT64_MAX;double_memory();if(xmm0.u[0]!=memory[4]||xmm0.u[1]!=memory[5]||xmm0.q[1]||reads[4]!=2||reads[5]!=1)return 10;
for(unsigned equal=0;equal<2;++equal){eax=2;ebx=2+equal;ecx=edx=0;xmm1.d[0]=4.;xmm1.d[1]=9.;preserve_flags();if(edx!=!equal||ecx!=!equal)return 11;}
puts("PASS decoded SSE double/packed/memory/condition consumers");return 0;}
'''
    execute(tmp_path,'decoded',code)
