"""Original x87 stack/rounding references and supplemental native x86 oracles.

The guest interface stores binary64 values; these checks do not claim extended
intermediates, exception delivery or bit-exact transcendental approximations.
"""
import os, re, shutil, subprocess, sys
from pathlib import Path
from types import SimpleNamespace as S
import pytest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import translation_x87 as own
from translation_decode import Operand
from test_translation_product import _compile
from native_compiler import native_compiler

def reg(i):return Operand('reg',reg=f'st({i})')
def mem(width):return Operand('mem',mem_base='eax',mem_size=width)
def emit(name,args=()):return own.emit(S(_unimplemented=lambda *_:['UNSUPPORTED;']),S(address=0x1000),name,args)

PRELUDE=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
#include <float.h>
static double g_fp_stack[8];static unsigned g_fp_top;static uint16_t g_fp_control_word=0x37f,g_fp_cc;
static int g_fp_cmp;static uint32_t eax,ebx,esp,g_ebp;static unsigned reads,writes,hooks;
static unsigned char memory[128];
#define fp_top() g_fp_stack[g_fp_top]
#define fp_st1() g_fp_stack[(g_fp_top+1)&7]
#define fp_push(v) (g_fp_top=(g_fp_top+7)&7,fp_top()=(v))
#define fp_pop() (g_fp_top=(g_fp_top+1)&7)
#define SET_LO16(r,v) ((r)=((r)&0xffff0000u)|(uint16_t)(v))
#define LO16(r) ((uint16_t)(r))
#define CHECK(x) do{if(!(x)){fprintf(stderr,"check line %d\n",__LINE__);return 1;}}while(0)
static double observe(uint32_t p,uint32_t v){if(p!=eax)abort();++hooks;return (v&0x80000000u)?-(double)((~v+1u)&0xffffffffu):(double)v;}
#define RECOMP_I32_TO_F64(p,v) observe(p,v)
static int same(double a,double b){return (isnan(a)&&isnan(b))||(!memcmp(&a,&b,sizeof a));}
'''
for width,ctype in ((16,'uint16_t'),(32,'uint32_t'),(64,'uint64_t'),('F','float'),('D','double')):
    PRELUDE+=f'''static {ctype} read{width}(uint32_t p){{{ctype} v;if(p>sizeof memory-sizeof v)abort();memcpy(&v,memory+p,sizeof v);++reads;return v;}}
