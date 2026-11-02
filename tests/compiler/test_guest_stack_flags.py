"""Protected-mode flag stack images, privilege masks and emitted execution."""
from pathlib import Path
import os,random,re,struct,subprocess
import pytest
from test_translation_product import _compile
from test_status_flags_translation import execute
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]
VALID=0x3f7fd7

def model(row):
    op,bits,cpl,value,known,direction,image=row
    if op==0:
        result=((value&~0x30400)|(0x400 if direction else 0)|2)&VALID
        return (result&0xffff if bits==16 else result if bits==32 else 0),value,known,direction
    if bits not in (16,32) or cpl>3 or value&0x20000:return 0,value,known,direction
    editable={0,2,4,6,7,8,9,10,11,12,13,14}
    if bits==32:editable|={18,21}
    if cpl:editable-={12,13}
    if cpl>((value>>12)&3):editable.discard(9)
    result=value
    for bit in editable:
        mask=1<<bit
        result=result|mask if image&mask else result&~mask
        known|=mask
    result=(result&VALID&~0x10000)|2
    known=(known|0x10002)&VALID
    return 1,result,known,int(bool(result&0x400))

def fixture(tmp_path,mutation=None):
    h=(ROOT/'src/runtime/guest_status_flags.h').read_text()
    if mutation:
        a,b=mutation;assert h.count(a)==1;h=h.replace(a,b)
    (tmp_path/'guest_status_flags.h').write_bytes(h.encode())
    p=tmp_path/'stack.c';p.write_bytes(r"""
#include "guest_status_flags.h"
#include <stdio.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
int main(void) {
 uint32_t q[7],r[4];
#ifdef _WIN32
 _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
 while(fread(q,sizeof q,1,stdin)==1) {
  tsfp_status_flags s={q[3],q[4]};int direction=(int)q[5];
  r[0]=q[0]?tsfp_status_stack_restore(&s,&direction,q[6],q[1],q[2]):tsfp_status_stack_image(&s,direction,q[1]);
  r[1]=s.value;r[2]=s.known;r[3]=(uint32_t)direction;
  if(fwrite(r,sizeof r,1,stdout)!=1)return 2;
 }
 return ferror(stdin)?3:0;
}
""".encode())
    return _compile(tmp_path,p,extra=['/W4'] if os.name=='nt' else ['-Wall','-Wextra'])

def requests():
    rng=random.Random(252)
    rows=[]
    for bits in (16,32):
        for cpl in range(4):
            for iopl in range(4):
                for image in (0,0xffffffff,0x8d5,0x10000,0x20000,0x200,0x400,0x200000):
                    for known in (2,VALID):
                        value=(rng.getrandbits(22)&VALID&~0x23000)|(iopl<<12)|2
                        rows.append((1,bits,cpl,value,known,rng.randrange(2),image))
    for _ in range(12000):
        rows.append((rng.randrange(2),rng.choice((16,32,8,64)),rng.randrange(5),rng.getrandbits(32),rng.getrandbits(32),rng.randrange(2),rng.getrandbits(32)))
    return rows

def agrees(exe,rows):
    out=subprocess.run([str(exe)],input=b''.join(struct.pack('<7I',*q) for q in rows),capture_output=True,timeout=20)
    return out.returncode==0 and out.stdout==b''.join(struct.pack('<4I',*model(q)) for q in rows)

def test_stack_images_restore_privilege_and_preservation(tmp_path):assert agrees(fixture(tmp_path),requests())

@pytest.mark.parametrize('mutation',[
 ('TSFP_FLAG_DF|TSFP_FLAG_RF|TSFP_FLAG_VM','TSFP_FLAG_DF|TSFP_FLAG_VM'),
 ('if(privilege)writable&=~TSFP_FLAG_IOPL;','if(0)writable&=~TSFP_FLAG_IOPL;'),
 ('if(bits==32)writable|=TSFP_FLAG_AC|TSFP_FLAG_ID;','writable|=TSFP_FLAG_AC|TSFP_FLAG_ID;'),
 ('privilege>((s->value&TSFP_FLAG_IOPL)>>12)','privilege>=((s->value&TSFP_FLAG_IOPL)>>12)'),
])
def test_compiled_incorrect_stack_helpers_rejected(tmp_path,mutation):assert not agrees(fixture(tmp_path,mutation),requests())

