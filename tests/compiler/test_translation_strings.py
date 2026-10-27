"""Original string semantics: compiled memory/state sweeps and decoded consumers."""
import os
import json
from pathlib import Path
import shutil
import subprocess
import sys
from types import SimpleNamespace as S

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/compiler'))
import translation_strings as own
from test_translation_product import _compile


def test_validation_and_independent_copy(tmp_path):
    for name in ('rep add', 'lock movsb', 'movsq', 'rep', 'rep rep movsb', 'unknown'):
        with pytest.raises(ValueError):
            own.emit(None, S(mnemonic=name))
    with pytest.raises(ValueError):
        own.emit(None, S(mnemonic='movsd', operands=[S(reg='xmm0')]))
    assert not own.is_compare(S(mnemonic='cmpsd', operands=[S(reg='xmm0')]))
    assert own.is_compare(S(mnemonic='repnz scasw'))
    assert own.direction(None, S(mnemonic='cld')) == ['g_df = 0; /* cld */']
    with pytest.raises(ValueError):
        own.direction(None, S(mnemonic='nop'))
    shutil.copyfile(ROOT / 'tools/compiler/translation_strings.py', tmp_path / 'translation_strings.py')
    run = subprocess.run([sys.executable, '-I', '-c',
        'import sys;sys.path.insert(0,sys.argv[1]);from types import SimpleNamespace as S;'
        'import translation_strings as s;assert s.emit(S(needs_cf=True),S(mnemonic="repe cmpsb"));'
        'assert not any(n.startswith("tools.recomp") for n in sys.modules)', str(tmp_path)],
        capture_output=True, text=True)
    assert run.returncode == 0, run.stderr


def _functions():
    bodies, rows = [], []
    for kind, stem in enumerate(('movs', 'stos', 'lods', 'cmps', 'scas')):
        for size, suffix in ((1, 'b'), (2, 'w'), (4, 'd')):
            for repeat, prefix in enumerate(('', 'repe ', 'repne ')):
                for narrow in (0, 1):
                    for fs in (0, 1):
                        name = f'run_{len(rows)}'
                        instruction = S(mnemonic=prefix+stem+suffix,
                                        bytes_hex=('67' if narrow else '')+('64' if fs else '')+'a4')
                        body = own.emit(S(needs_cf=True), instruction)
                        bodies.append(f'static void {name}(void) {{\n'+ '\n'.join(body)+'\n}')
                        rows.append(f'{{{name},{kind},{size},{repeat},{narrow},{fs}}}')
    return '\n'.join(bodies), ',\n'.join(rows)


