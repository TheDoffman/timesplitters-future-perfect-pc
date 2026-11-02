"""Production string status, repeat/zero-count and DF/address/segment contracts."""
from pathlib import Path
import importlib.util,os,random,struct,subprocess
import pytest
from test_guest_status_flags import ROOT,STATUS,model as alu_model
from test_translation_product import _compile
from test_translation_function import translator
from test_status_flags_translation import execute
from native_compiler import native_compiler

CASES=[(stem,width,prefix,narrow,fs) for stem in ('movs','stos','lods','cmps','scas') for width in (8,16,32)
       for prefix in (('','repe','repne') if stem in ('cmps','scas') else ('','rep')) for narrow in (0,1) for fs in (0,1)]
SIZE=0x10080
BASE=bytes((i*13+(i>>8)*7)&255 for i in range(SIZE))
WINDOWS=((0,64),(2016,2080),(65486,65550))


def raw(stem,width,prefix,narrow,fs):
    op={'movs':('a4','a5'),'stos':('aa','ab'),'lods':('ac','ad'),'cmps':('a6','a7'),'scas':('ae','af')}[stem][width!=8]
    return ('64' if fs else '')+('f3' if prefix in ('rep','repe') else 'f2' if prefix else '')+('66' if width==16 else '')+('67' if narrow else '')+op+'c3'


def setup_c():
    table=lambda position:','.join(str(v[position]) for v in CASES)
    stems=','.join(str(('movs','stos','lods','cmps','scas').index(v[0])) for v in CASES)
    return '''
#define RAM_SIZE 0x10080
static unsigned char ram[RAM_SIZE],base[RAM_SIZE],before[RAM_SIZE];
static const unsigned widths[]={'''+table(1)+'''},narrow[]={'''+table(3)+'''},segfs[]={'''+table(4)+'''},stems[]={'''+stems+'''};
static uint32_t initial_si(unsigned i){return narrow[i]?0xa555fffeu:1024u;}
static uint32_t initial_di(unsigned i){return narrow[i]?0xbbbbffeeu:2048u;}
static uint32_t address(uint32_t p,unsigned n,unsigned fs){return (n?p&0xffffu:p)+(fs?64u:0u);}
static void put(uint32_t a,uint32_t v,unsigned bytes){assert(a<=RAM_SIZE-bytes);memcpy(ram+a,&v,bytes);}
static void prepare(const uint32_t *p) {
 unsigned i=p[0],bytes=widths[i]/8;uint32_t mask=widths[i]==32?UINT32_MAX:(1u<<widths[i])-1u;
 uint32_t step=p[3]?0u-bytes:bytes;memcpy(ram,base,RAM_SIZE);
 if(stems[i]>=3)for(unsigned k=0;k<9;++k) {
  uint32_t left=stems[i]==3?(p[1]+k*13u)&mask:p[1]&mask;
  uint32_t right=k==p[4]?left:(left+13u)&mask;
  if(stems[i]==3)put(address(initial_si(i)+step*k,narrow[i],segfs[i]),left,bytes);
  put(address(initial_di(i)+step*k,narrow[i],0),right,bytes);
 }
 memcpy(before,ram,RAM_SIZE);
}
static void memory_result(uint32_t *out,unsigned i) {
 memcpy(out,ram,64);memcpy(out+16,ram+2016,64);memcpy(out+32,ram+65486,64);
 out[48]=stems[i]>=3?!memcmp(ram,before,RAM_SIZE):
  (!memcmp(ram+64,before+64,2016-64)&&!memcmp(ram+2080,before+2080,65486-2080)&&!memcmp(ram+65550,before+65550,RAM_SIZE-65550));
}
'''