static void write{width}(uint32_t p,{ctype} v){{if(p>sizeof memory-sizeof v)abort();memcpy(memory+p,&v,sizeof v);++writes;}}
#define MEM{width}(p) read{width}(p)
'''

def function(name,mnemonic,args=()):
    lines=emit(mnemonic,args)
    lines=[re.sub(r'^MEM(16|32|64|F|D)\((.*?)\) = (.*);$',r'write\1(\2, \3);',line) for line in lines]
    return f'static void {name}(void){{\n'+'\n'.join(lines)+'\n}\n'

def execute(tmp_path,code,objects=()):
    source=tmp_path/'x87.c';source.write_text(code,encoding='utf-8')
    result=subprocess.run([str(_compile(tmp_path,source,objects=objects))],capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr+f' exit={result.returncode}'


def test_standalone_and_invalid_inputs(tmp_path):
    for value in ('st(-1)','st(8)','eax','st(0)junk','junkst1',None):
        with pytest.raises(ValueError):own.stack_index(value)
    for i in range(8):assert own.stack_index(f'st{i}')==own.stack_index(f'st({i})')==i
    for name,args in [('fld',[]),('fld',[mem(2)]),('fild',[reg(0)]),('fist',[mem(10)]),('fsqrt',[reg(0)])]:
        with pytest.raises(ValueError):emit(name,args)
    assert emit('unknown')==['UNSUPPORTED;']
    for name in ('fld','frndint','fstp','fnstsw','fcom','fxam'):assert own.preserves_flags(S(mnemonic=name))
    for name in ('fcomi','fcomip','fcompi','fucompi','cmp'):assert not own.preserves_flags(S(mnemonic=name))
    for name in ('translation_x87.py','translation_operands.py'):shutil.copyfile(ROOT/'tools'/name,tmp_path/name)
    run=subprocess.run([sys.executable,'-I','-c',"import sys;sys.path.insert(0,sys.argv[1]);import translation_x87 as m;assert 'fld' in m.MNEMONICS",str(tmp_path)],capture_output=True,text=True)
    assert run.returncode==0,run.stdout+run.stderr


def test_stack_alias_arithmetic_and_memory_execute(tmp_path):
    code=PRELUDE
    for i in range(8):
        for n in ('fld','fst','fstp','fxch'):code+=function(f'{n}_{i}',n,[reg(i)])
    for n in ('fld','fst','fstp','fild','fist','fistp'):
        for w in ((4,8,10) if n in ('fld','fst','fstp') else (2,4,8)):code+=function(f'{n}_m{w}',n,[mem(w)])
    arithmetic=[];expected=[];pops=[];destinations=[]
    for n,(operator,reverse,pop,integer) in own._ARITHMETIC.items():
        if integer:continue
        for i in range(8):
            name=f'{n}_{i}';dst=i if pop else 0;src=0 if pop else i
            code+=function(name,n,[reg(i)])
            arithmetic.append(name);destinations.append(dst);pops.append(int(pop))
            left,right=(src+1,dst+1) if reverse else (dst+1,src+1)
            expected.append({'+' :lambda:left+right,'-':lambda:left-right,'*':lambda:left*right,'/':lambda:left/right}[operator]().hex() if operator=='/' else float({'+' :left+right,'-':left-right,'*':left*right}[operator]).hex())
    for n in ('fld','fst','fstp','fxch'):code+='static void(*'+n+'s[])(void)={'+','.join(f'{n}_{i}' for i in range(8))+'};\n'
    code+='static void(*arithmetic[])(void)={'+','.join(arithmetic)+'};\n'
    code+='static double expected[]={'+','.join(expected)+'};\n'
    code+='static unsigned pops[]={'+','.join(map(str,pops))+'},destinations[]={'+','.join(map(str,destinations))+'};\n'
    for n in ('fiadd','fisub','fisubr','fimul','fidiv','fidivr'):
        for w in (2,4):code+=function(f'{n}_m{w}',n,[mem(w)])
    code+=r'''