@pytest.mark.skipif(os.name!='nt',reason='actual x86 Windows hardware oracle')
def test_native_protected_ring3_flags_stack(tmp_path):
    cc,env=native_compiler(tmp_path);cc=Path(cc).parent.parent/'x86/cl.exe';assert cc.is_file()
    env=dict(env);env['PATH']=str(cc.parent)+os.pathsep+env['PATH'];env['LIB']=re.sub(r'(?i)([\\/])x64(?=;|$)',r'\1x86',env['LIB']);env['VSCMD_ARG_TGT_ARCH']='x86'
    src=tmp_path/'native.c';src.write_bytes((ROOT/'tests/compiler/flags_stack_x86.c').read_bytes());exe=tmp_path/'native.exe'
    r=subprocess.run([str(cc),'/nologo','/Od','/Oy-','/W4','/WX',str(src),'/Fe:'+str(exe)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60);assert r.returncode==0,r.stdout+r.stderr
    r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20);assert r.returncode==0,r.stdout+r.stderr
    rows=[]
    lines=r.stdout.splitlines();assert len(lines)==513
    for line in lines[1:]:
        width,seed,image,full,word=line.split();width=int(width);seed=int(seed,16);image=int(image,16);full=int(full,16);word=int(word,16)
        q=(1,width,3,seed,VALID,int(bool(seed&0x400)),image);expect=model(q)
        assert expect[0] and expect[1]==full,(q,expect,full)
        assert model((0,16,3,full,VALID,expect[3],0))[0]==word
        rows.append(q)
    assert agrees(fixture(tmp_path),rows)

@pytest.mark.parametrize('raw,initial,check',[
 ('539d9c58c3','g_ebx=0x200ed7;g_status_flags.value=0x202;g_status_flags.known=2;',
  'CHECK(g_eax==0x200ed7);CHECK(g_df==1);CHECK((g_status_flags.known&0x247fd5)==0x247fd5);'),
 ('6653669d669c6658c3','g_eax=0xabcd0000;g_ebx=0x8d5;g_status_flags.value=0x200202;g_status_flags.known=2;',
  'CHECK(g_eax==0xabcd08d7);CHECK(g_status_flags.value&0x200000);CHECK(g_df==0);'),
 ('9c58c3','g_status_flags.value=0xffffffff;g_status_flags.known=2;g_df=0;',
  'CHECK(g_eax==0x3c7bd7);CHECK(g_status_flags.value==0xffffffff);CHECK(g_status_flags.known==2);'),
 ('539d0f92c0c3','g_eax=0;g_ebx=1;g_status_flags.value=2;g_status_flags.known=2;',
  'CHECK(g_eax==1);CHECK(g_status_flags.known&1);'),
 ('9c31c09d0f90c0c3','g_status_flags.value=0x802;g_status_flags.known=0x8d7;g_df=1;',
  'CHECK(g_eax==1);CHECK(g_status_flags.value&0x800);CHECK(g_df==1);'),
 ('539d19c0c3','g_eax=42;g_ebx=1;g_status_flags.value=2;g_status_flags.known=2;',
  'CHECK(g_eax==0xffffffff);CHECK(g_status_flags.value&1);'),
])
def test_actual_translator_stack_and_flag_consumers(tmp_path,raw,initial,check):
    execute(tmp_path,raw,'g_esp=100;'+initial,check+'CHECK(g_esp==104);',support='void recomp_unimpl(const char *name,uint32_t pc) { (void)name;(void)pc;abort(); }\n')

@pytest.mark.parametrize('raw,size',[('9c58c3',4),('669c6658c3',2)])
def test_stack_image_exact_width_and_guarded_memory(tmp_path,raw,size):
    initial='memset(ram,0xa5,sizeof ram);g_eax=0xabcd0000;g_esp=100;g_status_flags.value=0x802;g_status_flags.known=0x8d7;g_df=1;'
    check=f"""CHECK(g_esp==104);CHECK(g_eax=={('0xc02' if size==4 else '0xabcd0c02')});
CHECK(g_status_flags.value==0x802);CHECK(g_status_flags.known==0x8d7);CHECK(g_df==1);
for(unsigned i=0;i<sizeof ram;i++) {{ unsigned v=0xa5;
 if(i>=100-{size} && i<100)v=(0xc02u>>(8*(i-(100-{size}))))&255;
 CHECK(ram[i]==v); }}"""
    execute(tmp_path,raw,initial,check)
