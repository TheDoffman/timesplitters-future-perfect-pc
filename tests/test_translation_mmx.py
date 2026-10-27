"""Independent packed-integer references and native MMX conformance."""
import json,os,random,shutil,subprocess,sys
from pathlib import Path
from types import SimpleNamespace as S
import pytest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import translation_mmx as own
from translation_decode import Operand
from test_translation_product import _compile
from native_compiler import native_compiler

def reg(name):return Operand('reg',reg=name)
def mem(size):return Operand('mem',mem_base='eax',mem_size=size)
def emit(name,ops):return own.emit(S(_unimplemented=lambda *_:['UNSUPPORTED;']),S(address=0x1000),name,ops)

_PRELUDE=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
typedef union{uint64_t q;} RecompMmx;
typedef union{float f[4];double d[2];uint32_t u[4];uint64_t q[2];} RecompXmm;
static RecompMmx mm0,mm1;static RecompXmm xmm0;
static uint32_t eax,ebx,esp,g_ebp;static unsigned reads,writes;
static unsigned char memory[64];
static uint32_t read32(uint32_t p){uint32_t v;if(p>60)abort();memcpy(&v,memory+p,4);reads++;return v;}
static uint16_t read16(uint32_t p){uint16_t v;if(p>62)abort();memcpy(&v,memory+p,2);reads++;return v;}
static void write32(uint32_t p,uint32_t v){if(p>60)abort();memcpy(memory+p,&v,4);writes++;}
#define MEM32(p) read32(p)
#define MEM16(p) read16(p)
'''

def function(name,instruction,args):
    import re
    lines=emit(instruction,args)
    lines=[re.sub(r'^MEM32\((.*?)\) = (.*);$',r'write32(\1, \2);',line) for line in lines]
    return f'static void {name}(void){{\n'+'\n'.join(lines)+'\n}\n'

def execute(tmp_path,code,objects=()):
    p=tmp_path/'mmx.c';p.write_text(code,encoding='utf-8')
    run=subprocess.run([str(_compile(tmp_path,p,objects=objects))],capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr+f' exit={run.returncode}'

def test_standalone_and_invalid_forms(tmp_path):
    for name,args in [('paddb',[]),('psllw',[reg('mm0'),reg('eax')]),
        ('pinsrw',[reg('mm0'),reg('eax'),reg('ecx')]),('movq',[reg('eax'),reg('mm0')]),('emms',[reg('mm0')])]:
        with pytest.raises(ValueError):emit(name,args)
    assert emit('unsupported',[reg('mm0')])==['UNSUPPORTED;']
    assert emit('paddb',[reg('xmm0'),reg('mm1')])==['UNSUPPORTED;']
    for name in ('translation_mmx.py','translation_operands.py'):shutil.copyfile(ROOT/'tools'/name,tmp_path/name)
    run=subprocess.run([sys.executable,'-I','-c',"import sys;sys.path.insert(0,sys.argv[1]);import translation_mmx as m;assert 'pmaddwd' in m.MNEMONICS",str(tmp_path)],capture_output=True,text=True)
    assert run.returncode==0,run.stdout+run.stderr

def lanes(value,width,signed=False):
    output=[(value//(2**(i*width)))%(2**width) for i in range(64//width)]
    return [v-2**width if signed and v>=2**(width-1) else v for v in output]
def pack(values,width):return sum((v%(2**width))*2**(width*i) for i,v in enumerate(values))
def clamp(value,width,signed):return max(-(2**(width-1)) if signed else 0,min(value,2**(width-1)-1 if signed else 2**width-1))

def reference(name,a,b):
    # Mathematical Python integers, independent of emitted C.
    if name in ('pand','pandn','por','pxor'):
        return {'pand':a&b,'pandn':(~a)&b,'por':a|b,'pxor':a^b}[name]%(2**64)
    if name.startswith(('padd','psub')):
        width=8 if name.endswith('b') else 16 if name.endswith('w') else 32
        saturation=name[4:-1];signed=saturation=='s';x,y=lanes(a,width,signed),lanes(b,width,signed)
        result=[u+v if name.startswith('padd') else u-v for u,v in zip(x,y)]
        return pack([clamp(v,width,signed) for v in result] if saturation else result,width)
    if name.startswith('pcmp'):
        width={'b':8,'w':16,'d':32}[name[-1]];equal=name.startswith('pcmpeq')
        return pack([-1 if (u==v if equal else u>v) else 0 for u,v in zip(lanes(a,width,not equal),lanes(b,width,not equal))],width)
    if name.startswith('punpck'):
        width={'bw':8,'wd':16,'dq':32}[name[-2:]];x,y=lanes(a,width),lanes(b,width);start=len(x)//2 if name[6]=='h' else 0
        return pack([v for pair in zip(x[start:start+len(x)//2],y[start:start+len(y)//2]) for v in pair],width)
    if name.startswith('pack'):
        wide=32 if name=='packssdw' else 16;return pack([clamp(v,wide//2,name!='packuswb') for v in lanes(a,wide,True)+lanes(b,wide,True)],wide//2)
    if name in ('pmullw','pmulhw','pmaddwd'):
        products=[x*y for x,y in zip(lanes(a,16,True),lanes(b,16,True))]
        if name=='pmaddwd':return pack([products[0]+products[1],products[2]+products[3]],32)
        return pack([v//65536 for v in products] if name=='pmulhw' else products,16)
    if name.startswith('pavg'):
        width=8 if name.endswith('b') else 16;return pack([(x+y+1)//2 for x,y in zip(lanes(a,width),lanes(b,width))],width)
    if name in ('pminsw','pmaxsw'):return pack([(min if name=='pminsw' else max)(x,y) for x,y in zip(lanes(a,16,True),lanes(b,16,True))],16)
    if name=='psadbw':return sum(abs(x-y) for x,y in zip(lanes(a,8),lanes(b,8)))
    if name.startswith(('psll','psrl','psra')):
        width={'w':16,'d':32,'q':64}[name[-1]];arithmetic=name.startswith('psra');count=min(b,width)
        x=lanes(a,width,arithmetic);return pack([v*2**count if name.startswith('psll') else v//2**count for v in x],width)
    raise AssertionError(name)

BINARY=tuple(sorted(set(own._WRAP)|set(own._SAT)|set(own._COMPARE)|set(own._UNPACK)|{'packsswb','packuswb','packssdw','pmullw','pmulhw','pmaddwd','pavgb','pavgw','pminsw','pmaxsw','psadbw','pand','pandn','por','pxor'}))

def test_integer_lanes_and_shift_references_execute(tmp_path):
    rng=random.Random(20261027)
    edges=[0,2**64-1,0x8000800080008000,0x7fff7fff7fff7fff,0x8080808080808080,0x7f7f7f7f7f7f7f7f,0x8000000080000000,0x7fffffff7fffffff]
    pairs=[(a,b) for a in edges for b in edges]+[(rng.getrandbits(64),rng.getrandbits(64)) for _ in range(128)]
    code=_PRELUDE
    for name in BINARY:
        code+=function(name,name,[reg('mm0'),reg('mm1')])+function(name+'_alias',name,[reg('mm0'),reg('mm0')])+function(name+'_mem',name,[reg('mm0'),mem(8)])
    code+='struct Case{uint64_t a,b,expected['+str(len(BINARY))+'],alias['+str(len(BINARY))+'];};\n'
    hexes=lambda values:','.join(f'UINT64_C(0x{v:X})' for v in values)
    code+='static struct Case cases[]={'+','.join('{'+hexes((a,b))+',{'+hexes(reference(n,a,b) for n in BINARY)+'},{'+hexes(reference(n,a,a) for n in BINARY)+'}}' for a,b in pairs)+'};\n'
    for suffix in ('','_alias','_mem'):code+='static void(*ops'+suffix+'[])(void)={'+','.join(n+suffix for n in BINARY)+'};\n'
    code+='static unsigned read_counts[]={'+','.join('1' if n in ('punpcklbw','punpcklwd','punpckldq') else '2' for n in BINARY)+'};\n'
    shifts=tuple(sorted(own._SHIFT));counts=(0,1,7,15,16,17,31,32,33,63,64,65,127,255,256,2**32,2**64-1)
    for name in shifts:
        code+=function(name,name,[reg('mm0'),reg('mm1')])+function(name+'_imm255',name,[reg('mm0'),Operand('imm',imm=255)])
    code+='static void(*shifts[])(void)={'+','.join(shifts)+'};\n'
    code+='static void(*shift255[])(void)={'+','.join(n+'_imm255' for n in shifts)+'};\n'
    code+='static struct Shift{uint64_t a,count,want['+str(len(shifts))+'];} shift_cases[]={'+','.join('{'+hexes((a,c))+',{'+hexes(reference(n,a,c) for n in shifts)+'}}' for a in edges for c in counts)+'};\n'
    code+=r'''
