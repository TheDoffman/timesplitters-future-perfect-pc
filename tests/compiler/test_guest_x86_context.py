"""Original full i386 context-record codec, group selection and host SDK oracle."""
from pathlib import Path
import os,random,re,struct,subprocess
import pytest
from test_translation_product import _compile
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]
FIELDS='eax ebx ecx edx esi edi esp ebp eip eflags cs ss ds es fs gs'.split()+['dr'+str(n) for n in (0,1,2,3,6,7)]
GROUPS={1:dict(ebp=180,eip=184,cs=188,eflags=192,esp=196,ss=200),2:dict(edi=156,esi=160,ebx=164,edx=168,ecx=172,eax=176),4:dict(gs=140,fs=144,es=148,ds=152),16:dict(zip(FIELDS[16:],range(4,28,4)))}

def model(row):
    op,flags,size,state,record=row;state=bytearray(state);record=bytearray(record)
    f=struct.unpack_from('<I',record)[0] if op else flags
    if size<716 or f>>16!=1 or f&0xffc0:return 0,bytes(state),bytes(record)
    for group,fields in GROUPS.items():
        if f&group:
            for name,target in fields.items():
                origin=FIELDS.index(name)*4
                if op:state[origin:origin+4]=record[target:target+4]
                else:record[target:target+4]=state[origin:origin+4]
    for group,a,b,size in ((8,88,28,112),(32,200,204,512)):
        if f&group:
            if op:state[a:a+size]=record[b:b+size]
            else:record[b:b+size]=state[a:a+size]
    if not op:struct.pack_into('<I',record,0,f)
    return (f if op else 1),bytes(state),bytes(record)

def requests():
    rng=random.Random(253);rows=[]
    flags=[0x10000|n for n in range(64)]+[0,63,0x20000,0x10040,0xffffffff]+[0x10000|(1<<n) for n in range(6,32)]
    for op in (0,1):
        for f in flags:
            for size in (0,715,716):
                for _ in range(3):
                    s=rng.randbytes(712);c=bytearray(rng.randbytes(716));struct.pack_into('<I',c,0,f)
                    rows.append((op,f,size,s,bytes(c)))
    for _ in range(7000):
        f=0x10000|rng.randrange(64);c=bytearray(rng.randbytes(716));struct.pack_into('<I',c,0,f)
        rows.append((rng.randrange(2),f,716,rng.randbytes(712),bytes(c)))
    return rows

def payload(rows):return b''.join(struct.pack('<3I',*r[:3])+r[3]+r[4] for r in rows)
def expected(rows):return b''.join(struct.pack('<I',a)+b+c for a,b,c in map(model,rows))

def fixture(tmp_path,mutation=None,native=False):
    for n in ('guest_x86_context.h','guest_x86_context.c'):(tmp_path/n).write_bytes((ROOT/'src/runtime'/n).read_bytes())
    if mutation:
        p=tmp_path/'guest_x86_context.c';s=p.read_text(encoding='utf-8');a,b=mutation;assert s.count(a)==1;p.write_bytes(s.replace(a,b).encode())
    p=tmp_path/'x86_context_contract.c';p.write_bytes((ROOT/'tests/compiler'/p.name).read_bytes())
    if not native:return _compile(tmp_path,p,extra=[tmp_path/'guest_x86_context.c',*(['/W4'] if os.name=='nt' else ['-Wall','-Wextra'])])
    cc,env=native_compiler(tmp_path);cc=Path(cc).parent.parent/'x86/cl.exe';assert cc.is_file()
    env=dict(env);env['PATH']=str(cc.parent)+os.pathsep+env['PATH'];env['LIB']=re.sub(r'(?i)([\\/])x64(?=;|$)',r'\1x86',env['LIB']);env['VSCMD_ARG_TGT_ARCH']='x86'
    exe=tmp_path/'native.exe';r=subprocess.run([str(cc),'/nologo','/std:c11','/O2','/W4','/WX','/DTSFP_NATIVE_CONTEXT_ORACLE=1',str(p),str(tmp_path/'guest_x86_context.c'),'/Fe:'+str(exe)],env=env,cwd=tmp_path,capture_output=True,text=True,timeout=60);assert r.returncode==0,r.stdout+r.stderr
    return exe

def agrees(exe):
    rows=requests();r=subprocess.run([str(exe)],input=payload(rows),capture_output=True,timeout=30)
    return r.returncode==0 and r.stdout==expected(rows)

def test_context_groups_bytes_guards_rejections_and_aliases(tmp_path):assert agrees(fixture(tmp_path))

@pytest.mark.parametrize('mutation',[
 ('WORD(1,184,r_eip)','WORD(1,184,r_eax)'),
 ('WORD(4,144,fs)','WORD(2,144,fs)'),
 ('flags=get(p);if(!flags_valid(flags))return 0;saved=*state;','flags=get(p);if(!flags_valid(flags))return 0;memset(&saved,0,sizeof saved);'),
 ('put(p,flags);return 1;','put(p,flags|32);return 1;'),
 ('memcpy(saved.x87,p+28,sizeof saved.x87);','memcpy(saved.x87,p+28,sizeof saved.x87-1);'),
])
def test_compiled_incorrect_context_codec_rejected(tmp_path,mutation):assert not agrees(fixture(tmp_path,mutation))

@pytest.mark.skipif(os.name!='nt',reason='actual Win32 SDK context layout')
def test_i386_sdk_context_groups_independent_of_host_abi(tmp_path):assert agrees(fixture(tmp_path,native=True))