def build(tmp_path,mutation=None):
    from translation_context import Lifter
    if mutation:
        import translation_strings as original
        old,new=mutation;text=Path(original.__file__).read_text(encoding='utf-8');assert text.count(old)==1
        path=tmp_path/'mutated_strings.py';path.write_bytes(text.replace(old,new).encode());spec=importlib.util.spec_from_file_location('mutated_strings',path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        saved=(Lifter._lift_string_op,Lifter._lift_rep_string);Lifter._lift_string_op=Lifter._lift_rep_string=module.emit
    try:
        functions=[]
        for i,case in enumerate(CASES):
            t,info=translator(raw(*case));info['name']='op'+str(i);functions.append(t.translate_function(0x1000,info))
    finally:
        if mutation:Lifter._lift_string_op,Lifter._lift_rep_string=saved
    for name in ('recomp_types.h','guest_status_flags.h','guest_state.c'):(tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    source=tmp_path/'strings.c';source.write_bytes(('''#define RECOMP_GENERATED_CODE 1
#define RECOMP_FAST_ENTRY(site) 0
#include "recomp_types.h"
#include <stdio.h>
#include <assert.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
ptrdiff_t g_xbox_mem_offset;
'''+setup_c()+'\n'.join(functions)+'''
int main(void) {
 uint32_t p[7],out[55];void(*ops[])(void)={'''+','.join('op'+str(i) for i in range(len(CASES)))+'''};
#ifdef _WIN32
 assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
#endif
 for(unsigned i=0;i<RAM_SIZE;++i)base[i]=(unsigned char)(i*13+(i>>8)*7);
 g_xbox_mem_offset=(ptrdiff_t)ram;
 while(fread(p,1,sizeof(p),stdin)==sizeof(p)) {
  assert(p[0]<sizeof(ops)/sizeof(ops[0])&&p[2]<=8&&p[3]<=1);prepare(p);
  unsigned i=p[0];g_eax=p[1];g_esi=initial_si(i);g_edi=initial_di(i);g_ecx=(narrow[i]?0x11220000u:0u)|p[2];g_df=(int)p[3];g_fs_base=64;
  g_esp=100;g_status_flags.value=p[5];g_status_flags.known=p[6];ops[i]();assert(g_esp==104);
  out[0]=g_eax;out[1]=g_esi;out[2]=g_edi;out[3]=g_ecx;out[4]=g_status_flags.value;out[5]=g_status_flags.known;memory_result(out+6,i);
  assert(fwrite(out,1,sizeof(out),stdout)==sizeof(out));
 }
 return 0;
}
''').encode());flag='/DTSFP_RECORD_STATUS_FLAGS=1' if os.name=='nt' else '-DTSFP_RECORD_STATUS_FLAGS=1'
    return _compile(tmp_path,source,extra=[tmp_path/'guest_state.c',flag])


def model(p):
    index,eax,count,df,stop,value,known=p;stem,width,prefix,narrow,fs=CASES[index];size=width//8;mask=2**width-1;step=-size if df else size
    si=0xa555fffe if narrow else 1024;di=0xbbbbffee if narrow else 2048;cx=(0x11220000 if narrow else 0)|count;ram=bytearray(BASE)
    address=lambda ptr,segment=0:((ptr&65535) if narrow else (ptr&0xffffffff))+(64 if segment else 0)
    def put(a,v):ram[a:a+size]=(v&mask).to_bytes(size,'little')
    def get(a):return int.from_bytes(ram[a:a+size],'little')
    def advance(v):return ((v&0xffff0000)|((v+step)&65535)) if narrow else (v+step)&0xffffffff
    if stem in ('cmps','scas'):
        for k in range(9):
            left=(eax+k*13)&mask if stem=='cmps' else eax&mask;right=left if k==stop else (left+13)&mask
            if stem=='cmps':put(address(si+step*k,fs),left)
            put(address(di+step*k),right)
    remaining=count if prefix else 1
    while remaining:
        left=get(address(si,fs)) if stem in ('movs','lods','cmps') else eax&mask
        if stem in ('movs','stos'):put(address(di),left)
        elif stem=='lods':eax=(eax&~mask)|left
        else:
            right=get(address(di));_,value,known,_=alu_model((1,width,left,right,0,value,known));equal=left==right
        if stem in ('movs','lods','cmps'):si=advance(si)
        if stem!='lods':di=advance(di)
        remaining-=1
        if prefix:
            cx=(cx&0xffff0000)|((cx-1)&65535) if narrow else (cx-1)&0xffffffff
            if stem in ('cmps','scas') and (equal if prefix=='repne' else not equal):break
    memory=b''.join(ram[a:b] for a,b in WINDOWS)
    return (eax,si,di,cx,value&0xffffffff,known&0xffffffff,*struct.unpack('<48I',memory),1)


def vectors(random_count=1000):
    edge=(0,1,0x7f,0x80,0xff,0x8000,0x80000000,0xffffffff)
    for index in range(len(CASES)):
        for a in edge:
            for count in (0,1,2,5,8):
                for df in (0,1):
                    for stop in (0,2,9):
                        for flags in (2,STATUS|2):yield index,a,count,df,stop,flags,STATUS|2
    rng=random.Random(252)
    for _ in range(random_count):yield rng.randrange(len(CASES)),rng.getrandbits(32),rng.randrange(9),rng.randrange(2),rng.randrange(10),rng.getrandbits(32)|2,rng.getrandbits(32)|2


def run(exe,data):return subprocess.run([str(exe)],input=b''.join(struct.pack('<7I',*p) for p in data),capture_output=True,timeout=90)


def test_production_strings_status_memory_and_zero_count(tmp_path):
    data=list(vectors());r=run(build(tmp_path),data);assert r.returncode==0,r.stderr.decode(errors='replace');assert len(r.stdout)==220*len(data)
    for i,p in enumerate(data):assert struct.unpack_from('<55I',r.stdout,i*220)==model(p),(i,p,CASES[p[0]])


@pytest.mark.parametrize('mutation',[
    ('_str_value - _str_other,1,TSFP_STATUS_FLAGS','_str_value - _str_other,0,TSFP_STATUS_FLAGS'),
    ("if getattr(context,'track_status_flags',False):",'if False:'),
    ('_str_value,_str_other,0,_str_value - _str_other','_str_other,_str_value,0,_str_value - _str_other'),
])
def test_compiled_wrong_string_status_fails(tmp_path,mutation):
    data=list(vectors(0));r=run(build(tmp_path,mutation),data);assert r.returncode==0
    assert any(struct.unpack_from('<55I',r.stdout,i*220)!=model(p) for i,p in enumerate(data))


@pytest.mark.parametrize('opcode',['f3a6','f2ae','67f3a7'])
def test_zero_repeat_does_not_read_invalid_memory(tmp_path,opcode):
    execute(tmp_path,opcode+'c3','g_esp=100;g_esi=0xf000ffffu;g_edi=0xe000ffffu;g_ecx='+('0x12340000u' if opcode.startswith('67') else '0')+';g_status_flags.value=0x8d7;g_status_flags.known=0x8d7;',
            'CHECK(g_status_flags.value==0x8d7);CHECK(g_status_flags.known==0x8d7);CHECK(g_esi==0xf000ffffu);CHECK(g_edi==0xe000ffffu);')


@pytest.mark.parametrize('raw,seed',[
    ('f3a6','g_ecx=1;g_esi=256;g_edi=512;MEM8(256)=0x80;MEM8(512)=1;g_status_flags.value=2;g_status_flags.known=2;'),
    ('f3a4','g_ecx=2;g_esi=256;g_edi=512;MEM16(256)=0x807f;g_status_flags.value=0x802;g_status_flags.known=0x802;'),
])
def test_string_status_reaches_overflow_branch(tmp_path,raw,seed):
    # The repeat comparison has no legacy OF descriptor. The branch must consume
    # the complete comparison status, or preserved status after a repeat move.
    execute(tmp_path,raw+'7006b801000000c3b802000000c3','g_esp=100;'+seed,'CHECK(g_eax==2);CHECK(g_status_flags.known&TSFP_FLAG_OF);')


@pytest.mark.skipif(os.name!='nt',reason='Actual x86 string flags and DF movement')
def test_native_string_flags(tmp_path):
    selected=[i for i,c in enumerate(CASES) if not c[3] and not c[4]];asm=['.code'];decl=[]
    for i in selected:
        stem,width,prefix,_,_=CASES[i];name=stem+{8:'b',16:'w',32:'d'}[width];instruction=(prefix+' ' if prefix else '')+name
        asm += [f'public hw{i}',f'hw{i} proc','push rsi','push rdi','mov r9,rcx','mov r10,rdx','pushfq','pop rax','and rax,-3286','mov r11d,[r10+20]','and r11d,2261','or rax,r11','mov r11d,[r10+12]','shl r11d,10','or rax,r11','push rax','popfq','mov eax,[r10+4]','mov ecx,[r10+8]','lea rsi,[r8+1024]','lea rdi,[r8+2048]',instruction,'pushfq','pop r11','cld','sub rsi,r8','sub rdi,r8','mov [r9],eax','mov [r9+4],esi','mov [r9+8],edi','mov [r9+12],ecx','mov [r9+16],r11d','pop rdi','pop rsi','ret',f'hw{i} endp']
        decl.append(f'extern void hw{i}(uint32_t*,const uint32_t*,unsigned char*);')
    asm+=['end'];compiler,env=native_compiler(tmp_path);source=tmp_path/'hardware.asm';obj=tmp_path/'hardware.obj';source.write_bytes(('\n'.join(asm)+'\n').encode());r=subprocess.run([str(Path(compiler).with_name('ml64.exe')),'/nologo','/c',f'/Fo{obj}',str(source)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60);assert r.returncode==0,r.stdout+r.stderr
    table=','.join('hw'+str(i) if i in selected else '0' for i in range(len(CASES)))
    source=tmp_path/'oracle.c';source.write_bytes(('''#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <io.h>
#include <fcntl.h>
'''+setup_c()+'\n'.join(decl)+'''
int main(void) {uint32_t p[7],out[54];void(*ops[])(uint32_t*,const uint32_t*,unsigned char*)={'''+table+'''};
assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
for(unsigned i=0;i<RAM_SIZE;++i)base[i]=(unsigned char)(i*13+(i>>8)*7);
while(fread(p,1,sizeof(p),stdin)==sizeof(p)) {assert(ops[p[0]]);prepare(p);ops[p[0]](out,p,ram);memory_result(out+5,p[0]);assert(fwrite(out,1,sizeof(out),stdout)==sizeof(out));}return 0;}
''').encode());exe=_compile(tmp_path,source,objects=[str(obj)])
    data=[p for p in vectors() if p[0] in selected];hw=run(exe,data);actual=run(build(tmp_path),data);assert hw.returncode==actual.returncode==0
    assert len(hw.stdout)==216*len(data) and len(actual.stdout)==220*len(data)
    for i,p in enumerate(data):
        have=struct.unpack_from('<55I',actual.stdout,i*220);want=struct.unpack_from('<54I',hw.stdout,i*216)
        assert have[:4]==want[:4] and have[6:]==want[5:],(i,p,have[:6],want[:5])
        assert have[4]&have[5]&STATUS==want[4]&have[5]&STATUS,(i,p,have[:6],want[:5])
