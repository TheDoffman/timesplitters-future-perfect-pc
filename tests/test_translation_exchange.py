"""Original exchange/atomic contracts, native x86 oracles and thread contention."""
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
import translation_exchange as exchange
from translation_decode import Operand
from native_compiler import native_compiler
from test_translation_product import _compile
from test_translation_moves import _production_macros


def atomic_source():
    text = (ROOT/'external/ts3FPrecomp/templates/runtime/recomp_types.h').read_text(encoding='utf-8')
    return text[text.index('/* TSFP own atomics begin'):text.index('/* TSFP own atomics end */')]


def test_reject_invalid_encodings():
    for args,op in [([], 'xchg'), ([Operand('reg',reg='eax'),Operand('imm',imm=1)],'xadd'),
                    ([Operand('reg',reg='ax'),Operand('reg',reg='ecx')],'cmpxchg'),
                    ([Operand('reg',reg='eax'),Operand('reg',reg='ecx')],'lock xadd'),
                    ([Operand('mem',mem_size=4),Operand('mem',mem_size=4)],'xchg'),
                    ([Operand('reg',reg='xmm0'),Operand('reg',reg='eax')],'xchg')]:
        with pytest.raises(ValueError): exchange.exchange(None,None,args,op)


def test_register_memory_alias_flags_and_hardware(tmp_path):
    functions=[];asm=['.code'];decl=[];ctx=SimpleNamespace(needs_cf=True)
    rows=[];memrows=[];hwrows=[];flagrows=[]
    def emit(name,args,op):
        functions.append('static void '+name+'(void){\n'+'\n'.join(exchange.exchange(ctx,None,args,op))+'\n}')
    for width in (8,16,32):
        d={8:'dl',16:'dx',32:'edx'}[width];s={8:'cl',16:'cx',32:'ecx'}[width]
        row=[];mrow=[];hrow=[];frow=[]
        for op in ('xchg','xadd','cmpxchg'):
            name=f'{op}{width}';row.append(name);hrow.append('hw_'+name)
            emit(name,[Operand('reg',reg=d),Operand('reg',reg=s)],op)
            emit('mem_'+name,[Operand('mem',mem_base='ebx',mem_size=width//8),Operand('reg',reg=s)],op)
            mrow.append('mem_'+name)
            asm += [f'public hw_{name}',f'hw_{name} proc','mov r10,rcx','mov eax,edx','mov edx,r8d','mov ecx,r9d',f'{op} {d},{s}',
                    'pushfq','pop r11','mov dword ptr [r10],eax','mov dword ptr [r10+4],ecx','mov dword ptr [r10+8],edx','mov dword ptr [r10+12],r11d','ret',f'hw_{name} endp']
            decl.append(f'extern void hw_{name}(State*,uint32_t,uint32_t,uint32_t);')
            if op!='xchg':
                expressions=[f'((!!({exchange.condition(j,op,[Operand("reg",reg=d),Operand("reg",reg=s)])})) << {shift})' for j,shift in [('jb',0),('jp',2),('jz',6),('js',7),('jo',11)]]
                functions.append(f'static unsigned flags_{name}(void){{return '+ '|'.join(expressions)+';}')
                frow.append('flags_'+name)
        rows.append('{'+','.join(row)+'}');memrows.append('{'+','.join(mrow)+'}');hwrows.append('{'+','.join(hrow)+'}');flagrows.append('{'+','.join(frow)+'}')
    for name,regs,op in [('swap_bytes',['al','ah'],'xchg'),('add_self',['al','al'],'xadd'),('add_overlap',['ah','al'],'xadd'),('cas_self',['al','ah'],'cmpxchg'),('cas_overlap',['ah','al'],'cmpxchg')]:
        emit(name,[Operand('reg',reg=x) for x in regs],op)
    for op in ('xchg','xadd','cmpxchg'):
        base='eax' if op!='xadd' else 'ecx'
        emit('address_'+op,[Operand('mem',mem_base=base,mem_size=4),Operand('reg',reg=base)],op)
    emit('reversed_xchg',[Operand('reg',reg='eax'),Operand('mem',mem_base='eax',mem_size=4)],'xchg')
    asm.append('end');objects=[]
    if os.name=='nt':
        compiler,env=native_compiler(tmp_path);source=tmp_path/'reference.asm';source.write_text('\n'.join(asm),encoding='utf-8');obj=tmp_path/'reference.obj'
        run=subprocess.run([str(Path(compiler).with_name('ml64.exe')),'/nologo','/c',f'/Fo{obj}',str(source)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
        assert run.returncode==0,run.stdout+run.stderr
        objects=[str(obj)]
    macros=_production_macros({'LO8','HI8','LO16','SET_LO8','SET_HI8','SET_LO16','CMP_EQ'})
    code=atomic_source()+r"""
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct {uint32_t a,c,d,flags;} State;
static uint32_t eax,ecx,edx,ebx,_fa,_fb;static int32_t _fas,_fbs;static int _cf;
static union {uint32_t alignment;unsigned char bytes[256];} memory;
static unsigned maps,last_address;
static uintptr_t mapped(uint32_t a){if(a>252)abort();++maps;last_address=a;return (uintptr_t)(memory.bytes+a);}
#define XBOX_PTR(a) mapped((uint32_t)(a))
"""+macros+'\n'+'\n'.join(functions)+'\n'
    code+='typedef void (*Fn)(void); static Fn ops[3][3]={'+','.join(rows)+'};\n'
    code+='static Fn memops[3][3]={'+','.join(memrows)+'};\n'
    code+='typedef unsigned (*Flags)(void); static Flags flags[3][2]={'+','.join(flagrows)+'};\n'
    if os.name=='nt':code+='\n'.join(decl)+'\ntypedef void (*HW)(State*,uint32_t,uint32_t,uint32_t); static HW hardware[3][3]={'+','.join(hwrows)+'};\n'
    code+=r"""
static uint32_t seed=1;static uint32_t next(void){return seed=seed*1664525u+1013904223u;}
static unsigned reference_flags(unsigned w,uint32_t a,uint32_t b,unsigned subtract){
 uint64_t mask=w==32?0xffffffffu:((1u<<w)-1);uint32_t result=(uint32_t)(subtract?(uint64_t)a-b:(uint64_t)a+b)&(uint32_t)mask;
 int64_t sa=(a&(1u<<(w-1)))?(int64_t)a-(INT64_C(1)<<w):a;
 int64_t sb=(b&(1u<<(w-1)))?(int64_t)b-(INT64_C(1)<<w):b;
 int64_t signed_result=subtract?sa-sb:sa+sb;unsigned parity=1;
 for(unsigned k=0;k<8;++k)parity^=(result>>k)&1u;
 return (subtract?a<b:(uint64_t)a+b>mask)|(parity<<2)|((result==0)<<6)|(((result>>(w-1))&1)<<7)|
 ((signed_result<-(INT64_C(1)<<(w-1))||signed_result>=(INT64_C(1)<<(w-1)))<<11);
}
int main(void){
for(unsigned wi=0;wi<3;++wi){unsigned w=8u<<wi;uint32_t mask=w==32?0xffffffffu:(1u<<w)-1;
 for(unsigned n=0;n<65536;++n){uint32_t left=wi?next():n>>8,right=wi?next():n&255u;
  uint32_t a=(n%3)?left:next(),d=left,c=right;if(w<32){a=(a&mask)|0xa5430000u;d=(d&mask)|0xb3210000u;c=(c&mask)|0xc7650000u;}
  for(unsigned op=0;op<3;++op){eax=a;edx=d;ecx=c;_fa=17;_fb=18;_fas=19;_fbs=20;_cf=21;ops[wi][op]();
   uint32_t ea=a,ed=d,ec=c;
   if(op==0){ed=(d&~mask)|(c&mask);ec=(c&~mask)|(d&mask);}
   if(op==1){ed=(d&~mask)|(((d&mask)+(c&mask))&mask);ec=(c&~mask)|(d&mask);}
   if(op==2){if((a&mask)==(d&mask))ed=(d&~mask)|(c&mask);else ea=(a&~mask)|(d&mask);}
   if(eax!=ea||edx!=ed||ecx!=ec)return 1;
   if(op==0){if(_fa!=17||_fb!=18||_fas!=19||_fbs!=20||_cf!=21)return 2;}
   else if(flags[wi][op-1]()!=reference_flags(w,op==1?d&mask:a&mask,op==1?c&mask:d&mask,op==2))return 3;
"""
    if os.name=='nt':code+='State ref;hardware[wi][op](&ref,a,d,c);if(ref.a!=eax||ref.c!=ecx||ref.d!=edx||(op&&(ref.flags&0x8c5u)!=flags[wi][op-1]()))return 4;\n'
    code+=r"""
  }
  if(n<2000)for(unsigned op=0;op<3;++op){memset(memory.bytes,0xa5,256);memcpy(memory.bytes+128,&d,w/8);eax=a;ecx=c;ebx=128;maps=0;memops[wi][op]();uint32_t got=0;memcpy(&got,memory.bytes+128,w/8);
   uint32_t want=op==0?c&mask:op==1?((d&mask)+(c&mask))&mask:(a&mask)==(d&mask)?c&mask:d&mask;
   if(got!=want||maps!=1||last_address!=128||memory.bytes[127]!=0xa5||memory.bytes[128+w/8]!=0xa5)return 5;
   if(op<2&&ecx!=((c&~mask)|(d&mask)))return 6;
   if(op==2&&eax!=((a&~mask)|((a&mask)==(d&mask)?a&mask:d&mask)))return 7;
   if(op&&flags[wi][op-1]()!=reference_flags(w,op==1?d&mask:a&mask,op==1?c&mask:d&mask,op==2))return 8;
  }
 }
}
for(unsigned n=0;n<65536;++n){uint32_t lo=n&255u,hi=n>>8;eax=0xabcd0000u|n;swap_bytes();if(eax!=(0xabcd0000u|(lo<<8)|hi))return 9;
 eax=0xabcd0000u|n;add_self();if(eax!=(0xabcd0000u|(hi<<8)|((lo*2)&255u)))return 10;
 eax=0xabcd0000u|n;add_overlap();if(eax!=(0xabcd0000u|(((lo+hi)&255u)<<8)|hi))return 11;
 eax=0xabcd0000u|n;cas_self();if(eax!=(0xabcd0000u|(hi<<8)|hi))return 12;
 eax=0xabcd0000u|n;cas_overlap();if(eax!=(0xabcd0000u|(hi<<8)|hi))return 13;
}
Fn alias[]={address_xchg,address_xadd,address_cmpxchg,reversed_xchg};
for(unsigned op=0;op<4;++op)for(unsigned equal=0;equal<2;++equal){uint32_t old=equal?128:0xaabbccdd;memcpy(memory.bytes+128,&old,4);eax=128;ecx=128;maps=0;alias[op]();uint32_t value;memcpy(&value,memory.bytes+128,4);
 if(maps!=1||last_address!=128||value!=(op==1?old+128:op==2?old:128)||((op==1?ecx:eax)!=old))return 14;
}
puts("PASS exchange widths, aliases, mapped-address sampling, captured conditions and x86 references");return 0;}
"""
    source=tmp_path/'exchange.c';source.write_text(code,encoding='utf-8');exe=_compile(tmp_path,source,objects=objects)
    run=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr+f' exit={run.returncode}'


def test_project_module_is_independent(tmp_path):
    for name in ('translation_exchange.py','translation_integer.py','translation_operands.py'):
        shutil.copyfile(ROOT/'tools'/name,tmp_path/name)
    script="import sys;sys.path.insert(0,sys.argv[1]);import translation_exchange;assert not any(n.startswith('tools.recomp') for n in sys.modules)"
    result=subprocess.run([sys.executable,'-I','-c',script,str(tmp_path)],capture_output=True,text=True,timeout=20)
    assert result.returncode==0,result.stderr


def test_atomic_contention_and_return_values(tmp_path):
    code=atomic_source()+r"""
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#define THREAD_RESULT DWORD WINAPI
#define THREAD_RETURN return 0
#else
#include <pthread.h>
#define THREAD_RESULT void *
#define THREAD_RETURN return NULL
#endif
static uint32_t cells[3],locks[3],cas_cells[3];static uint64_t protected_counts[3];
static unsigned count,width_index;static uint64_t sums[4];
static uint32_t add(unsigned w,void *p,uint32_t v){return w==0?tsfp_atomic_add8(p,v):w==1?tsfp_atomic_add16(p,v):tsfp_atomic_add32(p,v);}
static uint32_t swap(unsigned w,void *p,uint32_t v){return w==0?tsfp_atomic_exchange8(p,v):w==1?tsfp_atomic_exchange16(p,v):tsfp_atomic_exchange32(p,v);}
static uint32_t cas(unsigned w,void *p,uint32_t c,uint32_t v){return w==0?tsfp_atomic_cas8(p,c,v):w==1?tsfp_atomic_cas16(p,c,v):tsfp_atomic_cas32(p,c,v);}
static THREAD_RESULT worker(void *arg){unsigned id=(unsigned)(uintptr_t)arg,w=width_index;uint32_t mask=w==0?255u:w==1?65535u:0xffffffffu;
 for(unsigned n=0;n<count;++n){sums[id]+=add(w,&cells[w],1);
  uint32_t old=add(w,&cas_cells[w],0);while(cas(w,&cas_cells[w],old,(old+1)&mask)!=old)old=add(w,&cas_cells[w],0);
  while(swap(w,&locks[w],1)){}
  ++protected_counts[w];
  swap(w,&locks[w],0);
 }
 THREAD_RETURN;
}
int main(void){
 uint32_t compatibility=3;
 if(RECOMP_ATOMIC_ADD32((uintptr_t)&compatibility,5)!=3||compatibility!=8)return 4;
 if(RECOMP_ATOMIC_CAS32((uintptr_t)&compatibility,8,13)!=8||compatibility!=13)return 5;
 if(RECOMP_ATOMIC_CAS32((uintptr_t)&compatibility,8,0)!=13||compatibility!=13)return 6;
 for(width_index=0;width_index<3;++width_index){unsigned w=width_index;count=20003;
 memset(sums,0,sizeof(sums));
#ifdef _WIN32
 HANDLE threads[4];for(unsigned i=0;i<4;++i){threads[i]=CreateThread(NULL,0,worker,(void*)(uintptr_t)i,0,NULL);if(!threads[i])return 1;}
 if(WaitForMultipleObjects(4,threads,TRUE,30000)!=WAIT_OBJECT_0)return 2;
 for(unsigned i=0;i<4;++i)CloseHandle(threads[i]);
#else
 pthread_t threads[4];for(unsigned i=0;i<4;++i)if(pthread_create(&threads[i],NULL,worker,(void*)(uintptr_t)i))return 1;
 for(unsigned i=0;i<4;++i)if(pthread_join(threads[i],NULL))return 2;
#endif
 uint32_t mask=w==0?255u:w==1?65535u:0xffffffffu;uint64_t expected=0,total=0;
 for(unsigned i=0;i<count*4;++i)expected+=(i&mask);
 for(unsigned i=0;i<4;++i)total+=sums[i];
 if(total!=expected||cells[w]!=((count*4)&mask)||cas_cells[w]!=((count*4)&mask)||protected_counts[w]!=count*4||locks[w])return 3;
}puts("PASS four-thread ADD ticket sums, CAS updates and EXCHANGE mutual exclusion at all widths");return 0;}
"""
    source=tmp_path/'contention.c';source.write_text(code,encoding='utf-8');exe=_compile(tmp_path,source,extra=() if os.name=='nt' else ('-pthread',))
    run=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=45)
    assert run.returncode==0,run.stdout+run.stderr+f' exit={run.returncode}'


def test_decoded_consumers_capture_flags_after_writes(tmp_path):
    runtime=ROOT/'external/ts3FPrecomp'
    script='''import json
from tools.recomp.translator import FunctionTranslator
from tools.recomp.lifter import Lifter
import translation_exchange as e
assert Lifter._lift_xchg is e.exchange and Lifter._lift_exchange is e.exchange
cases={
 'add_probe':'0fc1cabA00000000b9000000000f92c30f94c70f90c10f9cc2c3',
 'cas_probe':'0fb1cab800000000ba000000000f92c30f94c70f90c10f9cc2c3',
 'add_byte':'0fc0cabA00000000b9000000000f92c30f94c70f90c10f9cc2c3',
 'cas_word':'660fb1cab800000000ba000000000f92c30f94c70f90c10f9cc2c3',
 'memory_add':'f00fc10bc703000000000f92c20f94c6c3',
 'memory_cas':'f00fb10bc70300000000b8000000000f92c20f94c6c3',
 'exchange_address':'8700c3',
 'cmov_probe':'0fc1caba000000000f42de0f44fec3',
 'join_probe':'83fe0074050fc1caeb030fb1ca0f94c3c3',
 'branch_probe':'0fc1ca7206bb00000000c3bb01000000c3'}
out={}
for name,raw in cases.items():
 data=bytes.fromhex(raw);info={'name':name,'size':len(data)};t=FunctionTranslator(b'',{0x1000:info});t._read_func_bytes=lambda a,b:data
 out[name]=t.translate_function(0x1000,info)
print(json.dumps(out))
'''
    run=subprocess.run([sys.executable,'-c',script],cwd=runtime,env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    macros=_production_macros({'LO8','HI8','LO16','SET_LO8','SET_HI8','SET_LO16','CMP_EQ'})
    code=atomic_source()+r"""
#include <stdio.h>
#include <string.h>
static uint32_t eax,ebx,ecx,edx,esi,edi,esp,g_ebp,g_seh_ebp;
static uint32_t memory[64];
#define XBOX_PTR(a) ((unsigned char*)memory+(uint32_t)(a))
#define MEM32(a) (*(uint32_t*)XBOX_PTR(a))
#define RECOMP_FAST_ENTRY(site) 0
#define RECOMP_AFTER_STORE32(a,v) ((void)0)
"""+macros+'\n'+'\n'.join(json.loads(run.stdout).values())+r"""
static unsigned flags(unsigned bits,uint32_t a,uint32_t b,int sub){uint32_t mask=bits==32?0xffffffffu:(1u<<bits)-1; a&=mask;b&=mask;
 uint32_t r=(sub?a-b:a+b)&mask;int64_t sa=(a&(1u<<(bits-1)))?(int64_t)a-(INT64_C(1)<<bits):a,sb=(b&(1u<<(bits-1)))?(int64_t)b-(INT64_C(1)<<bits):b;
 int64_t wide=sub?sa-sb:sa+sb;unsigned of=wide<-(INT64_C(1)<<(bits-1))||wide>=(INT64_C(1)<<(bits-1));unsigned sf=(r>>(bits-1))&1;
 return (sub?a<b:(uint64_t)a+b>mask)|((r==0)<<1)|(of<<2)|((sf!=of)<<3);
}
static unsigned observed(void){return (ebx&255u)|(((ebx>>8)&255u)<<1)|((ecx&255u)<<2)|((edx&255u)<<3);}
int main(void){const uint32_t values[]={0,1,127,128,255,256,32767,32768,65535,0x7fffffffu,0x80000000u,0xffffffffu};
for(unsigned i=0;i<12;++i)for(unsigned j=0;j<12;++j){uint32_t a=values[i],b=values[j];
 eax=73;edx=a;ecx=b;ebx=0;add_probe();if(observed()!=flags(32,a,b,0))return 1;
 eax=a;edx=b;ecx=11;ebx=0;cas_probe();if(observed()!=flags(32,a,b,1))return 2;
 edx=a;ecx=b;ebx=0;add_byte();if(observed()!=flags(8,a,b,0))return 3;
 eax=a;edx=b;ecx=11;ebx=0;cas_word();if(observed()!=flags(16,a,b,1))return 4;
 ebx=128;memory[32]=a;ecx=b;edx=0;memory_add();if((edx&255u)!=(flags(32,a,b,0)&1)||((edx>>8)&255u)!=((flags(32,a,b,0)>>1)&1))return 5;
 ebx=128;memory[32]=b;eax=a;ecx=11;edx=0;memory_cas();if((edx&255u)!=(flags(32,a,b,1)&1)||((edx>>8)&255u)!=((flags(32,a,b,1)>>1)&1))return 6;
 edx=a;ecx=b;ebx=99;branch_probe();if(ebx!=(flags(32,a,b,0)&1))return 7;
 edx=a;ecx=b;ebx=11;edi=22;esi=73;cmov_probe();if(ebx!=((flags(32,a,b,0)&1)?73u:11u)||edi!=((flags(32,a,b,0)&2)?73u:22u))return 9;
 eax=a;edx=b;ecx=17;esi=0;ebx=0;join_probe();if((ebx&255u)!=(a==b))return 10;
 eax=17;edx=a;ecx=b;esi=1;ebx=0;join_probe();if((ebx&255u)!=((a+b)==0))return 11;
}
eax=128;memory[32]=0x12345678;exchange_address();if(eax!=0x12345678||memory[32]!=128)return 8;
puts("PASS decoded register/memory exchange, arithmetic branches and captured SET flags after operand overwrite");return 0;}
"""
    source=tmp_path/'decoded.c';source.write_text(code,encoding='utf-8');exe=_compile(tmp_path,source)
    run=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr+f' exit={run.returncode}'