static void reset(unsigned top){g_fp_top=top;for(unsigned i=0;i<8;++i)g_fp_stack[(top+i)&7]=(double)(i+1);}
int main(void){
for(unsigned top=0;top<8;++top)for(unsigned i=0;i<8;++i){
reset(top);flds[i]();CHECK(g_fp_top==((top+7)&7)&&fp_top()==i+1);
reset(top);fsts[i]();CHECK(g_fp_top==top&&g_fp_stack[(top+i)&7]==1.);
reset(top);fstps[i]();CHECK(g_fp_top==((top+1)&7)&&g_fp_stack[(top+i)&7]==1.);
reset(top);fxchs[i]();CHECK(g_fp_top==top&&fp_top()==i+1&&g_fp_stack[(top+i)&7]==1.);}
for(unsigned top=0;top<8;++top)for(unsigned i=0;i<sizeof expected/sizeof expected[0];++i){reset(top);arithmetic[i]();CHECK(g_fp_top==((top+pops[i])&7)&&same(g_fp_stack[(top+destinations[i])&7],expected[i]));}
for(unsigned p=0;p<8;++p){eax=p;reset(3);double d=-123.5;float f=-123.5f;uint64_t u=UINT64_C(0x8000000000000000);
memcpy(memory+p,&f,4);reads=0;fld_m4();CHECK(fp_top()==d&&reads==1);writes=0;fst_m4();CHECK(writes==1&&g_fp_top==2);
memcpy(memory+p,&d,8);reads=0;fld_m8();CHECK(fp_top()==d&&reads==1);writes=0;fstp_m8();CHECK(writes==1&&g_fp_top==2);
memcpy(memory+p,&u,8);reads=0;fild_m8();CHECK(fp_top()==-0x1p63&&reads==1);uint16_t s=0x8001;memcpy(memory+p,&s,2);fild_m2();CHECK(fp_top()==-32767.);
uint32_t v=0x80000001u;memcpy(memory+p,&v,4);hooks=0;fild_m4();CHECK(fp_top()==-2147483647.&&hooks==1);
fp_top()=5.;fiadd_m4();CHECK(fp_top()==-2147483642.&&hooks==1);
fp_top()=5.;fisubr_m4();CHECK(fp_top()==-2147483652.&&hooks==1);}
// Every binary64 category is exactly representable in the external 80-bit format.
double values[]={0.,-0.,1.,-2.5,DBL_MIN,DBL_MAX,0x1p-1074,INFINITY,-INFINITY,NAN};
for(unsigned i=0;i<sizeof values/sizeof values[0];++i){eax=118;g_fp_top=6;fp_top()=values[i];writes=reads=0;fstp_m10();CHECK(g_fp_top==7&&writes==3);fld_m10();CHECK(g_fp_top==6&&reads==3&&same(fp_top(),values[i]));}
puts("PASS stack order/aliases, arithmetic destinations and bounded memory");return 0;}
'''
    execute(tmp_path,code)


def test_rounding_integer_bounds_and_status_execute(tmp_path):
    code=PRELUDE+function('round_guest','frndint')+function('classify','fxam')+function('status','fnstsw',[Operand('reg',reg='ax')])
    for n in ('fist','fistp','fisttp'):
        for w in (2,4,8):code+=function(f'{n}_{w}',n,[mem(w)])
    for n in sorted(own._COMPARE):code+=function(n,n,[reg(1)])
    code+=function('compare_memory','fcom',[mem(8)])+function('load_control','fldcw',[mem(2)])+function('store_control','fnstcw',[mem(2)])
    names=sorted(own._COMPARE)
    code+='static void(*compares[])(void)={'+','.join(names)+'};\n'
    code+='static unsigned pop_counts[]={'+','.join(str(2 if n.endswith('pp') else int(n.endswith(('p','pi')))) for n in names)+'};\n'
    code+='static unsigned integer_flags[]={'+','.join(str(int(n.endswith(('i','ip')))) for n in names)+'};\n'
    code+=r'''