int main(void){for(unsigned t=0;t<sizeof cases/sizeof cases[0];++t){struct Case *p=&cases[t];for(unsigned op=0;op<sizeof ops/sizeof ops[0];++op){
mm0.q=p->a;mm1.q=p->b;ops[op]();if(mm0.q!=p->expected[op]){fprintf(stderr,"register op=%u case=%u got=%llx want=%llx\n",op,t,(unsigned long long)mm0.q,(unsigned long long)p->expected[op]);return 1;}
mm0.q=p->a;ops_alias[op]();if(mm0.q!=p->alias[op])return 2;
mm0.q=p->a;eax=3;memcpy(memory+3,&p->b,8);reads=0;ops_mem[op]();if(mm0.q!=p->expected[op]||reads!=read_counts[op])return 3;}}
for(unsigned t=0;t<sizeof shift_cases/sizeof shift_cases[0];++t){struct Shift *p=&shift_cases[t];for(unsigned op=0;op<sizeof shifts/sizeof shifts[0];++op){mm0.q=p->a;mm1.q=p->count;shifts[op]();if(mm0.q!=p->want[op])return 4;
if(p->count==255){mm0.q=p->a;shift255[op]();if(mm0.q!=p->want[op])return 5;}}}
for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b){int sa=a<128?(int)a:(int)a-256,sb=b<128?(int)b:(int)b-256,sum=sa+sb,difference=sa-sb;
mm0.q=a;mm1.q=b;paddsb();int want=sum<-128?-128:sum>127?127:sum;if((mm0.q&255)!=(unsigned)(want&255))return 6;
mm0.q=a;psubsb();want=difference<-128?-128:difference>127?127:difference;if((mm0.q&255)!=(unsigned)(want&255))return 7;
mm0.q=a;paddusb();if((mm0.q&255)!=(a+b>255?255:a+b))return 8;
mm0.q=a;psubusb();if((mm0.q&255)!=(a<b?0:a-b))return 9;}
puts("PASS MMX arithmetic/saturation/packing/unpack/multiply/compare/shift math and alias/memory");return 0;}
'''
    execute(tmp_path,code)

def test_moves_shuffle_masks_and_conversions_execute(tmp_path):
    code=_PRELUDE
    for name,args in {
        'load32':('movd',[reg('mm0'),mem(4)]),'load64':('movq',[reg('mm0'),mem(8)]),
        'store32':('movd',[mem(4),reg('mm0')]),'store64':('movq',[mem(8),reg('mm0')]),
        'stream':('movntq',[mem(8),reg('mm0')]),'from_gpr':('movd',[reg('mm0'),reg('eax')]),
        'to_gpr':('movd',[reg('eax'),reg('mm0')]),'mask':('pmovmskb',[reg('eax'),reg('mm0')]),
        'pi_to_ps':('cvtpi2ps',[reg('xmm0'),reg('mm0')]),'pi_to_ps_mem':('cvtpi2ps',[reg('xmm0'),mem(8)]),
        'ps_to_pi':('cvtps2pi',[reg('mm0'),reg('xmm0')]),'ps_to_pi_mem':('cvtps2pi',[reg('mm0'),mem(8)]),
        'ps_truncate':('cvttps2pi',[reg('mm0'),reg('xmm0')]),'empty':('emms',[]),
    }.items():code+=function(name,*args)
    for name in ('punpcklbw','punpcklwd','punpckldq'):code+=function(name+'_boundary',name,[reg('mm0'),mem(4)])
    for i in range(256):
        for prefix,instruction,args in (
            ('shuffle','pshufw',[reg('mm0'),reg('mm0')]),
            ('insert','pinsrw',[reg('mm0'),reg('ebx')]),
            ('insert_mem','pinsrw',[reg('mm0'),mem(2)]),
            ('extract','pextrw',[reg('eax'),reg('mm0')])):
            code+=function(prefix+str(i),instruction,args+[Operand('imm',imm=i)])
    for prefix in ('shuffle','insert','insert_mem','extract'):
        code+='static void(*'+prefix+'[])(void)={'+','.join(prefix+str(i) for i in range(256))+'};\n'
    code+=r'''