def test_compiled_element_semantics_and_register_preservation(tmp_path):
    functions, rows = _functions()
    source = tmp_path / 'strings.c'
    source.write_text(r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static uint32_t eax,ecx,esi,edi,_fa,_fb; static int32_t _fas,_fbs;
static int g_df,_flags,_cf;static unsigned accesses;
static _Alignas(8) unsigned char actual[131104],expected[131104];
static _Alignas(8) unsigned char initial[6][131104];
static void *address(uint32_t p){if(p>=sizeof actual-4){fprintf(stderr,"invalid address %u\n",p);exit(90);}++accesses;return actual+p;}
#define XBOX_PTR(p) address((uint32_t)(p))
#define XBOX_FS_BASE 65536u
#define MEM8(p) (*(uint8_t*)address((uint32_t)(p)))
#define MEM16(p) (*(uint16_t*)address((uint32_t)(p)))
#define MEM32(p) (*(uint32_t*)address((uint32_t)(p)))
#define RECOMP_DF_STEP(n) (g_df ? -(n) : (n))
''' + functions + r'''
struct test {void (*run)(void);unsigned kind,size,repeat,narrow,fs;};
static const struct test tests[]={''' + rows + r'''};
static uint32_t read_bytes(uint32_t p,unsigned n){uint32_t v=0;for(unsigned b=0;b<n;++b)v|=(uint32_t)expected[p+b]<<(b*8);return v;}
static void write_bytes(uint32_t p,unsigned n,uint32_t v){for(unsigned b=0;b<n;++b)expected[p+b]=(unsigned char)(v>>(b*8));}
static uint32_t update(uint32_t initial,int64_t delta,int narrow){uint32_t v=initial+(uint32_t)delta;return narrow?(initial&0xffff0000u)|(v&65535):v;}
static int check(const struct test *t,unsigned count,int backwards,unsigned scenario,int initial_flags){
if(!t->narrow&&scenario==4)return 0;
memcpy(actual,initial[scenario],sizeof actual);
memcpy(expected,initial[scenario],sizeof expected);
uint32_t s=120,d=420;
if(scenario==1)d=s+t->size; if(scenario==2)d=s-t->size;
if(scenario==3)d=s; if(scenario==4){s=backwards?0:65536-t->size;d=s;}
eax=0xcdef3131u;ecx=count;esi=s;edi=d;
if(t->narrow){ecx|=0x12340000u;esi|=0xabcd0000u;edi|=0x98760000u;}
uint32_t si=esi,di=edi,c=ecx,a=eax,last_l=0,last_r=0;
g_df=backwards;_flags=initial_flags&1;_cf=(initial_flags>>1)&1;
int z=_flags,cf=_cf;unsigned iterations=0;
_fa=0x12345678;_fb=0x23456789;_fas=-123;_fbs=-456;
unsigned max=t->repeat?count:1;
for(unsigned n=0;n<max;++n){
uint32_t src=(t->narrow?(si&65535):si)+(t->fs?65536u:0),dst=t->narrow?(di&65535):di;
uint32_t mask=t->size==4?0xffffffffu:(1u<<(8*t->size))-1;
uint32_t left=(t->kind==0||t->kind==2||t->kind==3)?read_bytes(src,t->size):(a&mask);
if(t->kind<2)write_bytes(dst,t->size,left);
else if(t->kind==2)a=(a&~mask)|left;
else{last_l=left;last_r=read_bytes(dst,t->size);z=left==last_r;cf=left<last_r;}
int64_t step=(int64_t)t->size*(backwards?-1:1);
if(t->kind==0||t->kind==2||t->kind==3)si=update(si,step,t->narrow);
if(t->kind!=2)di=update(di,step,t->narrow);
if(t->repeat)c=update(c,-1,t->narrow);
++iterations;
if(t->repeat&&t->kind>=3&&((t->repeat==1&&!z)||(t->repeat==2&&z)))break;
}
accesses=0;t->run();
if(memcmp(actual,expected,sizeof actual)||eax!=a||ecx!=c||esi!=si||edi!=di||_flags!=z||_cf!=cf||g_df!=backwards)return 1;
if(t->repeat&&count==0&&accesses)return 2;
if(t->kind>=3&&accesses!=iterations*(t->kind==3?2u:1u))return 3;
if(t->kind>=3&&!t->repeat){unsigned shift=32-8*t->size;int32_t sl=(int32_t)((int64_t)last_l-((last_l>>(31-shift))?((int64_t)1<<(32-shift)):0));
int32_t sr=(int32_t)((int64_t)last_r-((last_r>>(31-shift))?((int64_t)1<<(32-shift)):0));
if(_fa!=last_l||_fb!=last_r||_fas!=sl||_fbs!=sr)return 4;}
else if(_fa!=0x12345678||_fb!=0x23456789||_fas!=-123||_fbs!=-456)return 5;
return 0;}
int main(void){unsigned counts[]={0,1,2,7};
/* Cache immutable input patterns; reset both full buffers for every case. */
for(unsigned scenario=0;scenario<6;++scenario)for(unsigned i=0;i<sizeof actual;++i)
initial[scenario][i]=scenario==5?0x31:(unsigned char)((i*37u+(i>>3)*11u+scenario*7u)&255u);
for(unsigned i=0;i<sizeof tests/sizeof tests[0];++i)for(unsigned c=0;c<4;++c)for(int df=0;df<2;++df)for(unsigned scenario=0;scenario<6;++scenario)for(int flags=0;flags<4;++flags){
int r=check(&tests[i],counts[c],df,scenario,flags);if(r){fprintf(stderr,"case=%u count=%u df=%d scenario=%u flags=%d error=%d\n",i,counts[c],df,scenario,flags,r);return r;}}
puts("PASS string widths, single/repeat modes, zero count, overlap, direction, address wrap, FS, reads and preserved state");return 0;}
''', encoding='utf-8')
    exe = _compile(tmp_path, source)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=90)
    assert run.returncode == 0, run.stdout + run.stderr


def test_decoded_production_consumers(tmp_path):
    script = r'''
import json
from tools.recomp.lifter import Lifter
from tools.recomp.translator import FunctionTranslator
import translation_strings as own
assert Lifter._lift_string_op is own.emit and Lifter._lift_rep_string is own.emit
cases={'single_cmp':'a70f94c30f92c10f9cc2c3',
       'bare_cmp':'a6c3','bare_rep':'f3a6c3',
       'single_scas':'ae0f94c30f92c10f9cc2c3',
       'rep_cmp':'31c0f3a60f94c219dbc3',
       'rep_load':'f3acc3','narrow_copy':'67f3a4c3',
       'preserve':'39d8f3aa0f94c2c3'}
out={}
for name,raw in cases.items():
 data=bytes.fromhex(raw);info={'name':name,'size':len(data)}
 t=FunctionTranslator(b'',{0x1000:info});t._read_func_bytes=lambda a,b:data
 out[name]=t.translate_function(0x1000,info)
print(json.dumps(out))
'''
    result = subprocess.run([sys.executable, '-c', script], cwd=ROOT/'external/ts3FPrecomp',
                            env=dict(os.environ, TSFP_PROJECT_ROOT=str(ROOT)), capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout+result.stderr
    from test_translation_moves import _production_macros
    code = '#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\n'
    code += _production_macros({'LO8','HI8','LO16','SET_LO8','SET_HI8','SET_LO16','CMP_EQ','CMP_B','CMP_L','RECOMP_SIGNED','RECOMP_FLAG_WIDTH'})
    code += r'''
static uint32_t eax,ebx,ecx,edx,esi,edi,esp,g_ebp,g_seh_ebp;
static int g_df;static _Alignas(8) unsigned char mem[65540];
#define RECOMP_FAST_ENTRY(site) 0
#define XBOX_PTR(p) (mem+(p))
#define MEM8(p) (*(uint8_t*)(mem+(p)))
#define MEM16(p) (*(uint16_t*)(mem+(p)))
#define MEM32(p) (*(uint32_t*)(mem+(p)))
#define RECOMP_DF_STEP(n) (g_df?-(n):(n))
'''+'\n'.join(json.loads(result.stdout).values())+r'''
int main(void){uint32_t values[]={0,1,127,128,255,0x7fffffffu,0x80000000u,0xffffffffu};
for(unsigned a=0;a<8;++a)for(unsigned b=0;b<8;++b){uint32_t x=values[a],y=values[b];
MEM32(32)=x;MEM32(64)=y;esi=32;edi=64;ebx=ecx=edx=0;single_cmp();
if(esi!=36||edi!=68||ebx!=(x==y)||ecx!=(x<y)||edx!=((int32_t)x<(int32_t)y))return 1;
eax=x;esi=32;edi=64;ebx=ecx=edx=0;single_scas();
if(esi!=32||edi!=65||ebx!=((uint8_t)x==(uint8_t)y)||ecx!=((uint8_t)x<(uint8_t)y)||edx!=((int8_t)x<(int8_t)y))return 2;
for(unsigned n=0;n<2;++n){MEM8(32)=(uint8_t)x;MEM8(64)=(uint8_t)y;esi=32;edi=64;ecx=n;edx=0;rep_cmp();
if(edx!=(n?((uint8_t)x==(uint8_t)y):1)||ebx!=(n&&(uint8_t)x<(uint8_t)y?0xffffffffu:0))return 3;
eax=x;ebx=y;edx=0;edi=64;ecx=n;preserve();if(edx!=(x==y))return 4;}}
mem[65535]=23;mem[0]=42;esi=0xabcdffffu;edi=0x12340020u;ecx=0x56780002u;narrow_copy();
if(mem[32]!=23||mem[33]!=42||esi!=0xabcd0001u||edi!=0x12340022u||ecx!=0x56780000u)return 5;
esi=32;ecx=2;eax=0x12345600u;rep_load();if(eax!=0x1234562au||esi!=34||ecx)return 6;
puts("PASS decoded single/repeated strings, flag consumers, prefix count and preserved incoming flags");return 0;}
'''
    source=tmp_path/'decoded.c';source.write_text(code,encoding='utf-8');exe=_compile(tmp_path,source)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr+f' exit={run.returncode}'


def test_inherited_compiled_rep_comparisons_and_negative_controls(tmp_path):
    # Execute the existing independent behavioral regressions with the available
    # host compiler too; their legacy runner only discovers Clang/GCC on PATH.
    script=r'''
import sys,json
from tools.recomp import test_lifter_rep_compare_flags as f
print(json.dumps({'sources':f._generated(),'prelude':f.PRELUDE,'main':f.MAIN,'fixtures':f.FIXTURES}))
'''
    result=subprocess.run([sys.executable,'-c',script],cwd=ROOT/'external/ts3FPrecomp',
                          env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    data=json.loads(result.stdout);names=list(data['fixtures'])
    declarations=f'#define NF {len(names)}\nstatic void (*fixtures[])(void)={{'+','.join('fixture_'+n for n in names)+'};\n'
    declarations+='static const char *names[]={'+','.join(json.dumps(n) for n in names)+'};\n'
    declarations+='static const unsigned sizes[]={'+','.join(str(data['fixtures'][n][2]) for n in names)+'};\n'
    declarations+='static const int scas[]={'+','.join(str(int(data['fixtures'][n][3])) for n in names)+'};\n'
    import re
    for mode in ('normal','missing-carry','missing-zero-preload'):
        generated='\n'.join(data['sources'][n] for n in names)
        if mode=='missing-carry':
            generated,count=re.subn(r'\n\s*_cf = \(_str_value[^\n]*;', '',generated)
            assert count>0
        elif mode=='missing-zero-preload':
            generated,count=re.subn(r'\n\s*_flags = \([^\n]*ZF in[^\n]*','',generated)
            assert count>0
        source=tmp_path/(mode+'.c');source.write_text(data['prelude']+generated+declarations+data['main'],encoding='utf-8')
        exe=_compile(tmp_path,source);run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
        if mode=='normal':
            assert run.returncode==0,run.stdout+run.stderr
            assert int(run.stdout)>1000
        else:
            assert run.returncode==1,run.stdout+run.stderr
            assert ('want -1' if mode=='missing-carry' else 'count 0') in run.stderr


@pytest.mark.skipif(os.name != 'nt', reason='Supplemental Windows x64 string-instruction oracle')
def test_against_native_x86_strings(tmp_path):
    from native_compiler import native_compiler
    compiler, environment = native_compiler(tmp_path)
    assembly = ['.code']
    code = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {uint64_t s,d,c,a,flags;} State;
static uint32_t eax,ecx,esi,edi,_fa,_fb;static int32_t _fas,_fbs;static int g_df,_flags,_cf;
static unsigned char actual[512],reference[512];
#define XBOX_PTR(p) (actual+(p))
#define MEM8(p) (*(uint8_t*)(actual+(p)))
#define MEM16(p) (*(uint16_t*)(actual+(p)))
#define MEM32(p) (*(uint32_t*)(actual+(p)))
#define RECOMP_DF_STEP(n) (g_df?-(n):(n))
'''
    records = []
    for stem in ('movs', 'stos', 'lods', 'cmps', 'scas'):
        for size, suffix in ((1, 'b'), (2, 'w'), (4, 'd')):
            for prefix in ('', 'repe ', 'repne '):
                index = len(records)
                name = prefix+stem+suffix
                native = name if stem in ('cmps', 'scas') else ('rep ' if prefix else '')+stem+suffix
                assembly += [f'public hw_{index}', f'hw_{index} proc',
                    'push rsi', 'push rdi', 'mov r10,rcx', 'mov rsi,[r10]',
                    'mov rdi,[r10+8]', 'mov rcx,[r10+16]', 'mov rax,[r10+24]',
                    'push qword ptr [r10+32]', 'popfq', native, 'pushfq', 'pop r11', 'cld',
                    'mov [r10],rsi', 'mov [r10+8],rdi', 'mov [r10+16],rcx',
                    'mov [r10+24],rax', 'mov [r10+32],r11', 'pop rdi', 'pop rsi', 'ret',
                    f'hw_{index} endp']
                code += f'extern void hw_{index}(State*);\nstatic void own_{index}(void){{\n'
                code += '\n'.join(own.emit(S(needs_cf=True), S(mnemonic=name)))+'\n}\n'
                records.append(f'{{own_{index},hw_{index},{size}}}')
    assembly.append('end')
    asm = tmp_path/'strings.asm'; asm.write_text('\n'.join(assembly))
    obj = tmp_path/'strings.obj'
    result = subprocess.run([str(Path(compiler).with_name('ml64.exe')), '/nologo', '/c', f'/Fo{obj}', str(asm)],
                            env=environment, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    code += 'struct entry {void (*own)(void);void (*hw)(State*);unsigned size;};\n'
    code += 'static struct entry entries[]={'+','.join(records)+'};\n'
    code += r'''
int main(void){for(unsigned f=0;f<45;++f)for(unsigned n=0;n<9;++n)for(unsigned df=0;df<2;++df)for(unsigned flags=0;flags<4;++flags)for(unsigned overlap=0;overlap<3;++overlap){
for(unsigned i=0;i<512;++i)actual[i]=reference[i]=(unsigned char)(i*13u+(i>>3));
eax=0x12345678;ecx=n;esi=128;edi=overlap?128+(overlap==1?entries[f].size:0):256;g_df=df;_flags=flags&1;_cf=flags>>1;
State s={(uintptr_t)(reference+esi),(uintptr_t)(reference+edi),n,eax,0x202u|(df<<10)|(_flags<<6)|_cf};
entries[f].hw(&s);entries[f].own();
if(memcmp(actual,reference,512)||esi!=(uint32_t)(s.s-(uintptr_t)reference)||edi!=(uint32_t)(s.d-(uintptr_t)reference)||ecx!=s.c||eax!=s.a||_flags!=(int)((s.flags>>6)&1)||_cf!=(int)(s.flags&1)||g_df!=(int)((s.flags>>10)&1)){
fprintf(stderr,"native mismatch function=%u n=%u df=%u flags=%u overlap=%u\n",f,n,df,flags,overlap);return 1;}}
puts("PASS native x86 strings, memory, indices, accumulator, count, direction and zero/carry flags");return 0;}
'''
    source = tmp_path/'oracle.c'; source.write_text(code, encoding='utf-8')
    exe = _compile(tmp_path, source, objects=[str(obj)])
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    assert run.returncode == 0, run.stdout + run.stderr