static double reference(double x,unsigned mode){if(!isfinite(x))return x;double r=mode==1?floor(x):mode==2?ceil(x):mode==3?trunc(x):nearbyint(x);return r==0?copysign(0.,x):r;}
int main(void){fesetround(FE_TONEAREST);double values[]={-INFINITY,-0x1p63,-2147483649.,-32768.5,-32768.,-3.5,-2.5,-1.5,-.5,-.1,-0.,0.,.1,.5,1.5,2.5,3.5,32767.5,32768.,2147483647.5,2147483648.,0x1p63,INFINITY,NAN,0x1p-1074};
void(*stores[3][3])(void)={{fist_2,fist_4,fist_8},{fistp_2,fistp_4,fistp_8},{fisttp_2,fisttp_4,fisttp_8}};unsigned widths[]={2,4,8};
for(unsigned mode=0;mode<4;++mode)for(unsigned i=0;i<sizeof values/sizeof values[0];++i){g_fp_control_word=(uint16_t)(0x37f|(mode<<10));g_fp_top=5;fp_top()=values[i];round_guest();CHECK(g_fp_top==5&&same(fp_top(),reference(values[i],mode)));
for(unsigned op=0;op<3;++op)for(unsigned w=0;w<3;++w){unsigned width=widths[w];double r=reference(values[i],op==2?3:mode),limit=ldexp(1.,8*(int)width-1);uint64_t expected=r>=-limit&&r<limit?(uint64_t)(int64_t)r:UINT64_C(1)<<(width*8-1),actual=0;
eax=128-width;memset(memory,0x5a,sizeof memory);g_fp_top=5;fp_top()=values[i];reads=writes=0;stores[op][w]();memcpy(&actual,memory+eax,width);uint64_t mask=width==8?UINT64_MAX:(UINT64_C(1)<<(8*width))-1;
CHECK(actual==(expected&mask)&&reads==0&&writes==1&&g_fp_top==(op?6:5)&&memory[eax-1]==0x5a);}}
for(unsigned i=0;i<sizeof values/sizeof values[0];++i)for(unsigned j=0;j<sizeof values/sizeof values[0];++j)for(unsigned op=0;op<sizeof compares/sizeof compares[0];++op){
g_fp_top=7;fp_top()=values[i];fp_st1()=values[j];g_fp_cc=0x2222;compares[op]();int want=(isnan(values[i])||isnan(values[j]))?2:values[i]<values[j]?-1:values[i]>values[j]?1:0;uint16_t bits=want==2?0x4500:want<0?0x0100:want==0?0x4000:0;
CHECK(g_fp_cmp==want&&g_fp_top==((7+pop_counts[op])&7)&&g_fp_cc==(integer_flags[op]?0x2222:bits));}
g_fp_top=5;fp_top()=1.;double rhs=2.;eax=119;memcpy(memory+eax,&rhs,8);reads=0;compare_memory();CHECK(reads==1&&g_fp_cmp==-1);
eax=0xabcd0000;status();CHECK(eax==(0xabcd0000u|(5u<<11)|0x100));
for(unsigned i=0;i<sizeof values/sizeof values[0];++i){fp_top()=values[i];classify();uint16_t want=isnan(values[i])?0x100:isinf(values[i])?0x500:values[i]==0?0x4000:0x400;if(signbit(values[i]))want|=0x200;CHECK(g_fp_cc==want);}
eax=126;uint16_t control=0x0b7f;memcpy(memory+eax,&control,2);load_control();CHECK(g_fp_control_word==control);writes=0;store_control();CHECK(writes==1&&read16(eax)==control);
puts("PASS guest rounding, integer indefinite/bounds and status/compare semantics");return 0;}
'''
    execute(tmp_path,code)


@pytest.mark.skipif(os.name!='nt',reason='Supplemental native x86 x87 oracle')
def test_native_x87_rounding_and_storage(tmp_path):
    code=PRELUDE+function('round_guest','frndint')
    asm=['.code'];declarations=[]
    operations={'round':['frndint','fstp qword ptr [rcx]'], 'extended':['fstp tbyte ptr [rcx]']}
    code+=function('store_extended','fstp',[mem(10)])
    for width,spelling in ((2,'word'),(4,'dword'),(8,'qword')):
        for n in ('fistp','fisttp'):
            key=f'{n}_{width}';code+=function(key,n,[mem(width)]);operations[key]=[f'{n} {spelling} ptr [rcx]']
    operations['scale']=['fld qword ptr [r9]','fxch','fscale','fstp qword ptr [rcx]','fstp st(0)']
    code+=function('scale','fscale')
    for name,instructions in operations.items():
        asm += [f'public hw_{name}',f'hw_{name} proc','sub rsp,16','fnstcw word ptr [rsp]','fnclex','fldcw word ptr [r8]', 'fld qword ptr [rdx]',*instructions,'fnclex','fldcw word ptr [rsp]','add rsp,16','ret',f'hw_{name} endp']
        declarations.append(f'extern void hw_{name}(void*,const double*,const uint16_t*,const double*);')
    asm+=['end'];compiler,env=native_compiler(tmp_path);source=tmp_path/'oracle.asm';obj=tmp_path/'native_x87.obj';source.write_text('\n'.join(asm)+'\n',encoding='utf-8')
    run=subprocess.run([str(Path(compiler).with_name('ml64.exe')),'/nologo','/c',f'/Fo{obj}',str(source)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
    code+='\n'.join(declarations)+r'''