int main(void){uint64_t pattern=UINT64_C(0x80017ffeff00abcd);
for(unsigned offset=0;offset<8;++offset){eax=offset;memcpy(memory+offset,&pattern,8);reads=writes=0;mm0.q=UINT64_MAX;load32();
if(mm0.q!=(uint32_t)pattern||reads!=1||writes)return 1;load64();if(mm0.q!=pattern||reads!=3)return 2;
memset(memory,0x55,64);writes=0;store32();if(writes!=1)return 3;
for(unsigned i=0;i<64;++i)if(memory[i]!=(i>=offset&&i<offset+4?(unsigned char)(pattern>>(8*(i-offset))):0x55))return 4;
memset(memory,0x55,64);writes=0;store64();if(writes!=2||memcmp(memory+offset,&pattern,8))return 5;
stream();if(writes!=4||memcmp(memory+offset,&pattern,8))return 6;}
eax=0x87654321u;mm0.q=UINT64_MAX;from_gpr();if(mm0.q!=eax)return 7;mm0.q=pattern;to_gpr();if(eax!=(uint32_t)pattern)return 8;
for(unsigned i=0;i<256;++i){mm0.q=pattern;shuffle[i]();uint64_t want=0;
for(unsigned j=0;j<4;++j)want|=((pattern>>(((i>>(2*j))&3)*16))&65535)<<(16*j);if(mm0.q!=want)return 9;
mm0.q=pattern;ebx=0x12345678u;insert[i]();want=(pattern&~(UINT64_C(65535)<<(16*(i%4))))|(UINT64_C(0x5678)<<(16*(i%4)));if(mm0.q!=want)return 10;
mm0.q=pattern;eax=3;memcpy(memory+3,&ebx,2);reads=0;insert_mem[i]();if(mm0.q!=want||reads!=1)return 11;
mm0.q=pattern;extract[i]();if(eax!=((pattern>>(16*(i%4)))&65535))return 12;}
for(unsigned bits=0;bits<256;++bits){mm0.q=0;for(unsigned i=0;i<8;++i)mm0.q|=(uint64_t)((bits>>i)&1)<<(8*i+7);mask();if(eax!=bits)return 13;}
mm0.q=UINT64_C(0x800000007fffffff);xmm0.q[1]=pattern;pi_to_ps();if(xmm0.f[0]!=(float)2147483647||xmm0.f[1]!=-2147483648.f||xmm0.q[1]!=pattern)return 14;
eax=3;memcpy(memory+3,&mm0.q,8);reads=0;pi_to_ps_mem();if(xmm0.f[1]!=-2147483648.f||xmm0.q[1]!=pattern||reads!=2)return 15;
int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
float values[]={-INFINITY,-2147483648.f,-3.5f,-2.5f,-1.9f,-0.f,0.f,1.9f,2.5f,3.5f,2147483648.f,INFINITY,NAN};
for(unsigned mode=0;mode<4;++mode){fesetround(modes[mode]);for(unsigned i=0;i<13;++i)for(unsigned j=0;j<13;++j){
xmm0.f[0]=values[i];xmm0.f[1]=values[j];uint64_t rounded=0,truncated=0;
for(unsigned k=0;k<2;++k){double r=nearbyint(xmm0.f[k]),t=trunc(xmm0.f[k]);
uint32_t ri=r>=-2147483648.&&r<2147483648.?(uint32_t)(int32_t)r:0x80000000u,ti=t>=-2147483648.&&t<2147483648.?(uint32_t)(int32_t)t:0x80000000u;
rounded|=(uint64_t)ri<<(32*k);truncated|=(uint64_t)ti<<(32*k);}
ps_to_pi();if(mm0.q!=rounded)return 16;ps_truncate();if(mm0.q!=truncated)return 17;
eax=3;memcpy(memory+3,&xmm0,8);reads=0;ps_to_pi_mem();if(mm0.q!=rounded||reads!=2)return 18;}}
fesetround(FE_TONEAREST);eax=60;uint32_t low=0x44332211u;memcpy(memory+60,&low,4);reads=0;
mm0.q=UINT64_C(0x88776655);punpcklbw_boundary();if(mm0.q!=UINT64_C(0x4488337722661155)||reads!=1)return 19;
mm0.q=UINT64_C(0x88776655);punpcklwd_boundary();if(mm0.q!=UINT64_C(0x4433887722116655)||reads!=2)return 20;
mm0.q=UINT64_C(0x88776655);punpckldq_boundary();if(mm0.q!=UINT64_C(0x4433221188776655)||reads!=3)return 21;
empty();puts("PASS MMX moves, unaligned widths, all lane controls, masks and conversion modes");return 0;}
'''
    execute(tmp_path,code)


@pytest.mark.skipif(os.name!='nt',reason='Supplemental native x86 MMX oracle')
def test_against_native_mmx(tmp_path):
    names=BINARY+tuple(sorted(own._SHIFT))
    assembly=['.code'];declarations=[];code=_PRELUDE
    for name in names:
        code+=function('own_'+name,name,[reg('mm0'),reg('mm1')])
        assembly += [f'public hw_{name}',f'hw_{name} proc','movq mm0,rdx','movq mm1,r8',
                     f'{name} mm0,mm1','movq qword ptr [rcx],mm0','emms','ret',f'hw_{name} endp']
        declarations.append(f'extern void hw_{name}(uint64_t*,uint64_t,uint64_t);')
    for name in ('cvtps2pi','cvttps2pi'):
        code+=function('own_'+name,name,[reg('mm0'),reg('xmm0')])
        assembly += [f'public hw_{name}',f'hw_{name} proc','movq xmm0,qword ptr [rdx]',f'{name} mm0,xmm0',
                     'movq qword ptr [rcx],mm0','emms','ret',f'hw_{name} endp']
        declarations.append(f'extern void hw_{name}(uint64_t*,RecompXmm*);')
    assembly+=['end'];compiler,env=native_compiler(tmp_path);asm=tmp_path/'mmx.asm';obj=tmp_path/'native_mmx.obj'
    asm.write_text('\n'.join(assembly)+'\n',encoding='utf-8')
    run=subprocess.run([str(Path(compiler).with_name('ml64.exe')),'/nologo','/c',f'/Fo{obj}',str(asm)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
    code+='\n'.join(declarations)
    code+='struct Case{void(*own)(void);void(*hw)(uint64_t*,uint64_t,uint64_t);};static struct Case ops[]={'+','.join('{own_'+n+',hw_'+n+'}' for n in names)+'};\n'
    code+=r'''
