"""Canonical producers: products, bit strings/scans and real atomic exchanges."""
from pathlib import Path
from types import SimpleNamespace as S
import importlib.util,os,random,struct,subprocess
import pytest
from test_guest_status_flags import ROOT,STATUS,model as alu_model
from test_translation_product import _compile
from translation_decode import Operand
import translation_product as product
import translation_bitops as bitops
import translation_exchange as exchange
from native_compiler import native_compiler

CASES=[(op,w,'reg') for op in ('mul','imul') for w in (8,16,32)]
CASES += [(op,w,'reg') for op in ('imul2','imul3') for w in (16,32)]
CASES += [(op,w,form) for op in ('bt','bts','btr','btc') for w in (16,32) for form in ('reg','mem')]
CASES += [(op,w,'reg') for op in ('bsf','bsr') for w in (16,32)]
CASES += [(op,w,form) for op in ('xadd','cmpxchg') for w in (8,16,32) for form in ('reg','mem','lock','alias')]
CASES += [('xchg',w,form) for w in (8,16,32) for form in ('reg','mem','lock')]
CASES += [('xadd',8,'overlap')]
MULTIPLIER=0x9e3779b9


def operands(op,width,form):
    reg=lambda family:Operand('reg',reg={'a':{8:'al',16:'ax',32:'eax'},'c':{8:'cl',16:'cx',32:'ecx'},'d':{8:'dl',16:'dx',32:'edx'}}[family][width])
    memory=Operand('mem',mem_base='ebx',mem_size=width//8)
    if op in ('mul','imul'):return [reg('c')]
    if op=='imul2':return [reg('a'),reg('c')]
    if op=='imul3':return [reg('a'),reg('c'),Operand('imm',imm=-7)]
    if op in ('bsf','bsr'):return [reg('a'),reg('c')]
    if op.startswith('bt'):return [memory if form=='mem' else reg('a'),reg('c')]
    if form=='overlap':return [reg('a'),Operand('reg',reg='ah')]
    destination=memory if form in ('mem','lock') else reg('c') if form=='alias' and op=='xadd' else reg('a') if form=='alias' or op!='cmpxchg' else reg('d')
    return [destination,reg('c')]


def build(tmp_path,mutation=None):
    providers=dict(product=product,bitops=bitops,exchange=exchange)
    if mutation:
        group,old,new=mutation;original=Path(providers[group].__file__);text=original.read_text(encoding='utf-8');assert text.count(old)==1
        path=tmp_path/'mutated.py';path.write_bytes(text.replace(old,new).encode())
        spec=importlib.util.spec_from_file_location('mutated_producer',path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);providers[group]=module
    for name in ('recomp_types.h','guest_status_flags.h','guest_state.c'):(tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    functions=[];context=S(needs_cf=True,track_status_flags=True);insn=S(address=0x1000)
    for i,(op,width,form) in enumerate(CASES):
        args=operands(op,width,form)
        if op.startswith('imul'):lines=providers['product'].imul(context,insn,args)
        elif op=='mul':lines=providers['product'].muldiv(context,insn,args,op)
        elif op.startswith('bt'):lines=providers['bitops'].bit_test(context,insn,args,op)
        elif op in ('bsf','bsr'):lines=providers['bitops'].scan(context,insn,args,op)
        else:lines=providers['exchange'].exchange(context,insn,args,('lock ' if form=='lock' else '')+op)
        functions.append(f'''static void op{i}(void) {{
 uint32_t _fa=0,_fb=0;int32_t _fas=0,_fbs=0;int _cf=(int)RECOMP_STATUS_CARRY(0);
 {chr(10).join(lines)}
 (void)_fa;(void)_fb;(void)_fas;(void)_fbs;(void)_cf;
}}''')
    source=tmp_path/'integer.c';source.write_bytes(('''#define RECOMP_GENERATED_CODE 1
#include "recomp_types.h"
#include <stdio.h>
#include <assert.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
ptrdiff_t g_xbox_mem_offset;
#ifdef _MSC_VER
__declspec(align(16))
#else
_Alignas(16)
#endif
static unsigned char ram[4096];
'''+ '\n'.join(functions)+'''
int main(void) {
 uint32_t p[7],out[22];void(*ops[])(void)={'''+','.join('op'+str(i) for i in range(len(CASES)))+'''};
#ifdef _WIN32
 assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
#endif
 g_xbox_mem_offset=(ptrdiff_t)ram;
 while(fread(p,1,sizeof(p),stdin)==sizeof(p)) {
  assert(p[0]<sizeof(ops)/sizeof(ops[0]));g_eax=p[2];g_edx=p[3];g_ecx=p[4];g_ebx=1024;g_status_flags.value=p[5];g_status_flags.known=p[6];
  for(int i=-8;i<=8;++i){uint32_t v=p[3]^((uint32_t)i*UINT32_C(0x9e3779b9));memcpy(ram+1024+i*4,&v,4);}
  ops[p[0]]();out[0]=g_eax;out[1]=g_edx;out[2]=g_ecx;memcpy(out+3,ram+992,68);out[20]=g_status_flags.value;out[21]=g_status_flags.known;
  assert(fwrite(out,1,sizeof(out),stdout)==sizeof(out));
 }
 return 0;
}
''').encode())
    flags=['/DTSFP_RECORD_STATUS_FLAGS=1','/W4'] if os.name=='nt' else ['-DTSFP_RECORD_STATUS_FLAGS=1','-Wall','-Wextra']
    return _compile(tmp_path,source,extra=[tmp_path/'guest_state.c',*flags])


def model(p):
    index,width,a,d,c,value,known=p;op,w,form=CASES[index];assert width==w;mask=2**width-1
    memory=bytearray(struct.pack('<17I',*((d^((i*MULTIPLIER)&0xffffffff)) for i in range(-8,9))))
    def low(old,new):return (old&~mask)|(new&mask)
    def publish(defined,v,undefined=0):
        nonlocal value,known
        value=(value&~(defined|undefined))|(v&defined)|2;known=(known&~undefined)|defined|2
    signed=lambda v:(v&mask)-2**width if v&2**(width-1) else v&mask
    def read(offset):return int.from_bytes(memory[offset:offset+width//8],'little')
    def write(offset,v):memory[offset:offset+width//8]=(v&mask).to_bytes(width//8,'little')
    if op in ('mul','imul','imul2','imul3'):
        wide=(a&mask)*(c&mask) if op=='mul' else signed(c)*-7 if op=='imul3' else signed(a)*signed(c)
        overflow=wide>mask if op=='mul' else not -2**(width-1)<=wide<2**(width-1)
        if op in ('mul','imul'):
            if width==8:a=(a&0xffff0000)|(wide&0xffff)
            else:a=low(a,wide);d=low(d,(wide>>width))
        else:a=low(a,wide)
        publish(1|2048,(1|2048)*int(overflow),STATUS&~(1|2048))
    elif op.startswith('bt'):
        offset=32
        if form=='mem':offset+=(signed(c)//width)*(width//8)
        old=read(offset) if form=='mem' else a&mask;bit=(c&mask)%width
        cf=(old>>bit)&1;changed=old|2**bit if op=='bts' else old&~(2**bit) if op=='btr' else old^2**bit if op=='btc' else old
        if form=='mem':write(offset,changed)
        else:a=low(a,changed)
        publish(1,cf,STATUS&~(1|64))
    elif op in ('bsf','bsr'):
        source=c&mask
        if source:a=low(a,(source&-source).bit_length()-1 if op=='bsf' else source.bit_length()-1)
        publish(64,0 if source else 64,STATUS&~64)
    elif op=='xchg':
        old=read(32) if form in ('mem','lock') else a&mask
        if form in ('mem','lock'):write(32,c)
        else:a=low(a,c)
        c=low(c,old)
    else:
        left=a&mask if op=='cmpxchg' else c&mask if form=='alias' else a&mask
        old=read(32) if form in ('mem','lock') else a&mask if form=='alias' and op=='cmpxchg' else d&mask if op=='cmpxchg' else c&mask if form=='alias' else a&mask
        right=old if op=='cmpxchg' else (a>>8)&255 if form=='overlap' else c&mask
        if op=='xadd':left=old
        result=(left-right if op=='cmpxchg' else left+right)&mask
        _,value,known,_=alu_model((1 if op=='cmpxchg' else 0,width,left,right,0,value,known))
        if op=='cmpxchg':
            if left!=old:a=low(a,old)
            elif form in ('mem','lock'):write(32,c)
            elif form=='alias':a=low(a,c)
            else:d=low(d,c)
        elif form=='overlap':a=(a&0xffff0000)|((old&255)<<8)|result
        elif form=='alias':c=low(c,result)
        else:
            c=low(c,old)
            if form in ('mem','lock'):write(32,result)
            else:a=low(a,result)
    return (a,d,c,*struct.unpack('<17I',memory),value&0xffffffff,known&0xffffffff)


def vectors(exhaustive=False):
    rng=random.Random(250);edge=(0,1,2,0x7f,0x80,0xff,0x7fff,0x8000,0xffff,0x7fffffff,0x80000000,0xffffffff)
    for index,(op,width,form) in enumerate(CASES):
        for a in edge:
            for d in edge:
                for c in (0,1,0xffffffff):
                    if op.startswith('bt') and form=='mem':c=((c%129)-64)&0xffffffff
                    for flags in (2,STATUS|2):yield index,width,a,d,c,flags,STATUS|2
        if exhaustive and op in ('mul','imul') and width==8:
            for a in range(256):
                for c in range(256):yield index,width,a,0x87654321,c,STATUS|2,STATUS|2
    for _ in range(12000):
        index=rng.randrange(len(CASES));op,width,form=CASES[index];c=rng.getrandbits(32)
        if op.startswith('bt') and form=='mem':c=((c%129)-64)&0xffffffff
        yield index,width,rng.getrandbits(32),rng.getrandbits(32),c,rng.getrandbits(32)|2,rng.getrandbits(32)|2


def run(exe,data):return subprocess.run([str(exe)],input=b''.join(struct.pack('<7I',*p) for p in data),capture_output=True,timeout=60)


def test_defined_integer_status_and_full_memory_state(tmp_path):
    data=list(vectors(True));r=run(build(tmp_path),data);assert r.returncode==0,r.stderr.decode(errors='replace')
    assert len(r.stdout)==len(data)*88
    for i,p in enumerate(data):assert struct.unpack_from('<22I',r.stdout,i*88)==model(p),(i,p,CASES[p[0]])


@pytest.mark.parametrize('mutation',[
    ('product',"carry = f'(_product_wide > {mask})'","carry = f'(_product_wide >= {mask})'"),
    ('product',"flags='TSFP_FLAG_CF | TSFP_FLAG_OF'","flags='TSFP_FLAG_CF'"),
    ('bitops','TSFP_FLAG_CF,(_bitop_value >> _bitop_index) & 1u','TSFP_FLAG_CF,1u ^ ((_bitop_value >> _bitop_index) & 1u)'),
    ('bitops','_bitop_value ? 0 : TSFP_FLAG_ZF','_bitop_value ? TSFP_FLAG_ZF : 0'),
    ('exchange','int(name=="cmpxchg")','int(name!="cmpxchg")'),
])
def test_compiled_wrong_integer_producers_fail(tmp_path,mutation):
    data=list(vectors());r=run(build(tmp_path,mutation),data);assert r.returncode==0
    assert any(struct.unpack_from('<22I',r.stdout,i*88)!=model(p) for i,p in enumerate(data))


@pytest.mark.skipif(os.name!='nt',reason='Actual x86 integer instruction flags')
def test_native_integer_flags_and_memory(tmp_path):
    asm=['.code'];declarations=[]
    for i,(op,width,form) in enumerate(CASES):
        a={8:'al',16:'ax',32:'eax'}[width];c={8:'cl',16:'cx',32:'ecx'}[width];d={8:'dl',16:'dx',32:'edx'}[width];mem={8:'byte',16:'word',32:'dword'}[width]+' ptr [r8]'
        if op in ('mul','imul'):instruction=f'{op} {c}'
        elif op=='imul2':instruction=f'imul {a},{c}'
        elif op=='imul3':instruction=f'imul {a},{c},-7'
        elif op.startswith('bt'):instruction=f'{op} {mem if form=="mem" else a},{c}'
        elif op in ('bsf','bsr'):instruction=f'{op} {a},{c}'
        else:
            dest=mem if form in ('mem','lock') else c if form=='alias' and op=='xadd' else a if form=='alias' or op!='cmpxchg' else d
            instruction=('lock ' if form=='lock' else '')+f'{op} {dest},{"ah" if form=="overlap" else c}'
        asm += [f'public hw{i}',f'hw{i} proc','mov r9,rcx','mov r10,rdx','pushfq','pop rax','and rax,-2262','mov r11d,[r10+20]','and r11d,2261','or rax,r11','push rax','popfq','mov eax,[r10+8]','mov edx,[r10+12]','mov ecx,[r10+16]',instruction,'pushfq','pop r11','mov [r9],eax','mov [r9+4],edx','mov [r9+8],ecx','mov [r9+12],r11d','ret',f'hw{i} endp']
        declarations.append(f'extern void hw{i}(uint32_t*,const uint32_t*,uint32_t*);')
    asm+=['end'];compiler,env=native_compiler(tmp_path);source=tmp_path/'hardware.asm';obj=tmp_path/'hardware.obj';source.write_bytes(('\n'.join(asm)+'\n').encode())
    r=subprocess.run([str(Path(compiler).with_name('ml64.exe')),'/nologo','/c',f'/Fo{obj}',str(source)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60);assert r.returncode==0,r.stdout+r.stderr
    source=tmp_path/'oracle.c';source.write_bytes(('''#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
'''+ '\n'.join(declarations)+'''
int main(void) {uint32_t p[7],memory[17],out[21],cpu[4];void(*ops[])(uint32_t*,const uint32_t*,uint32_t*)={'''+','.join('hw'+str(i) for i in range(len(CASES)))+'''};
assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
while(fread(p,1,sizeof(p),stdin)==sizeof(p)) {for(int i=-8;i<=8;++i)memory[i+8]=p[3]^((uint32_t)i*UINT32_C(0x9e3779b9));ops[p[0]](cpu,p,memory+8);memcpy(out,cpu,12);memcpy(out+3,memory,68);out[20]=cpu[3];assert(fwrite(out,1,sizeof(out),stdout)==sizeof(out));}return 0;}
''').encode());exe=_compile(tmp_path,source,objects=[str(obj)])
    data=list(vectors(True));hw=run(exe,data);actual=run(build(tmp_path),data);assert hw.returncode==actual.returncode==0
    assert len(hw.stdout)==len(data)*84 and len(actual.stdout)==len(data)*88
    for i,p in enumerate(data):
        have=struct.unpack_from('<22I',actual.stdout,i*88);want=struct.unpack_from('<21I',hw.stdout,i*84);op,width,_=CASES[p[0]]
        first=1 if op in ('bsf','bsr') and not p[4]&(2**width-1) else 0
        assert have[first:20]==want[first:20],(i,p,have,want)
        assert have[20]&have[21]&STATUS==want[20]&have[21]&STATUS,(i,p,have[20:],want[20])