int main(void){double values[]={-INFINITY,-0x1p63,-2147483649.,-32768.5,-32768.,-3.5,-2.5,-1.5,-.5,-.1,-0.,0.,.1,.5,1.5,2.5,3.5,32767.5,32768.,2147483647.5,2147483648.,0x1p63,INFINITY,NAN,0x1p-1074};
void(*mine[])(void)={fistp_2,fistp_4,fistp_8,fisttp_2,fisttp_4,fisttp_8};
void(*hardware[])(void*,const double*,const uint16_t*,const double*)={hw_fistp_2,hw_fistp_4,hw_fistp_8,hw_fisttp_2,hw_fisttp_4,hw_fisttp_8};unsigned widths[]={2,4,8,2,4,8};
for(unsigned mode=0;mode<4;++mode)for(unsigned i=0;i<sizeof values/sizeof values[0];++i){g_fp_control_word=(uint16_t)(0x37f|(mode<<10));double want;hw_round(&want,&values[i],&g_fp_control_word,0);g_fp_top=3;fp_top()=values[i];round_guest();CHECK(same(fp_top(),want));
for(unsigned op=0;op<6;++op){uint64_t bits=0;hardware[op](&bits,&values[i],&g_fp_control_word,0);eax=117;g_fp_top=3;fp_top()=values[i];mine[op]();CHECK(!memcmp(memory+eax,&bits,widths[op])&&g_fp_top==4);}}
g_fp_control_word=0x37f;double exponents[]={-INFINITY,-1e300,-4096.,-1024.,-2.7,-0.,0.,2.7,1024.,4096.,1e300,INFINITY,NAN};
for(unsigned i=0;i<sizeof values/sizeof values[0];++i)for(unsigned j=0;j<sizeof exponents/sizeof exponents[0];++j){double want;hw_scale(&want,&values[i],&g_fp_control_word,&exponents[j]);g_fp_top=2;fp_top()=values[i];fp_st1()=exponents[j];scale();CHECK(same(fp_top(),want)&&g_fp_top==2&&same(fp_st1(),exponents[j]));}
for(unsigned i=0;i<sizeof values/sizeof values[0];++i){if(isnan(values[i]))continue;unsigned char want[10];hw_extended(want,&values[i],&g_fp_control_word,0);g_fp_top=2;fp_top()=values[i];eax=118;store_extended();CHECK(g_fp_top==3&&!memcmp(want,memory+118,10));}
puts("PASS native x87 rounding, integer/80-bit stores and bounded scaling");return 0;}
'''
    execute(tmp_path,code,[str(obj)])


def test_transcendental_stack_contracts_and_specials(tmp_path):
    code=PRELUDE
    for name in (*own._UNARY,*own._CONSTANTS,'fsincos','fptan','fpatan','fyl2x','fyl2xp1','fprem','fprem1','fscale','ftst'):
        code+=function('op_'+name,name)
    code+=r'''