int main(void){uint64_t seed=1234567,want;
uint64_t edges[]={0,UINT64_MAX,UINT64_C(0x8000800080008000),UINT64_C(0x7fff7fff7fff7fff),UINT64_C(0x8080808080808080),UINT64_C(0x7f7f7f7f7f7f7f7f),UINT64_C(0x8000000080000000),UINT64_C(0x7fffffff7fffffff)};
for(unsigned t=0;t<2000;++t)for(unsigned op=0;op<sizeof ops/sizeof ops[0];++op){
seed=seed*UINT64_C(6364136223846793005)+1;uint64_t a=t<64?edges[t/8]:seed;seed=seed*UINT64_C(6364136223846793005)+1;uint64_t b=t<64?edges[t%8]:t<256?t-64:seed;
ops[op].hw(&want,a,b);mm0.q=a;mm1.q=b;ops[op].own();if(mm0.q!=want){fprintf(stderr,"native op=%u trial=%u got=%llx want=%llx\n",op,t,(unsigned long long)mm0.q,(unsigned long long)want);return 1;}}
int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};float values[]={-INFINITY,-2147483648.f,-3.5f,-2.5f,-1.9f,-0.f,0.f,1.9f,2.5f,3.5f,2147483648.f,INFINITY,NAN};
for(unsigned mode=0;mode<4;++mode){fesetround(modes[mode]);for(unsigned i=0;i<13;++i)for(unsigned j=0;j<13;++j){xmm0.f[0]=values[i];xmm0.f[1]=values[j];
hw_cvtps2pi(&want,&xmm0);own_cvtps2pi();if(mm0.q!=want)return 2;
hw_cvttps2pi(&want,&xmm0);own_cvttps2pi();if(mm0.q!=want)return 3;}}
fesetround(FE_TONEAREST);puts("PASS hardware MMX packed integers, shifts, overflow, saturation and FP conversion");return 0;}
'''
    execute(tmp_path,code,[str(obj)])

def test_decoded_production_mmx_execute(tmp_path):
    script=r'''
