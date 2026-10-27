"""Original architectural condition references and native x86 flag oracles."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from types import SimpleNamespace as S

import pytest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import translation_conditions as own
import translation_integer as integer
import translation_compare as compare
import translation_exchange as exchange
from translation_decode import Operand
from test_translation_moves import _production_macros
from test_translation_product import _compile

_CODES='o no b ae e ne be a s ns p np l ge le g'.split()
_OPS='cmp test add sub adc sbb and or xor inc dec neg xadd cmpxchg'.split()


def test_aliases_unknown_flags_and_standalone_copy(tmp_path):
    ops=[Operand('reg',reg='al'),Operand('reg',reg='bl')]
    for aliases in own._GROUPS:
        canonical=aliases.split()[0]
        for alias in aliases.split():
            for setter in _OPS+['ucomiss','fcomi','__zf_snapshot','rol','mul','bsf']:
                got=own.make_condition('j'+alias,setter,ops)
                expected=own.make_condition('j'+canonical,setter,ops)
                assert got==expected
                expression=got[0] if got else None
                assert own.setcc('set'+alias,setter,ops)==expression
                assert own.cmovcc('cmov'+alias,setter,ops)==expression
    for setter,known in [('mul',{'o','no','b','ae'}),('rol',{'b','ae'}),
                         ('bsf',{'e','ne'}),('__zf_snapshot',{'e','ne'})]:
        for code in _CODES:
            assert (own.make_condition('j'+code,setter,ops) is not None)==(code in known)
    assert own.make_condition('jecxz','cmp',ops) is None
    assert own.make_condition('jbad','cmp',ops) is None
    assert own.make_condition('je','unknown',ops) is None
    assert own.lahf('mul',ops) is None
    assert own.zf_expression(('inc',ops))=='!_fa'
    for name in ('translation_conditions.py','translation_operands.py'):
        shutil.copyfile(ROOT/'tools'/name,tmp_path/name)
    result=subprocess.run([sys.executable,'-I','-c',
        'import sys;sys.path.insert(0,sys.argv[1]);import translation_conditions as c;'
        'assert c.make_condition("je","__zf_snapshot",[])[0]=="_flags";'
        'assert not any(n.startswith("tools.recomp") for n in sys.modules)',str(tmp_path)],capture_output=True,text=True)
    assert result.returncode==0,result.stderr


def _producer(op,bits):
    a,b,c={8:('al','bl','cl'),16:('ax','bx','cx'),32:('eax','ebx','ecx')}[bits]
    operands=[Operand('reg',reg=a),Operand('reg',reg=b)]
    context=S(needs_cf=True);instruction=S(address=0x1000,mnemonic=op)
    if op in ('cmp','test'): code=getattr(compare,op)(context,instruction,operands)
    elif op in ('add','sub','and','or','xor'): code=integer.alu_binop(context,instruction,operands,op)
    elif op in ('adc','sbb'): code=getattr(integer,op)(context,instruction,operands)
    elif op in ('inc','dec'):
        operands=operands[:1];code=integer.inc_dec(context,instruction,operands,op)
    elif op=='neg':
        operands=operands[:1];code=integer.neg(context,instruction,operands)
    else:
        if op=='cmpxchg':operands=[Operand('reg',reg=b),Operand('reg',reg=c)]
        code=exchange.exchange(context,instruction,operands,op)
    return operands,code


def _integer_source():
    macros=_production_macros({'LO8','HI8','LO16','SET_LO8','SET_HI8','SET_LO16'})
    code='#include <stdint.h>\n#include <stdio.h>\n'+macros+'\n'
    for width in (8,16,32):
        for op in _OPS:
            operands,lines=_producer(op,width)
            code+=f'static uint32_t own_{op}{width}(uint32_t a,uint32_t b,unsigned carry){{\n'
            code+='uint32_t eax=a,ebx=b,ecx=17,_fa=0,_fb=0;int32_t _fas=0,_fbs=0;int _cf=carry;\n'
            code+='\n'.join(lines)+'\neax=ebx=ecx=0; /* Conditions must survive operand overwrite. */\n'
            expressions=[own.make_condition('j'+cc,op,operands)[0] for cc in _CODES]
            code+='uint32_t result='+ ' | '.join(f'(({expr}) ? (1u<<{i}) : 0u)' for i,expr in enumerate(expressions))+';\n'
            code+=f'return result | ((uint32_t)({own.lahf(op,operands)})<<16);\n}}\n'
    code+='typedef uint32_t (*Own)(uint32_t,uint32_t,unsigned);\nstatic Own functions[3][14]={\n'
    code+=',\n'.join('{'+','.join(f'own_{op}{bits}' for op in _OPS)+'}' for bits in (8,16,32))+'};\n'
    return code


_MODEL=r'''
static uint32_t packed(unsigned flags){unsigned c=flags&1,p=(flags>>2)&1,z=(flags>>6)&1,s=(flags>>7)&1,o=(flags>>11)&1;
unsigned conditions[]={o,!o,c,!c,z,!z,c||z,!c&&!z,s,!s,p,!p,s!=o,s==o,z||(s!=o),!z&&(s==o)};
uint32_t result=0;for(unsigned i=0;i<16;++i)result|=conditions[i]<<i;
return result|(((flags&0xd5u)|2u)<<16);}
static uint32_t model(unsigned bits,unsigned op,uint32_t a,uint32_t b,unsigned carry){
uint64_t modulus=UINT64_C(1)<<bits,mask=modulus-1;uint32_t x=a&(uint32_t)mask,y=b&(uint32_t)mask,r=0;
int64_t sx=x<modulus/2?x:(int64_t)x-(int64_t)modulus,sy=y<modulus/2?y:(int64_t)y-(int64_t)modulus,total=0;
unsigned c=0,af=0,of=0;
switch(op){
case 0:case 3:case 13:r=x-y;c=x<y;af=(x&15)<(y&15);total=sx-sy;break;
case 1:case 6:r=x&y;break;
case 2:case 12:r=x+y;c=(uint64_t)x+y>=modulus;af=(x&15)+(y&15)>15;total=sx+sy;break;
case 4:r=x+y+carry;c=(uint64_t)x+y+carry>=modulus;af=(x&15)+(y&15)+carry>15;total=sx+sy+carry;break;
case 5:r=x-y-carry;c=(uint64_t)x<(uint64_t)y+carry;af=(x&15)<(y&15)+carry;total=sx-sy-carry;break;
case 7:r=x|y;break;case 8:r=x^y;break;
case 9:r=x+1;c=carry;af=(x&15)==15;total=sx+1;break;
case 10:r=x-1;c=carry;af=(x&15)==0;total=sx-1;break;
case 11:r=0u-x;c=x!=0;af=(x&15)!=0;total=-sx;break;
}
r&=(uint32_t)mask;of=total<-(int64_t)(modulus/2)||total>=(int64_t)(modulus/2);
unsigned ones=0;for(unsigned bit=0;bit<8;++bit)ones+=(r>>bit)&1;
unsigned flags=c|((!(ones&1))<<2)|(af<<4)|((r==0)<<6)|(((r>>(bits-1))&1)<<7)|(of<<11);
return packed(flags);}
'''


def test_integer_conditions_and_lahf_against_mathematical_reference(tmp_path):
    code=_integer_source()+_MODEL+r'''
static int verify(unsigned width,uint32_t a,uint32_t b){for(unsigned op=0;op<14;++op)for(unsigned carry=0;carry<2;++carry){
uint32_t got=functions[width][op](a,b,carry),expected=model(8u<<width,op,a,b,carry);
if(got!=expected){fprintf(stderr,"width=%u op=%u a=%x b=%x c=%u got=%x expected=%x\n",8u<<width,op,a,b,carry,got,expected);return 1;}}return 0;}
int main(void){for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b)if(verify(0,a,b))return 1;
uint32_t edges[]={0,1,15,16,127,128,255,32767,32768,65535,0x7fffffff,0x80000000,0xffffffff};
for(unsigned w=1;w<3;++w)for(unsigned a=0;a<13;++a)for(unsigned b=0;b<13;++b)if(verify(w,edges[a],edges[b]))return 2;
uint32_t seed=81278;for(unsigned i=0;i<3000;++i){seed=seed*1664525u+1013904223u;uint32_t a=seed;seed=seed*1664525u+1013904223u;
if(verify(1,a,seed)||verify(2,a,seed))return 3;}puts("PASS all conditions and LAHF after integer results and operand overwrite");return 0;}
'''
    source=tmp_path/'conditions.c';source.write_text(code,encoding='utf-8');exe=_compile(tmp_path,source)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr


@pytest.mark.skipif(os.name!='nt',reason='Supplemental x86 instruction oracle')
def test_integer_condition_results_against_x86(tmp_path):
    from native_compiler import native_compiler
    compiler,environment=native_compiler(tmp_path);assembly=['.code'];code=_integer_source()+_MODEL
    for bits in (8,16,32):
        a,b,c={8:('al','dl','cl'),16:('ax','dx','cx'),32:('eax','edx','ecx')}[bits]
        for op in _OPS:
            name=f'hw_{op}{bits}'
            operation=f'{op} {a}' if op in ('inc','dec','neg') else (f'cmpxchg {b},{c}' if op=='cmpxchg' else f'{op} {a},{b}')
            assembly += [f'public {name}',f'{name} proc','mov eax,ecx','mov ecx,17','bt r8d,0',operation,'pushfq','pop rax','ret',f'{name} endp']
            code+=f'extern uint32_t {name}(uint32_t,uint32_t,unsigned);\n'
    assembly.append('end');asm=tmp_path/'flags.asm';asm.write_text('\n'.join(assembly))
    obj=tmp_path/'flags.obj';r=subprocess.run([str(Path(compiler).with_name('ml64.exe')),'/nologo','/c',f'/Fo{obj}',str(asm)],env=environment,capture_output=True,text=True,timeout=60)
    assert r.returncode==0,r.stdout+r.stderr
    code+='static Own native[3][14]={'+','.join('{'+','.join(f'hw_{op}{bits}' for op in _OPS)+'}' for bits in (8,16,32))+'};\n'
    code+=r'''
static int verify(unsigned w,uint32_t a,uint32_t b){for(unsigned op=0;op<14;++op)for(unsigned c=0;c<2;++c){
uint32_t expected=packed(native[w][op](a,b,c)),got=functions[w][op](a,b,c);
uint32_t mask=(op==1||op==6||op==7||op==8)?~(1u<<20):0xffffffffu;
if((got&mask)!=(expected&mask)){fprintf(stderr,"hardware w=%u op=%u a=%x b=%x c=%u got=%x expected=%x\n",w,op,a,b,c,got,expected);return 1;}}return 0;}
int main(void){for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b)if(verify(0,a,b))return 1;
uint32_t seed=93791;for(unsigned i=0;i<10000;++i){seed=seed*1664525u+1013904223u;uint32_t a=seed;seed=seed*1664525u+1013904223u;
if(verify(1,a,seed)||verify(2,a,seed))return 2;}puts("PASS native x86 conditions and defined LAHF bits");return 0;}
'''
    source=tmp_path/'native.c';source.write_text(code,encoding='utf-8');exe=_compile(tmp_path,source,objects=[str(obj)])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr


def test_float_conditions_and_lahf_execute(tmp_path):
    code='#include <stdint.h>\n#include <math.h>\n#include <stdio.h>\n'+_MODEL
    for setter in ('ucomiss','comisd','fucomip'):
        code+=f'static uint32_t {setter}(double _fca,double _fcb,int g_fp_cmp){{unsigned r=0;\n'
        for i,cc in enumerate(_CODES):
            # Actual if spelling also exposes compiler parenthesis warnings.
            code+=f'if ({own.make_condition("j"+cc,setter,[])[0]}) r|=1u<<{i};\n'
        code+=f'return r|((uint32_t)({own.lahf(setter,[])})<<16);}}\n'
    code+=r'''
int main(void){double values[]={-INFINITY,-1.,-0.,0.,1.,INFINITY,NAN};
for(unsigned a=0;a<7;++a)for(unsigned b=0;b<7;++b){double x=values[a],y=values[b];
int unordered=isnan(x)||isnan(y),cmp=unordered?2:x<y?-1:x>y?1:0;
unsigned flags=unordered?0x45u:x<y?1u:x==y?0x40u:0u,want=packed(flags);
if(ucomiss(x,y,cmp)!=want||comisd(x,y,cmp)!=want||fucomip(x,y,cmp)!=want)return 1;}
return 0;}
'''
    source=tmp_path/'float.c';source.write_text(code,encoding='utf-8')
    run=subprocess.run([str(_compile(tmp_path,source))],capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr


def test_decoded_condition_consumers_execute(tmp_path):
    cases={};rows=[]
    for op,raw,model_op in [('cmp','39d8',0),('add','01d8',2),('sub','29d8',3),
                            ('adc','f911d8',4),('sbb','f919d8',5)]:
        for cc in range(16):
            for consumer in ('set','cmov','branch'):
                name=f'{op}_{cc}_{consumer}'
                # MOV preserves flags while deliberately destroying the ALU result.
                prefix=raw+'b800000000ba00000000b901000000'
                tail=(f'0f{0x90+cc:02x}c2c3' if consumer=='set' else
                      f'0f{0x40+cc:02x}d1c3' if consumer=='cmov' else
                      f'{0x70+cc:02x}01c3ba01000000c3')
                cases[name]=prefix+tail;rows.append((name,model_op,cc,int(op in ('adc','sbb'))))
    cases['lahf_add']='01d89fc3'
    script=r'''
import json,sys
from tools.recomp.translator import FunctionTranslator
from tools.recomp import lifter
import translation_conditions as own
assert lifter._make_condition is own.make_condition
out={}
for name,raw in json.loads(sys.argv[1]).items():
 data=bytes.fromhex(raw);info={'name':name,'size':len(data)}
 t=FunctionTranslator(b'',{0x1000:info});t._read_func_bytes=lambda a,b:data
 out[name]=t.translate_function(0x1000,info)
print(json.dumps(out))
'''
    result=subprocess.run([sys.executable,'-c',script,json.dumps(cases)],cwd=ROOT/'external/ts3FPrecomp',
        env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    generated=json.loads(result.stdout)
    code='#include <stdint.h>\n#include <stdio.h>\n'+_production_macros({'LO8','HI8','LO16','SET_LO8','SET_HI8','SET_LO16'})+'\n'
    code+='static uint32_t eax,ebx,ecx,edx,esi,edi,esp,g_ebp,g_seh_ebp;\n#define RECOMP_FAST_ENTRY(site) 0\n'
    code+='\n'.join(generated.values())+_MODEL
    code+='struct Case{void(*run)(void);unsigned op,cc,carry;};static struct Case cases[]={'+','.join(f'{{{n},{op},{cc},{c}}}' for n,op,cc,c in rows)+'};\n'
    code+=r'''
int main(void){uint32_t v[]={0,1,127,128,255,32767,32768,65535,0x7fffffff,0x80000000,0xffffffff};
for(unsigned a=0;a<11;++a)for(unsigned b=0;b<11;++b){
for(unsigned i=0;i<sizeof cases/sizeof cases[0];++i){struct Case *t=&cases[i];eax=v[a];ebx=v[b];edx=13;
t->run();unsigned want=(model(32,t->op,v[a],v[b],t->carry)>>t->cc)&1;
if(edx!=want){fprintf(stderr,"decoded case=%u a=%x b=%x got=%u want=%u\n",i,v[a],v[b],edx,want);return 1;}}
eax=v[a];ebx=v[b];lahf_add();if(((eax>>8)&255)!=((model(32,2,v[a],v[b],0)>>16)&255))return 2;}
puts("PASS decoded Jcc/SETcc/CMOVcc and LAHF with overwritten operands");return 0;}
'''
    source=tmp_path/'decoded.c';source.write_text(code,encoding='utf-8')
    run=subprocess.run([str(_compile(tmp_path,source))],capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