int main(void){g_fp_top=0;fp_top()=.25;op_fsincos();CHECK(g_fp_top==7&&same(fp_top(),cos(.25))&&same(fp_st1(),sin(.25)));
g_fp_top=0;fp_top()=.25;op_fptan();CHECK(g_fp_top==7&&fp_top()==1.&&same(fp_st1(),tan(.25)));
g_fp_top=7;fp_top()=2.;fp_st1()=3.;op_fpatan();CHECK(g_fp_top==0&&same(fp_top(),atan2(3.,2.)));
g_fp_top=7;fp_top()=8.;fp_st1()=3.;op_fyl2x();CHECK(g_fp_top==0&&fp_top()==9.);
g_fp_top=7;fp_top()=7.;fp_st1()=3.;op_fyl2xp1();CHECK(g_fp_top==0&&fp_top()==9.);
g_fp_top=7;fp_top()=7.;fp_st1()=2.;op_fprem();CHECK(g_fp_top==7&&fp_top()==1.&&fp_st1()==2.);
fp_top()=7.;op_fprem1();CHECK(g_fp_top==7&&fp_top()==-1.&&fp_st1()==2.);
fp_top()=-0.;op_fabs();CHECK(!signbit(fp_top()));op_fchs();CHECK(signbit(fp_top()));
fp_top()=4.;op_fsqrt();CHECK(fp_top()==2.);fp_top()=.25;op_fsin();CHECK(same(fp_top(),sin(.25)));fp_top()=.25;op_fcos();CHECK(same(fp_top(),cos(.25)));fp_top()=1.;op_f2xm1();CHECK(fp_top()==1.);
fp_top()=NAN;op_ftst();CHECK(g_fp_cmp==2&&g_fp_cc==0x4500);fp_top()=-1.;op_ftst();CHECK(g_fp_cmp==-1&&g_fp_cc==0x100);
'''
    import math
    for name,value in {'fldz':0.,'fld1':1.,'fldpi':math.pi,'fldl2e':math.log2(math.e),'fldl2t':math.log2(10),'fldlg2':math.log10(2),'fldln2':math.log(2)}.items():
        code+=f'g_fp_top=0;op_{name}();CHECK(g_fp_top==7&&same(fp_top(),{value.hex()}));\n'
    code+='puts("PASS libm result/stack contracts and binary64 constants");return 0;}\n'
    execute(tmp_path,code)


def test_decoded_production_x87_execute(tmp_path):
    import json
    cases={'stack_sequence':'d9c0d9c2dec1dddac3',
           'integer_conversion':'db00d9fcdb5804c3',
           'memory_arithmetic':'dd00dc4008dd5810c3',
           'integer_flags':'39d8d9e8d9fcddd80f94c2c3',
           'float_flags':'dff10f92c2c3',
           'status_branch':'d8d9dfe0f6c4440f9ac2c3'}
    script=r'''
import json,sys
from tools.recomp.translator import FunctionTranslator
from tools.recomp.lifter import Lifter
import translation_x87 as own
assert Lifter._lift_fpu is own.emit
assert not any(hasattr(Lifter,n) for n in ('_st_index','_st_expr','_fcom_rhs'))
result={}
for name,raw in json.loads(sys.argv[1]).items():
 data=bytes.fromhex(raw);info={'name':name,'size':len(data)}
 translator=FunctionTranslator(b'',{0x1000:info});translator._read_func_bytes=lambda a,b:data
 result[name]=translator.translate_function(0x1000,info)
print(json.dumps(result))
'''
    run=subprocess.run([sys.executable,'-c',script,json.dumps(cases)],cwd=ROOT/'external/ts3FPrecomp',env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    from test_translation_moves import _production_macros
    routines='\n'.join(json.loads(run.stdout).values())
    routines='\n'.join(re.sub(r'^(\s*)MEM(16|32|64|F|D)\((.*?)\) = (.*);$',r'\1write\2(\3, \4);',line) for line in routines.splitlines())
    code=PRELUDE+'\n'+_production_macros({'SET_LO8','HI8'})+'\nstatic uint32_t ecx,edx,esi,edi,g_seh_ebp;\n#define RECOMP_FAST_ENTRY(site) 0\n#undef fp_top\n#undef fp_st1\n#undef fp_push\n#undef fp_pop\n'+routines+'\n'+ '\n'.join(line for line in PRELUDE.splitlines() if line.startswith('#define fp_'))
    code+=r'''
int main(void){g_fp_top=0;fp_top()=3.;fp_st1()=7.;stack_sequence();CHECK(g_fp_top==0&&fp_top()==3.&&fp_st1()==10.);
eax=13;int32_t value=-32767;memcpy(memory+eax,&value,4);g_fp_top=6;hooks=0;integer_conversion();CHECK(g_fp_top==6&&hooks==1&&read32(eax+4)==(uint32_t)value);
double a=1.25,b=-2.5;memcpy(memory+eax,&a,8);memcpy(memory+eax+8,&b,8);g_fp_top=5;reads=writes=0;memory_arithmetic();CHECK(g_fp_top==5&&reads==2&&writes==1&&readD(eax+16)==-1.25);
for(unsigned i=0;i<2;++i){eax=3;ebx=3+i;edx=0;g_fp_top=4;integer_flags();CHECK(edx==!i&&g_fp_top==4);}
double values[]={-INFINITY,-2.,-0.,0.,3.,INFINITY,NAN};
for(unsigned i=0;i<7;++i)for(unsigned j=0;j<7;++j){g_fp_top=7;fp_top()=values[i];fp_st1()=values[j];edx=0;float_flags();CHECK(edx==(unsigned)(isnan(values[i])||isnan(values[j])||values[i]<values[j])&&g_fp_top==0);
g_fp_top=7;fp_top()=values[i];fp_st1()=values[j];edx=0;status_branch();CHECK(edx==(unsigned)(isnan(values[i])||isnan(values[j])||values[i]!=values[j])&&g_fp_top==0);}
puts("PASS decoded x87 stack/arithmetic, integer hooks, flags and status branch");return 0;}
'''
    execute(tmp_path,code)