import json
from tools.recomp.lifter import Lifter
from tools.recomp.translator import FunctionTranslator
import translation_mmx as own
import translation_sse as sse
assert Lifter._lift_mmx is own.emit
assert Lifter._lift_sse is sse.emit
cases={'packed_math':'0ffcc10ff5c10f7ec00f77c3',
       'preserve_flags':'39d80fddc10f94c20f77c3',
       'convert_pair':'0f2ac00f2dc00f7ec00f77c3',
       'shift_pair':'0fe1c10f7ec00f77c3',
       'memory_pair':'0f6f000f7f40080f77c3'}
out={}
for name,hexadecimal in cases.items():
 data=bytes.fromhex(hexadecimal);info={'name':name,'size':len(data)}
 t=FunctionTranslator(b'',{0x1000:info});t._read_func_bytes=lambda a,b:data
 out[name]=t.translate_function(0x1000,info)
print(json.dumps(out))
'''
    run=subprocess.run([sys.executable,'-c',script],cwd=ROOT/'external/ts3FPrecomp',env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    from test_translation_moves import _production_macros
    code=_PRELUDE+'\n'+_production_macros({'SET_LO8'})+'\nstatic uint32_t ecx,edx,esi,edi,g_seh_ebp;\n#define RECOMP_FAST_ENTRY(site) 0\n'
    import re
    routines='\n'.join(json.loads(run.stdout).values())
    routines='\n'.join(re.sub(r'^(\s*)MEM32\((.*?)\) = (.*);$',r'\1write32(\2, \3);',line) for line in routines.splitlines())
    code+=routines
    a,b=0x80017ffe7fff8000,0x7fff800080017ffe
    want=reference('pmaddwd',reference('paddb',a,b),b)
    code+=f'\nint main(void){{mm0.q=UINT64_C(0x{a:X});mm1.q=UINT64_C(0x{b:X});packed_math();if(mm0.q!=UINT64_C(0x{want:X})||eax!=0x{want&0xffffffff:X}u)return 1;\n'
    code+=r'''
for(unsigned equal=0;equal<2;++equal){eax=2;ebx=2+equal;edx=0;mm0.q=UINT64_MAX;mm1.q=1;preserve_flags();if(edx!=!equal)return 2;}
mm0.q=UINT64_C(0xfffffffe00000003);xmm0.q[1]=UINT64_C(0x123456789abcdef0);convert_pair();if(mm0.q!=UINT64_C(0xfffffffe00000003)||eax!=3||xmm0.q[1]!=UINT64_C(0x123456789abcdef0))return 3;
mm0.q=UINT64_C(0x80017ffe8000ffff);mm1.q=UINT64_C(0x100000000);shift_pair();if(mm0.q!=UINT64_C(0xffff0000ffffffff)||eax!=0xffffffffu)return 4;
eax=3;uint64_t bits=UINT64_C(0x123456789abcdef0);memcpy(memory+3,&bits,8);reads=writes=0;memory_pair();if(mm0.q!=bits||memcmp(memory+11,&bits,8)||reads!=2||writes!=2)return 5;
puts("PASS decoded MMX arithmetic, integer flags, conversion, 64-bit shift counts and memory");return 0;}
'''
    execute(tmp_path,code)
