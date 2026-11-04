"""Execute original shift/rotate emitters with real storage, a bit-loop model and x86."""
from pathlib import Path
from types import SimpleNamespace
import os,random,struct,subprocess
import pytest
from test_guest_status_flags import ROOT,STATUS
from test_translation_product import _compile
import translation_bits as own
from translation_decode import Operand
from native_compiler import native_compiler

OPS=('shl','shr','sar','shld','shrd','rol','ror','rcl','rcr')
COMBOS=[(op,width) for op in OPS for width in (8,16,32) if width!=8 or op not in ('shld','shrd')]


def build(tmp_path,mutation=None):
    for name in ('recomp_types.h','guest_status_flags.h','guest_state.c'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_status_flags.h';s=p.read_text(encoding='utf-8');assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    functions=[]
    for i,(op,width) in enumerate(COMBOS):
        args=[Operand('reg',reg={8:'al',16:'ax',32:'eax'}[width])]
        if op in ('shld','shrd'):args.append(Operand('reg',reg={16:'dx',32:'edx'}[width]))
        args.append(Operand('reg',reg='cl'));ctx=SimpleNamespace(needs_cf=True,track_status_flags=True);insn=SimpleNamespace(address=0x1000)
        if op in ('shl','shr'):body=own.shift(ctx,insn,args,'<<' if op=='shl' else '>>')
        elif op in ('shld','shrd'):body=own.double_shift(ctx,insn,args,op)
        elif op in ('rol','ror'):body=own.rotate(ctx,insn,args,op)
        elif op in ('rcl','rcr'):body=own.rotate_carry(ctx,insn,args,op)
        else:body=own.sar(ctx,insn,args)
        functions.append(f'''static uint32_t op{i}(void) {{
uint32_t _fa=0;int32_t _fas=0;int _cf=(int)RECOMP_STATUS_CARRY(0);
(void)_fa;(void)_fas;
{chr(10).join(body)}
return (uint32_t)_cf;
}}''')
    source=tmp_path/'shift.c'
    source.write_bytes(('''#define RECOMP_GENERATED_CODE 1
#include "recomp_types.h"
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
ptrdiff_t g_xbox_mem_offset;
'''+ '\n'.join(functions)+'''
int main(void) {
 uint32_t p[7],out[4];uint32_t(*ops[])(void)={'''+','.join('op'+str(i) for i in range(len(COMBOS)))+'''};
#ifdef _WIN32
 assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
#endif
 while(fread(p,1,sizeof(p),stdin)==sizeof(p)) {
  assert(p[0]<sizeof(ops)/sizeof(ops[0]));g_eax=p[2];g_edx=p[3];g_ecx=p[4];g_status_flags.value=p[5];g_status_flags.known=p[6];
  out[3]=ops[p[0]]();out[0]=g_eax;out[1]=g_status_flags.value;out[2]=g_status_flags.known;
  assert(fwrite(out,1,sizeof(out),stdout)==sizeof(out));
 }
 return 0;
}
''').encode())
    flag='/DTSFP_RECORD_STATUS_FLAGS=1' if os.name=='nt' else '-DTSFP_RECORD_STATUS_FLAGS=1'
    return _compile(tmp_path,source,extra=[tmp_path/'guest_state.c',flag])


def model(p):
    index,width,a,b,count,value,known=p;op,assert_width=COMBOS[index];assert width==assert_width
    mask=2**width-1;sign=2**(width-1);r=a&mask;source=b&mask;k=count&31
    cf=(value&1) if known&1 else 0;old_cf_known=known&1
    def publish(defined,v,undefined=0):
        nonlocal value,known
        value=(value&~(defined|undefined))|(v&defined)|2;known=(known&~undefined)|defined|2
    if not k:return a,value,known,cf
    if op in ('shld','shrd') and k>width:return a,value|2,(known&~STATUS)|2,cf
    n=k%(width+1) if op in ('rcl','rcr') else k
    for _ in range(n):
        if op=='shl':cf=r//sign;r=r*2&mask
        elif op=='shr':cf=r%2;r//=2
        elif op=='sar':cf=r%2;r=r//2|(r&sign)
        elif op=='rol':cf=r//sign;r=(r*2&mask)|cf
        elif op=='ror':cf=r%2;r=r//2|cf*sign
        elif op=='rcl':old=cf;cf=r//sign;r=(r*2&mask)|old
        elif op=='rcr':old=cf;cf=r%2;r=r//2|old*sign
        elif op=='shld':cf=r//sign;r=(r*2&mask)|(source//sign);source=source*2&mask
        else:cf=r%2;r=r//2|(source%2)*sign;source//=2
    if op in ('rol','ror','rcl','rcr'):
        if not n:publish(0,0,2048)
        else:
            v=cf;defined=1;undefined=2048
            if k==1 and (op!='rcr' or old_cf_known):
                overflow=bool(r&sign)!=(bool(cf) if op in ('rol','rcl') else bool(r&(sign//2)))
                v|=int(overflow)*2048;defined|=2048;undefined=0
            publish(defined,v,undefined)
    else:
        defined=4|64|128;undefined=1|16|2048
        v=int((r&255).bit_count()%2==0)*4|int(r==0)*64|int(bool(r&sign))*128
        if op not in ('shl','shr') or k<width:defined|=1;undefined&=~1;v|=cf
        if k==1:
            overflow=False if op=='sar' else bool(a&sign) if op=='shr' else bool((a^r)&sign)
            defined|=2048;undefined&=~2048;v|=int(overflow)*2048
        publish(defined,v,undefined)
    return (a&~mask)|r,value&0xffffffff,known&0xffffffff,cf


def vectors(exhaustive=False):
    rng=random.Random(249);edge=(0,1,2,3,0x7f,0x80,0xff,0x8000,0x7fff,0xffffffff,0x80000000,0x7fffffff)
    for i,(op,width) in enumerate(COMBOS):
        for count in range(256):
            for a in edge:
                for carry in (0,1):yield i,width,a,a^0xdeadbeef,count,STATUS|2 if carry else 2,STATUS|2
        if exhaustive and width==8:
            for a in range(256):
                for count in range(32):
                    for carry in (0,1):yield i,width,a,0,count,STATUS|2 if carry else 2,STATUS|2
    for i in range(12000):
        index=rng.randrange(len(COMBOS));yield index,COMBOS[index][1],rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32)|2,rng.getrandbits(32)|2


def run(exe,data):
    return subprocess.run([str(exe)],input=b''.join(struct.pack('<7I',*p) for p in data),capture_output=True,timeout=60)


def test_all_counts_widths_and_defined_flag_knowledge(tmp_path):
    data=list(vectors(True));actual=run(build(tmp_path),data);assert actual.returncode==0
    expected=b''.join(struct.pack('<4I',*model(p)) for p in data)
    assert actual.stdout==expected


@pytest.mark.parametrize('mutation',[
    ('|| count<bits)', '|| count<=bits)'),
    ('carry=(input>>(bits-count))&1u;', 'carry=1u ^ ((input>>(bits-count))&1u);'),
    ('if(count==1) {','if(count!=1) {'),
    ('undefined=TSFP_FLAG_CF|TSFP_FLAG_OF|TSFP_FLAG_AF','undefined=TSFP_FLAG_CF|TSFP_FLAG_OF'),
    ('if(count==1 &&','if(distance==1 &&'),
    ('operation!=TSFP_ROTATE_CARRY_RIGHT || prior_carry_known','operation!=TSFP_ROTATE_CARRY_RIGHT || 1'),
])
def test_compiled_bad_shift_status_producers_are_rejected(tmp_path,mutation):
    data=list(vectors());actual=run(build(tmp_path,mutation),data);assert actual.returncode==0
    assert actual.stdout!=b''.join(struct.pack('<4I',*model(p)) for p in data)


@pytest.mark.skipif(os.name!='nt',reason='Actual x86 EFLAGS shift/rotate oracle')
def test_native_shift_rotate_flags(tmp_path):
    asm=['.code'];declarations=[]
    for i,(op,width) in enumerate(COMBOS):
        target={8:'al',16:'ax',32:'eax'}[width];source={16:'dx',32:'edx'}.get(width)
        operands=f'{target}, {source}, cl' if op in ('shld','shrd') else f'{target}, cl'
        asm += [f'public hw{i}',f'hw{i} proc','pushfq','pop r10','and r10,-2262','and r9d,2261','or r10,r9','push r10','popfq',
                'mov eax,ecx','mov ecx,r8d',f'{op} {operands}','pushfq','pop r11','shl r11,32','or rax,r11','ret',f'hw{i} endp']
        declarations.append(f'extern uint64_t hw{i}(uint32_t,uint32_t,uint32_t,uint32_t);')
    asm+=['end'];compiler,env=native_compiler(tmp_path);source=tmp_path/'hardware.asm';obj=tmp_path/'hardware.obj';source.write_bytes(('\n'.join(asm)+'\n').encode())
    result=subprocess.run([str(Path(compiler).with_name('ml64.exe')),'/nologo','/c',f'/Fo{obj}',str(source)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60);assert result.returncode==0,result.stdout+result.stderr
    source=tmp_path/'oracle.c';source.write_bytes(('''#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <io.h>
#include <fcntl.h>
'''+ '\n'.join(declarations)+'''
int main(void) {uint32_t p[7];uint64_t out;uint64_t(*ops[])(uint32_t,uint32_t,uint32_t,uint32_t)={'''+','.join('hw'+str(i) for i in range(len(COMBOS)))+'''};
assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
while(fread(p,1,sizeof(p),stdin)==sizeof(p)) {uint32_t flags=p[5];if(!(p[6]&1))flags&=~1u;out=ops[p[0]](p[2],p[3],p[4],flags);assert(fwrite(&out,1,sizeof(out),stdout)==sizeof(out));}return 0;}
''').encode())
    exe=_compile(tmp_path,source,objects=[str(obj)]);data=[p for p in vectors(True) if COMBOS[p[0]][0] not in ('shld','shrd') or (p[4]&31)<=p[1]]
    native=run(exe,data);actual=run(build(tmp_path),data);assert native.returncode==actual.returncode==0
    assert len(native.stdout)==8*len(data) and len(actual.stdout)==16*len(data)
    for i,p in enumerate(data):
        result,value,known,_=struct.unpack_from('<4I',actual.stdout,i*16);want,flags=struct.unpack_from('<2I',native.stdout,i*8)
        assert result==want,(i,p,result,want)
        assert value&known&STATUS==flags&known&STATUS,(i,p,value,known,flags)
